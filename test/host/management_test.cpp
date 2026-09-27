// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "management/Management.h"
#include "network/HttpRequest.h"
#include <assert.h>
#include <stdio.h>
#include <string>
using namespace aac;
class FakePlatform : public ResetPlatform {
public:
    bool eraseOwnedConfiguration() override { ++erases; return eraseOk; }
    void restart() override { ++restarts; }
    bool eraseOk = true;
    unsigned erases = 0, restarts = 0;
};
HttpRequest::Result route(const std::string& wire) {
    HttpRequest request;
    auto result = HttpRequest::Result::Pending;
    for (char c : wire) result = request.append(c);
    return result;
}
int main() {
    using Result = HttpRequest::Result;
    for (const char* path : {"reboot", "factory-reset"}) {
        const bool factory = !strcmp(path, "factory-reset");
        const auto action = factory ? ManagementAction::FactoryReset : ManagementAction::Reboot;
        const auto page = factory ? Result::FactoryResetPage : Result::RebootPage;
        const auto post = factory ? Result::FactoryReset : Result::Reboot;
        const std::string url = std::string("/manage/") + path;
        assert(route("GET " + url + " HTTP/1.1\r\n\r\n") == page);
        assert(route("POST " + url + " HTTP/1.1\r\n\r\n") == Result::Reject);
        const std::string body = std::string("confirm=") + path;
        const std::string header = "POST " + url + " HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        assert(route(header + body.substr(0, body.size() - 1)) == Result::Pending);
        assert(route(header + body) == post);
        assert(confirmedManagementAction(action, body.c_str()));
        assert(!confirmedManagementAction(action, ""));
        assert(!confirmedManagementAction(action, "confirm=yes"));
        assert(!confirmedManagementAction(action, (body + "&confirm=no").c_str()));
        assert(!confirmedManagementAction(action, factory ? "confirm=reboot" : "confirm=factory-reset"));
    }
    assert(serialManagementAction("factory reset") == ManagementAction::FactoryReset);
    for (const char* line : {"FACTORY RESET", "factory reset ", " factory reset", "FACTORY", "factory reset now", "REBOOT"})
        assert(serialManagementAction(line) == ManagementAction::None);
    FakePlatform platform;
    Management reboot(platform);
    assert(reboot.request(ManagementAction::None, 0) == ManagementResult::Invalid);
    assert(reboot.request(ManagementAction::Reboot, 5000000000ULL) == ManagementResult::Scheduled);
    assert(platform.erases == 0 && platform.restarts == 0);
    assert(reboot.request(ManagementAction::FactoryReset, 5000000001ULL) == ManagementResult::Busy);
    assert(platform.erases == 0); // A queued reboot cannot be escalated into an erase.
    reboot.poll(4999999999ULL); reboot.poll(5002499999ULL);
    assert(platform.restarts == 0);
    reboot.poll(5002500000ULL); reboot.poll(5003500000ULL);
    assert(platform.restarts == 1 && platform.erases == 0);
    for (bool serial : {false, true}) {
        FakePlatform resetPlatform;
        Management reset(resetPlatform);
        const auto action = serial ? serialManagementAction("factory reset") : ManagementAction::FactoryReset;
        assert(reset.request(action, 0) == ManagementResult::Scheduled);
        assert(resetPlatform.erases == 1 && resetPlatform.restarts == 0);
        assert(reset.request(action, 1) == ManagementResult::Busy);
        reset.poll(2500000);
        assert(resetPlatform.erases == 1 && resetPlatform.restarts == 1);
    }
    FakePlatform failure;
    failure.eraseOk = false;
    Management reset(failure);
    assert(reset.request(ManagementAction::FactoryReset, 0) == ManagementResult::StorageError);
    assert(!reset.pending()); reset.poll(3000000);
    assert(failure.restarts == 0);
    failure.eraseOk = true;
    assert(reset.request(ManagementAction::FactoryReset, 4000000) == ManagementResult::Scheduled);
    reset.poll(6500000);
    assert(failure.restarts == 1 && failure.erases == 2);
    puts("management routes, confirmation, shared reset, storage failure and restart timing tests passed");
}
