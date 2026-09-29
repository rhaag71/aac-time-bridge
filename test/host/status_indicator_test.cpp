#include "status/StatusIndicatorState.h"
#include <assert.h>

using namespace aac;

static SourceState qualifiedSource() {
    SourceState source;
    source.id = SourceId::Pico;
    source.availability = Availability::Available;
    source.validity = TimeValidity::Valid;
    source.quality = SyncQuality::Locked;
    source.error = SourceError::None;
    source.anchor.presence = Presence::Known;
    source.anchor.utcSeconds = 1800000000;
    source.anchor.localUs = 1000000;
    source.lastUpdate.presence = Presence::Known;
    source.lastUpdate.atUs = 1000000;
    source.usableUntil.presence = Presence::Known;
    source.usableUntil.atUs = 5000000;
    return source;
}

static ApplianceState goodState() {
    ApplianceState state;
    state.initialized = true;
    state.uptimeUs = 1100000;
    state.network.status = NetworkStatus::Connected;
    state.clock = selectClock(qualifiedSource(), state.uptimeUs);
    return state;
}

int main() {
    ApplianceState state = goodState();
    state.initialized = false;
    assert(statusLedPatternFor(state) == StatusLedPattern::Off);

    state = goodState();
    assert(statusLedPatternFor(state) == StatusLedPattern::SolidOn);
    assert(statusLedOnAt(StatusLedPattern::SolidOn, 0));
    assert(!statusLedOnAt(StatusLedPattern::Off, 0));

    state.network.status = NetworkStatus::Disconnected;
    assert(statusLedPatternFor(state) == StatusLedPattern::DoubleBlink);
    state.clock.pico.report.availability = Availability::Unavailable;
    assert(statusLedPatternFor(state) == StatusLedPattern::DoubleBlink); // Network fault has priority over AAC loss.
    state.network.status = NetworkStatus::Connected;
    state.network.provisioning = true;
    assert(statusLedPatternFor(state) == StatusLedPattern::DoubleBlink);
    state.network.provisioning = false;
    state.network.status = NetworkStatus::Unconfigured;
    assert(statusLedPatternFor(state) == StatusLedPattern::DoubleBlink); // Virgin setup AP / no station link.

    state = goodState();
    state.clock.pico.report.availability = Availability::Unavailable;
    assert(statusLedPatternFor(state) == StatusLedPattern::FastBlink);

    state = goodState();
    state.clock.pico.report.validity = TimeValidity::Invalid;
    assert(statusLedPatternFor(state) == StatusLedPattern::SlowBlink);
    state = goodState();
    state.uptimeUs = 5000000;
    assert(statusLedPatternFor(state) == StatusLedPattern::SlowBlink); // Authority expired, endpoint still responds.

    const auto slow = StatusLedPattern::SlowBlink;
    assert(statusLedOnAt(slow, 0));
    assert(statusLedOnAt(slow, kStatusLedSlowOnUs - 1));
    assert(!statusLedOnAt(slow, kStatusLedSlowOnUs));
    assert(statusLedOnAt(slow, kStatusLedSlowOnUs + kStatusLedSlowOffUs));

    const auto fast = StatusLedPattern::FastBlink;
    assert(statusLedOnAt(fast, 0));
    assert(statusLedOnAt(fast, kStatusLedFastOnUs - 1));
    assert(!statusLedOnAt(fast, kStatusLedFastOnUs));
    assert(statusLedOnAt(fast, kStatusLedFastOnUs + kStatusLedFastOffUs));

    const auto doubleBlink = StatusLedPattern::DoubleBlink;
    constexpr MonotonicUs secondPulse = kStatusLedDoublePulseUs + kStatusLedDoubleGapUs;
    constexpr MonotonicUs cycle = kStatusLedDoublePulseUs * 2 + kStatusLedDoubleGapUs + kStatusLedDoublePauseUs;
    assert(statusLedOnAt(doubleBlink, 0));
    assert(!statusLedOnAt(doubleBlink, kStatusLedDoublePulseUs));
    assert(statusLedOnAt(doubleBlink, secondPulse));
    assert(!statusLedOnAt(doubleBlink, secondPulse + kStatusLedDoublePulseUs));
    assert(!statusLedOnAt(doubleBlink, cycle - 1));
    assert(statusLedOnAt(doubleBlink, cycle));
}
