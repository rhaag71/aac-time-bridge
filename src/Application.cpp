// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "Application.h"
#include <Arduino.h>
#include <esp_timer.h>
namespace aac {
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
namespace {
void appendMetric(char* output, size_t capacity, size_t& used, const char* name,
                  const TimingMetric& metric, bool withBuckets) {
    if (used >= capacity) return;
    if (!metric.hasSamples()) {
        const int n = snprintf(output + used, capacity - used, "%s=n/a", name);
        if (n > 0 && static_cast<size_t>(n) < capacity - used) used += static_cast<size_t>(n);
        return;
    }
    int n = snprintf(output + used, capacity - used, "%s[n=%lu min=%llu avg=%llu max=%llu",
        name, static_cast<unsigned long>(metric.count),
        static_cast<unsigned long long>(metric.minUs),
        static_cast<unsigned long long>(metric.meanUs()),
        static_cast<unsigned long long>(metric.maxUs));
    if (n < 0 || static_cast<size_t>(n) >= capacity - used) return;
    used += static_cast<size_t>(n);
    if (withBuckets) {
        n = snprintf(output + used, capacity - used,
            " h=%lu/%lu/%lu/%lu/%lu/%lu/%lu",
            static_cast<unsigned long>(metric.buckets[0]),
            static_cast<unsigned long>(metric.buckets[1]),
            static_cast<unsigned long>(metric.buckets[2]),
            static_cast<unsigned long>(metric.buckets[3]),
            static_cast<unsigned long>(metric.buckets[4]),
            static_cast<unsigned long>(metric.buckets[5]),
            static_cast<unsigned long>(metric.buckets[6]));
        if (n < 0 || static_cast<size_t>(n) >= capacity - used) return;
        used += static_cast<size_t>(n);
    }
    if (used < capacity) output[used++] = ']';
    if (used < capacity) output[used] = '\0';
}
}
#endif

void Application::begin() {
    indicator_.begin(); // Explicit LOW before any initialized status is rendered.
    state_ = ApplianceState{};
    Serial.begin(115200);
    watchdog_.captureReset(state_.diagnostics); // Read-only; does not subscribe.
    printBootBanner();
    Serial.println("Starting in 5 seconds (serial service delay)...");
    delay(5000); // Yield to RTOS/idle watchdogs; application not subscribed yet.
    printBootBanner(); // Monitor may have attached during the delay.
    Serial.println("Initializing...");
    watchdog_.begin(state_.diagnostics);
    const auto now = static_cast<MonotonicUs>(esp_timer_get_time());
    clock_.begin(now); // Every reset discards all authority and associations.
    state_.clock = clock_.state();
    network_.begin();
    state_.initialized = true;
    poll(); // Publish initial network/clock state and complete first healthy pass.
    lastDiagnosticUs_ = state_.uptimeUs;
    printStatus("Application ready");
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    lastTimingReportAtUs_ = state_.uptimeUs;
#endif
}
void Application::poll() {
    auto now = static_cast<MonotonicUs>(esp_timer_get_time());
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    const MonotonicUs pollStartedAt = now;
    if (havePreviousPoll_ && now >= previousPollAtUs_) loopIntervals_.add(now - previousPollAtUs_);
    previousPollAtUs_ = now;
    havePreviousPoll_ = true;
#endif
    network_.poll(now, state_.network);
#ifdef AAC_WATCHDOG_BENCH
    if (network_.takeBenchRequest()) watchdog_.wedgeForBench();
#endif
    now = static_cast<MonotonicUs>(esp_timer_get_time());
    clock_.poll(now);
    state_.clock = clock_.state();
    state_.diagnostics.pico = pico_.diagnostics();
    state_.uptimeUs = now;
    indicator_.render(state_);
    network_.serve(state_);
    if (!picoSerialLine_.pending()) {
        char picoLine[BoundedSerialLine::kCapacity];
        size_t picoLineLength = 0;
        const PicoSerialReportKind report = picoSerialDiagnostics_.prepare(
            state_.clock, state_.diagnostics.pico, now, picoLine, sizeof(picoLine), picoLineLength);
        if (report != PicoSerialReportKind::None && picoSerialLine_.begin(picoLine, picoLineLength)) {
            pendingPicoReport_ = report;
            pendingPicoClock_ = state_.clock;
            pendingPicoSnapshot_ = state_.diagnostics.pico;
        }
    }
    if (picoSerialLine_.pending()) {
        const int available = Serial.availableForWrite();
        if (available > 0) {
            size_t chunkLength = 0;
            const char* chunk = picoSerialLine_.nextChunk(static_cast<size_t>(available), 32, chunkLength);
            if (chunk && chunkLength) {
                const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(chunk), chunkLength);
                if (written && picoSerialLine_.consume(written)) {
                    picoSerialDiagnostics_.commit(pendingPicoClock_, pendingPicoSnapshot_, now,
                                                  pendingPicoReport_);
                    pendingPicoReport_ = PicoSerialReportKind::None;
                }
            }
        }
    }
    if (now - lastDiagnosticUs_ >= 60000000ULL) {
        lastDiagnosticUs_ = now;
        printStatus("Status");
    }
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    const MonotonicUs beforeWatchdogAt = static_cast<MonotonicUs>(esp_timer_get_time());
    loopDurations_.add(beforeWatchdogAt >= pollStartedAt ? beforeWatchdogAt - pollStartedAt : 0);
    prepareTimingReport(beforeWatchdogAt);
    serviceTimingReport();
#endif
    watchdog_.completedPass(state_.diagnostics); // Sole feed, after all application work returns.
}
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
void Application::prepareTimingReport(MonotonicUs now) {
    if (now < lastTimingReportAtUs_ || now - lastTimingReportAtUs_ < 30000000ULL ||
        nextTimingLine_ < 3) return;
    lastTimingReportAtUs_ = now;
    const NtpTimingSnapshot ntp = network_.takeNtpTimingSnapshot();
    for (size_t i = 0; i < 3; ++i) timingBuffers_[i][0] = '\0';
    size_t used = 0;
    const char* prefix = "NTP-TIMING APP ";
    used = static_cast<size_t>(snprintf(timingBuffers_[0], sizeof(timingBuffers_[0]), "%s", prefix));
    appendMetric(timingBuffers_[0], sizeof(timingBuffers_[0]), used, "loop-us", loopIntervals_, true);
    if (used < sizeof(timingBuffers_[0])) timingBuffers_[0][used++] = ' ';
    appendMetric(timingBuffers_[0], sizeof(timingBuffers_[0]), used, "poll-us", loopDurations_, true);
    if (used + 3 <= sizeof(timingBuffers_[0])) {
        timingBuffers_[0][used++] = '\r'; timingBuffers_[0][used++] = '\n'; timingBuffers_[0][used] = '\0';
        timingLines_[0].begin(timingBuffers_[0], used);
    }

    used = static_cast<size_t>(snprintf(timingBuffers_[1], sizeof(timingBuffers_[1]), "NTP-TIMING SERVICE "));
    appendMetric(timingBuffers_[1], sizeof(timingBuffers_[1]), used, "gap-us", ntp.opportunities, true);
    if (used < sizeof(timingBuffers_[1])) timingBuffers_[1][used++] = ' ';
    appendMetric(timingBuffers_[1], sizeof(timingBuffers_[1]), used, "duration-us", ntp.serviceDuration, true);
    if (used < sizeof(timingBuffers_[1])) timingBuffers_[1][used++] = ' ';
    appendMetric(timingBuffers_[1], sizeof(timingBuffers_[1]), used, "poll-rx-us", ntp.pollToReceive, true);
    if (used < sizeof(timingBuffers_[1])) {
        const int n = snprintf(timingBuffers_[1] + used, sizeof(timingBuffers_[1]) - used,
            " empty=%lu dequeued=%lu\r\n", static_cast<unsigned long>(ntp.noDatagram),
            static_cast<unsigned long>(ntp.dequeued));
        if (n > 0 && static_cast<size_t>(n) < sizeof(timingBuffers_[1]) - used) {
            used += static_cast<size_t>(n);
            timingLines_[1].begin(timingBuffers_[1], used);
        }
    }

    used = static_cast<size_t>(snprintf(timingBuffers_[2], sizeof(timingBuffers_[2]), "NTP-TIMING PACKET "));
    appendMetric(timingBuffers_[2], sizeof(timingBuffers_[2]), used, "recv-call-us", ntp.recvCall, false);
    if (used < sizeof(timingBuffers_[2])) timingBuffers_[2][used++] = ' ';
    appendMetric(timingBuffers_[2], sizeof(timingBuffers_[2]), used, "return-T2-us", ntp.receiveToT2, false);
    if (used < sizeof(timingBuffers_[2])) timingBuffers_[2][used++] = ' ';
    appendMetric(timingBuffers_[2], sizeof(timingBuffers_[2]), used, "T2-T3-us", ntp.t2ToT3, false);
    if (used < sizeof(timingBuffers_[2])) timingBuffers_[2][used++] = ' ';
    appendMetric(timingBuffers_[2], sizeof(timingBuffers_[2]), used, "T3-send-us", ntp.t3ToSend, false);
    if (used < sizeof(timingBuffers_[2])) timingBuffers_[2][used++] = ' ';
    appendMetric(timingBuffers_[2], sizeof(timingBuffers_[2]), used, "send-us", ntp.sendCall, false);
    if (used < sizeof(timingBuffers_[2])) {
        const int n = snprintf(timingBuffers_[2] + used, sizeof(timingBuffers_[2]) - used,
            " req=%lu reply=%lu error=%lu\r\n", static_cast<unsigned long>(ntp.requests),
            static_cast<unsigned long>(ntp.replies), static_cast<unsigned long>(ntp.errors));
        if (n > 0 && static_cast<size_t>(n) < sizeof(timingBuffers_[2]) - used) {
            used += static_cast<size_t>(n);
            timingLines_[2].begin(timingBuffers_[2], used);
        }
    }
    nextTimingLine_ = 0;
    loopIntervals_.clear();
    loopDurations_.clear();
}

