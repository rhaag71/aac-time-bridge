#include "diagnostics/TimingMetric.h"
#include <assert.h>
#include <stdio.h>

using namespace aac;

int main() {
    TimingMetric metric;
    metric.add(4999);
    metric.add(5000);
    metric.add(10000);
    metric.add(20000);
    metric.add(50000);
    metric.add(100000);
    metric.add(250000);
    assert(metric.count == 7);
    assert(metric.minUs == 4999 && metric.maxUs == 250000);
    assert(metric.meanUs() == (4999 + 5000 + 10000 + 20000 + 50000 + 100000 + 250000) / 7);
    for (size_t i = 0; i < 7; ++i) assert(metric.buckets[i] == 1);

    NtpTimingSnapshot snapshot;
    snapshot.opportunities.add(1234);
    snapshot.noDatagram = 4;
    snapshot.dequeued = 2;
    snapshot.requests = 2;
    snapshot.replies = 2;
    snapshot.errors = 1;
    snapshot.clear();
    assert(snapshot.opportunities.count == 0);
    assert(snapshot.opportunities.minUs == UINT64_MAX);
    assert(snapshot.noDatagram == 0 && snapshot.dequeued == 0);
    assert(snapshot.requests == 0 && snapshot.replies == 0 && snapshot.errors == 0);
    puts("NTP timing metric histogram and reset tests passed");
}
