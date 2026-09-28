// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "ApplianceState.h"
#include "network/Provisioning.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
namespace aac {
constexpr size_t kUiResponseCapacity = 8192;
// Fixed-capacity renderer. Configuration/passwords are deliberately not an input.
class PageWriter {
public:
    PageWriter(char* data, size_t capacity) : data_(data), capacity_(capacity) { if (capacity_) data_[0] = 0; }
    void add(const char* format, ...) __attribute__((format(printf, 2, 3))) {
        if (!ok_) return;
        va_list args; va_start(args, format);
        const int count = vsnprintf(data_ + used_, capacity_ - used_, format, args);
        va_end(args);
        if (count < 0 || static_cast<size_t>(count) >= capacity_ - used_) { ok_ = false; return; }
        used_ += static_cast<size_t>(count);
    }
    void row(const char* name, const char* value) {
        char escaped[512]; escapeHtml(value, escaped, sizeof(escaped));
        add("<div class='row'><dt>%s</dt><dd>%s</dd></div>", name, escaped);
    }
    size_t size() const { return ok_ ? used_ : 0; }
private:
    char* data_; size_t capacity_, used_ = 0; bool ok_ = true;
};
inline size_t renderStatusPage(char* output, size_t capacity, const ApplianceState& s, MonotonicUs now, bool setupAccess = false) {
    if (!capacity) return 0;
    PageWriter p(output, capacity);
    int64_t utc = 0;
    const bool valid = s.initialized && currentUtc(s.clock, now, utc);
    char clock[16] = "--:--:--";
    if (valid) {
        const int daySecond = static_cast<int>((utc % 86400 + 86400) % 86400);
        snprintf(clock, sizeof(clock), "%02d:%02d:%02d", daySecond / 3600, daySecond / 60 % 60, daySecond % 60);
    }
    const bool setup = setupAccess && s.network.provisioning;
    p.add("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nCache-Control: no-store\r\n"
        "Content-Security-Policy: default-src 'none'; style-src 'self'; script-src 'self'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'\r\n\r\n"
        "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>AAC / Time Bridge</title><link rel='stylesheet' href='/ui.css'><script src='/ui.js' defer></script></head><body><main>"
        "<header><div class='brand'><span class='mark' aria-hidden='true'>+</span><div><strong>AAC / TIME BRIDGE</strong>"
        "<div class='eyebrow'>Absurdly Accurate Clock</div></div></div><div class='top-meta eyebrow'>External authority<br>Local observability</div></header>"
        "<div class='intro'><div><div class='eyebrow'>Time reference / appliance console</div><h1>Every second needs a source.</h1></div>"
        "<button id='refresh' type='button'>Refresh status</button></div>"
        "<section class='clock' id='clock-panel' aria-label='Authoritative clock'><div class='clock-main'><div class='clock-top'>"
        "<span class='eyebrow'>01 / Authoritative UTC</span><span id='clock-status' class='badge%s'>%s</span></div>"
        "<output id='clock-readout' class='clock-readout'>%s</output><p id='clock-note' class='clock-note'>%s</p>"
        "<div class='ruler' aria-hidden='true'></div></div><div class='clock-side'><div class='source'><div class='eyebrow'>External source / Pico</div>"
        "<strong>%s</strong><small>%s</small></div><div><div class='eyebrow'>Selected authority</div><strong>%s</strong>"
        "<p class='micro'>No Internet or system-clock fallback.</p></div></div></section>",
        valid ? " valid" : "", valid ? "SYNCHRONIZED / SNAPSHOT" : "UNSYNCHRONIZED", clock,
        valid ? "Qualified external UTC at this snapshot. Refresh to update; this display does not run a local clock." :
                "No qualified external time. The appliance is available; authoritative UTC is not.",
        s.clock.pico.report.availability == Availability::Available ? "Responding" : "Unavailable",
        sourceErrorName(s.clock.pico.report.error),
        valid ? "Pico" : "None");
    p.add("<div class='grid'><section class='section' id='network-panel'><div class='section-heading'><div><div class='eyebrow'>02 / Connectivity</div>"
        "<h2>Network</h2></div><span class='network-value micro'>%s</span></div><dl>", networkStatusName(s.network.status));
    p.row("Network state", networkStatusName(s.network.status));
    p.row("LAN address", s.network.address[0] ? s.network.address : "Unavailable");
    p.row("Recovery AP", s.network.provisioning ? "Open / active" : "Off");
    p.row("AP address", s.network.provisioning ? s.network.apAddress : "Not active");
    p.row("Configuration storage", s.network.storageOk ? "OK" : "Error");
    p.row("AP operation", s.network.apError ? "Error / retry pending" : "No error");
    p.add("</dl></section><section class='section' id='diagnostics-panel'><div class='section-heading'><div><div class='eyebrow'>03 / System health</div>"
        "<h2>Diagnostics</h2></div></div><dl>");
    char detail[128];
    snprintf(detail, sizeof(detail), "%s / %u s", s.diagnostics.watchdogArmed ? "Armed" : "Not armed", s.diagnostics.watchdogTimeoutSeconds);
    p.row("Application watchdog", detail);
    snprintf(detail, sizeof(detail), "%s (%d)", s.diagnostics.resetReason, s.diagnostics.resetCode); p.row("Reset reason", detail);
    snprintf(detail, sizeof(detail), "%s / API result %d", s.diagnostics.watchdogReset ? "Yes" : "No", s.diagnostics.watchdogError); p.row("Watchdog reset", detail);
    snprintf(detail, sizeof(detail), "%llu s", static_cast<unsigned long long>(now / 1000000ULL)); p.row("Uptime at snapshot", detail);
    p.row("Source UTC / quality", s.clock.pico.report.validity == TimeValidity::Valid ?
        (s.clock.pico.report.quality == SyncQuality::Locked ? "Valid / locked" : "Valid / not locked") : "Invalid / not qualified");
    const PicoDiagnostics& pd = s.diagnostics.pico;
    p.row("Pico packet", picoPacketResultName(pd.lastResult));
    snprintf(detail, sizeof(detail), "%lu / boundary %lu / sync %lu",
        static_cast<unsigned long>(pd.packetSequence), static_cast<unsigned long>(pd.boundarySequence),
        static_cast<unsigned long>(pd.syncSequence)); p.row("Packet / boundary / sync sequence", detail);
    snprintf(detail, sizeof(detail), "0x%04X / %s", static_cast<unsigned>(pd.flags),
        (pd.flags & kPicoSatValid) ? "satellites valid" : "satellites invalid"); p.row("Pico flags", detail);
    if (pd.flags & kPicoSatValid) snprintf(detail, sizeof(detail), "%u", pd.satellites);
    else snprintf(detail, sizeof(detail), "Unavailable");
    p.row("Satellites", detail);
    if (pd.hasValidPacketAt && now >= pd.lastValidPacketAtUs)
        snprintf(detail, sizeof(detail), "%llu ms", static_cast<unsigned long long>((now - pd.lastValidPacketAtUs) / 1000ULL));
    else snprintf(detail, sizeof(detail), "Unavailable");
    p.row("Last valid packet age", detail);
    if (pd.hasEdgeAt && now >= pd.lastQualifiedEdgeAtUs)
        snprintf(detail, sizeof(detail), "%llu ms", static_cast<unsigned long long>((now - pd.lastQualifiedEdgeAtUs) / 1000ULL));
    else snprintf(detail, sizeof(detail), "Unavailable");
    p.row("TIME_SYNC edge age", detail);
    p.row("Phase association", pd.phaseAssociated ? "Established" : "Not established");
    p.row("Source qualification", sourceErrorName(s.clock.pico.report.error));
    snprintf(detail, sizeof(detail), "%lu transactions / %lu valid / %lu captured edges / %lu overruns",
        static_cast<unsigned long>(pd.transactions), static_cast<unsigned long>(pd.validPackets),
        static_cast<unsigned long>(pd.capturedEdges),
        static_cast<unsigned long>(pd.edgeOverflows)); p.row("Pico acquisition", detail);
    p.row("Firmware", s.diagnostics.benchBuild ? "Pico v1 / WATCHDOG BENCH" : "Pico v1 / PRODUCTION");
    p.row("Build", __DATE__ " " __TIME__);
    p.add("</dl></section></div>");
    if (setup) {
        char feedback[1024]; escapeHtml(s.network.setupMessage, feedback, sizeof(feedback));
        p.add("<section class='setup' id='setup'><div class='section-heading'><div><div class='eyebrow'>04 / Recovery network</div><h2>Connect the bridge</h2></div>"
            "<span class='micro'>OPEN AP</span></div><p>You are on <strong>%s</strong> at 192.168.4.1. This is the setup network; enter your router's 2.4 GHz network below.</p>"
            "<p class='notice' id='setup-feedback'>%s</p><form method='post' action='/configure' data-action='wifi' accept-charset='UTF-8'>"
            "<div class='form-grid'><label>Infrastructure Wi-Fi SSID<input name='ssid' required maxlength='32' autocomplete='off' placeholder='Network name' autocapitalize='none' spellcheck='false'></label>"
            "<label>New Wi-Fi password<input name='password' type='text' maxlength='64' autocomplete='off' autocapitalize='none' spellcheck='false' placeholder='Visible while you type'></label></div>"
            "<div class='actions'><button class='primary' type='submit'>Save &amp; connect</button><span class='micro'>Hidden SSIDs supported. Blank password = open network.</span></div>"
            "<p>Stored passwords are never displayed. After joining, this setup AP shuts down. Return to your normal network; find the LAN address in Serial or your router's DHCP list.</p></form></section>",
            s.network.apName[0] ? s.network.apName : "AAC-Bridge-XXXXXX", feedback);
    }
    p.add("<section class='section management' id='manage'><div class='section-heading'><div><div class='eyebrow'>%s / Appliance control</div><h2>Management</h2></div>"
        "<span class='micro'>Confirmation required</span></div><div class='management-grid'>"
        "<details class='confirm'><summary><span>Reboot<small>Restart. Keep configuration.</small></span></summary><div class='confirm-body'>"
        "<p>Restart AAC Time Bridge? Stored Wi-Fi credentials and all configuration will be preserved. The clock will start untrusted.</p>"
        "<form method='post' action='/manage/reboot' data-action='reboot'><div class='actions'><button type='submit' name='confirm' value='reboot'>Confirm reboot</button>"
        "<button type='button' data-cancel>Cancel</button></div></form></div></details>"
        "<details class='confirm danger'><summary><span>Factory Reset<small>Erase appliance configuration.</small></span></summary><div class='confirm-body'>"
        "<p>This erases all stored AAC Time Bridge configuration, including Wi-Fi credentials, then reboots into setup mode. Unrelated ESP32/platform storage is preserved. "
        "Reconnect to the open AAC-Bridge-XXXXXX network and browse to 192.168.4.1 after startup.</p>"
        "<form method='post' action='/manage/factory-reset' data-action='factory-reset'><div class='actions'><button class='danger' type='submit' name='confirm' value='factory-reset'>Factory Reset</button>"
        "<button type='button' data-cancel>Cancel</button></div></form></div></details></div>"
        "<p id='feedback' class='notice feedback' role='status' aria-live='polite' hidden></p>"
        "<noscript><p class='notice'>JavaScript is disabled. Expand a panel to confirm an action; form submissions will show a plain acknowledgement. Reload this page to update status.</p></noscript>"
        "<p id='snapshot-note' class='footnote'>Status snapshot from the appliance. No browser clock is used. Refresh for current state.</p></section>"
        "<footer><span>AAC / TIME BRIDGE</span><span>EXTERNAL TIME. LOCAL TRUST.</span></footer></main></body></html>", setup ? "05" : "04");
    return p.size();
}
}
