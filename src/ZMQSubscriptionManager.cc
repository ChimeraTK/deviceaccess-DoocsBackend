#include "ZMQSubscriptionManager.h"
#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK { namespace DoocsBackendNamespace {

  /******************************************************************************************************************/

  pthread_t ZMQSubscriptionManager::pthread_t_invalid;

  /******************************************************************************************************************/

  ZMQSubscriptionManager::ZMQSubscriptionManager() { pthread_t_invalid = pthread_self(); }

  /******************************************************************************************************************/

  ZMQSubscriptionManager::~ZMQSubscriptionManager() {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    for(auto subscription = subscriptionMap.begin(); subscription != subscriptionMap.end();) {
      {
        std::unique_lock<std::mutex> listeners_lock(subscription->second.listeners_mutex);
        subscription->second.listeners.clear();
      }
      lock.unlock();
      deactivate(subscription->first);
      lock.lock();
      subscription = subscriptionMap.erase(subscription);
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    bool initialValueReadRequired = false;

    {
      std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

      // create subscription if not yet existing
      if(subscriptionMap.find(path) == subscriptionMap.end()) {
        std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
        if(subscriptionsActive) {
          activate(path);
        }
      }
      else {
        // No new DOOCS subscription is made => we need to poll the initial value on our own
        initialValueReadRequired = true;
      }

      // gain lock for listener, to exclude concurrent access with the zmq_callback()
      std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

      // add accessor to list of listeners
      subscriptionMap[path].listeners.push_back(accessor);
    }

    // If required, poll the initial value and push it into the queue. This must be done after the subcription has been
    // made.
    if(initialValueReadRequired) {
      EqData src, dst;
      EqAdr adr;
      EqCall eq;
      adr.adr(path);
      auto rc = eq.get(&adr, &src, &dst);
      if(rc) {
        // This will push exceptions to all receivers, incuding the accessor. It will have the wrong message, though.
        // This should be corrected, but we need a hasException per accessor then...
        accessor->_backend->informRuntimeError(path);
      }
      else {
        accessor->notifications.push_overwrite(dst);
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::unsubscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // ignore if no subscription exists
    if(subscriptionMap.find(path) == subscriptionMap.end()) return;

    // gain lock for listener, to exclude concurrent access with the zmq_callback()
    std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

    // ignore if subscription exists but not for this accessor
    if(std::find(subscriptionMap[path].listeners.begin(), subscriptionMap[path].listeners.end(), accessor) ==
        subscriptionMap[path].listeners.end())
      return;

    // remove accessor from list of listeners
    subscriptionMap[path].listeners.erase(
        std::remove(subscriptionMap[path].listeners.begin(), subscriptionMap[path].listeners.end(), accessor));

    // if no listener left, delete the subscription
    if(subscriptionMap[path].listeners.empty()) {
      // temporarily unlock the locks which might block the ZQM subscription thread
      listeners_lock.unlock();
      lock.unlock();

      // remove ZMQ subscription. This will also join the ZMQ subscription thread
      deactivate(path);

      // obtain locks again
      lock.lock();

      // remove subscription from map
      subscriptionMap.erase(subscriptionMap.find(path));
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::activate(const std::string& path) {
    // do nothing if already active
    if(subscriptionMap[path].active) return;

    // subscribe to property
    EqData dst;
    EqAdr ea;
    ea.adr(path);
    dmsg_t tag;
    int err = dmsg_attach(&ea, &dst, (void*)&(subscriptionMap[path]), &zmq_callback, &tag);
    if(err) {
      /// FIXME put error into queue of all accessors!
      throw ChimeraTK::runtime_error(
          std::string("Cannot subscribe to DOOCS property '" + path + "' via ZeroMQ: ") + dst.get_string());
    }

    // run dmsg_start() once
    std::unique_lock<std::mutex> lck(dmsgStartCalled_mutex);
    if(!dmsgStartCalled) {
      dmsg_start();
      dmsgStartCalled = true;
    }

    // set active flag, reset hasException flag
    subscriptionMap[path].active = true;
    subscriptionMap[path].hasException = false;
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivate(const std::string& path) {
    // do nothing if already inactive
    {
      std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
      std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);
      if(!subscriptionMap[path].active) return;

      // Wait until we have seen any reaction from ZMQ. This is to work around a race condition in DOOCS's ZMQ
      // subcription mechanism
      while(not subscriptionMap[path].started) subscriptionMap[path].startedCv.wait(listeners_lock);

      // clear active flag
      subscriptionMap[path].active = false;
    }

    // remove ZMQ subscription. This will also join the ZMQ subscription thread
    EqAdr ea;
    ea.adr(path);
    dmsg_detach(&ea, nullptr); // nullptr = remove all subscriptions for that address
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::activateAll() {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
    std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
    subscriptionsActive = true;

    for(auto& subscription : subscriptionMap) {
      std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
      activate(subscription.first);
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivateAll() {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
    std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
    subscriptionsActive = false;

    for(auto& subscription : subscriptionMap) {
      // decativate subscription
      lock.unlock();
      deactivate(subscription.first);
      lock.lock();
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivateAllAndPushException() {
    deactivateAll();

    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
    std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);

    for(auto& subscription : subscriptionMap) {
      // put exception to queue
      {
        std::unique_lock<std::mutex> listeners_lock(subscription.second.listeners_mutex);
        if(subscription.second.hasException) continue;
        subscription.second.hasException = true;
        for(auto& listener : subscription.second.listeners) {
          try {
            throw ChimeraTK::runtime_error("Exception reported by another accessor.");
          }
          catch(...) {
            listener->notifications.push_overwrite_exception(std::current_exception());
          }
        }
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::zmq_callback(void* self_, EqData* data, dmsg_info_t* info) {
    // obtain pointer to subscription object
    auto* subscription = static_cast<ZMQSubscriptionManager::Subscription*>(self_);

    // Make sure the stamp is used from the ZeroMQ header. TODO: Is this really wanted?
    data->time(info->sec, info->usec);
    data->mpnum(info->ident);

    std::unique_lock<std::mutex> lock(subscription->listeners_mutex);

    // As long as we get a callback from ZMQ, we consider it started
    if(not subscription->started) {
      subscription->started = true;
      subscription->startedCv.notify_all();
    }

    // store thread id of the thread calling this function, if not yet done
    if(pthread_equal(subscription->zqmThreadId, pthread_t_invalid)) {
      subscription->zqmThreadId = pthread_self();
    }

    // check for error
    if(data->error() != no_connection) {
      // no error: push the data
      for(auto& listener : subscription->listeners) {
        listener->notifications.push_overwrite(*data);
      }
    }
    else {
      // no error: push the data
      try {
        throw ChimeraTK::runtime_error("ZeroMQ connection interrupted: " + data->get_string());
      }
      catch(...) {
        subscription->hasException = true;
        for(auto& listener : subscription->listeners) {
          listener->notifications.push_overwrite_exception(std::current_exception());
          lock.unlock();
          listener->_backend->informRuntimeError(listener->_path);
          lock.lock();
        }
      }
    }
  }

}} // namespace ChimeraTK::DoocsBackendNamespace
