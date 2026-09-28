#include "Watchdog.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#if ESP_IDF_VERSION_MAJOR != 4
#error Review watchdog API and Arduino automatic loop feeding before changing SDK major version
#endif
namespace aac {
namespace {
const char* resetName(esp_reset_reason_t reason) {
    switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "other-watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
    }
}
}
void Watchdog::captureReset(Diagnostics& d) {
    const auto reason = esp_reset_reason();
    d.resetReason = resetName(reason);
    d.resetCode = static_cast<int>(reason);
    d.watchdogReset = reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT || reason == ESP_RST_WDT;
#ifdef AAC_WATCHDOG_BENCH
    d.benchBuild = true;
    d.buildFlavor = "WATCHDOG BENCH";
#elif defined(AAC_DIAGNOSTIC_BUILD)
    // Diagnostic-only PlatformIO environments should define AAC_DIAGNOSTIC_BUILD.
    d.buildFlavor = "DIAGNOSTIC";
#else
    d.buildFlavor = "PRODUCTION";
#endif
}
void Watchdog::begin(Diagnostics& d) {
    // Reconfigure existing IDF TWDT, preserving idle-task subscriptions.
    // Do not enable Arduino's automatic loop feeding.
    esp_err_t result = esp_task_wdt_init(d.watchdogTimeoutSeconds, true);
    if (result == ESP_OK) result = esp_task_wdt_add(nullptr);
    d.watchdogError = result;
    d.watchdogArmed = result == ESP_OK;
}
void Watchdog::completedPass(Diagnostics& d) {
    if (d.watchdogArmed) {
        d.watchdogError = esp_task_wdt_reset();
        if (d.watchdogError != ESP_OK) d.watchdogArmed = false;
    }
}
#ifdef AAC_WATCHDOG_BENCH
[[noreturn]] void Watchdog::wedgeForBench() {
    // Yielding proves the subscribed application task must make progress even
    // while the scheduler, Wi-Fi and idle tasks are healthy. No automatic trigger.
    for (;;) delay(100);
}
#endif
}
