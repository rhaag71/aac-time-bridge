#include "network/NtpProtocol.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

using namespace aac;

static SourceState qualifiedAac() {
    SourceState source;
    source.id = SourceId::Pico;
    source.availability = Availability::Available;
    source.validity = TimeValidity::Valid;
    source.quality = SyncQuality::Locked;
    source.error = SourceError::None;
    source.anchor.presence = Presence::Known;
    source.anchor.utcSeconds = 0;
    source.anchor.nanoseconds = 250000000;
    source.anchor.localUs = 1000000;
    source.lastUpdate.presence = Presence::Known;
    source.lastUpdate.atUs = 1000000;
    source.usableUntil.presence = Presence::Known;
    source.usableUntil.atUs = 10000000;
    return source;
}

static uint32_t get32(const uint8_t* bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
}

int main() {
    uint8_t request[kNtpPacketSize] = {};
    request[0] = static_cast<uint8_t>((4U << 3) | 3U); // NTPv4 client.
    request[2] = 6;
    const uint8_t originate[8] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
    memcpy(request + 40, originate, sizeof(originate));
    uint8_t response[kNtpPacketSize] = {};
    bool synchronized = false;

    const ClockState clock = selectClock(qualifiedAac(), 1500000);
    assert(buildNtpResponse(request, sizeof(request), clock, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::None);
    assert(synchronized);
    assert(response[0] == 0x24); // LI=0, VN=4, server mode.
    assert(response[1] == kNtpStratumAac);
    assert(response[2] == request[2]);
    assert(static_cast<int8_t>(response[3]) == -20);
    assert(get32(response + 8) == 66); // 1 ms dispersion.
    assert(!memcmp(response + 12, "AAC ", 4));
    assert(get32(response + 16) == 0x83aa7e80U); // Unix epoch + NTP epoch offset.
    assert(get32(response + 20) == 0x40000000U); // Reference retains the source's quarter-second phase.
    assert(!memcmp(response + 24, originate, sizeof(originate)));
    assert(get32(response + 32) == 0x83aa7e80U);
    assert(get32(response + 36) == 0xc0000000U); // Receive time is 0.5 s after anchor, retaining its phase.
    assert(get32(response + 40) == 0x83aa7e81U);
    assert(get32(response + 44) == 0x40000000U); // Transmit time is 1 s after anchor.

    // A version-3 client is supported; malformed/unsupported requests are rejected.
    request[0] = static_cast<uint8_t>((3U << 3) | 3U);
    assert(buildNtpResponse(request, sizeof(request), clock, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::None && synchronized);
    request[0] = static_cast<uint8_t>((4U << 3) | 4U);
    assert(buildNtpResponse(request, sizeof(request), clock, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::Mode);
    request[0] = static_cast<uint8_t>((2U << 3) | 3U);
    assert(buildNtpResponse(request, sizeof(request), clock, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::Version);
    request[0] = static_cast<uint8_t>((4U << 3) | 3U);
    assert(buildNtpResponse(request, sizeof(request) - 1, clock, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::Size);

    const ClockState unavailable;
    assert(buildNtpResponse(request, sizeof(request), unavailable, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::None);
    assert(!synchronized && (response[0] >> 6) == 3 && (response[0] & 7) == 4);
    assert(((response[0] >> 3) & 7) == 4 && response[1] == kNtpStratumUnsynchronized);
    assert(!memcmp(response + 12, "INIT", 4));
    assert(!memcmp(response + 24, originate, sizeof(originate)));
    assert(get32(response + 16) == 0 && get32(response + 20) == 0);
    assert(get32(response + 32) == 0 && get32(response + 36) == 0);
    assert(get32(response + 40) == 0 && get32(response + 44) == 0);

    // A previously synchronized snapshot cannot serve after its source deadline.
    assert(buildNtpResponse(request, sizeof(request), clock, 11000000, 11000000,
        response, sizeof(response), synchronized) == NtpRequestError::None);
    assert(!synchronized && response[1] == kNtpStratumUnsynchronized);
    ClockState invalidAac = clock;
    invalidAac.pico.report.validity = TimeValidity::Invalid;
    assert(buildNtpResponse(request, sizeof(request), invalidAac, 1500000, 2000000,
        response, sizeof(response), synchronized) == NtpRequestError::None);
    assert(!synchronized && response[1] == kNtpStratumUnsynchronized);
    puts("NTP v4 packet, epoch/fraction conversion, originate echo and invalid-authority tests passed");
}
