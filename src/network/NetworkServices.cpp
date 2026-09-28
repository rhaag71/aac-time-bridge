#include "NetworkServices.h"
#include "Provisioning.h"
#include "status/StatusPage.h"
#include <WiFi.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <lwip/sockets.h>
#include <errno.h>
namespace aac {
void NetworkServices::begin() {
    Preferences prefs;
    storageOk_ = prefs.begin(kConfigurationNamespace, false);
    if (storageOk_) {
        const size_t length = prefs.getBytesLength(kWifiConfigurationKey);
        if (length) {
            configured_ = length == sizeof(config_) &&
                prefs.getBytes(kWifiConfigurationKey, &config_, sizeof(config_)) == sizeof(config_) && validConfiguration(config_);
            storageOk_ = configured_;
        }
        prefs.end();
    }
    WiFi.persistent(false); // Application NVS record is the sole credential store.
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("aac-time-bridge");
    const auto now = static_cast<MonotonicUs>(esp_timer_get_time());
    networkPolicy_.begin(now);
    if (configured_) {
        setupMessage_ = "Connecting to stored Wi-Fi. This may take up to 60 seconds.";
        startConnection(now);
    }
    else startAp();
    web_.begin();
    Serial.println("Normal setup: connect to the recovery AP, then open http://192.168.4.1/");
    Serial.println("Emergency USB config: wifi <SSID><TAB><password><ENTER>; blank password supports open networks.");
    Serial.println("USB command: provision (start setup/recovery AP). Commands/SSID echo; password after TAB is hidden. Disable terminal local echo.");
    Serial.println("Emergency factory recovery: exact command factory reset erases appliance configuration and reboots.");
#ifdef AAC_WATCHDOG_BENCH
    Serial.println("BENCH BUILD: exact USB command wedge watchdog deliberately hangs application.");
#endif
}
void NetworkServices::startConnection(MonotonicUs now) {
    WiFi.disconnect();
    startingConnection_ = true; // Wait for asynchronous disconnect acknowledgement.
    lastAttempt_ = now;
}
void NetworkServices::startAp() {
    if (apActive_) return;
    snprintf(apName_, sizeof(apName_), "AAC-Bridge-%06llX", static_cast<unsigned long long>(ESP.getEfuseMac() & 0xffffff));
    WiFi.mode(WIFI_AP_STA);
    const IPAddress address(192, 168, 4, 1), mask(255, 255, 255, 0);
    apActive_ = WiFi.softAPConfig(address, address, mask) && WiFi.softAP(apName_);
    apError_ = !apActive_;
    if (apActive_) Serial.printf("Setup/recovery AP (open): %s URL: http://%s/\n", apName_, WiFi.softAPIP().toString().c_str());
    else Serial.println("Recovery AP start failed; retry later.");
}
void NetworkServices::stopAp(MonotonicUs now) {
    if (apStopFailed_ && now - lastApStopAttempt_ < 30000000ULL) return;
    lastApStopAttempt_ = now;
    if (client_ && client_.localIP() == WiFi.softAPIP()) finishClient();
    // Keep STA and its IP/credentials; disable only the AP interface.
    const bool stopped = WiFi.mode(WIFI_STA);
    apStopFailed_ = !stopped;
    apError_ = !stopped;
    if (stopped) {
        apActive_ = false;
        Serial.println("Setup/recovery AP stopped; use the LAN status URL.");
    } else Serial.println("Setup/recovery AP stop failed; will retry.");
}
void NetworkServices::command(const char* line, MonotonicUs now) {
    const auto kind = consoleCommand(line);
    if (kind == ConsoleCommand::FactoryReset)
        requestManagement(ManagementAction::FactoryReset, now);
    else if (kind == ConsoleCommand::Provision) {
        if (networkPolicy_.connected()) Serial.println("Infrastructure Wi-Fi is connected; use the LAN status URL. Setup AP stays off.");
        else startAp();
    }
#ifdef AAC_WATCHDOG_BENCH
    else if (kind == ConsoleCommand::WedgeWatchdog) benchRequest_ = true;
#endif
    else {
        Configuration candidate;
        if (kind != ConsoleCommand::Wifi || !parseConfiguration(line, candidate)) Serial.println(kInvalidConsoleCommand);
        else {
            if (saveConfiguration(candidate, now)) Serial.println("Configuration saved; connecting.");
            else Serial.println("Configuration storage failed; active configuration unchanged.");
            memset(&candidate, 0, sizeof(candidate));
        }
    }
}
bool NetworkServices::saveConfiguration(const Configuration& candidate, MonotonicUs now) {
    if (management_.pending() || !validConfiguration(candidate)) return false;
    Preferences prefs;
    storageOk_ = prefs.begin(kConfigurationNamespace, false);
    if (storageOk_) {
        storageOk_ = prefs.putBytes(kWifiConfigurationKey, &candidate, sizeof(candidate)) == sizeof(candidate);
        prefs.end();
    }
    if (!storageOk_) {
        setupMessage_ = "Could not save credentials. Existing configuration is unchanged. Please retry.";
        return false;
    }
    config_ = candidate;
    configured_ = true;
    networkPolicy_.begin(now);
    failureReported_ = false;
    pendingConnection_ = true; // Send HTTP acknowledgement before changing radio channel.
    setupMessage_ = "Credentials saved. Joining target Wi-Fi; allow up to 60 seconds.";
    return true;
}
void NetworkServices::poll(MonotonicUs now, NetworkState& state) {
    management_.poll(now);
    if (management_.pending()) return; // Never re-save erased credentials during reset grace.
    // At most 64 input bytes and 192 echo bytes/pass; no waiting for Enter.
    for (unsigned i = 0; i < 64 && Serial.available() && Serial.availableForWrite() >= 3; ++i) {
        const auto event = console_.accept(static_cast<unsigned char>(Serial.read()));
        Serial.print(console_.echo());
        if (event != LineEditor::Event::None) {
            if (event == LineEditor::Event::Submit) command(console_.line(), now);
            else Serial.println(kInvalidConsoleInput);
            console_.clearLine();
            if (management_.pending()) return;
        }
    }
    if (pendingConnection_ && !client_) {
        pendingConnection_ = false;
        networkPolicy_.begin(now);
        startConnection(now);
    }
    if (startingConnection_ && WiFi.status() != WL_CONNECTED) {
        startingConnection_ = false;
        WiFi.begin(config_.ssid, config_.password);
    }
    const bool linkReady = !pendingConnection_ && !startingConnection_ &&
        WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP()) != 0;
    auto observation = networkPolicy_.observe(configured_, linkReady, apActive_, lastAttempt_, now);
    if (observation.lost) {
        failureReported_ = false;
        setupMessage_ = "Wi-Fi connection lost. Retrying; setup AP becomes available after 60 seconds.";
    }
    if (observation.announceLan) {
        setupMessage_ = "Connected to target Wi-Fi. Return to your normal network; setup AP is shutting down.";
        char announcement[64];
        formatLanAnnouncement(announcement, sizeof(announcement), WiFi.localIP().toString().c_str());
        Serial.println(announcement); // Immediate usable-link transition, not periodic logging.
    }
    if (observation.stopAp) stopAp(now);
    if (configured_ && observation.recoveryDue && !pendingConnection_ && !failureReported_) {
        failureReported_ = true;
        setupMessage_ = "Unable to join within 60 seconds. Check SSID/password and router availability. Retries continue; you can repair credentials here.";
    }
    if (configured_ && !observation.connected && !pendingConnection_ && now - lastAttempt_ >= 30000000ULL) {
        startConnection(now);
        observation.status = NetworkStatus::Connecting;
    }
    if (observation.startAp && now - lastApAttempt_ >= 30000000ULL) {
        lastApAttempt_ = now;
        startAp();
    }
    state.status = observation.status;
    state.setupMessage = setupMessage_;
    state.provisioning = apActive_;
    state.storageOk = storageOk_;
    state.apError = apError_;
    snprintf(state.address, sizeof(state.address), "%s", observation.connected ? WiFi.localIP().toString().c_str() : "unavailable");
    snprintf(state.apAddress, sizeof(state.apAddress), "%s", apActive_ ? WiFi.softAPIP().toString().c_str() : "unavailable");
    snprintf(state.apName, sizeof(state.apName), "%s", apActive_ ? apName_ : "");
}
void NetworkServices::serve(const ApplianceState& state) {
    const auto now = static_cast<MonotonicUs>(esp_timer_get_time());
    if (!client_) {
        request_.clear(); // Also discard partial bodies when a peer disconnects.
        client_ = web_.available();
        if (!client_) return;
        clientSince_ = now;
        responseSize_ = sent_ = 0;
        staticResponse_ = nullptr;
    }
    // Absolute lifetime, not an inactivity timer: slow clients cannot wedge us.
    if (now - clientSince_ >= 2000000ULL) { finishClient(); return; }
    if (!responseSize_) {
        for (unsigned i = 0; i < 128 && client_.available(); ++i) {
            const auto result = request_.append(static_cast<char>(client_.read()));
            if (result == HttpRequest::Result::Pending) continue;
            if (management_.pending()) reply("503 Service Unavailable", "Restart already scheduled. Please wait.");
            else if (result == HttpRequest::Result::Reject) reply("400 Bad Request", "Invalid or unsupported request.");
            else if (result == HttpRequest::Result::Status) statusPage(state);
            else if (result == HttpRequest::Result::Telemetry)
                responseSize_ = renderTelemetryResponse(response_, sizeof(response_), state, now);
            else if (result == HttpRequest::Result::Css) { staticResponse_ = kUiCss; responseSize_ = sizeof(kUiCss) - 1; }
            else if (result == HttpRequest::Result::Js) { staticResponse_ = kUiJs; responseSize_ = sizeof(kUiJs) - 1; }
            else if (result == HttpRequest::Result::RebootPage || result == HttpRequest::Result::FactoryResetPage) {
                snprintf(response_, sizeof(response_), "HTTP/1.1 303 See Other\r\nLocation: /\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Length: 0\r\n\r\n");
                responseSize_ = strlen(response_);
            }
            else if (result == HttpRequest::Result::Reboot || result == HttpRequest::Result::FactoryReset) {
                const auto action = result == HttpRequest::Result::Reboot ? ManagementAction::Reboot : ManagementAction::FactoryReset;
                if (!confirmedManagementAction(action, request_.body()))
                    reply("400 Bad Request", "Explicit confirmation required. Return to the appliance page and open the confirmation panel.");
                else {
                    const auto outcome = requestManagement(action, now);
                    if (outcome == ManagementResult::Scheduled)
                        reply("200 OK", action == ManagementAction::FactoryReset ?
                            "AAC Time Bridge configuration erased. Rebooting into setup mode. Connect to the open AAC-Bridge-XXXXXX network and visit http://192.168.4.1/setup after startup." :
                            "Rebooting AAC Time Bridge. Configuration and Wi-Fi credentials are unchanged. Reopen status after startup.");
                    else if (outcome == ManagementResult::StorageError)
                        reply("500 Internal Server Error", "Factory reset could not confirm configuration erasure. No reboot scheduled. Retry factory reset; do not assume configuration was erased.");
                    else reply("503 Service Unavailable", "Restart already scheduled. Please wait.");
                }
            }
            else if (!apActive_ || client_.localIP() != WiFi.softAPIP())
                reply("403 Forbidden", "Connect to the AAC Time Bridge setup AP to configure Wi-Fi.");
            else if (result == HttpRequest::Result::Setup || result == HttpRequest::Result::Progress) {
                snprintf(response_, sizeof(response_), "HTTP/1.1 303 See Other\r\nLocation: /\r\nConnection: close\r\nCache-Control: no-store\r\nContent-Length: 0\r\n\r\n");
                responseSize_ = strlen(response_);
            }
            else if (result == HttpRequest::Result::Save) {
                Configuration candidate;
                if (!parseProvisioningForm(request_.body(), candidate))
                    reply("400 Bad Request", "Invalid form. Return to the appliance page and retry. SSID: 1-32 bytes; password: empty, 8-63 characters, or 64 hex digits.");
                else {
                    if (saveConfiguration(candidate, now)) reply("200 OK", "Credentials saved. Joining target Wi-Fi; allow up to 60 seconds.");
                    else reply("500 Internal Server Error", setupMessage_);
                }
                memset(&candidate, 0, sizeof(candidate));
            }
            request_.clear(); // Submitted credentials do not remain in the request buffer.
            break;
        }
    }
    if (responseSize_) {
        const size_t remaining = responseSize_ - sent_;
        const char* response = staticResponse_ ? staticResponse_ : response_;
        const int n = ::send(client_.fd(), response + sent_, remaining > 512 ? 512 : remaining, MSG_DONTWAIT);
        if (n > 0) sent_ += static_cast<size_t>(n);
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { finishClient(); return; }
        if (sent_ == responseSize_) finishClient();
    }
}

