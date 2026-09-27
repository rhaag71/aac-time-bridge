// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "ApplianceReset.h"
#include <Preferences.h>
#include <esp_system.h>
namespace aac {
bool ApplianceReset::eraseOwnedConfiguration() {
    Preferences prefs;
    if (!prefs.begin(kConfigurationNamespace, false)) return false;
    // Preferences::clear erases and commits this namespace only, not the NVS partition.
    const bool erased = prefs.clear();
    prefs.end();
    return erased;
}
void ApplianceReset::restart() {
    esp_restart(); // Normal software reset, never a deliberate watchdog expiration.
}
}
