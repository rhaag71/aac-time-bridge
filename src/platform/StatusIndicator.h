// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "status/StatusIndicatorState.h"
namespace aac {
constexpr int kStatusLedGpio = 16; // NodeMCU-32S P16, physical header 27; WROOM only.
class StatusIndicator {
public:
    void begin();
    void render(const ApplianceState& state);
};
}
