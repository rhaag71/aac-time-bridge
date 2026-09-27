// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

namespace aac {

// Local monotonic microseconds since ESP32 boot, never Unix time.
using MonotonicUs = uint64_t;
enum class SourceId { None, Pico };
enum class Availability { Unavailable, Available };
enum class TimeValidity { Invalid, Valid };
enum class SyncQuality { Unsynchronized, Locked, Holdover };
enum class Freshness { Unknown, Fresh, Stale };
enum class SourceError { None, NotImplemented, Transport, InvalidData, AssociationLost };
enum class Presence { Unknown, Known };

struct ObservationTime {
    Presence presence = Presence::Unknown;
    MonotonicUs atUs = 0;
};

// A mapping, not a running wall clock. No extrapolation/accuracy claim yet.
struct TimeAnchor {
    Presence presence = Presence::Unknown;
    int64_t utcSeconds = 0;
    uint32_t nanoseconds = 0;
    MonotonicUs localUs = 0;
};

struct SourceState {
    SourceId id = SourceId::None;
    Availability availability = Availability::Unavailable;
    TimeValidity validity = TimeValidity::Invalid;
    SyncQuality quality = SyncQuality::Unsynchronized;
    SourceError error = SourceError::None;
    TimeAnchor anchor;
    // Contact may advance without a new usable timing update (e.g. repeated SPI).
    ObservationTime lastContact;
    ObservationTime lastUpdate;
    // Exclusive deadline for timing usability; unknown means never eligible.
    ObservationTime usableUntil;
};

class TimeSource {
public:
    virtual ~TimeSource() = default;
    virtual void begin(MonotonicUs now) = 0;
    virtual void poll(MonotonicUs now) = 0;
    virtual const SourceState& state() const = 0;
};

} // namespace aac
