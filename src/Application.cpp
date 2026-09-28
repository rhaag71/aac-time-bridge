// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "Application.h"
#include <Arduino.h>
#include <esp_timer.h>
namespace aac {
void Application::begin() {
    indicator_.begin(); // Explicit LOW before any initialized status is rendered.
    state_ = ApplianceState{};
    Serial.begin(115200);
    watchdog_.captureReset(state_.diagnostics); // Read-only; does not subscribe.
    printBootBanner();
    Serial.println("Starting in 5 seconds (serial service delay)...");
    delay(5000); // Yield to RTOS/idle watchdogs; application not subscribed yet.
    printBootBanner(); // Monitor may have attached during the delay.
    Serial.println("Initializing...");
    watchdog_.begin(state_.diagnostics);
    const auto now = static_cast<MonotonicUs>(esp_timer_get_time());
    clock_.begin(now); // Every reset discards all authority and associations.
    state_.clock = clock_.state();
    network_.begin();
    state_.initialized = true;
    poll(); // Publish initial network/clock state and complete first healthy pass.
    lastDiagnosticUs_ = state_.uptimeUs;
    printStatus("Application ready");
}
void Application::poll() {
    auto now = static_cast<MonotonicUs>(esp_timer_get_time());
    network_.poll(now, state_.network);
#ifdef AAC_WATCHDOG_BENCH
    if (network_.takeBenchRequest()) watchdog_.wedgeForBench();
#endif
    now = static_cast<MonotonicUs>(esp_timer_get_time());
    clock_.poll(now);
    state_.clock = clock_.state();
    state_.diagnostics.pico = pico_.diagnostics();
    state_.uptimeUs = now;
    indicator_.render(state_);
    network_.serve(state_);
    if (!picoSerialLine_.pending()) {
        char picoLine[BoundedSerialLine::kCapacity];
        size_t picoLineLength = 0;
        const PicoSerialReportKind report = picoSerialDiagnostics_.prepare(
            state_.clock, state_.diagnostics.pico, now, picoLine, sizeof(picoLine), picoLineLength);
        if (report != PicoSerialReportKind::None && picoSerialLine_.begin(picoLine, picoLineLength)) {
            pendingPicoReport_ = report;
            pendingPicoClock_ = state_.clock;
            pendingPicoSnapshot_ = state_.diagnostics.pico;
        }
    }
    if (picoSerialLine_.pending()) {
        const int available = Serial.availableForWrite();
        if (available > 0) {
            size_t chunkLength = 0;
            const char* chunk = picoSerialLine_.nextChunk(static_cast<size_t>(available), 32, chunkLength);
            if (chunk && chunkLength) {
                const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(chunk), chunkLength);
                if (written && picoSerialLine_.consume(written)) {
                    picoSerialDiagnostics_.commit(pendingPicoClock_, pendingPicoSnapshot_, now,
                                                  pendingPicoReport_);
                    pendingPicoReport_ = PicoSerialReportKind::None;
                }
            }
        }
    }
    if (now - lastDiagnosticUs_ >= 60000000ULL) {
        lastDiagnosticUs_ = now;
        printStatus("Status");
    }
    watchdog_.completedPass(state_.diagnostics); // Sole feed, after all application work returns.
}
void Application::printBootBanner() const {
    Serial.printf("\nAAC Time Bridge | Firmware: Pico Protocol v1 acquisition | Build: %s | %s %s\nReset: %s (%d)\n",
        state_.diagnostics.benchBuild ? "WATCHDOG BENCH" : "PRODUCTION", __DATE__, __TIME__,
        state_.diagnostics.resetReason, state_.diagnostics.resetCode);
}
void Application::printStatus(const char* prefix) const {
    const auto& pico = state_.diagnostics.pico;
    const MonotonicUs packetAge = pico.hasValidPacketAt && state_.uptimeUs >= pico.lastValidPacketAtUs ? state_.uptimeUs - pico.lastValidPacketAtUs : 0;
    const MonotonicUs edgeAge = pico.hasEdgeAt && state_.uptimeUs >= pico.lastQualifiedEdgeAtUs ? state_.uptimeUs - pico.lastQualifiedEdgeAtUs : 0;
    Serial.printf("%s | network=%s IP=%s setup-AP=%s AP-IP=%s | clock=%s selected=%s | Pico=%s/%s pkt=%lu bnd=%lu sync=%lu flags=0x%04X sat=%s%u tx=%lu edge=%lu overrun=%lu packet-age=%s%llu ms edge-age=%s%llu ms phase=%s reason=%s | watchdog=%s error=%d\n",
        prefix, networkStatusName(state_.network.status), state_.network.address,
        state_.network.provisioning ? "active" : "off", state_.network.apAddress,
        statusLedOn(state_) ? "synchronized" : "unsynchronized",
        state_.clock.selected == SourceId::Pico ? "pico" : "none",
        state_.clock.pico.report.availability == Availability::Available ? "responding" : "unavailable",
        picoPacketResultName(pico.lastResult), static_cast<unsigned long>(pico.packetSequence),
        static_cast<unsigned long>(pico.boundarySequence), static_cast<unsigned long>(pico.syncSequence), static_cast<unsigned>(pico.flags),
        (pico.flags & kPicoSatValid) ? "" : "invalid/", static_cast<unsigned>(pico.satellites),
        static_cast<unsigned long>(pico.transactions), static_cast<unsigned long>(pico.capturedEdges),
        static_cast<unsigned long>(pico.edgeOverflows),
        pico.hasValidPacketAt ? "" : "unknown/", static_cast<unsigned long long>(packetAge / 1000ULL),
        pico.hasEdgeAt ? "" : "unknown/", static_cast<unsigned long long>(edgeAge / 1000ULL),
        pico.phaseAssociated ? "yes" : "no", sourceErrorName(state_.clock.pico.report.error),
        state_.diagnostics.watchdogArmed ? "armed" : "FAULT", state_.diagnostics.watchdogError);
}
}
