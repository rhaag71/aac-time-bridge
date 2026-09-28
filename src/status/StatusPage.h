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
    void row(const char* name, const char* value, const char* id = nullptr) {
        char escaped[512]; escapeHtml(value, escaped, sizeof(escaped));
        if (id) add("<div class='row'><dt>%s</dt><dd id='%s'>%s</dd></div>", name, id, escaped);
        else add("<div class='row'><dt>%s</dt><dd>%s</dd></div>", name, escaped);
    }
    size_t size() const { return ok_ ? used_ : 0; }
private:
    char* data_; size_t capacity_, used_ = 0; bool ok_ = true;
};
class TelemetryWriter {
public:
    TelemetryWriter(char* output, size_t capacity) : output_(output), capacity_(capacity) {
        if (capacity_) output_[0] = '\0';
        append('{');
    }
    void field(const char* key, const char* value) {
        prefix(key); append('\"');
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value ? value : ""); *p && ok_; ++p) {
            const unsigned char c = *p;
            if (c == '\"' || c == '\\') { append('\\'); append(static_cast<char>(c)); }
            else if (c < 0x20) {
                char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04X", c); append(escaped);
            } else append(static_cast<char>(c));
        }
        append('\"');
    }
    void field(const char* key, bool value) { prefix(key); append(value ? "true" : "false"); }
    void number(const char* key, unsigned long long value) {
        prefix(key); format("%llu", value);
    }
    void signedNumber(const char* key, long long value) {
        prefix(key); format("%lld", value);
    }
    void null(const char* key) { prefix(key); append("null"); }
    void finish() { append("}\n"); }
    size_t size() const { return ok_ ? used_ : 0; }
