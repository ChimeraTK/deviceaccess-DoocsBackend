#ifndef CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H
#define CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H

#include <string>
#include <mutex>
#include <map>
#include <vector>

#include <eq_fct.h>

#include <ChimeraTK/Exception.h>

namespace ChimeraTK {

  class DoocsBackendRegisterAccessorBase;

  namespace DoocsBackendNamespace {

    class ZMQSubscriptionManager {
     public:
      static ZMQSubscriptionManager& getInstance() {
        static ZMQSubscriptionManager manager;
        return manager;
      }

      void subscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor);
      void unsubscribe(const std::string& path, DoocsBackendRegisterAccessorBase* accessor);

     private:
      /** static flag if dmsg_start() has been called already, with mutex for thread safety */
      bool dmsgStartCalled;
      std::mutex dmsgStartCalled_mutex;

      /// Structure describing a single subscription
      struct Subscription {
        /// list of accessors listening to the ZMQ subscription.
        /// Accessing this vector requires holding zmq_callback_extra_listeners_mutex
        std::vector<DoocsBackendRegisterAccessorBase*> listeners;

        /// Mutex for zmq_callback_extra_listeners
        std::mutex listeners_mutex;

        /// cached dmsg tag needed for cleanup
        dmsg_t tag;
      };

      /// map of subscriptions
      std::map<std::string, Subscription> subscriptionMap;

      /// mutex for subscriptionMap
      std::mutex subscriptionMap_mutex;

      /// callback function for ZeroMQ
      /// This is a static function so we can pass a plain pointer to the DOOCS
      /// client. The first argument will contain the pointer to the object (will be
      /// statically casted into DoocsBackendRegisterAccessor<UserType>*).
      static void zmq_callback(void* self, EqData* data, dmsg_info_t* info);
    };

  } // namespace DoocsBackendNamespace

} // namespace ChimeraTK

#endif // CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H
