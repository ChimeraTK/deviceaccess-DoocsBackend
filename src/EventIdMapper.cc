#include "EventIdMapper.h"

ChimeraTK::VersionNumber EventIdMapper::getVersionForEventId(const doocs::EventId& eventId) {
  std::lock_guard locker{_mapMutex};
  return _eventIdToVersionMap[eventId];
}