private:
    void prefix(const char* key) {
        if (!first_) append(',');
        first_ = false;
        append('\"'); append(key); append("\":");
    }
    void append(char value) {
        if (!ok_) return;
        if (used_ + 1 >= capacity_) { ok_ = false; return; }
        output_[used_++] = value; output_[used_] = '\0';
    }
    void append(const char* value) { while (*value && ok_) append(*value++); }
    void format(const char* format, unsigned long long value) {
        if (!ok_) return;
        const int n = snprintf(output_ + used_, capacity_ - used_, format, value);
        if (n < 0 || static_cast<size_t>(n) >= capacity_ - used_) { ok_ = false; return; }
        used_ += static_cast<size_t>(n);
    }
    void format(const char* format, long long value) {
        if (!ok_) return;
        const int n = snprintf(output_ + used_, capacity_ - used_, format, value);
        if (n < 0 || static_cast<size_t>(n) >= capacity_ - used_) { ok_ = false; return; }
        used_ += static_cast<size_t>(n);
    }
    char* output_; size_t capacity_; size_t used_ = 0; bool first_ = true; bool ok_ = true;
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
        "<div class='eyebrow'>Absurdly Accurate Clock</div></div></div><div class='header-actions'><div class='top-meta eyebrow'>External authority<br>Local observability</div>"
        "<button id='refresh' type='button'>Refresh status</button></div></header>"
        "<section class='clock' id='clock-panel' aria-label='Authoritative clock'><div class='clock-main'><div class='clock-top'>"
        "<span class='eyebrow'>01 / Authoritative UTC</span><span id='clock-status' class='badge%s'>%s</span></div>"
        "<output id='clock-readout' class='clock-readout'>%s</output><p id='clock-note' class='clock-note'>%s</p>"
        "<div class='ruler' aria-hidden='true'></div></div><div class='clock-side'><div class='source'><div class='eyebrow'>Authoritative source / AAC</div>"
        "<strong id='source-availability'>%s</strong><small id='source-error'>%s</small></div><div><div class='eyebrow'>Selected authority</div><strong id='selected-authority'>%s</strong>"
        "<p class='micro'>No Internet or system-clock fallback.</p></div></div></section>",
        valid ? " valid" : "", valid ? "SYNCHRONIZED" : "UNSYNCHRONIZED", clock,
        valid ? "Qualified AAC-derived UTC. Updates arrive from the appliance once per second." :
                "No qualified external time. The appliance is available; authoritative UTC is not.",
        s.clock.pico.report.availability == Availability::Available ? "Responding" : "Unavailable",
        sourceErrorName(s.clock.pico.report.error),
        valid ? "Absurdly Accurate Clock" : "None");
    p.add("<div class='grid'><section class='section' id='network-panel'><div class='section-heading'><div><div class='eyebrow'>02 / Connectivity</div>"
        "<h2>Network</h2></div><span id='network-state-heading' class='network-value micro'>%s</span></div><dl>", networkStatusName(s.network.status));
    p.row("Network state", networkStatusName(s.network.status), "network-state");
    p.row("LAN address", s.network.address[0] ? s.network.address : "Unavailable", "lan-address");
    p.row("Recovery AP", s.network.provisioning ? "Open / active" : "Off", "recovery-ap");
    p.row("AP address", s.network.provisioning ? s.network.apAddress : "Not active", "ap-address");
    p.row("Configuration storage", s.network.storageOk ? "OK" : "Error", "configuration-storage");
    p.row("AP operation", s.network.apError ? "Error / retry pending" : "No error", "ap-operation");
    p.add("</dl></section><section class='section' id='diagnostics-panel'><div class='section-heading'><div><div class='eyebrow'>03 / System health</div>"
        "<h2>Diagnostics</h2></div></div><dl>");
    char detail[128];
    snprintf(detail, sizeof(detail), "%s / %u s", s.diagnostics.watchdogArmed ? "Armed" : "Not armed", s.diagnostics.watchdogTimeoutSeconds);
    p.row("Application watchdog", detail, "watchdog-status");
    snprintf(detail, sizeof(detail), "%s (%d)", s.diagnostics.resetReason, s.diagnostics.resetCode); p.row("Reset reason", detail, "reset-reason");
    snprintf(detail, sizeof(detail), "%s / API result %d", s.diagnostics.watchdogReset ? "Yes" : "No", s.diagnostics.watchdogError); p.row("Watchdog reset", detail, "watchdog-reset");
    snprintf(detail, sizeof(detail), "%llu s", static_cast<unsigned long long>(now / 1000000ULL)); p.row("Uptime at snapshot", detail, "uptime-snapshot");
    p.row("Source UTC / quality", s.clock.pico.report.validity == TimeValidity::Valid ?
        (s.clock.pico.report.quality == SyncQuality::Locked ? "Valid / locked" : "Valid / not locked") : "Invalid / not qualified", "source-utc-quality");
    const PicoDiagnostics& pd = s.diagnostics.pico;
    p.row("AAC packet", picoPacketResultName(pd.lastResult), "pico-packet");
    snprintf(detail, sizeof(detail), "%lu / boundary %lu / sync %lu",
        static_cast<unsigned long>(pd.packetSequence), static_cast<unsigned long>(pd.boundarySequence),
        static_cast<unsigned long>(pd.syncSequence)); p.row("Packet / boundary / sync sequence", detail, "pico-sequences");
    snprintf(detail, sizeof(detail), "0x%04X / %s", static_cast<unsigned>(pd.flags),
        (pd.flags & kPicoSatValid) ? "satellites valid" : "satellites invalid"); p.row("AAC flags", detail, "pico-flags");
    if (pd.flags & kPicoSatValid) snprintf(detail, sizeof(detail), "%u", pd.satellites);
    else snprintf(detail, sizeof(detail), "Unavailable");
    p.row("Satellites", detail, "pico-satellites");
    if (pd.hasValidPacketAt && now >= pd.lastValidPacketAtUs)
        snprintf(detail, sizeof(detail), "%llu ms", static_cast<unsigned long long>((now - pd.lastValidPacketAtUs) / 1000ULL));
    else snprintf(detail, sizeof(detail), "Unavailable");
    p.row("Last valid packet age", detail, "packet-age");
    if (pd.hasEdgeAt && now >= pd.lastQualifiedEdgeAtUs)
        snprintf(detail, sizeof(detail), "%llu ms", static_cast<unsigned long long>((now - pd.lastQualifiedEdgeAtUs) / 1000ULL));
    else snprintf(detail, sizeof(detail), "Unavailable");
    p.row("TIME_SYNC edge age", detail, "edge-age");
    p.row("Phase association", pd.phaseAssociated ? "Established" : "Not established", "phase-association");
    p.row("Source qualification", sourceErrorName(s.clock.pico.report.error), "source-qualification");
    snprintf(detail, sizeof(detail), "%lu transactions / %lu valid / %lu captured edges / %lu overruns",
        static_cast<unsigned long>(pd.transactions), static_cast<unsigned long>(pd.validPackets),
        static_cast<unsigned long>(pd.capturedEdges),
        static_cast<unsigned long>(pd.edgeOverflows)); p.row("AAC acquisition", detail, "pico-acquisition");
    p.row("Firmware", s.diagnostics.benchBuild ? "AAC Protocol v1 / WATCHDOG BENCH" : "AAC Protocol v1 / PRODUCTION");
    p.row("Build", __DATE__ " " __TIME__);
    p.add("</dl></section></div>");
    if (setup) {
        char feedback[1024]; escapeHtml(s.network.setupMessage, feedback, sizeof(feedback));
        p.add("<section class='setup' id='setup'><div class='section-heading'><div><div class='eyebrow'>04 / Recovery network</div><h2>Connect the bridge</h2></div>"
            "<span class='micro'>OPEN AP</span></div><p>You are on <strong id='setup-ap-name'>%s</strong> at 192.168.4.1. This is the setup network; enter your router's 2.4 GHz network below.</p>"
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
        "<p id='snapshot-note' class='footnote'>Connecting to live appliance telemetry. The browser does not supply or advance authoritative UTC.</p></section>"
        "<footer><span>AAC / TIME BRIDGE</span><span>EXTERNAL TIME. LOCAL TRUST.</span></footer></main></body></html>", setup ? "05" : "04");
    return p.size();
}

