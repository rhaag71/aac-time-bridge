// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "platform/ApplianceReset.h"
#include <Preferences.h>
#include <assert.h>
#include <stdio.h>
namespace fake {
std::map<std::string, std::map<std::string, std::string>> storage;
bool openOk = true, clearOk = true;
unsigned opens = 0, clears = 0, closes = 0;
std::string openedNamespace;
}
unsigned restarts = 0;
void esp_restart() { ++restarts; }
int main() {
    aac::ApplianceReset adapter;
    fake::storage["aac-bridge"]["wifi-v1"] = "test-configuration";
    fake::storage["aac-bridge"]["future-setting"] = "also-owned";
    fake::storage["platform"]["unrelated"] = "preserve";
    adapter.restart();
    assert(restarts == 1 && fake::opens == 0 && fake::clears == 0);
    assert(fake::storage["aac-bridge"].size() == 2);
    assert(adapter.eraseOwnedConfiguration());
    assert(fake::openedNamespace == "aac-bridge");
    assert(fake::storage["aac-bridge"].empty());
    assert(fake::storage["platform"]["unrelated"] == "preserve");
    assert(fake::clears == 1 && fake::closes == 1 && restarts == 1);
    assert(adapter.eraseOwnedConfiguration()); // Already unconfigured is valid.
    fake::clearOk = false;
    assert(!adapter.eraseOwnedConfiguration());
    assert(fake::closes == 3);
    fake::openOk = false;
    assert(!adapter.eraseOwnedConfiguration());
    assert(fake::clears == 3 && fake::closes == 3);
    puts("reset adapter namespace isolation and software-restart contract tests passed (fake SDK)");
}
