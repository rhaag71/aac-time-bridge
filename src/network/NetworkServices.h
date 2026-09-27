// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "Configuration.h"
#include "console/LineEditor.h"
#include "console/Commands.h"
#include "NetworkPolicy.h"
#include "status/StatusPage.h"
#include <WiFi.h>
#include "HttpRequest.h"
#include "platform/ApplianceReset.h"
#include "web/UiAssets.h"
namespace aac {
class NetworkServices {
public:
    void begin();
    void poll(MonotonicUs now, NetworkState& state);
    void serve(const ApplianceState& state);
    bool takeBenchRequest();
private:
    void startConnection(MonotonicUs now);
    void startAp();
    void stopAp(MonotonicUs now);
    void command(const char* line, MonotonicUs now);
    void statusPage(const ApplianceState& state);
    ManagementResult requestManagement(ManagementAction action, MonotonicUs now);
    ApplianceReset resetPlatform_;
    Management management_{resetPlatform_};
    bool saveConfiguration(const Configuration& candidate, MonotonicUs now);
    void finishClient();
    void reply(const char* status, const char* message);
    Configuration config_;
    WiFiServer web_{80};
    WiFiClient client_;
    HttpRequest request_;
    char response_[kUiResponseCapacity] = {};
    const char* staticResponse_ = nullptr;
    char apName_[32] = {};
    const char* setupMessage_ = "No Wi-Fi configuration. Enter your target network below.";
    bool pendingConnection_ = false, startingConnection_ = false, failureReported_ = false;
    size_t responseSize_ = 0, sent_ = 0;
    MonotonicUs clientSince_ = 0;
    bool configured_ = false, storageOk_ = true, apActive_ = false, apError_ = false;
    bool benchRequest_ = false;
    LineEditor console_;
    NetworkPolicy networkPolicy_;
    bool apStopFailed_ = false;
    MonotonicUs lastApStopAttempt_ = 0;
    MonotonicUs lastAttempt_ = 0, lastApAttempt_ = 0;
};
}
