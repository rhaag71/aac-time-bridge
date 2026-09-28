// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "clock/ClockState.h"
#include "sources/PicoTimeSource.h"
#include "sources/PicoSerialDiagnostics.h"
#include "platform/Watchdog.h"
#include "platform/StatusIndicator.h"
#include "network/NetworkServices.h"
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
#include "diagnostics/TimingMetric.h"
#endif
namespace aac {
class Application {
public:
    Application() : clock_(pico_) {}
    void begin();
    void poll();
private:
    void printBootBanner() const;
    void printStatus(const char* prefix) const;
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    void prepareTimingReport(MonotonicUs now);
    void serviceTimingReport();
    TimingMetric loopIntervals_;
    TimingMetric loopDurations_;
    MonotonicUs previousPollAtUs_ = 0;
    bool havePreviousPoll_ = false;
    MonotonicUs lastTimingReportAtUs_ = 0;
    BoundedSerialLine timingLines_[3];
    char timingBuffers_[3][512] = {};
    size_t nextTimingLine_ = 3;
#endif
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
