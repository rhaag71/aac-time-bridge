// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "clock/TimeSource.h"
#include "PicoProtocol.h"

namespace aac {
// SPI and GPIO boundary for Pico Protocol v1. Policy/packet validation is host-testable.
class PicoTimeSource final : public TimeSource {
public:
    void begin(MonotonicUs now) override;
    void poll(MonotonicUs now) override;
    const SourceState& state() const override { return qualification_.state(); }
    const PicoDiagnostics& diagnostics() const { return qualification_.diagnostics(); }
private:
    PicoQualification qualification_;
    MonotonicUs startedAtUs_ = 0;
    MonotonicUs nextTransactionAtUs_ = 0;
    MonotonicUs lastTransactionAtUs_ = 0;
    MonotonicUs lastCsHighAtUs_ = 0;
    uint8_t tx_[kPicoPacketSize] = {};
    uint8_t rx_[kPicoPacketSize] = {};
};
} // namespace aac
