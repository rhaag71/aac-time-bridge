// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "TimeSource.h"

namespace aac {

struct SourceView {
    SourceState report;
    Freshness freshness = Freshness::Unknown;
    Presence ageKnown = Presence::Unknown;
    MonotonicUs ageUs = 0;
};

enum class ClockStatus { Unsynchronized, Synchronized };
struct ClockState {
    MonotonicUs evaluatedAtUs = 0;
    SourceView pico;
    SourceId selected = SourceId::None;
    ClockStatus status = ClockStatus::Unsynchronized;
    TimeAnchor anchor;
    SyncQuality quality = SyncQuality::Unsynchronized;
};

// Pure policy: Only qualified external time is eligible. Holdover is representable but ineligible.
ClockState selectClock(const SourceState& pico, MonotonicUs now);

// Single writer in the application loop; consumers receive a const snapshot.
class ClockCoordinator {
public:
    explicit ClockCoordinator(TimeSource& pico) : pico_(pico) {}
    void begin(MonotonicUs now);
    void poll(MonotonicUs now);
    const ClockState& state() const { return state_; }
private:
    TimeSource& pico_;
    ClockState state_;
};
} // namespace aac
