// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "clock/ClockState.h"
#include "sources/PicoTimeSource.h"
#include "sources/PicoSerialDiagnostics.h"
#include "platform/Watchdog.h"
#include "platform/StatusIndicator.h"
#include "network/NetworkServices.h"
namespace aac {
class Application {
public:
    Application() : clock_(pico_) {}
    void begin();
    void poll();
private:
    void printBootBanner() const;
    void printStatus(const char* prefix) const;
    PicoTimeSource pico_;
    ClockCoordinator clock_;
    ApplianceState state_;
    Watchdog watchdog_;
    StatusIndicator indicator_;
    NetworkServices network_;
    PicoSerialDiagnostics picoSerialDiagnostics_;
    BoundedSerialLine picoSerialLine_;
    PicoSerialReportKind pendingPicoReport_ = PicoSerialReportKind::None;
    ClockState pendingPicoClock_;
    PicoDiagnostics pendingPicoSnapshot_;
    MonotonicUs lastDiagnosticUs_ = 0;
};
}