void Application::serviceTimingReport() {
    if (picoSerialLine_.pending()) return;
    while (nextTimingLine_ < 3 && !timingLines_[nextTimingLine_].pending()) ++nextTimingLine_;
    if (nextTimingLine_ >= 3) return;
    const int available = Serial.availableForWrite();
    if (available <= 0) return;
    size_t chunkLength = 0;
    const char* chunk = timingLines_[nextTimingLine_].nextChunk(static_cast<size_t>(available), 32, chunkLength);
    if (chunk && chunkLength && timingLines_[nextTimingLine_].consume(
            Serial.write(reinterpret_cast<const uint8_t*>(chunk), chunkLength))) ++nextTimingLine_;
}
#endif
void Application::printBootBanner() const {
    Serial.printf("\nAAC Time Bridge | Firmware: AAC Protocol v1 acquisition | Build: %s | %s %s\nReset: %s (%d)\n",
        state_.diagnostics.benchBuild ? "WATCHDOG BENCH" : "PRODUCTION", __DATE__, __TIME__,
        state_.diagnostics.resetReason, state_.diagnostics.resetCode);
}
void Application::printStatus(const char* prefix) const {
    const auto& pico = state_.diagnostics.pico;
    const MonotonicUs packetAge = pico.hasValidPacketAt && state_.uptimeUs >= pico.lastValidPacketAtUs ? state_.uptimeUs - pico.lastValidPacketAtUs : 0;
    const MonotonicUs edgeAge = pico.hasEdgeAt && state_.uptimeUs >= pico.lastQualifiedEdgeAtUs ? state_.uptimeUs - pico.lastQualifiedEdgeAtUs : 0;
    Serial.printf("%s | network=%s IP=%s setup-AP=%s AP-IP=%s | clock=%s selected=%s | AAC=%s/%s pkt=%lu bnd=%lu sync=%lu flags=0x%04X sat=%s%u tx=%lu edge=%lu overrun=%lu packet-age=%s%llu ms edge-age=%s%llu ms phase=%s reason=%s | NTP=%s req=%lu sync=%lu unsync=%lu reject=%lu stratum=%u err=%d | watchdog=%s error=%d\n",
        prefix, networkStatusName(state_.network.status), state_.network.address,
        state_.network.provisioning ? "active" : "off", state_.network.apAddress,
        statusLedOn(state_) ? "synchronized" : "unsynchronized",
        state_.clock.selected == SourceId::Pico ? "AAC" : "none",
        state_.clock.pico.report.availability == Availability::Available ? "responding" : "unavailable",
        picoPacketResultName(pico.lastResult), static_cast<unsigned long>(pico.packetSequence),
        static_cast<unsigned long>(pico.boundarySequence), static_cast<unsigned long>(pico.syncSequence), static_cast<unsigned>(pico.flags),
        (pico.flags & kPicoSatValid) ? "" : "invalid/", static_cast<unsigned>(pico.satellites),
        static_cast<unsigned long>(pico.transactions), static_cast<unsigned long>(pico.capturedEdges),
        static_cast<unsigned long>(pico.edgeOverflows),
        pico.hasValidPacketAt ? "" : "unknown/", static_cast<unsigned long long>(packetAge / 1000ULL),
        pico.hasEdgeAt ? "" : "unknown/", static_cast<unsigned long long>(edgeAge / 1000ULL),
        pico.phaseAssociated ? "yes" : "no", sourceErrorName(state_.clock.pico.report.error),
        ntpServiceStateName(state_.diagnostics.ntp.state), static_cast<unsigned long>(state_.diagnostics.ntp.requests),
        static_cast<unsigned long>(state_.diagnostics.ntp.synchronizedReplies),
        static_cast<unsigned long>(state_.diagnostics.ntp.unsynchronizedReplies),
        static_cast<unsigned long>(state_.diagnostics.ntp.rejectedRequests),
        static_cast<unsigned>(state_.diagnostics.ntp.stratum),
        state_.diagnostics.ntp.lastError,
        state_.diagnostics.watchdogArmed ? "armed" : "FAULT", state_.diagnostics.watchdogError);
}
}
