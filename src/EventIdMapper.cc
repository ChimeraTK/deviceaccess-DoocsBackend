#include "EventIdMapper.h"

ChimeraTK::VersionNumber EventIdMapper::getVersionForEventId(const doocs::EventId& eventId) {
  std::lock_guard<decltype(_mapMutex)> locker{_mapMutex};
  return _eventIdToVersionMap[eventId];
}
