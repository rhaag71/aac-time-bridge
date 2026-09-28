// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "PicoSerialDiagnostics.h"
#include <stdio.h>

namespace aac {
namespace {
constexpr MonotonicUs kReportPeriodUs = 2000000ULL;

const char* packetText(PicoPacketResult result) {
    switch (result) {
    case PicoPacketResult::Never: return "NO TRANSACTION";
    case PicoPacketResult::Valid: return "VALID";
    case PicoPacketResult::BadSize: return "BAD SIZE";
    case PicoPacketResult::BadMagic: return "BAD MAGIC";
    case PicoPacketResult::BadVersion: return "BAD VERSION";
    case PicoPacketResult::BadLength: return "BAD LENGTH";
    case PicoPacketResult::BadFlags: return "BAD FLAGS/HOLDOVER";
    case PicoPacketResult::BadReserved: return "BAD RESERVED";
    case PicoPacketResult::BadFields: return "BAD FIELDS";
    case PicoPacketResult::BadCrc: return "BAD CRC";
    }
    return "UNKNOWN";
}

const char* reasonText(SourceError error) {
    switch (error) {
    case SourceError::None: return "qualified";
    case SourceError::NotImplemented: return "not implemented";
    case SourceError::Transport: return "transport error";
    case SourceError::InvalidData: return "invalid data";
    case SourceError::AssociationLost: return "phase not associated";
    case SourceError::NoCommunication: return "no communication";
    case SourceError::InvalidPacket: return "invalid packet";
    case SourceError::UtcUnavailable: return "UTC invalid";
    case SourceError::NotLocked: return "PPS not locked/qualified";
    case SourceError::SequenceDiscontinuity: return "sequence discontinuity";
    case SourceError::EdgeTimeout: return "TIME_SYNC expired";
    case SourceError::EdgeOverflow: return "TIME_SYNC capture overflow";
    }
    return "unknown";
}

void readableFlags(uint16_t flags, char* output, size_t capacity) {
    output[0] = '\0';
    struct FlagName { uint16_t bit; const char* name; };
    static const FlagName names[] = {
        {kPicoGpsValid, "GPS_VALID"}, {kPicoPpsPresent, "PPS_PRESENT"},
        {kPicoPpsLocked, "PPS_LOCKED"}, {kPicoUtcValid, "UTC_VALID"},
        {kPicoHoldover, "HOLDOVER"}, {kPicoSyncValid, "SYNC_VALID"},
        {kPicoSatValid, "SAT_VALID"}
    };
    size_t used = 0;
    for (const auto& item : names) {
        if (!(flags & item.bit)) continue;
        const int written = snprintf(output + used, capacity - used, "%s%s",
                                     used ? "|" : "", item.name);
        if (written < 0 || static_cast<size_t>(written) >= capacity - used) return;
        used += static_cast<size_t>(written);
    }
    if (!used) snprintf(output, capacity, "none");
}

bool healthy(const ClockState& clock) {
    return clock.status == ClockStatus::Synchronized && clock.selected == SourceId::Pico &&
           clock.pico.report.error == SourceError::None;
}
}

bool BoundedSerialLine::begin(const char* bytes, size_t length) {
    if (!bytes || length == 0 || length > sizeof(bytes_)) return false;
    for (size_t i = 0; i < length; ++i) bytes_[i] = bytes[i];
    length_ = length;
    offset_ = 0;
    return true;
}

const char* BoundedSerialLine::nextChunk(size_t available, size_t maxChunk, size_t& length) const {
    length = 0;
    if (!pending() || !available || !maxChunk) return nullptr;
    length = length_ - offset_;
    if (length > available) length = available;
    if (length > maxChunk) length = maxChunk;
    return bytes_ + offset_;
}

bool BoundedSerialLine::consume(size_t length) {
    if (!pending() || !length || length > length_ - offset_) return false;
    offset_ += length;
    if (offset_ != length_) return false;
    length_ = 0;
    offset_ = 0;
    return true;
}

PicoSerialReportKind PicoSerialDiagnostics::prepare(const ClockState& clock, const PicoDiagnostics& pico,
                                                    MonotonicUs now, char* output, size_t capacity,
                                                    size_t& length) const {
    length = 0;
    if (!output || !capacity) return PicoSerialReportKind::None;
    const bool isHealthy = healthy(clock);
    const bool changed = !hasCommitted_ || isHealthy != lastHealthy_ ||
        clock.status != lastClockStatus_ || clock.pico.report.error != lastError_ ||
        pico.lastResult != lastPacketResult_ || pico.phaseAssociated != lastPhaseAssociated_ ||
        (pico.lastResult == PicoPacketResult::Valid && pico.flags != lastFlags_);

    if (isHealthy) {
        if (hasCommitted_ && lastHealthy_ && !changed) return PicoSerialReportKind::None;
        const int n = snprintf(output, capacity,
            "PICO: authority acquired UTC_VALID phase=yes seq=%lu boundary=%lu sync=%lu flags=0x%04X\r\n",
            static_cast<unsigned long>(pico.packetSequence),
            static_cast<unsigned long>(pico.boundarySequence),
            static_cast<unsigned long>(pico.syncSequence), static_cast<unsigned>(pico.flags));
        if (n < 0 || static_cast<size_t>(n) >= capacity) return PicoSerialReportKind::None;
        length = static_cast<size_t>(n);
        return PicoSerialReportKind::Acquired;
    }

    const bool due = !hasCommitted_ || now < lastPrintedAtUs_ || now - lastPrintedAtUs_ >= kReportPeriodUs;
    if (!changed && !due) return PicoSerialReportKind::None;

    char details[256] = "";
    if (pico.lastResult == PicoPacketResult::Valid) {
        char flags[96];
        readableFlags(pico.flags, flags, sizeof(flags));
        const int n = snprintf(details, sizeof(details),
            " flags=%s(0x%04X) seq=%lu boundary=%lu sync=%lu sat=%s",
            flags, static_cast<unsigned>(pico.flags),
            static_cast<unsigned long>(pico.packetSequence),
            static_cast<unsigned long>(pico.boundarySequence),
            static_cast<unsigned long>(pico.syncSequence),
            (pico.flags & kPicoSatValid) ? "valid" : "invalid");
        if (n < 0 || static_cast<size_t>(n) >= sizeof(details)) return PicoSerialReportKind::None;
        if (pico.flags & kPicoSatValid) {
            const size_t used = static_cast<size_t>(n);
            const int appended = snprintf(details + used, sizeof(details) - used, ":%u",
                                          static_cast<unsigned>(pico.satellites));
            if (appended < 0 || static_cast<size_t>(appended) >= sizeof(details) - used)
                return PicoSerialReportKind::None;
        }
    }

    char raw[96] = "rx=none";
    if (pico.hasRawResponse) {
        char hex[24];
        char ascii[9];
        for (size_t i = 0; i < sizeof(pico.rawPreview); ++i) {
            snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X%s",
                     static_cast<unsigned>(pico.rawPreview[i]), i == 7 ? "" : " ");
            const uint8_t byte = pico.rawPreview[i];
            ascii[i] = byte >= 0x20 && byte <= 0x7e ? static_cast<char>(byte) : '.';
        }
        ascii[8] = '\0';
        const int n = snprintf(raw, sizeof(raw), "rx[0:8]=%s ascii=\"%s\"", hex, ascii);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(raw)) return PicoSerialReportKind::None;
    }

    const char* packetAge = "never";
    unsigned long long packetAgeMs = 0;
    if (pico.hasValidPacketAt && now >= pico.lastValidPacketAtUs) {
        packetAge = "";
        packetAgeMs = static_cast<unsigned long long>((now - pico.lastValidPacketAtUs) / 1000ULL);
    }
    const char* edgeAge = "never";
    unsigned long long edgeAgeMs = 0;
    if (pico.hasEdgeAt && now >= pico.lastQualifiedEdgeAtUs) {
        edgeAge = "";
        edgeAgeMs = static_cast<unsigned long long>((now - pico.lastQualifiedEdgeAtUs) / 1000ULL);
    }
    const int n = snprintf(output, capacity,
        "PICO: tx=%lu packet=%s %s edge=%lu phase=%s packet-age=%s%llu ms qualified-edge-age=%s%llu ms reason=%s%s\r\n",
        static_cast<unsigned long>(pico.transactions), packetText(pico.lastResult), raw,
        static_cast<unsigned long>(pico.capturedEdges), pico.phaseAssociated ? "yes" : "no",
        packetAge, packetAgeMs, edgeAge, edgeAgeMs,
        reasonText(clock.pico.report.error), details);
    if (n < 0 || static_cast<size_t>(n) >= capacity) return PicoSerialReportKind::None;
    length = static_cast<size_t>(n);
    return PicoSerialReportKind::Unhealthy;
}

void PicoSerialDiagnostics::commit(const ClockState& clock, const PicoDiagnostics& pico,
                                   MonotonicUs now, PicoSerialReportKind kind) {
    if (kind == PicoSerialReportKind::None) return;
    hasCommitted_ = true;
    lastHealthy_ = healthy(clock);
    lastPrintedAtUs_ = now;
    lastClockStatus_ = clock.status;
    lastError_ = clock.pico.report.error;
    lastPacketResult_ = pico.lastResult;
    lastPhaseAssociated_ = pico.phaseAssociated;
    lastFlags_ = pico.flags;
}
} // namespace aac
