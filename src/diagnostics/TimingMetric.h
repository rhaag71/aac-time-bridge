// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace aac {

// Fixed-storage timing accumulator. Boundaries are expressed in microseconds.
struct TimingMetric {
    uint32_t count = 0;
    uint64_t totalUs = 0;
    uint64_t minUs = UINT64_MAX;
    uint64_t maxUs = 0;
    uint32_t buckets[7] = {};

    void add(uint64_t elapsedUs) {
        ++count;
        totalUs += elapsedUs;
        if (elapsedUs < minUs) minUs = elapsedUs;
        if (elapsedUs > maxUs) maxUs = elapsedUs;
        size_t bucket = elapsedUs < 5000ULL ? 0 : elapsedUs < 10000ULL ? 1 :
            elapsedUs < 20000ULL ? 2 : elapsedUs < 50000ULL ? 3 :
            elapsedUs < 100000ULL ? 4 : elapsedUs < 250000ULL ? 5 : 6;
        ++buckets[bucket];
    }

    uint64_t meanUs() const { return count ? totalUs / count : 0; }
    bool hasSamples() const { return count != 0; }

    void clear() {
        count = 0;
        totalUs = 0;
        minUs = UINT64_MAX;
        maxUs = 0;
        memset(buckets, 0, sizeof(buckets));
    }
};

struct NtpTimingSnapshot {
    TimingMetric opportunities;
    TimingMetric serviceDuration;
    TimingMetric pollToReceive;
    TimingMetric recvCall;
    TimingMetric receiveToT2;
    TimingMetric t2ToT3;
    TimingMetric t3ToSend;
    TimingMetric sendCall;
    uint32_t noDatagram = 0;
    uint32_t dequeued = 0;
    uint32_t requests = 0;
    uint32_t replies = 0;
    uint32_t errors = 0;

    void clear() {
        opportunities.clear();
        serviceDuration.clear();
        pollToReceive.clear();
        recvCall.clear();
        receiveToT2.clear();
        t2ToT3.clear();
        t3ToSend.clear();
        sendCall.clear();
        noDatagram = dequeued = requests = replies = errors = 0;
    }
};

} // namespace aac
