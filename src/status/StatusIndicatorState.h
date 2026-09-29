// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "ApplianceState.h"

namespace aac {

enum class StatusLedPattern { Off, SolidOn, SlowBlink, FastBlink, DoubleBlink };

constexpr MonotonicUs kStatusLedSlowOnUs = 500000ULL;
constexpr MonotonicUs kStatusLedSlowOffUs = 500000ULL;
constexpr MonotonicUs kStatusLedFastOnUs = 125000ULL;
constexpr MonotonicUs kStatusLedFastOffUs = 125000ULL;
constexpr MonotonicUs kStatusLedDoublePulseUs = 150000ULL;
constexpr MonotonicUs kStatusLedDoubleGapUs = 200000ULL;
constexpr MonotonicUs kStatusLedDoublePauseUs = 1100000ULL;

inline StatusLedPattern statusLedPatternFor(const ApplianceState& state) {
    if (!state.initialized) return StatusLedPattern::Off;

    // The normal LAN path has priority over source health. Recovery AP activity
    // also signals that normal station operation is not the current user path.
    switch (state.network.status) {
    case NetworkStatus::Unconfigured:
    case NetworkStatus::Connecting:
    case NetworkStatus::Disconnected:
        return StatusLedPattern::DoubleBlink;
    case NetworkStatus::Connected:
        if (state.network.provisioning) return StatusLedPattern::DoubleBlink;
        break;
    default:
        return StatusLedPattern::Off;
    }

    switch (state.clock.pico.report.availability) {
    case Availability::Unavailable:
        return StatusLedPattern::FastBlink;
    case Availability::Available:
        break;
    default:
        return StatusLedPattern::Off;
    }

    if (authoritativeUtcAvailable(state)) return StatusLedPattern::SolidOn;

    // A responding endpoint without currently qualified authority remains a
    // source-quality state, not a communication failure.
    return StatusLedPattern::SlowBlink;
}

inline bool statusLedOnAt(StatusLedPattern pattern, MonotonicUs nowUs) {
    switch (pattern) {
    case StatusLedPattern::Off: return false;
    case StatusLedPattern::SolidOn: return true;
    case StatusLedPattern::SlowBlink:
        return nowUs % (kStatusLedSlowOnUs + kStatusLedSlowOffUs) < kStatusLedSlowOnUs;
    case StatusLedPattern::FastBlink:
        return nowUs % (kStatusLedFastOnUs + kStatusLedFastOffUs) < kStatusLedFastOnUs;
    case StatusLedPattern::DoubleBlink: {
        const MonotonicUs phase = nowUs %
            (kStatusLedDoublePulseUs * 2 + kStatusLedDoubleGapUs + kStatusLedDoublePauseUs);
        return phase < kStatusLedDoublePulseUs ||
            (phase >= kStatusLedDoublePulseUs + kStatusLedDoubleGapUs &&
             phase < kStatusLedDoublePulseUs * 2 + kStatusLedDoubleGapUs);
    }
    }
    return false;
}

} // namespace aac
