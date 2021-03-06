#pragma once

#include <string>
#include <future>
#include <utility>
#include <memory>

#include <ChimeraTK/RegisterCatalogue.h>

class CatalogueFetcher {
 public:
  CatalogueFetcher(const std::string& serverAddress, std::future<void> cancelIndicator)
  : serverAddress_(serverAddress), cancelFlag_(std::move(cancelIndicator)) {}

  std::pair<std::unique_ptr<ChimeraTK::RegisterCatalogue>, bool> fetch();

 private:
  std::string serverAddress_;
  std::future<void> cancelFlag_;
  std::unique_ptr<ChimeraTK::RegisterCatalogue> catalogue_{};
  bool locationLookupError_{false};

  void fillCatalogue(std::string fixedComponents, long level);
  bool isCancelled() const { return (cancelFlag_.wait_for(std::chrono::microseconds(0)) == std::future_status::ready); }
  bool checkZmqAvailability(const std::string& fullLocationPath, const std::string& propertyName) const;
};

