#include "sources/PicoSerialDiagnostics.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

using namespace aac;

int main() {
    BoundedSerialLine uartLine;
    const char longLine[] = "A diagnostic line longer than UART free space\r\n";
    assert(uartLine.begin(longLine, sizeof(longLine) - 1));
    size_t chunkLength = 0;
    size_t emitted = 0;
    while (uartLine.pending()) {
        const char* chunk = uartLine.nextChunk(5, 3, chunkLength);
        assert(chunk && chunkLength > 0 && chunkLength <= 3 && chunkLength <= 5);
        emitted += chunkLength;
        const bool complete = uartLine.consume(chunkLength);
        assert(complete == !uartLine.pending());
    }
    assert(emitted == sizeof(longLine) - 1);
    assert(uartLine.nextChunk(128, 32, chunkLength) == nullptr && chunkLength == 0);
    assert(!uartLine.begin(longLine, BoundedSerialLine::kCapacity + 1));

    PicoSerialDiagnostics reporter;
    ClockState clock;
    clock.pico.report.error = SourceError::NoCommunication;
    PicoDiagnostics pico;
    char line[512];
    size_t length = 0;

    auto kind = reporter.prepare(clock, pico, 0, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::Unhealthy);
    assert(strstr(line, "AAC: tx=") != nullptr);
    assert(strstr(line, "packet=NO TRANSACTION") != nullptr);
    assert(strstr(line, "reason=no communication") != nullptr);
    reporter.commit(clock, pico, 0, kind);
    assert(reporter.prepare(clock, pico, 29999999, line, sizeof(line), length) == PicoSerialReportKind::None);
    assert(reporter.prepare(clock, pico, 30000000, line, sizeof(line), length) == PicoSerialReportKind::Unhealthy);
    reporter.commit(clock, pico, 30000000, PicoSerialReportKind::Unhealthy);

    pico.transactions = 42;
    pico.lastResult = PicoPacketResult::BadMagic;
    pico.hasRawResponse = true;
    const uint8_t zeroes[8] = {};
    memcpy(pico.rawPreview, zeroes, sizeof(zeroes));
    kind = reporter.prepare(clock, pico, 30000100, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::Unhealthy);
    assert(strstr(line, "AAC: tx=42") != nullptr);
    assert(strstr(line, "packet=BAD MAGIC") != nullptr);
    assert(strstr(line, "rx[0:8]=00 00 00 00 00 00 00 00 ascii=\"........\"") != nullptr);
    reporter.commit(clock, pico, 30000100, kind);

    pico.transactions = 43;
    const uint8_t printable[8] = {'A', 'C', 'T', 'X', 1, 40, ' ', '!' };
    memcpy(pico.rawPreview, printable, sizeof(printable));
    kind = reporter.prepare(clock, pico, 30000200, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::None); // Same rejection reason; transactions do not spam.
    kind = reporter.prepare(clock, pico, 60000100, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::Unhealthy);
    assert(strstr(line, "ascii=\"ACTX.( !\"") != nullptr);
    reporter.commit(clock, pico, 60000100, kind);

    pico.lastResult = PicoPacketResult::Valid;
    pico.flags = kPicoUtcValid | kPicoPpsPresent | kPicoPpsLocked | kPicoSatValid;
    pico.packetSequence = 123;
    pico.boundarySequence = 456;
    pico.syncSequence = 456;
    pico.satellites = 8;
    pico.hasValidPacketAt = true;
    pico.lastValidPacketAtUs = 60000000;
    pico.hasEdgeAt = true;
    pico.lastQualifiedEdgeAtUs = 59900000;
    clock.pico.report.error = SourceError::NotLocked;
    kind = reporter.prepare(clock, pico, 60000200, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::Unhealthy);
    assert(strstr(line, "flags=PPS_PRESENT|PPS_LOCKED|UTC_VALID|SAT_VALID") != nullptr);
    assert(strstr(line, "seq=123 boundary=456 sync=456 sat=valid:8") != nullptr);
    assert(strstr(line, "reason=PPS not locked/qualified") != nullptr);
    assert(strstr(line, "rx[0:8]") != nullptr); // Valid-but-unqualified responses include both decoded data and raw bytes.
    reporter.commit(clock, pico, 60000200, kind);

    clock.status = ClockStatus::Synchronized;
    clock.selected = SourceId::Pico;
    clock.pico.report.error = SourceError::None;
    pico.phaseAssociated = true;
    kind = reporter.prepare(clock, pico, 61000000, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::Acquired);
    assert(strstr(line, "AAC: authority acquired") != nullptr);
    assert(strstr(line, "authority acquired UTC_VALID phase=yes") != nullptr);
    reporter.commit(clock, pico, 61000000, kind);
    pico.lastResult = PicoPacketResult::BadMagic; // Healthy authority suppresses diagnostic chatter.
    assert(reporter.prepare(clock, pico, 100000000, line, sizeof(line), length) == PicoSerialReportKind::None);

    clock.status = ClockStatus::Unsynchronized;
    clock.selected = SourceId::None;
    clock.pico.report.error = SourceError::EdgeTimeout;
    pico.phaseAssociated = false;
    kind = reporter.prepare(clock, pico, 100000100, line, sizeof(line), length);
    assert(kind == PicoSerialReportKind::Unhealthy);
    assert(strstr(line, "reason=TIME_SYNC expired") != nullptr);

    char shortLine[8];
    assert(reporter.prepare(clock, pico, 100000100, shortLine, sizeof(shortLine), length) == PicoSerialReportKind::None);

    PicoSerialDiagnostics transitionReporter;
    ClockState transitionClock;
    transitionClock.pico.report.error = SourceError::NoCommunication;
    PicoDiagnostics transitionPico;
    assert(transitionReporter.prepare(transitionClock, transitionPico, 0, line, sizeof(line), length) == PicoSerialReportKind::Unhealthy);
    transitionReporter.commit(transitionClock, transitionPico, 0, PicoSerialReportKind::Unhealthy);
    transitionClock.pico.report.availability = Availability::Available;
    assert(transitionReporter.prepare(transitionClock, transitionPico, 1000000, line, sizeof(line), length) == PicoSerialReportKind::Unhealthy);
    puts("AAC serial diagnostics tests passed");
    return 0;
}
