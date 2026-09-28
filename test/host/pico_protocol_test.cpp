// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "sources/PicoProtocol.h"
#include "clock/ClockState.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <fstream>
#include <sstream>

using namespace aac;

static void put32(uint8_t* p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
static void put64(uint8_t* p, uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
static void seal(uint8_t* p) { put32(p + 36, picoCrc32(p, 36)); }
static void makePacket(uint8_t* p, uint32_t packet, uint32_t boundary, uint32_t sync,
                       int64_t utc, uint16_t flags = 0x6e, uint32_t delay = 100) {
    memset(p, 0, kPicoPacketSize);
    memcpy(p, "ACT1", 4); p[4] = 1; p[5] = 40;
    p[6] = static_cast<uint8_t>(flags); p[7] = static_cast<uint8_t>(flags >> 8);
    put32(p + 8, packet); put32(p + 12, boundary); put64(p + 16, static_cast<uint64_t>(utc));
    put32(p + 24, sync); put32(p + 28, delay);
    p[32] = flags & kPicoSatValid ? 12 : 0xff;
    seal(p);
}
static std::string field(const std::string& object, const char* key) {
    const std::string needle = std::string("\"") + key + "\": ";
    const size_t at = object.find(needle); assert(at != std::string::npos);
    size_t start = at + needle.size();
    if (object[start] == '"') { ++start; return object.substr(start, object.find('"', start) - start); }
    size_t end = object.find_first_of(",\n}", start); return object.substr(start, end - start);
}
static void checkGolden(const char* path) {
    std::ifstream input(path); assert(input.good());
    std::ostringstream contents; contents << input.rdbuf();
    const std::string json = contents.str();
    size_t cursor = 0; unsigned vectors = 0;
    while ((cursor = json.find('{', cursor)) != std::string::npos) {
        const size_t end = json.find('}', cursor); assert(end != std::string::npos);
        const std::string object = json.substr(cursor, end - cursor + 1);
        const std::string hex = field(object, "hex");
        assert(hex.size() == kPicoPacketSize * 2);
        uint8_t bytes[kPicoPacketSize];
        for (size_t i = 0; i < sizeof(bytes); ++i)
            bytes[i] = static_cast<uint8_t>(strtoul(hex.substr(i * 2, 2).c_str(), nullptr, 16));
        PicoPacket packet;
        assert(decodePicoPacket(bytes, sizeof(bytes), packet) == PicoPacketResult::Valid);
        assert(packet.packetSequence == strtoul(field(object, "sequence").c_str(), nullptr, 10));
        assert(packet.boundarySequence == strtoul(field(object, "boundary").c_str(), nullptr, 10));
        assert(packet.utcSeconds == strtoll(field(object, "epoch").c_str(), nullptr, 10));
        assert(packet.flags == strtoul(field(object, "flags").c_str(), nullptr, 10));
        assert(packet.syncSequence == strtoul(field(object, "sync_sequence").c_str(), nullptr, 10));
        assert(packet.syncDelayUs == strtoul(field(object, "sync_delay_us").c_str(), nullptr, 10));
        assert(packet.satellites == strtoul(field(object, "satellites").c_str(), nullptr, 10));
        ++vectors; cursor = end + 1;
    }
    assert(vectors >= 2);
}

int main(int argc, char** argv) {
    const char* check = "123456789";
    assert(picoCrc32(reinterpret_cast<const uint8_t*>(check), 9) == 0xcbf43926u);
    if (argc > 1) checkGolden(argv[1]);

    uint8_t packet[kPicoPacketSize];
    makePacket(packet, 12, 20, 8, 2200000000LL);
    PicoPacket decoded;
    assert(decodePicoPacket(packet, sizeof(packet), decoded) == PicoPacketResult::Valid);
    assert(decoded.packetSequence == 12 && decoded.boundarySequence == 20 && decoded.syncSequence == 8);
    assert(decoded.utcSeconds == 2200000000LL && decoded.syncDelayUs == 100 && decoded.satellites == 12);

    PicoPacket unchanged; unchanged.packetSequence = 0xdeadbeef;
    uint8_t bad[kPicoPacketSize]; memcpy(bad, packet, sizeof(bad)); bad[0] ^= 1;
    assert(decodePicoPacket(bad, sizeof(bad), unchanged) == PicoPacketResult::BadMagic && unchanged.packetSequence == 0xdeadbeef);
    memcpy(bad, packet, sizeof(bad)); bad[4] = 2; seal(bad);
    assert(decodePicoPacket(bad, sizeof(bad), unchanged) == PicoPacketResult::BadVersion);
    memcpy(bad, packet, sizeof(bad)); bad[5] = 39; seal(bad);
    assert(decodePicoPacket(bad, sizeof(bad), unchanged) == PicoPacketResult::BadLength);
    memcpy(bad, packet, sizeof(bad)); bad[33] = 1; seal(bad);
    assert(decodePicoPacket(bad, sizeof(bad), unchanged) == PicoPacketResult::BadReserved);
    memcpy(bad, packet, sizeof(bad)); bad[36] ^= 1;
    assert(decodePicoPacket(bad, sizeof(bad), unchanged) == PicoPacketResult::BadCrc);
    assert(decodePicoPacket(packet, sizeof(packet) - 1, unchanged) == PicoPacketResult::BadSize);
    makePacket(bad, 13, 21, 9, 1, static_cast<uint16_t>(0x6e | kPicoHoldover));
    assert(decodePicoPacket(bad, sizeof(bad), unchanged) == PicoPacketResult::BadFlags);
    makePacket(bad, 13, 21, 9, 0, 0x07, 0xffffffffu);
    assert(decodePicoPacket(bad, sizeof(bad), decoded) == PicoPacketResult::Valid && decoded.utcSeconds == 0);
    makePacket(bad, 13, 21, 9, 1, 0x07, 0xffffffffu);
    assert(decodePicoPacket(bad, sizeof(bad), decoded) == PicoPacketResult::BadFields);
    makePacket(bad, 13, 21, 9, 2200000000LL, kPicoUtcValid, 0xffffffffu); // UTC but no sync/lock.
    assert(decodePicoPacket(bad, sizeof(bad), decoded) == PicoPacketResult::Valid);
    makePacket(bad, 13, 21, 9, 2200000000LL, 0x6e, 5001);
    assert(decodePicoPacket(bad, sizeof(bad), decoded) == PicoPacketResult::BadFields);
    makePacket(bad, 13, 21, 9, 0, static_cast<uint16_t>(kPicoPpsPresent | kPicoPpsLocked | kPicoSyncValid), 100);
    assert(decodePicoPacket(bad, sizeof(bad), decoded) == PicoPacketResult::BadFlags);
    makePacket(bad, 13, 21, 9, -1, kPicoUtcValid, 0xffffffffu);
    assert(decodePicoPacket(bad, sizeof(bad), decoded) == PicoPacketResult::Valid && decoded.utcSeconds == -1);
    assert(decodePicoPacket(nullptr, sizeof(bad), decoded) == PicoPacketResult::BadSize);

    PicoQualification source;
    source.begin(0);
    assert(selectClock(source.state(), 0).selected == SourceId::None);
    makePacket(packet, 100, 200, 50, 2200000000LL);
    source.observePacket(packet, sizeof(packet), 500000, false);
    assert(selectClock(source.state(), 500000).selected == SourceId::None); // No captured TIME_SYNC.
    source.begin(0);
    makePacket(packet, 100, 200, 50, 2200000000LL);
    assert(source.observePacket(packet, sizeof(packet), 1001100, true, 1001000) == PicoPacketResult::Valid);
    auto selected = selectClock(source.state(), 1001200);
    assert(selected.status == ClockStatus::Synchronized && selected.selected == SourceId::Pico);
    assert(selected.anchor.utcSeconds == 2200000000LL && selected.anchor.localUs == 1000900);
    assert(source.diagnostics().phaseAssociated);

    // Repeated reads are contact only. A matched next edge advances the anchor.
    assert(source.observePacket(packet, sizeof(packet), 1100000) == PicoPacketResult::Valid);
    assert(selectClock(source.state(), 1100000).selected == SourceId::Pico);
    makePacket(packet, 101, 201, 51, 2200000001LL);
    assert(source.observePacket(packet, sizeof(packet), 2001200, true, 2001100) == PicoPacketResult::Valid);
    assert(selectClock(source.state(), 2001200).anchor.utcSeconds == 2200000001LL);

    // A protocol edge that arrives without a locally captured edge is lost.
    makePacket(packet, 102, 202, 52, 2200000002LL);
    source.observePacket(packet, sizeof(packet), 2100000);
    assert(selectClock(source.state(), 2100000).selected == SourceId::None);
    assert(source.state().error == SourceError::SequenceDiscontinuity);

    // Invalid packets cannot partially advance an anchor or sequence baseline.
    source.begin(0);
    makePacket(packet, 7, 9, 4, 2200000000LL);
    source.observePacket(packet, sizeof(packet), 1000000, true, 999900);
    memcpy(bad, packet, sizeof(bad)); bad[36] ^= 0x40;
    source.observePacket(bad, sizeof(bad), 1100000, true, 1099900);
    assert(selectClock(source.state(), 1100000).selected == SourceId::None);
    assert(source.state().error == SourceError::InvalidPacket);
    assert(source.diagnostics().packetSequence == 7); // Malformed bytes did not partially replace decoded diagnostics.

    // Every sequence must continue modulo 2^32; a restart/skip loses trust and
    // requires a fresh edge/packet relationship before authority returns.
    source.begin(0);
    makePacket(packet, 1000, 500, 80, 2200000000LL);
    source.observePacket(packet, sizeof(packet), 1000000, true, 999900);
    makePacket(packet, 1, 1, 1, 2200000001LL);
    source.observePacket(packet, sizeof(packet), 2000000, true, 1999900);
    assert(selectClock(source.state(), 2000000).selected == SourceId::None);
    assert(source.state().error == SourceError::SequenceDiscontinuity);
    makePacket(packet, 2, 2, 2, 2200000002LL);
    source.observePacket(packet, sizeof(packet), 3000000, true, 2999900);
    assert(selectClock(source.state(), 3000000).selected == SourceId::Pico);

    // Timeouts use monotonic time and clear the selected anchor at 1.5 s.
    source.poll(4499899);
    assert(selectClock(source.state(), 4499899).selected == SourceId::Pico);
    source.poll(4499900);
    assert(selectClock(source.state(), 4499900).selected == SourceId::None);
    assert(source.state().error == SourceError::EdgeTimeout);

    // Invalid UTC's zero sentinel stays invalid; unlocked UTC remains visible
    // as a valid source report but can never be selected as authoritative.
    source.begin(0);
    makePacket(packet, 1, 1, 1, 0, 0, 0xffffffffu);
    source.observePacket(packet, sizeof(packet), 1000, true, 1000);
    assert(source.state().validity == TimeValidity::Invalid && selectClock(source.state(), 1000).selected == SourceId::None);
    makePacket(packet, 2, 2, 2, 2200000000LL, kPicoUtcValid, 0xffffffffu);
    source.observePacket(packet, sizeof(packet), 2000, true, 2000);
    assert(source.state().validity == TimeValidity::Valid && source.state().quality == SyncQuality::Unsynchronized);
    assert(selectClock(source.state(), 2000).selected == SourceId::None);

    source.transactionFailure(3000);
    assert(source.state().availability == Availability::Unavailable);
    assert(selectClock(source.state(), 3000).selected == SourceId::None);

    // V1 counters wrap modulo 2^32 without looking like a reboot.
    source.begin(0);
    makePacket(packet, 0xffffffffu, 0xffffffffu, 0xffffffffu, 2200000000LL);
    source.observePacket(packet, sizeof(packet), 1000000, true, 999900);
    makePacket(packet, 0, 0, 0, 2200000001LL);
    source.observePacket(packet, sizeof(packet), 2000000, true, 1999900);
    assert(selectClock(source.state(), 2000000).selected == SourceId::Pico);
    puts("AAC Protocol v1 CRC, golden vectors, packet validation and edge qualification tests passed");
}
