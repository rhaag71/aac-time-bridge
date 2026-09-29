// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "clock/ClockState.h"
#include "sources/PicoProtocol.h"
#include "network/NtpState.h"
#include <limits.h>

namespace aac {
enum class NetworkStatus { Unconfigured, Connecting, Connected, Disconnected };
inline const char* networkStatusName(NetworkStatus status) {
    switch (status) {
    case NetworkStatus::Unconfigured: return "unconfigured";
    case NetworkStatus::Connecting: return "connecting";
    case NetworkStatus::Connected: return "connected";
    case NetworkStatus::Disconnected: return "disconnected";
    }
    return "unknown";
}
struct Diagnostics {
    const char* resetReason = "unknown";
    int resetCode = 0;
    bool watchdogReset = false;
    bool watchdogArmed = false;
    int watchdogError = 0;
    unsigned watchdogTimeoutSeconds = 30;
    bool benchBuild = false;
    const char* buildFlavor = "PRODUCTION";
    PicoDiagnostics pico;
    NtpDiagnostics ntp;
};
struct NetworkState {
    NetworkStatus status = NetworkStatus::Unconfigured;
    bool provisioning = false;
    bool storageOk = true;
    bool apError = false;
    const char* setupMessage = "No Wi-Fi configuration submitted this boot.";
    char address[16] = {};
    char apAddress[16] = {};
    char apName[32] = {};
};
struct ApplianceState {
    bool initialized = false;
    MonotonicUs uptimeUs = 0;
    ClockState clock;
    Diagnostics diagnostics;
    NetworkState network;
};
// Recheck the exclusive deadline at consumption time. Never read system UTC.
inline bool currentUtc(const ClockState& clock, MonotonicUs now, int64_t& seconds) {
    if (clock.status != ClockStatus::Synchronized || clock.selected != SourceId::Pico ||
        selectClock(clock.pico.report, now).status != ClockStatus::Synchronized) return false;
    const auto& a = clock.pico.report.anchor;
    const uint64_t elapsed = (now - a.localUs) / 1000000ULL;
    const uint64_t carry = ((now - a.localUs) % 1000000ULL * 1000ULL + a.nanoseconds) / 1000000000ULL;
    const uint64_t increment = elapsed + carry;
    if (increment > INT64_MAX || a.utcSeconds > INT64_MAX - static_cast<int64_t>(increment)) return false;
    seconds = a.utcSeconds + static_cast<int64_t>(increment);
    return true;
}
inline bool authoritativeUtcAvailable(const ApplianceState& state) {
    int64_t ignored;
    return state.initialized && currentUtc(state.clock, state.uptimeUs, ignored);
}
} // namespace aac
