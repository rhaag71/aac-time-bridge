// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "management/Management.h"
namespace aac {
// All firmware-owned persistent data must stay in this dedicated namespace.
constexpr const char* kConfigurationNamespace = "aac-bridge";
constexpr const char* kWifiConfigurationKey = "wifi-v1";
class ApplianceReset final : public ResetPlatform {
public:
    bool eraseOwnedConfiguration() override;
    void restart() override;
};
}
