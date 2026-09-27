// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <string.h>
namespace aac {
class LineEditor {
public:
    enum class Event { None, Submit, Reject };
    static constexpr size_t capacity = 112;
    Event accept(unsigned char c) {
        echo_[0] = 0;
        if (finished_) clearLine();
        if (skipLf_ && c == '\n') { skipLf_ = false; return Event::None; }
        skipLf_ = false;
        if (c == '\r' || c == '\n') {
            skipLf_ = c == '\r'; finished_ = true;
            strcpy(echo_, "\r\n");
            return invalid_ ? Event::Reject : used_ ? Event::Submit : Event::None;
        }
        if (invalid_) return Event::None; // Sticky until Enter; never execute a truncated line.
        if (c == '\b' || c == 127) {
            if (used_) {
                const bool visible = hiddenAt_ == capacity || used_ - 1 == hiddenAt_;
                line_[--used_] = 0;
                if (used_ == hiddenAt_) hiddenAt_ = capacity;
                if (visible) strcpy(echo_, "\b \b");
            }
            return Event::None;
        }
        if ((c < 32 && c != '\t') || used_ + 1 >= capacity) {
            invalid_ = true; return Event::None;
        }
        // Hide everything after the first TAB, even in a malformed command.
        // Deleting hidden bytes emits nothing; deleting the now-empty separator
        // restores visible SSID editing without ever redisplaying password bytes.
        if (hiddenAt_ == capacity) {
            if (c == '\t') { hiddenAt_ = used_; strcpy(echo_, " "); }
            else { echo_[0] = static_cast<char>(c); echo_[1] = 0; }
        }
        line_[used_++] = static_cast<char>(c); line_[used_] = 0;
        return Event::None;
    }
    const char* echo() const { return echo_; }
    const char* line() const { return invalid_ ? "" : line_; }
    void clearLine() {
        memset(line_, 0, sizeof(line_)); used_ = 0;
        hiddenAt_ = capacity; invalid_ = finished_ = false;
        // Keep skipLf_ across command execution so CRLF is one Enter.
    }
private:
    char line_[capacity] = {};
    char echo_[4] = {};
    size_t used_ = 0, hiddenAt_ = capacity;
    bool invalid_ = false, finished_ = false, skipLf_ = false;
};
}
