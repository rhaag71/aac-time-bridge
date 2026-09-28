// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "status/ApplianceState.h"
#include <stddef.h>
#include <stdint.h>

namespace aac {
constexpr size_t kNtpPacketSize = 48;
constexpr uint8_t kNtpStratumUnsynchronized = 16;
constexpr uint8_t kNtpStratumAac = 1;

enum class NtpRequestError : uint8_t { None, Size, Mode, Version };

// Builds a v4 server response for a basic v3/v4 client request. Timestamp
// values are derived only from the qualified centralized AAC clock anchor.
NtpRequestError buildNtpResponse(const uint8_t* request, size_t requestSize,
                                const ClockState& clock, MonotonicUs receivedAtUs,
                                MonotonicUs transmitAtUs, uint8_t* response,
                                size_t responseCapacity, bool& synchronized);
} // namespace aac