ManagementResult NetworkServices::requestManagement(ManagementAction action, MonotonicUs now) {
    const auto result = management_.request(action, now);
    if (result == ManagementResult::Scheduled) {
        pendingConnection_ = startingConnection_ = false;
        if (action == ManagementAction::FactoryReset) {
            config_ = Configuration{};
            configured_ = false;
            Serial.println("factory reset: AAC Time Bridge configuration erased. Rebooting into open setup mode.");
        } else Serial.println("Reboot requested. Persistent configuration unchanged. Restarting.");
    } else if (result == ManagementResult::StorageError) {
        storageOk_ = false;
        Serial.println("factory reset FAILED: configuration erasure not confirmed; no reboot scheduled. Retry factory reset.");
    }
    return result;
}
void NetworkServices::finishClient() {
    client_.stop();
    request_.clear();
}
void NetworkServices::reply(const char* status, const char* message) {
    snprintf(response_, sizeof(response_), "HTTP/1.1 %s\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n%s", status, message);
    responseSize_ = strlen(response_);
}
bool NetworkServices::takeBenchRequest() {
    const bool result = benchRequest_; benchRequest_ = false; return result;
}
void NetworkServices::statusPage(const ApplianceState& s) {
    const bool setupAccess = apActive_ && client_.localIP() == WiFi.softAPIP();
    responseSize_ = renderStatusPage(response_, sizeof(response_), s, static_cast<MonotonicUs>(esp_timer_get_time()), setupAccess);
}
}
