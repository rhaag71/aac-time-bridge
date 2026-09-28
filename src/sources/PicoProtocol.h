// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "clock/TimeSource.h"
#include <stddef.h>
#include <stdint.h>

namespace aac {
constexpr size_t kPicoPacketSize = 40;
constexpr uint16_t kPicoGpsValid = 1u << 0;
constexpr uint16_t kPicoPpsPresent = 1u << 1;
constexpr uint16_t kPicoPpsLocked = 1u << 2;
constexpr uint16_t kPicoUtcValid = 1u << 3;
constexpr uint16_t kPicoHoldover = 1u << 4;
constexpr uint16_t kPicoSyncValid = 1u << 5;
constexpr uint16_t kPicoSatValid = 1u << 6;
constexpr uint16_t kPicoDefinedFlags = 0x7f;
constexpr MonotonicUs kPicoEdgeTimeoutUs = 1500000;

enum class PicoPacketResult : uint8_t {
    Never, Valid, BadSize, BadMagic, BadVersion, BadLength, BadFlags,
    BadReserved, BadFields, BadCrc
};

struct PicoPacket {
    uint16_t flags = 0;
    uint32_t packetSequence = 0;
    uint32_t boundarySequence = 0;
    int64_t utcSeconds = 0;
    uint32_t syncSequence = 0;
    uint32_t syncDelayUs = 0xffffffffu;
    uint8_t satellites = 0xff;
};

struct PicoDiagnostics {
    PicoPacketResult lastResult = PicoPacketResult::Never;
    uint32_t transactions = 0;
    uint32_t validPackets = 0;
    uint32_t capturedEdges = 0;
    uint32_t edgeOverflows = 0;
    uint32_t packetSequence = 0;
    uint32_t boundarySequence = 0;
    uint32_t syncSequence = 0;
    uint16_t flags = 0;
    uint8_t satellites = 0xff;
    bool hasValidPacket = false;
    bool hasValidPacketAt = false;
    MonotonicUs lastValidPacketAtUs = 0;
    bool hasEdgeAt = false;
    MonotonicUs lastQualifiedEdgeAtUs = 0;
    bool phaseAssociated = false;
    SourceError rejection = SourceError::NoCommunication;
    bool hasRawResponse = false;
    uint8_t rawPreview[8] = {};
};

uint32_t picoCrc32(const uint8_t* data, size_t length);
PicoPacketResult decodePicoPacket(const uint8_t* bytes, size_t length, PicoPacket& packet);
const char* picoPacketResultName(PicoPacketResult result);
const char* sourceErrorName(SourceError error);

// Hardware-independent freshness and sequence/edge qualification policy.
// Only an unambiguous captured edge paired with a complete valid v1 snapshot
// can establish/update the authoritative UTC anchor.
class PicoQualification {
public:
    void begin(MonotonicUs now);
    void transactionFailure(MonotonicUs now, SourceError error = SourceError::NoCommunication);
    void edgeOverflow(MonotonicUs now);
    void recordCapturedEdges(uint32_t total) { diagnostics_.capturedEdges = total; }
    void poll(MonotonicUs now);
    PicoPacketResult observePacket(const uint8_t* bytes, size_t length, MonotonicUs now,
                                   bool hasCapturedEdge = false, MonotonicUs edgeAtUs = 0);
    const SourceState& state() const { return state_; }
    const PicoDiagnostics& diagnostics() const { return diagnostics_; }
private:
    void reject(SourceError reason, bool clearSequence);
    bool sequenceForward(uint32_t previous, uint32_t current) const;
    SourceState state_;
    PicoDiagnostics diagnostics_;
    bool haveSequence_ = false;
    uint32_t lastPacketSequence_ = 0;
    uint32_t lastBoundarySequence_ = 0;
    uint32_t lastSyncSequence_ = 0;
    MonotonicUs lastEdgeAtUs_ = 0;
};
} // namespace aac
