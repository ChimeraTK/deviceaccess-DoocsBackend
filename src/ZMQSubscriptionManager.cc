#include "ZMQSubscriptionManager.h"
#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK { namespace DoocsBackendNamespace {

  /******************************************************************************************************************/

  pthread_t ZMQSubscriptionManager::pthread_t_invalid;

  /******************************************************************************************************************/

  ZMQSubscriptionManager::ZMQSubscriptionManager() { pthread_t_invalid = pthread_self(); }

  /******************************************************************************************************************/

  ZMQSubscriptionManager::~ZMQSubscriptionManager() {
    for(auto subscription = subscriptionMap.begin(); subscription != subscriptionMap.end();) {
      auto subscriptionToWork = subscription++;
      for(auto accessor = subscriptionToWork->second.listeners.rbegin();
          accessor != subscriptionToWork->second.listeners.rend();) {
        auto accessorToWork = accessor++;
        unsubscribe(subscriptionToWork->first, *accessorToWork);
      }
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // create subscription if not yet existing
    if(subscriptionMap.find(path) == subscriptionMap.end()) {
      std::unique_lock<std::mutex> lk_subact(subscriptionsActive_mutex);
      if(subscriptionsActive) {
        activate(path);
      }
    }

    // gain lock for listener, to exclude concurrent access with the zmq_callback()
    std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

    // add accessor to list of listeners
    subscriptionMap[path].listeners.push_back(accessor);
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::unsubscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // ignore if no subscription exists
    if(subscriptionMap.find(path) == subscriptionMap.end()) return;

    // gain lock for listener, to exclude concurrent access with the zmq_callback()
    std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);

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

    // set active flag
    subscriptionMap[path].active = true;
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::deactivate(const std::string& path) {
    // do nothing if already inactive
    {
      std::unique_lock<std::mutex> lock(subscriptionMap_mutex);
      std::unique_lock<std::mutex> listeners_lock(subscriptionMap[path].listeners_mutex);
      if(!subscriptionMap[path].active) return;

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
      lock.unlock();
      deactivate(subscription.first);
      lock.lock();
    }
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::zmq_callback(void* self_, EqData* data, dmsg_info_t* info) {
    // obtain pointer to accessor object
    auto* subscription = static_cast<ZMQSubscriptionManager::Subscription*>(self_);

    data->time(info->sec, info->usec);
    data->mpnum(info->ident);

    // if there are more listeners, add a copy of the EqData to their queues as well
    std::unique_lock<std::mutex> lock(subscription->listeners_mutex);
    if(pthread_equal(subscription->zqmThreadId, pthread_t_invalid)) {
      subscription->zqmThreadId = pthread_self();
    }
    for(auto& listener : subscription->listeners) {
      listener->notifications.push_overwrite(*data);
    }
  }

}} // namespace ChimeraTK::DoocsBackendNamespace
