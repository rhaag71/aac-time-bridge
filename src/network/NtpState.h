// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

namespace aac {
enum class NtpServiceState : uint8_t { Offline, Listening, Error };
struct NtpDiagnostics {
    NtpServiceState state = NtpServiceState::Offline;
    uint32_t requests = 0;
    uint32_t replies = 0;
    uint32_t synchronizedReplies = 0;
    uint32_t unsynchronizedReplies = 0;
    uint32_t rejectedRequests = 0;
    uint8_t stratum = 16;
    bool authorityQualified = false;
    int lastError = 0;
};
inline const char* ntpServiceStateName(NtpServiceState state) {
    switch (state) {
    case NtpServiceState::Offline: return "off-LAN";
    case NtpServiceState::Listening: return "listening";
    case NtpServiceState::Error: return "socket-error";
    }
    return "unknown";
}
} // namespace aac
