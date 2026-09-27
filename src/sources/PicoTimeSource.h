// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "clock/TimeSource.h"

namespace aac {
// Production external-source slot. Future transport/decoder stays behind this API.
// Locked requires v1 UTC_VALID + PPS_PRESENT + PPS_LOCKED + SYNC_VALID AND
// unambiguous captured-edge association. GPS_VALID is not a prerequisite.
class PicoTimeSource final : public TimeSource {
public:
    PicoTimeSource() { reset(); }
    void begin(MonotonicUs) override { reset(); }
    void poll(MonotonicUs) override {} // No SPI, pins, interrupts, or fabricated UTC.
    const SourceState& state() const override { return state_; }
private:
    void reset() {
        state_ = SourceState{};
        state_.id = SourceId::Pico;
        state_.error = SourceError::NotImplemented;
    }
    SourceState state_;
};
} // namespace aac
