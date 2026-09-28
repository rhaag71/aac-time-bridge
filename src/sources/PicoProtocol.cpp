// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "PicoProtocol.h"
#include <limits.h>

namespace aac {
namespace {
uint16_t u16le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
uint32_t u32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint64_t u64le(const uint8_t* p) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= static_cast<uint64_t>(p[i]) << (8 * i);
    return value;
}
int64_t signed64(uint64_t bits) {
    if (bits <= static_cast<uint64_t>(INT64_MAX)) return static_cast<int64_t>(bits);
    return -1 - static_cast<int64_t>(~bits);
}
}

uint32_t picoCrc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc ^ 0xffffffffu;
}

PicoPacketResult decodePicoPacket(const uint8_t* bytes, size_t length, PicoPacket& packet) {
    if (!bytes || length != kPicoPacketSize) return PicoPacketResult::BadSize;
    if (bytes[0] != 'A' || bytes[1] != 'C' || bytes[2] != 'T' || bytes[3] != '1') return PicoPacketResult::BadMagic;
    if (bytes[4] != 1) return PicoPacketResult::BadVersion;
    if (bytes[5] != kPicoPacketSize) return PicoPacketResult::BadLength;
    const uint16_t flags = u16le(bytes + 6);
    if ((flags & static_cast<uint16_t>(~kPicoDefinedFlags)) || (flags & kPicoHoldover)) return PicoPacketResult::BadFlags;
    if (bytes[33] || bytes[34] || bytes[35]) return PicoPacketResult::BadReserved;
    const uint32_t expected = u32le(bytes + 36);
    if (picoCrc32(bytes, 36) != expected) return PicoPacketResult::BadCrc;

    PicoPacket decoded;
    decoded.flags = flags;
    decoded.packetSequence = u32le(bytes + 8);
    decoded.boundarySequence = u32le(bytes + 12);
    decoded.utcSeconds = signed64(u64le(bytes + 16));
    decoded.syncSequence = u32le(bytes + 24);
    decoded.syncDelayUs = u32le(bytes + 28);
    decoded.satellites = bytes[32];
    if (!(flags & kPicoUtcValid) && decoded.utcSeconds != 0) return PicoPacketResult::BadFields;
    if (flags & kPicoSyncValid) {
        const uint16_t syncRequirements = kPicoUtcValid | kPicoPpsPresent | kPicoPpsLocked;
        if ((flags & syncRequirements) != syncRequirements) return PicoPacketResult::BadFlags;
        if (decoded.syncDelayUs > 5000u) return PicoPacketResult::BadFields;
    } else if (decoded.syncDelayUs != 0xffffffffu) return PicoPacketResult::BadFields;
    if (flags & kPicoSatValid) {
        if (decoded.satellites > 99) return PicoPacketResult::BadFields;
    } else if (decoded.satellites != 0xffu) return PicoPacketResult::BadFields;
    packet = decoded; // Commit only after every byte and semantic field validates.
    return PicoPacketResult::Valid;
}

const char* picoPacketResultName(PicoPacketResult result) {
    switch (result) {
    case PicoPacketResult::Never: return "not read";
    case PicoPacketResult::Valid: return "valid";
    case PicoPacketResult::BadSize: return "bad size";
    case PicoPacketResult::BadMagic: return "bad magic";
    case PicoPacketResult::BadVersion: return "bad version";
    case PicoPacketResult::BadLength: return "bad length";
    case PicoPacketResult::BadFlags: return "reserved flags / HOLDOVER";
    case PicoPacketResult::BadReserved: return "reserved bytes";
    case PicoPacketResult::BadFields: return "inconsistent fields";
    case PicoPacketResult::BadCrc: return "CRC mismatch";
    }
    return "unknown packet result";
}

const char* sourceErrorName(SourceError error) {
    switch (error) {
    case SourceError::None: return "qualified";
    case SourceError::NotImplemented: return "not implemented";
    case SourceError::Transport: return "transport error";
    case SourceError::InvalidData: return "invalid data";
    case SourceError::AssociationLost: return "phase association lost";
    case SourceError::NoCommunication: return "no valid communication";
    case SourceError::InvalidPacket: return "invalid packet";
    case SourceError::UtcUnavailable: return "UTC invalid";
    case SourceError::NotLocked: return "PPS not qualified";
    case SourceError::SequenceDiscontinuity: return "sequence discontinuity";
    case SourceError::EdgeTimeout: return "TIME_SYNC timeout";
    case SourceError::EdgeOverflow: return "TIME_SYNC capture overflow";
    }
    return "unknown source error";
}

void PicoQualification::begin(MonotonicUs) {
    state_ = SourceState{};
    state_.id = SourceId::Pico;
    state_.availability = Availability::Unavailable;
    state_.error = SourceError::NoCommunication;
    diagnostics_ = PicoDiagnostics{};
    haveSequence_ = false;
    lastPacketSequence_ = lastBoundarySequence_ = lastSyncSequence_ = 0;
    lastEdgeAtUs_ = 0;
}

void PicoQualification::reject(SourceError reason, bool clearSequence) {
    state_.quality = SyncQuality::Unsynchronized;
    state_.error = reason;
    state_.anchor = TimeAnchor{};
    state_.usableUntil = ObservationTime{};
    diagnostics_.phaseAssociated = false;
    diagnostics_.rejection = reason;
    if (clearSequence) {
        haveSequence_ = false;
        lastEdgeAtUs_ = 0;
    }
}

bool PicoQualification::sequenceForward(uint32_t previous, uint32_t current) const {
    const uint32_t delta = current - previous;
    return delta != 0 && delta < 0x80000000u;
}

