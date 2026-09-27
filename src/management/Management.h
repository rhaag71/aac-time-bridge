// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <string.h>
namespace aac {
enum class ManagementAction { None, Reboot, FactoryReset };
enum class ManagementResult { Scheduled, Busy, StorageError, Invalid };
inline ManagementAction serialManagementAction(const char* line) {
    return !strcmp(line, "factory reset") ? ManagementAction::FactoryReset : ManagementAction::None;
}
inline bool confirmedManagementAction(ManagementAction action, const char* body) {
    return (action == ManagementAction::Reboot && !strcmp(body, "confirm=reboot")) ||
           (action == ManagementAction::FactoryReset && !strcmp(body, "confirm=factory-reset"));
}
class ResetPlatform {
public:
    virtual ~ResetPlatform() = default;
    virtual bool eraseOwnedConfiguration() = 0;
    virtual void restart() = 0;
};
// Shared web/serial path. Normal reboot never touches persistent storage.
class Management {
public:
    explicit Management(ResetPlatform& platform) : platform_(platform) {}
    ManagementResult request(ManagementAction action, uint64_t now) {
        if (pending()) return ManagementResult::Busy;
        if (action == ManagementAction::None) return ManagementResult::Invalid;
        if (action == ManagementAction::FactoryReset && !platform_.eraseOwnedConfiguration())
            return ManagementResult::StorageError;
        action_ = action;
        requestedAt_ = now;
        return ManagementResult::Scheduled;
    }
    bool pending() const { return action_ != ManagementAction::None; }
    void poll(uint64_t now) {
        // Bounded grace period for HTTP/serial acknowledgement; no blocking wait.
        // Independent of later client traffic; cannot postpone reset indefinitely.
        if (pending() && !restartIssued_ && now >= requestedAt_ && now - requestedAt_ >= 2500000ULL) {
            restartIssued_ = true;
            platform_.restart();
        }
    }
private:
    ResetPlatform& platform_;
    ManagementAction action_ = ManagementAction::None;
    uint64_t requestedAt_ = 0;
    bool restartIssued_ = false;
};
}
