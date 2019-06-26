#include "ZMQSubscriptionManager.h"
#include "DoocsBackendRegisterAccessor.h"

namespace ChimeraTK { namespace DoocsBackendNamespace {

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // create subscription if not yet existing
    if(subscriptionMap.find(path) == subscriptionMap.end()) {
      // subscribe to property
      EqData dst;
      EqAdr ea;
      ea.adr(path);
      int err = dmsg_attach(&ea, &dst, (void*)&(subscriptionMap[path]), &zmq_callback, &subscriptionMap[path].tag);
      if(err) {
        throw ChimeraTK::runtime_error(
            std::string("Cannot subscribe to DOOCS property '" + path + "' via ZeroMQ: ") + dst.get_string());
      }

      // run dmsg_start() once
      std::unique_lock<std::mutex> lck(dmsgStartCalled_mutex);
      if(!dmsgStartCalled) {
        dmsg_start();
        dmsgStartCalled = true;
      }
    }

    // add accessor to list of listeners
    subscriptionMap[path].listeners.push_back(accessor);
  }

  /******************************************************************************************************************/

  void ZMQSubscriptionManager::unsubscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor) {
    std::unique_lock<std::mutex> lock(subscriptionMap_mutex);

    // ignore if no subscription exists
    if(subscriptionMap.find(path) == subscriptionMap.end()) return;

    // remove accessor from list of listeners
    std::remove(subscriptionMap[path].listeners.begin(), subscriptionMap[path].listeners.end(), accessor);

    // if no listener left, delete the subscription
    if(subscriptionMap[path].listeners.empty()) {
      EqAdr ea;
      ea.adr(path);
      dmsg_detach(&ea, subscriptionMap[path].tag);
      subscriptionMap.erase(subscriptionMap.find(path));
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
    for(auto& listener : subscription->listeners) {
      listener->notifications.push_overwrite(*data);
    }
  }

}} // namespace ChimeraTK::DoocsBackendNamespace
