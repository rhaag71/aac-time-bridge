// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "network/HttpRequest.h"
#include "network/Provisioning.h"
#include <assert.h>
#include <stdio.h>
#include <string>
using namespace aac;
using Result = HttpRequest::Result;
Result feed(HttpRequest& request, const std::string& bytes) {
    Result result = Result::Pending;
    for (char c : bytes) result = request.append(c);
    return result;
}
std::string post(const std::string& body, const std::string& extra = "") {
    return "POST /configure HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n" + extra + "\r\n" + body;
}
int main() {
    const std::string body = "ssid=Home+%26+Lab&password=p%2Bass%26word";
    HttpRequest request;
    const auto bytes = post(body);
    // Every prefix remains pending; only the last declared body byte permits saving.
    for (size_t i = 0; i < bytes.size(); ++i)
        assert(request.append(bytes[i]) == (i + 1 == bytes.size() ? Result::Save : Result::Pending));
    Configuration c;
    assert(parseProvisioningForm(request.body(), c));
    assert(!strcmp(c.ssid, "Home & Lab") && !strcmp(c.password, "p+ass&word"));
    request.clear(); assert(!*request.body());
    assert(feed(request, "GET /setup HTTP/1.1\r\nHost: bridge\r\n\r\n") == Result::Setup);
    request.clear();
    assert(feed(request, "GET /setup/result HTTP/1.0\r\n\r\n") == Result::Progress);
    request.clear();
    assert(feed(request, "GET /telemetry HTTP/1.1\r\nHost: bridge\r\n\r\n") == Result::Telemetry);
    request.clear();
    assert(feed(request, "POST /telemetry HTTP/1.1\r\n\r\n") == Result::Reject);
    request.clear(); assert(feed(request, "GET / HTTP/1.1\r\nHost: bridge\r\n\r\n") == Result::Status);
    request.clear(); assert(feed(request, "GET /ui.css HTTP/1.1\r\nHost: bridge\r\n\r\n") == Result::Css);
    request.clear(); assert(feed(request, "GET /ui.js HTTP/1.1\r\nHost: bridge\r\n\r\n") == Result::Js);
    request.clear(); assert(feed(request, "GET /manage/reboot HTTP/1.1\r\n\r\n") == Result::RebootPage);
    request.clear(); assert(feed(request, "GET /manage/factory-reset HTTP/1.1\r\n\r\n") == Result::FactoryResetPage);
    for (const char* extra : {"Content-Length: 1\r\n", "Transfer-Encoding: chunked\r\n",
            "Content-Type: text/plain\r\n", "Expect: 100-continue\r\n", "Content-Length : 1\r\n"}) {
        request.clear(); assert(feed(request, post(body, extra)) == Result::Reject);
    }
    for (const char* length : {"-1", "+1", "513", "9999999999999999999999999", "1x", ""}) {
        request.clear();
        assert(feed(request, std::string("POST /configure HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ") + length + "\r\n\r\n") == Result::Reject);
    }
    request.clear();
    assert(feed(request, post(std::string(512, 'x'))) == Result::Save); // Size boundary; form decoder rejects content.
    request.clear();
    assert(feed(request, post(std::string(513, 'x'))) == Result::Reject);
    request.clear();
    assert(feed(request, "POST /configure HTTP/1.1\r\nContent-Length: 1\r\n\r\nx") == Result::Reject);
    request.clear();
    assert(feed(request, "GET / HTTP/1.1\r\nContent-Length: 1\r\n\r\nx") == Result::Reject);
    request.clear();
    assert(feed(request, "POST /configure HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 2\r\n\r\nx") == Result::Pending);
    assert(request.append(0) == Result::Reject);
    for (const char* invalid : {"ssid=x&ssid=y&password=12345678", "ssid=x&password=short",
            "ssid=&password=12345678", "ssid=%00x&password=12345678", "ssid=%0Ax&password=12345678",
            "ssid=%&password=12345678", "ssid=%Z1&password=12345678", "ssid=x", "password=12345678",
            "ssid=x&password=12345678&extra=x", "ssid=x&password=12345678&password=12345678"})
        assert(!parseProvisioningForm(invalid, c));
    assert(!parseProvisioningForm((body + "&").c_str(), c));
    assert(parseProvisioningForm("ssid=Hidden&password=", c));
    assert(!strcmp(c.password, ""));
    assert(parseProvisioningForm(("ssid=" + std::string(32, 's') + "&password=" + std::string(63, 'p')).c_str(), c));
    assert(!parseProvisioningForm(("ssid=" + std::string(33, 's') + "&password=" + std::string(63, 'p')).c_str(), c));
    char escaped[200];
    escapeHtml("<script>&\"'", escaped, sizeof(escaped));
    assert(!strcmp(escaped, "&lt;script&gt;&amp;&quot;&#39;"));
    char small[5]; escapeHtml("&&", small, sizeof(small)); assert(!*small);
    puts("web provisioning framing, decoding, validation and escaping tests passed");
}