inline size_t renderTelemetryResponse(char* output, size_t capacity, const ApplianceState& s, MonotonicUs now) {
    static const char header[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";
    const size_t headerSize = sizeof(header) - 1;
    if (!output || capacity <= headerSize) return 0;
    memcpy(output, header, headerSize);
    TelemetryWriter json(output + headerSize, capacity - headerSize);
    int64_t utc = 0;
    const bool valid = s.initialized && currentUtc(s.clock, now, utc);
    const PicoDiagnostics& pd = s.diagnostics.pico;
    json.field("utcValid", valid);
    if (valid) json.signedNumber("utcSeconds", static_cast<long long>(utc));
    else json.null("utcSeconds");
    json.field("clockStatus", valid ? "SYNCHRONIZED" : "UNSYNCHRONIZED");
    json.field("clockNote", valid ? "Qualified AAC-derived UTC." : "No qualified external time. The appliance is available; authoritative UTC is not.");
    json.field("sourceAvailability", s.clock.pico.report.availability == Availability::Available ? "Responding" : "Unavailable");
    json.field("sourceError", sourceErrorName(s.clock.pico.report.error));
    json.field("selectedAuthority", valid ? "Absurdly Accurate Clock" : "None");
    json.field("networkState", networkStatusName(s.network.status));
    json.field("lanAddress", s.network.address[0] ? s.network.address : "Unavailable");
    json.field("recoveryAp", s.network.provisioning ? "Open / active" : "Off");
    json.field("apAddress", s.network.provisioning ? s.network.apAddress : "Not active");
    json.field("apName", s.network.apName);
    json.field("configurationStorage", s.network.storageOk ? "OK" : "Error");
    json.field("apOperation", s.network.apError ? "Error / retry pending" : "No error");
    json.field("setupMessage", s.network.setupMessage);
    char detail[128];
    snprintf(detail, sizeof(detail), "%s / %u s", s.diagnostics.watchdogArmed ? "Armed" : "Not armed", s.diagnostics.watchdogTimeoutSeconds);
    json.field("watchdogStatus", detail);
    snprintf(detail, sizeof(detail), "%s (%d)", s.diagnostics.resetReason, s.diagnostics.resetCode);
    json.field("resetReason", detail);
    snprintf(detail, sizeof(detail), "%s / API result %d", s.diagnostics.watchdogReset ? "Yes" : "No", s.diagnostics.watchdogError);
    json.field("watchdogReset", detail);
    json.number("uptimeSeconds", static_cast<unsigned long long>(now / 1000000ULL));
    json.field("sourceUtcQuality", s.clock.pico.report.validity == TimeValidity::Valid ?
        (s.clock.pico.report.quality == SyncQuality::Locked ? "Valid / locked" : "Valid / not locked") : "Invalid / not qualified");
    json.field("picoPacket", picoPacketResultName(pd.lastResult));
    snprintf(detail, sizeof(detail), "%lu / boundary %lu / sync %lu",
        static_cast<unsigned long>(pd.packetSequence), static_cast<unsigned long>(pd.boundarySequence),
        static_cast<unsigned long>(pd.syncSequence));
    json.field("picoSequences", detail);
    snprintf(detail, sizeof(detail), "0x%04X / %s", static_cast<unsigned>(pd.flags),
        (pd.flags & kPicoSatValid) ? "satellites valid" : "satellites invalid");
    json.field("picoFlags", detail);
    if (pd.flags & kPicoSatValid) snprintf(detail, sizeof(detail), "%u", pd.satellites);
    else snprintf(detail, sizeof(detail), "Unavailable");
    json.field("picoSatellites", detail);
    if (pd.hasValidPacketAt && now >= pd.lastValidPacketAtUs)
        json.number("packetAgeMs", static_cast<unsigned long long>((now - pd.lastValidPacketAtUs) / 1000ULL));
    else json.null("packetAgeMs");
    if (pd.hasEdgeAt && now >= pd.lastQualifiedEdgeAtUs)
        json.number("edgeAgeMs", static_cast<unsigned long long>((now - pd.lastQualifiedEdgeAtUs) / 1000ULL));
    else json.null("edgeAgeMs");
    json.field("phaseAssociation", pd.phaseAssociated ? "Established" : "Not established");
    json.field("sourceQualification", sourceErrorName(s.clock.pico.report.error));
    snprintf(detail, sizeof(detail), "%lu transactions / %lu valid / %lu captured edges / %lu overruns",
        static_cast<unsigned long>(pd.transactions), static_cast<unsigned long>(pd.validPackets),
        static_cast<unsigned long>(pd.capturedEdges), static_cast<unsigned long>(pd.edgeOverflows));
    json.field("picoAcquisition", detail);
    json.field("firmware", s.diagnostics.benchBuild ? "AAC Protocol v1 / WATCHDOG BENCH" : "AAC Protocol v1 / PRODUCTION");
    json.field("build", __DATE__ " " __TIME__);
    json.finish();
    const size_t jsonSize = json.size();
    return jsonSize ? headerSize + jsonSize : 0;
}
}
