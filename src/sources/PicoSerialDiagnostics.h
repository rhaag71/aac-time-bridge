// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "PicoProtocol.h"
#include "clock/ClockState.h"

namespace aac {
enum class PicoSerialReportKind : uint8_t { None, Unhealthy, Acquired };

// Fixed-capacity line queue for UARTs whose available write space is smaller
// than a formatted report. Call nextChunk() with current UART capacity, write
// no more than the returned length, then consume() the bytes actually queued.
class BoundedSerialLine {
public:
    static constexpr size_t kCapacity = 512;
    bool begin(const char* bytes, size_t length);
    const char* nextChunk(size_t available, size_t maxChunk, size_t& length) const;
    bool consume(size_t length);
    bool pending() const { return offset_ < length_; }
private:
    char bytes_[kCapacity] = {};
    size_t length_ = 0;
    size_t offset_ = 0;
};

// Bounded application-context formatter. A report is committed only after its
// complete line fits in the UART transmit queue.
class PicoSerialDiagnostics {
public:
    PicoSerialReportKind prepare(const ClockState& clock, const PicoDiagnostics& pico,
                                 MonotonicUs now, char* output, size_t capacity,
                                 size_t& length) const;
    void commit(const ClockState& clock, const PicoDiagnostics& pico,
                MonotonicUs now, PicoSerialReportKind kind);
private:
    bool hasCommitted_ = false;
    bool lastHealthy_ = false;
    MonotonicUs lastPrintedAtUs_ = 0;
    ClockStatus lastClockStatus_ = ClockStatus::Unsynchronized;
    SourceError lastError_ = SourceError::NoCommunication;
    PicoPacketResult lastPacketResult_ = PicoPacketResult::Never;
    bool lastPhaseAssociated_ = false;
    uint16_t lastFlags_ = 0;
};
} // namespace aac
