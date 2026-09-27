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
    state_.uptimeUs = now;
    indicator_.render(state_);
    network_.serve(state_);
    if (now - lastDiagnosticUs_ >= 60000000ULL) {
        lastDiagnosticUs_ = now;
        printStatus("Status");
    }
    watchdog_.completedPass(state_.diagnostics); // Sole feed, after all application work returns.
}
void Application::printBootBanner() const {
    Serial.printf("\nAAC Time Bridge | Firmware: Round 2 | Build: %s | %s %s\nReset: %s (%d)\n",
        state_.diagnostics.benchBuild ? "WATCHDOG BENCH" : "PRODUCTION", __DATE__, __TIME__,
        state_.diagnostics.resetReason, state_.diagnostics.resetCode);
}
void Application::printStatus(const char* prefix) const {
    Serial.printf("%s | network=%s IP=%s setup-AP=%s AP-IP=%s | clock=%s pico=not-implemented selected=%s | watchdog=%s error=%d\n",
        prefix, networkStatusName(state_.network.status), state_.network.address,
        state_.network.provisioning ? "active" : "off", state_.network.apAddress,
        statusLedOn(state_) ? "synchronized" : "unsynchronized",
        state_.clock.selected == SourceId::Pico ? "pico" : "none",
        state_.diagnostics.watchdogArmed ? "armed" : "FAULT", state_.diagnostics.watchdogError);
}
}