void PicoQualification::transactionFailure(MonotonicUs now, SourceError error) {
    ++diagnostics_.transactions;
    state_.lastContact.presence = Presence::Known;
    state_.lastContact.atUs = now;
    state_.availability = Availability::Unavailable;
    state_.validity = TimeValidity::Invalid;
    reject(error, true);
}

void PicoQualification::edgeOverflow(MonotonicUs now) {
    ++diagnostics_.edgeOverflows;
    state_.lastContact.presence = Presence::Known;
    state_.lastContact.atUs = now;
    reject(SourceError::EdgeOverflow, true);
}

void PicoQualification::poll(MonotonicUs now) {
    if (haveSequence_ && (now < lastEdgeAtUs_ || now - lastEdgeAtUs_ >= kPicoEdgeTimeoutUs))
        reject(SourceError::EdgeTimeout, true);
}

PicoPacketResult PicoQualification::observePacket(const uint8_t* bytes, size_t length, MonotonicUs now,
                                                   bool hasCapturedEdge, MonotonicUs edgeAtUs) {
    ++diagnostics_.transactions;
    state_.lastContact.presence = Presence::Known;
    state_.lastContact.atUs = now;
    state_.availability = Availability::Available; // A complete SPI transaction occurred.
    diagnostics_.hasRawResponse = bytes && length == kPicoPacketSize;
    if (diagnostics_.hasRawResponse) {
        for (size_t i = 0; i < sizeof(diagnostics_.rawPreview); ++i)
            diagnostics_.rawPreview[i] = bytes[i];
    }
    PicoPacket packet;
    const auto result = decodePicoPacket(bytes, length, packet);
    diagnostics_.lastResult = result;
    if (result != PicoPacketResult::Valid) {
        state_.availability = Availability::Unavailable;
        state_.validity = TimeValidity::Invalid;
        reject(SourceError::InvalidPacket, true);
        return result;
    }

    ++diagnostics_.validPackets;
    diagnostics_.hasValidPacket = true;
    diagnostics_.hasValidPacketAt = true;
    diagnostics_.lastValidPacketAtUs = now;
    diagnostics_.packetSequence = packet.packetSequence;
    diagnostics_.boundarySequence = packet.boundarySequence;
    diagnostics_.syncSequence = packet.syncSequence;
    diagnostics_.flags = packet.flags;
    diagnostics_.satellites = packet.satellites;
    state_.validity = (packet.flags & kPicoUtcValid) ? TimeValidity::Valid : TimeValidity::Invalid;

    if (!(packet.flags & kPicoUtcValid)) {
        reject(SourceError::UtcUnavailable, true);
        return result;
    }
    const uint16_t required = kPicoUtcValid | kPicoPpsPresent | kPicoPpsLocked | kPicoSyncValid;
    if ((packet.flags & required) != required) {
        reject(SourceError::NotLocked, true);
        return result;
    }
    if (packet.syncDelayUs > 5000u) {
        reject(SourceError::InvalidPacket, true);
        return result;
    }

    if (hasCapturedEdge) {
        if (edgeAtUs > now || now - edgeAtUs >= kPicoEdgeTimeoutUs || edgeAtUs < packet.syncDelayUs) {
            reject(SourceError::AssociationLost, true);
            return result;
        }
        if (haveSequence_) {
            const bool syncContinues = packet.syncSequence == lastSyncSequence_ + 1u;
            const bool boundaryContinues = packet.boundarySequence == lastBoundarySequence_ + 1u;
            const bool packetContinues = sequenceForward(lastPacketSequence_, packet.packetSequence);
            if (!syncContinues || !boundaryContinues || !packetContinues) {
                reject(SourceError::SequenceDiscontinuity, true);
                return result;
            }
        }
        const MonotonicUs boundaryAtUs = edgeAtUs - packet.syncDelayUs;
        haveSequence_ = true;
        lastPacketSequence_ = packet.packetSequence;
        lastBoundarySequence_ = packet.boundarySequence;
        lastSyncSequence_ = packet.syncSequence;
        lastEdgeAtUs_ = edgeAtUs;
        state_.anchor.presence = Presence::Known;
        state_.anchor.utcSeconds = packet.utcSeconds;
        state_.anchor.nanoseconds = 0;
        state_.anchor.localUs = boundaryAtUs;
        state_.lastUpdate.presence = Presence::Known;
        state_.lastUpdate.atUs = boundaryAtUs;
        state_.usableUntil.presence = Presence::Known;
        state_.usableUntil.atUs = edgeAtUs + kPicoEdgeTimeoutUs;
        state_.quality = SyncQuality::Locked;
        state_.error = SourceError::None;
        diagnostics_.phaseAssociated = true;
        diagnostics_.hasEdgeAt = true;
        diagnostics_.lastQualifiedEdgeAtUs = edgeAtUs;
        diagnostics_.rejection = SourceError::None;
        return result;
    }

    if (haveSequence_) {
        // A new protocol boundary without a captured local edge means at least
        // one TIME_SYNC observation was missed; never guess its phase.
        if (packet.syncSequence != lastSyncSequence_ || packet.boundarySequence != lastBoundarySequence_ ||
            (packet.packetSequence != lastPacketSequence_ && !sequenceForward(lastPacketSequence_, packet.packetSequence))) {
            reject(SourceError::SequenceDiscontinuity, true);
            return result;
        }
        if (now >= lastEdgeAtUs_ && now - lastEdgeAtUs_ < kPicoEdgeTimeoutUs) return result;
    }
    reject(SourceError::AssociationLost, true);
    return result;
}
} // namespace aac
