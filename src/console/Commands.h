// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "management/Management.h"
namespace aac {
enum class ConsoleCommand { Unknown, Provision, Wifi, FactoryReset
#ifdef AAC_WATCHDOG_BENCH
    , WedgeWatchdog
#endif
};
inline ConsoleCommand consoleCommand(const char* line) {
    if (serialManagementAction(line) == ManagementAction::FactoryReset) return ConsoleCommand::FactoryReset;
    if (!strcmp(line, "provision")) return ConsoleCommand::Provision;
    if (!strncmp(line, "wifi ", 5)) return ConsoleCommand::Wifi;
#ifdef AAC_WATCHDOG_BENCH
    if (!strcmp(line, "wedge watchdog")) return ConsoleCommand::WedgeWatchdog;
#endif
    return ConsoleCommand::Unknown;
}
constexpr const char* kInvalidConsoleInput = "Input discarded (too long or unsupported control). Re-enter the command.";
constexpr const char* kInvalidConsoleCommand = "Invalid command/configuration; nothing changed.";
}
