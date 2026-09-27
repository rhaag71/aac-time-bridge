// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include "Configuration.h"
namespace aac {
inline int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
inline bool decodeFormValue(const char* first, const char* end, char* out, size_t capacity) {
    size_t n = 0;
    while (first < end) {
        unsigned char c = static_cast<unsigned char>(*first++);
        if (c == '+') c = ' ';
        else if (c == '%') {
            if (end - first < 2 || hexDigit(first[0]) < 0 || hexDigit(first[1]) < 0) return false;
            c = static_cast<unsigned char>(hexDigit(first[0]) * 16 + hexDigit(first[1])); first += 2;
        }
        if (c < 32 || c == 127 || n + 1 >= capacity) return false;
        out[n++] = static_cast<char>(c);
    }
    out[n] = 0;
    return true;
}
// Exactly one SSID and password. No authentication or form token. Never echo input.
inline bool parseProvisioningForm(const char* body, Configuration& out) {
    Configuration candidate;
    unsigned seen = 0;
    for (const char* field = body; *field;) {
        const char* end = strchr(field, '&');
        if (!end) end = field + strlen(field);
        const char* equals = static_cast<const char*>(memchr(field, '=', end - field));
        if (!equals) return false;
        const size_t keyLength = equals - field;
        unsigned bit; char* target; size_t capacity;
        if (keyLength == 4 && !memcmp(field, "ssid", 4)) { bit = 1; target = candidate.ssid; capacity = sizeof(candidate.ssid); }
        else if (keyLength == 8 && !memcmp(field, "password", 8)) { bit = 2; target = candidate.password; capacity = sizeof(candidate.password); }
        else return false;
        if ((seen & bit) || !decodeFormValue(equals + 1, end, target, capacity)) return false;
        seen |= bit;
        if (*end && !end[1]) return false;
        field = *end ? end + 1 : end;
    }
    if (seen != 3 || !validConfiguration(candidate)) return false;
    out = candidate;
    return true;
}
inline void escapeHtml(const char* input, char* out, size_t capacity) {
    size_t used = 0;
    for (; *input; ++input) {
        const char* replacement = nullptr;
        switch (*input) {
        case '&': replacement = "&amp;"; break;
        case '<': replacement = "&lt;"; break;
        case '>': replacement = "&gt;"; break;
        case '"': replacement = "&quot;"; break;
        case '\'': replacement = "&#39;"; break;
        }
        const size_t count = replacement ? strlen(replacement) : 1;
        if (used + count >= capacity) break;
        if (replacement) memcpy(out + used, replacement, count);
        else out[used] = *input;
        used += count;
    }
    if (capacity) out[used] = 0;
}
}
