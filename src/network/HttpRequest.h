// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <string.h>
#include <strings.h>
namespace aac {
// Bounded, single-request HTTP subset. No chunking, keep-alive or pipelining.
class HttpRequest {
public:
    enum class Result { Pending, Status, Setup, Progress, Css, Js, Save, RebootPage, FactoryResetPage, Reboot, FactoryReset, Reject };
    Result append(char c) {
        if (result_ != Result::Pending) return result_;
        if (c == 0) return result_ = Result::Reject;
        if (headerSize_) {
            body_[bodySize_++] = c;
            body_[bodySize_] = 0;
            if (bodySize_ == contentLength_) result_ = postRoute_;
            return result_;
        }
        if (used_ + 1 >= sizeof(headers_)) return result_ = Result::Reject;
        headers_[used_++] = c; headers_[used_] = 0;
        if (used_ < 4 || memcmp(headers_ + used_ - 4, "\r\n\r\n", 4)) return result_;
        headerSize_ = used_;
        return result_ = parseHeaders();
    }
    const char* body() const { return body_; }
    void clear() { memset(headers_, 0, sizeof(headers_)); memset(body_, 0, sizeof(body_));
        used_ = headerSize_ = bodySize_ = contentLength_ = 0; result_ = Result::Pending; postRoute_ = Result::Reject; }
private:
    Result parseHeaders() {
        char* end = strstr(headers_, "\r\n");
        if (!end) return Result::Reject;
        *end = 0;
        Result route = Result::Reject;
        if (!strcmp(headers_, "GET / HTTP/1.1") || !strcmp(headers_, "GET / HTTP/1.0")) route = Result::Status;
        if (!strcmp(headers_, "GET /ui.css HTTP/1.1") || !strcmp(headers_, "GET /ui.css HTTP/1.0")) route = Result::Css;
        if (!strcmp(headers_, "GET /ui.js HTTP/1.1") || !strcmp(headers_, "GET /ui.js HTTP/1.0")) route = Result::Js;
        if (!strcmp(headers_, "GET /setup HTTP/1.1") || !strcmp(headers_, "GET /setup HTTP/1.0")) route = Result::Setup;
        if (!strcmp(headers_, "GET /setup/result HTTP/1.1") || !strcmp(headers_, "GET /setup/result HTTP/1.0")) route = Result::Progress;
        if (!strcmp(headers_, "POST /configure HTTP/1.1") || !strcmp(headers_, "POST /configure HTTP/1.0")) route = Result::Save;
        if (!strcmp(headers_, "GET /manage/reboot HTTP/1.1") || !strcmp(headers_, "GET /manage/reboot HTTP/1.0")) route = Result::RebootPage;
        if (!strcmp(headers_, "GET /manage/factory-reset HTTP/1.1") || !strcmp(headers_, "GET /manage/factory-reset HTTP/1.0")) route = Result::FactoryResetPage;
        if (!strcmp(headers_, "POST /manage/reboot HTTP/1.1") || !strcmp(headers_, "POST /manage/reboot HTTP/1.0")) route = Result::Reboot;
        if (!strcmp(headers_, "POST /manage/factory-reset HTTP/1.1") || !strcmp(headers_, "POST /manage/factory-reset HTTP/1.0")) route = Result::FactoryReset;
        bool lengthSeen = false, typeSeen = false;
        for (char* line = end + 2; *line && strncmp(line, "\r\n", 2); line = end + 2) {
            end = strstr(line, "\r\n");
            if (!end) return Result::Reject;
            *end = 0;
            char* colon = strchr(line, ':');
            if (!colon || colon == line || *line == ' ' || *line == '\t') return Result::Reject;
            *colon = 0;
            // Do not accept whitespace before the colon (ambiguous framing).
            for (char* p = line; *p; ++p) if (*p <= ' ' || *p >= 127) return Result::Reject;
            char* value = colon + 1;
            while (*value == ' ' || *value == '\t') ++value;
            char* tail = end;
            while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t')) *--tail = 0;
            if (!strcasecmp(line, "Transfer-Encoding") || !strcasecmp(line, "Expect")) return Result::Reject;
            if (!strcasecmp(line, "Content-Length")) {
                if (lengthSeen || !*value) return Result::Reject;
                lengthSeen = true;
                for (char* p = value; *p; ++p) {
                    if (*p < '0' || *p > '9') return Result::Reject;
                    contentLength_ = contentLength_ * 10 + (*p - '0');
                    if (contentLength_ >= sizeof(body_)) return Result::Reject;
                }
            }
            if (!strcasecmp(line, "Content-Type")) {
                if (typeSeen || strcasecmp(value, "application/x-www-form-urlencoded")) return Result::Reject;
                typeSeen = true;
            }
        }
        if (route == Result::Save || route == Result::Reboot || route == Result::FactoryReset) {
            postRoute_ = route;
            return lengthSeen && typeSeen && contentLength_ > 0 ? Result::Pending : Result::Reject;
        }
        return contentLength_ == 0 ? route : Result::Reject;
    }
    char headers_[2048] = {};
    char body_[513] = {};
    size_t used_ = 0, headerSize_ = 0, bodySize_ = 0, contentLength_ = 0;
    Result result_ = Result::Pending;
    Result postRoute_ = Result::Reject;
};
}
