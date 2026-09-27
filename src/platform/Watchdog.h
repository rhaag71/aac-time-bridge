// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "status/ApplianceState.h"
namespace aac {
class Watchdog {
public:
    void captureReset(Diagnostics& diagnostics);
    void begin(Diagnostics& diagnostics);
    void completedPass(Diagnostics& diagnostics);
#ifdef AAC_WATCHDOG_BENCH
    [[noreturn]] void wedgeForBench();
#endif
};
}
