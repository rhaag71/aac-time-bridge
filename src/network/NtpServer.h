// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "NtpProtocol.h"
#include "NtpState.h"
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
#include "diagnostics/TimingMetric.h"
#endif

namespace aac {
class NtpServer {
public:
    ~NtpServer();
    void poll(bool lanConnected, uint32_t lanAddress, const ClockState& clock, MonotonicUs now);
    NtpDiagnostics diagnostics() const { return diagnostics_; }
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    NtpTimingSnapshot takeTimingSnapshot();
#endif
private:
    void closeSocket();
    bool openSocket(uint32_t lanAddress);
    int socket_ = -1;
    uint32_t boundAddress_ = 0;
    MonotonicUs lastOpenAttemptUs_ = 0;
    NtpDiagnostics diagnostics_;
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    NtpTimingSnapshot timing_;
    MonotonicUs lastOpportunityUs_ = 0;
    bool haveOpportunity_ = false;
#endif
};
} // namespace aac
