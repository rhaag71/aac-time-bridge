// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <string.h>
namespace aac {
struct Configuration {
    uint8_t version = 1;
    char ssid[33] = {};
    char password[65] = {};
};
inline bool validConfiguration(const Configuration& c) {
    if (c.version != 1 || !memchr(c.ssid, 0, sizeof(c.ssid)) ||
        !memchr(c.password, 0, sizeof(c.password))) return false;
    const auto n = strlen(c.password);
    if (!c.ssid[0]) return false;
    if (n == 64) {
        for (size_t i = 0; i < n; ++i)
            if (!((c.password[i] >= '0' && c.password[i] <= '9') ||
                  (c.password[i] >= 'a' && c.password[i] <= 'f') ||
                  (c.password[i] >= 'A' && c.password[i] <= 'F'))) return false;
        return true;
    }
    return n == 0 || (n >= 8 && n <= 63);
}
// USB serial only: wifi <SSID><TAB><password>. Never echo the password.
inline bool parseConfiguration(const char* line, Configuration& out) {
    if (strncmp(line, "wifi ", 5)) return false;
    const char* ssid = line + 5;
    const char* tab = strchr(ssid, '\t');
    if (!tab || tab - ssid < 1 || tab - ssid > 32 || strlen(tab + 1) > 64) return false;
    Configuration c;
    memcpy(c.ssid, ssid, tab - ssid);
    strcpy(c.password, tab + 1);
    if (!validConfiguration(c)) return false;
    out = c;
    return true;
}
}
