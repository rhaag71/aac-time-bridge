// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "ClockState.h"

namespace aac {
namespace {
SourceView observe(const SourceState& source, MonotonicUs now) {
    SourceView view;
    view.report = source;
    if (source.lastUpdate.presence == Presence::Known && source.lastUpdate.atUs <= now) {
        view.ageKnown = Presence::Known;
        view.ageUs = now - source.lastUpdate.atUs;
        if (source.usableUntil.presence == Presence::Known &&
            source.usableUntil.atUs > source.lastUpdate.atUs) {
            view.freshness = now < source.usableUntil.atUs ? Freshness::Fresh : Freshness::Stale;
        }
    }
    return view;
}

bool usable(const SourceView& view, SourceId expected, SyncQuality quality, MonotonicUs now) {
    const auto& s = view.report;
    return s.id == expected && s.availability == Availability::Available &&
           s.validity == TimeValidity::Valid && s.quality == quality &&
           s.error == SourceError::None && view.freshness == Freshness::Fresh &&
           s.anchor.presence == Presence::Known && s.anchor.localUs <= now &&
           s.anchor.localUs <= s.lastUpdate.atUs && s.anchor.nanoseconds < 1000000000UL;
}
} // namespace

ClockState selectClock(const SourceState& pico, MonotonicUs now) {
    ClockState state;
    state.evaluatedAtUs = now;
    state.pico = observe(pico, now);
    const SourceState* selected = nullptr;
    if (usable(state.pico, SourceId::Pico, SyncQuality::Locked, now)) {
        selected = &pico;
    }
    if (selected) {
        state.selected = selected->id;
        state.status = ClockStatus::Synchronized;
        state.anchor = selected->anchor;
        state.quality = selected->quality;
    }
    return state;
}

void ClockCoordinator::begin(MonotonicUs now) {
    pico_.begin(now);
    poll(now);
}

void ClockCoordinator::poll(MonotonicUs now) {
    pico_.poll(now);
    state_ = selectClock(pico_.state(), now);
}
} // namespace aac
