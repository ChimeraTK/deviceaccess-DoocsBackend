#pragma once

#include <map>
#include <mutex>

#include <eq_fct.h>

#include <ChimeraTK/VersionNumber.h>

constexpr uint32_t MaxSizeEventIdMap = 2000;

class EventIdMapper {
 public:
  static EventIdMapper& getInstance() {
    static EventIdMapper instance;
    return instance;
  }

  ChimeraTK::VersionNumber getVersionForEventId(const doocs::EventId& eventId);

 private:
  EventIdMapper() = default;
  ~EventIdMapper() = default;
  EventIdMapper(const EventIdMapper&) = delete;
  EventIdMapper& operator=(const EventIdMapper&) = delete;

  std::mutex _mapMutex;
  std::map<doocs::EventId, ChimeraTK::VersionNumber> _eventIdToVersionMap{};
};
