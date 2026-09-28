// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "NtpProtocol.h"
#include "NtpState.h"

namespace aac {
class NtpServer {
public:
    ~NtpServer();
    void poll(bool lanConnected, uint32_t lanAddress, const ClockState& clock, MonotonicUs now);
    NtpDiagnostics diagnostics() const { return diagnostics_; }
private:
    void closeSocket();
    bool openSocket(uint32_t lanAddress);
    int socket_ = -1;
    uint32_t boundAddress_ = 0;
    MonotonicUs lastOpenAttemptUs_ = 0;
    NtpDiagnostics diagnostics_;
};
} // namespace aac
