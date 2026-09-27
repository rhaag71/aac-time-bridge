// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "status/ApplianceState.h"
#include <stdio.h>
namespace aac {
struct NetworkObservation {
    NetworkStatus status = NetworkStatus::Unconfigured;
    bool connected = false, announceLan = false, lost = false;
    bool recoveryDue = false, startAp = false, stopAp = false;
};
// One credential-first state producer for Serial, web and AP lifecycle.
// linkReady means associated AND a nonzero LAN IP, not just radio association.
class NetworkPolicy {
public:
    void begin(MonotonicUs now) { disconnectedSince_ = now; connected_ = false; }
    bool connected() const { return connected_; }
    NetworkObservation observe(bool configured, bool linkReady, bool apActive,
                               MonotonicUs lastAttempt, MonotonicUs now) {
        NetworkObservation o;
        o.connected = configured && linkReady;
        o.lost = connected_ && !o.connected;
        o.announceLan = !connected_ && o.connected;
        if (o.lost) disconnectedSince_ = now;
        o.recoveryDue = !o.connected && (!configured || now - disconnectedSince_ >= 60000000ULL);
        o.startAp = o.recoveryDue && !apActive;
        o.stopAp = o.connected && apActive;
        o.status = !configured ? NetworkStatus::Unconfigured : o.connected ? NetworkStatus::Connected :
            now - lastAttempt < 10000000ULL ? NetworkStatus::Connecting : NetworkStatus::Disconnected;
        connected_ = o.connected;
        return o;
    }
private:
    bool connected_ = false;
    MonotonicUs disconnectedSince_ = 0;
};
inline void formatLanAnnouncement(char* output, size_t size, const char* address) {
    snprintf(output, size, "LAN status: http://%s/", address);
}
}
