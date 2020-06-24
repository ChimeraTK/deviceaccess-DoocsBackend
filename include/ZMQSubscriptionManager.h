#ifndef CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H
#define CHIMERATK_DOOCS_BACKEND_ZMQSUBSCRIPTIONMANAGER_H

#include <pthread.h>

#include <string>
#include <mutex>
#include <map>
#include <vector>

#include <boost/shared_ptr.hpp>

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

      /// Activate all subscriptions. Should be called from DoocsBackend::activateAsyncRead().
      void activateAll();

      /// Deactivate all subscriptions and push exceptions into the queues. Should be called from
      /// DoocsBackend::setException().
      void deactivateAllAndPushException();

     private:
      ZMQSubscriptionManager();
      ~ZMQSubscriptionManager();

      // Activate ZeroMQ subscription.
      // Caller need to own subscriptionMap_mutex and corresponding listeners_mutex already.
      void activate(const std::string& path);

      // Deactivate ZeroMQ subscription. Caller must not own subscriptionMap_mutex or the listeners_mutex.
      void deactivate(const std::string& path);

      /** static flag if dmsg_start() has been called already, with mutex for thread safety */
      bool dmsgStartCalled;
      std::mutex dmsgStartCalled_mutex;

      /// Flag whether new subscriptions should be active or not, with mutex for thread safety.
      bool subscriptionsActive{false};
      std::mutex subscriptionsActive_mutex;

      /// Structure describing a single subscription
      struct Subscription {
        Subscription() : zqmThreadId(ZMQSubscriptionManager::pthread_t_invalid) {}

        /// list of accessors listening to the ZMQ subscription.
        /// Accessing this vector requires holding zmq_callback_extra_listeners_mutex
        /// It's ok to use plain pointers here, since accessors will unsubscribe themselves in their destructor
        std::vector<DoocsBackendRegisterAccessorBase*> listeners;

        /// Mutex for zmq_callback_extra_listeners
        std::mutex listeners_mutex;

        /// Thread ID of the ZeroMQ subscription thread which calls zmq_callback. This is a DOOCS thread and hence
        /// outside our control. We store the ID inside zmq_callback so we can check whether the thread has been
        /// properly terminated in the cleanup phase. listeners_mutex must be held while accessing this variable.
        pthread_t zqmThreadId;

        /// Flag whether the subscription is currently active, i.e. whether the actual DOOCS subscription has been made.
        /// This is used to implement activateAsyncRead() properly. listeners_mutex must be held while accessing this
        /// variable.
        bool active{false};

        /// Flag whether an exception has been reported to the listeners since the last activation. Used to prevent
        /// duplicate exceptions in setException(). Will be cleared during activation. Access requires listeners_mutex.
        bool hasException{false};
      };

      /// A "random" value of pthread_t which we consider "invalid" in context of Subscription::zqmThreadId. Technically
      /// we use a valid pthread_t of a thread which cannot be a ZeroMQ subscription thread. All values of
      /// Subscription::zqmThreadId will be initialised with this value.
      static pthread_t pthread_t_invalid;

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
