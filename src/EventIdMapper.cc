#include "EventIdMapper.h"

ChimeraTK::VersionNumber EventIdMapper::getVersionForEventId(const doocs::EventId& eventId) {
  if(eventId.to_int() == 0) return {}; // event id will be 0 e.g. if value was written from the panel
  std::lock_guard<decltype(_mapMutex)> locker{_mapMutex};
  return _eventIdToVersionMap[eventId];
}