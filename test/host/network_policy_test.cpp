// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "network/NetworkPolicy.h"
#include "status/StatusPage.h"
#include "web/UiAssets.h"
#include <assert.h>
#include <initializer_list>
#include <string.h>
#include <stdio.h>
using namespace aac;
int main() {
    NetworkPolicy policy;
    policy.begin(0);
    ApplianceState state;
    char page[kUiResponseCapacity];
    for (bool linkReady : {false, true}) {
        const auto o = policy.observe(false, linkReady, true, 0, 10);
        state.network.status = o.status;
        assert(o.status == NetworkStatus::Unconfigured && !o.connected && !o.announceLan && !o.stopAp);
        assert(renderStatusPage(page, sizeof(page), state, 10) > 0);
        assert(strstr(page, "UNSYNCHRONIZED"));
        assert(strstr(page, "--:--:--"));
        assert(strstr(page, "unconfigured"));
        assert(!strstr(page, "connecting"));
        assert(!strstr(page, "Current UTC:"));
        assert(!strstr(page, "new Date("));
        assert(!strstr(page, "Every second needs a source."));
        assert(!strstr(page, "Time reference / appliance console"));
        const char* header = strstr(page, "<header>");
        const char* refresh = strstr(page, "id='refresh'");
        const char* clockPanel = strstr(page, "id='clock-panel'");
        assert(header && refresh && clockPanel && refresh > header && refresh < clockPanel);
        assert(strstr(page, "method='post' action='/manage/reboot'"));
        assert(strstr(page, "method='post' action='/manage/factory-reset'"));
        assert(!strstr(page, "confirmation page"));
        assert(!strcmp(networkStatusName(state.network.status), "unconfigured"));
    }
    policy.begin(0);
    auto o = policy.observe(true, false, false, 0, 0);
    assert(o.status == NetworkStatus::Connecting && !o.startAp && !o.announceLan);
    o = policy.observe(true, false, false, 30000000, 59999999);
    assert(!o.startAp); // Association without a usable DHCP IP is not readiness.
    o = policy.observe(true, false, false, 30000000, 60000000);
    assert(o.startAp && o.recoveryDue);
    o = policy.observe(true, true, true, 60000000, 65000000);
    assert(o.connected && o.status == NetworkStatus::Connected && o.stopAp && o.announceLan);
    char announcement[64];
    formatLanAnnouncement(announcement, sizeof(announcement), "192.168.88.123");
    assert(!strcmp(announcement, "LAN status: http://192.168.88.123/"));
    o = policy.observe(true, true, false, 60000000, 66000000);
    assert(!o.stopAp && !o.announceLan && !o.startAp);
    o = policy.observe(true, false, false, 70000000, 70000000);
    assert(o.lost && !o.startAp);
    o = policy.observe(true, false, false, 120000000, 129999999);
    assert(!o.startAp);
    o = policy.observe(true, false, false, 120000000, 130000000);
    assert(o.startAp);
    o = policy.observe(true, true, true, 130000000, 131000000);
    assert(o.stopAp && o.announceLan); // Recovery repeats the immediate LAN announcement.
    state.network.status = o.status;
    strcpy(state.network.address, "192.168.88.123");
    SourceState unavailablePico; unavailablePico.id = SourceId::Pico;
    unavailablePico.error = SourceError::NoCommunication;
    state.clock = selectClock(unavailablePico, 131000000);
    assert(renderStatusPage(page, sizeof(page), state, 131000000) > 0);
    assert(strstr(page, "connected"));
    assert(strstr(page, "--:--:--"));
    assert(strstr(page, "Authoritative source / AAC"));
    assert(!strstr(page, "External source / Pico"));
    assert(!strstr(page, "Selected authority</div><strong id='selected-authority'>Pico"));
    assert(!strstr(page, "PRIVATE-secret9")); // Status rendering has no credential input.
    assert(!strstr(page, "name='ssid'")); // Setup is absent off the recovery AP.
    assert(strstr(page, "href='/ui.css'"));
    assert(strstr(page, "src='/ui.js'"));
    assert(strstr(kUiJs, "fetch('/telemetry'"));
    assert(strstr(kUiJs, "schedule(1000)"));
    assert(strstr(kUiJs, "APPLIANCE UNREACHABLE"));
    assert(!strstr(kUiJs, "new Date("));
    char telemetry[4096];
    SourceState qualified;
    qualified.id = SourceId::Pico;
    qualified.availability = Availability::Available;
    qualified.validity = TimeValidity::Valid;
    qualified.quality = SyncQuality::Locked;
    qualified.error = SourceError::None;
    qualified.anchor.presence = Presence::Known;
    qualified.anchor.utcSeconds = 1700000000;
    qualified.anchor.localUs = 1000000;
    qualified.lastUpdate.presence = Presence::Known;
    qualified.lastUpdate.atUs = 1000000;
    qualified.usableUntil.presence = Presence::Known;
    qualified.usableUntil.atUs = 5000000;
    state.initialized = true;
    state.clock = selectClock(qualified, 2000000);
    state.network.status = NetworkStatus::Connected;
    snprintf(state.network.address, sizeof(state.network.address), "192.168.1.20");
    state.network.setupMessage = "Saved \"bridge\"\nJoining";
    const size_t telemetrySize = renderTelemetryResponse(telemetry, sizeof(telemetry), state, 3000000);
    assert(telemetrySize > 0 && telemetry[telemetrySize - 1] == '\n');
    assert(strstr(telemetry, "Content-Type: application/json"));
    assert(strstr(telemetry, "\"utcValid\":true"));
    assert(strstr(telemetry, "\"utcSeconds\":1700000002"));
    assert(strstr(telemetry, "\"selectedAuthority\":\"Absurdly Accurate Clock\""));
    assert(strstr(telemetry, "Qualified AAC-derived UTC"));
    assert(strstr(telemetry, "\"networkState\":\"connected\""));
    assert(strstr(telemetry, "\"lanAddress\":\"192.168.1.20\""));
    assert(strstr(telemetry, "Saved \\\"bridge\\\"\\u000AJoining"));
    assert(!strstr(telemetry, "PRIVATE-secret9"));
    state.clock = selectClock(unavailablePico, 3000000);
    const size_t invalidTelemetrySize = renderTelemetryResponse(telemetry, sizeof(telemetry), state, 3000000);
    assert(invalidTelemetrySize > 0 && strstr(telemetry, "\"utcValid\":false"));
    assert(strstr(telemetry, "\"utcSeconds\":null"));
    state.network.provisioning = true;
    strcpy(state.network.apName, "AAC-Bridge-A1B2C3");
    assert(renderStatusPage(page, sizeof(page), state, 131000000, true) > 0);
    assert(strstr(page, "name='ssid'"));
    assert(strstr(page, "type='text'"));
    assert(!strstr(page, "type='password'"));
    assert(strstr(page, "AAC-Bridge-A1B2C3"));
    assert(!strstr(page, "name='password' value"));
    policy.begin(5000000000ULL);
    o = policy.observe(true, false, false, 5000000000ULL, 5060000000ULL);
    assert(o.startAp); // No 32-bit uptime rollover.
    puts("central network state, real status HTML, AP lifecycle and LAN announcement tests passed");
}
