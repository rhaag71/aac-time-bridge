// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "NtpProtocol.h"
#include <string.h>

namespace aac {
namespace {
constexpr uint64_t kNtpEpochOffset = 2208988800ULL;

void put32(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value >> 24);
    out[1] = static_cast<uint8_t>(value >> 16);
    out[2] = static_cast<uint8_t>(value >> 8);
    out[3] = static_cast<uint8_t>(value);
}

void putTimestamp(uint8_t* out, int64_t unixSeconds, uint32_t nanoseconds) {
    const uint64_t ntpSeconds = static_cast<uint64_t>(unixSeconds) + kNtpEpochOffset;
    const uint32_t fraction = static_cast<uint32_t>((static_cast<uint64_t>(nanoseconds) << 32) / 1000000000ULL);
    put32(out, static_cast<uint32_t>(ntpSeconds));
    put32(out + 4, fraction);
}

bool timestampAt(const ClockState& clock, MonotonicUs atUs, uint8_t* out) {
    int64_t seconds = 0;
    if (!currentUtc(clock, atUs, seconds)) return false;
    const TimeAnchor& anchor = clock.pico.report.anchor;
    const uint64_t deltaUs = atUs - anchor.localUs;
    uint64_t nanoseconds = static_cast<uint64_t>(anchor.nanoseconds) + (deltaUs % 1000000ULL) * 1000ULL;
    if (nanoseconds >= 1000000000ULL) nanoseconds -= 1000000000ULL;
    putTimestamp(out, seconds, static_cast<uint32_t>(nanoseconds));
    return true;
}
} // namespace

NtpRequestError buildNtpResponse(const uint8_t* request, size_t requestSize,
                                const ClockState& clock, MonotonicUs receivedAtUs,
                                MonotonicUs transmitAtUs, uint8_t* response,
                                size_t responseCapacity, bool& synchronized) {
    synchronized = false;
    if (!request || requestSize != kNtpPacketSize || !response || responseCapacity < kNtpPacketSize)
        return NtpRequestError::Size;
    const uint8_t version = static_cast<uint8_t>((request[0] >> 3) & 0x07);
    const uint8_t mode = static_cast<uint8_t>(request[0] & 0x07);
    if (mode != 3) return NtpRequestError::Mode;
    if (version != 3 && version != 4) return NtpRequestError::Version;

    memset(response, 0, kNtpPacketSize);
    // Version 4 server response; LI=3 and stratum=16 explicitly mean unsynced.
    response[0] = static_cast<uint8_t>((3U << 6) | (4U << 3) | 4U);
    response[1] = kNtpStratumUnsynchronized;
    response[2] = request[2]; // Poll interval.
    response[3] = static_cast<uint8_t>(-20); // Approximately one-microsecond precision.
    memcpy(response + 24, request + 40, 8); // Originate echoes client transmit timestamp.
    memcpy(response + 12, "INIT", 4);

    uint8_t reference[8], receive[8], transmit[8];
    if (!timestampAt(clock, receivedAtUs, receive) ||
        !timestampAt(clock, transmitAtUs, transmit)) return NtpRequestError::None;

    const TimeAnchor& anchor = clock.pico.report.anchor;
    putTimestamp(reference, anchor.utcSeconds, anchor.nanoseconds);
    memcpy(response + 12, "AAC ", 4);
    // 1 ms root dispersion in unsigned 16.16 seconds.
    put32(response + 8, 66U);
    response[0] = static_cast<uint8_t>((4U << 3) | 4U); // LI=0, VN=4, mode=4 (server).
    response[1] = kNtpStratumAac;
    memcpy(response + 16, reference, sizeof(reference));
    memcpy(response + 32, receive, sizeof(receive));
    memcpy(response + 40, transmit, sizeof(transmit));
    synchronized = true;
    return NtpRequestError::None;
}
} // namespace aac
