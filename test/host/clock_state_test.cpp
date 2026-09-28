// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "clock/ClockState.h"
#include "status/ApplianceState.h"
#include "network/Configuration.h"
#include "network/HttpRequest.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>

using namespace aac;

class UnavailablePico : public TimeSource {
public:
    UnavailablePico() { state_.id = SourceId::Pico; state_.error = SourceError::NoCommunication; }
    void begin(MonotonicUs) override { state_.anchor = TimeAnchor{}; }
    void poll(MonotonicUs) override {}
    const SourceState& state() const override { return state_; }
private:
    SourceState state_;
};

SourceState ready(SourceId id) {
    SourceState s;
    s.id = id;
    s.availability = Availability::Available;
    s.validity = TimeValidity::Valid;
    s.quality = SyncQuality::Locked;
    s.anchor.presence = Presence::Known;
    s.anchor.utcSeconds = 2200000000LL; // Beyond 2038, without narrowing.
    s.anchor.localUs = 100;
    s.lastContact.presence = Presence::Known;
    s.lastContact.atUs = 110;
    s.lastUpdate = s.lastContact;
    s.usableUntil.presence = Presence::Known;
    s.usableUntil.atUs = 160;
    return s;
}

int main() {
    UnavailablePico pico;
    ClockCoordinator clock(pico);
    clock.begin(0);
    for (MonotonicUs now : {0ULL, 1500000ULL, 5000000000ULL}) {
        clock.poll(now);
        const auto& s = clock.state();
        assert(s.selected == SourceId::None);
        assert(s.status == ClockStatus::Unsynchronized);
        assert(s.anchor.presence == Presence::Unknown);
        assert(s.pico.report.id == SourceId::Pico);
        assert(s.pico.report.error == SourceError::NoCommunication);
        assert(s.pico.report.availability == Availability::Unavailable);
        assert(s.pico.report.validity == TimeValidity::Invalid);
        assert(s.pico.freshness == Freshness::Unknown);
        assert(s.pico.ageKnown == Presence::Unknown);
    }
    auto p = ready(SourceId::Pico);
    auto s = selectClock(p, 120);
    assert(s.selected == SourceId::Pico);
    assert(s.anchor.utcSeconds == 2200000000LL);
    assert(s.pico.ageUs == 10);
    assert(selectClock(p, 159).selected == SourceId::Pico);
    s = selectClock(p, 160);
    assert(s.selected == SourceId::None);
    assert(s.pico.freshness == Freshness::Stale);
    // Repeated contact does not refresh the timing anchor/deadline.
    p.lastContact.atUs = 170;
    assert(selectClock(p, 170).selected == SourceId::None);
    s = selectClock(p, 300);
    assert(s.selected == SourceId::None);
    assert(s.anchor.presence == Presence::Unknown);
    assert(s.quality == SyncQuality::Unsynchronized);
    assert(s.pico.report.anchor.presence == Presence::Known); // Diagnostics retained.
    assert(selectClock(ready(SourceId::Pico), 120).selected == SourceId::Pico);

    // Every disqualification must clear authority; there is no fallback.
    for (int fault = 0; fault < 12; ++fault) {
        p = ready(SourceId::Pico);
        switch (fault) {
        case 0: p.availability = Availability::Unavailable; break;
        case 1: p.validity = TimeValidity::Invalid; break;
        case 2: p.quality = SyncQuality::Unsynchronized; break;
        case 3: p.quality = SyncQuality::Holdover; break;
        case 4: p.error = SourceError::AssociationLost; break;
        case 5: p.anchor.presence = Presence::Unknown; break;
        case 6: p.lastUpdate.presence = Presence::Unknown; break;
        case 7: p.usableUntil.presence = Presence::Unknown; break;
        case 8: p.lastUpdate.atUs = 121; break;
        case 9: p.anchor.localUs = 121; break;
        case 10: p.anchor.nanoseconds = 1000000000UL; break;
        case 11: p.usableUntil.atUs = p.lastUpdate.atUs; break;
        }
        assert(selectClock(p, 120).selected == SourceId::None);
    }
    p = ready(SourceId::Pico);
    p.id = SourceId::None;
    assert(selectClock(p, 120).selected == SourceId::None);
    p = ready(SourceId::Pico);
    // Monotonic timestamps above the 32-bit micros rollover remain intact.
    p.anchor.localUs += 5000000000ULL;
    p.lastUpdate.atUs += 5000000000ULL;
    p.usableUntil.atUs += 5000000000ULL;
    assert(selectClock(p, 5000000120ULL).pico.ageUs == 10);
    clock.begin(0); // Restart cannot inherit a usable association.
    assert(clock.state().selected == SourceId::None);
    ApplianceState app;
    app.clock = selectClock(ready(SourceId::Pico), 120);
    app.uptimeUs = 120;
    assert(!statusLedOn(app)); // Initialization gates the renderer.
    app.initialized = true;
    assert(statusLedOn(app));
    app.uptimeUs = 160;
    assert(!statusLedOn(app)); // A retained snapshot cannot outlive its deadline.
    int64_t utc = -7;
    assert(!currentUtc(app.clock, 160, utc) && utc == -7);
    assert(!currentUtc(app.clock, 99, utc));
    p = ready(SourceId::Pico);
    p.usableUntil.atUs = 4000000;
    p.anchor.nanoseconds = 900000000;
    s = selectClock(p, 120);
    assert(currentUtc(s, 200100, utc) && utc == 2200000001LL);
    assert(currentUtc(s, 1200100, utc) && utc == 2200000002LL);
    p.anchor.utcSeconds = INT64_MAX;
    s = selectClock(p, 120);
    assert(!currentUtc(s, 200100, utc)); // No signed overflow.
    p = ready(SourceId::Pico);
    p.anchor.localUs = p.lastUpdate.atUs = 0;
    assert(selectClock(p, 0).status == ClockStatus::Synchronized);

    Configuration c;
    assert(!validConfiguration(c));
    assert(parseConfiguration("wifi Test network\t12345678", c));
    assert(!strcmp(c.ssid, "Test network"));
    assert(parseConfiguration("wifi Open\t", c));
    assert(!parseConfiguration("wifi \t12345678", c));
    assert(!parseConfiguration("wifi SSID\tshort", c));
    assert(!parseConfiguration("wifi SSID password", c));
    assert(!parseConfiguration("wifi 123456789012345678901234567890123\t12345678", c));
    memset(c.ssid, 'x', sizeof(c.ssid));
    assert(!validConfiguration(c));
    c = Configuration{}; strcpy(c.ssid, "test");
    memset(c.password, 'a', 64); assert(validConfiguration(c));
    c.password[0] = 'z'; assert(!validConfiguration(c));
    c = Configuration{}; strcpy(c.ssid, "test"); c.version = 2;
    assert(!validConfiguration(c));
    HttpRequest request;
    const char* get = "GET / HTTP/1.1\r\nHost: bridge\r\n\r\n";
    HttpRequest::Result result = HttpRequest::Result::Pending;
    for (const char* ch = get; *ch; ++ch) result = request.append(*ch);
    assert(result == HttpRequest::Result::Status);
    request = HttpRequest{};
    const char* post = "POST / HTTP/1.1\r\n\r\n";
    for (const char* ch = post; *ch; ++ch) result = request.append(*ch);
    assert(result == HttpRequest::Result::Reject);
    request = HttpRequest{};
    for (int i = 0; i < 2048; ++i) result = request.append('x');
    assert(result == HttpRequest::Result::Reject);
    request = HttpRequest{};
    assert(request.append(0) == HttpRequest::Result::Reject);
    puts("clock, status, configuration and HTTP tests passed");
}
