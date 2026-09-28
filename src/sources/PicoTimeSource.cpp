// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "PicoTimeSource.h"
#include <Arduino.h>
#include <SPI.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <string.h>

namespace aac {
namespace {
constexpr int kPicoMosi = 23;
constexpr int kPicoCs = 27;
constexpr int kPicoSclk = 18;
constexpr int kPicoMiso = 19;
constexpr int kTimeSync = 25;
constexpr uint8_t kEdgeRingSize = 8;
constexpr MonotonicUs kFirstRequestDelayUs = 1000000ULL;
constexpr MonotonicUs kTransactionPeriodUs = 100000ULL; // Exactly at most 10 per second.
constexpr MonotonicUs kCsHighMinimumUs = 1000ULL;
constexpr MonotonicUs kEdgeSettleUs = 1000ULL;

portMUX_TYPE edgeMux = portMUX_INITIALIZER_UNLOCKED;
volatile MonotonicUs edgeTimes[kEdgeRingSize] = {};
volatile uint8_t edgeHead = 0;
volatile uint8_t edgeTail = 0;
volatile bool edgeOverrun = false;
volatile uint32_t edgeTotal = 0;

void IRAM_ATTR timeSyncRiseIsr() {
    const MonotonicUs capturedAt = static_cast<MonotonicUs>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&edgeMux);
    ++edgeTotal;
    const uint8_t next = static_cast<uint8_t>((edgeHead + 1u) % kEdgeRingSize);
    if (next == edgeTail) edgeOverrun = true;
    else {
        edgeTimes[edgeHead] = capturedAt;
        edgeHead = next;
    }
    portEXIT_CRITICAL_ISR(&edgeMux);
}

struct EdgeBatch {
    bool available = false;
    bool overrun = false;
    bool multiple = false;
    MonotonicUs latestAtUs = 0;
    uint32_t total = 0;
};

EdgeBatch takeReadyEdges(MonotonicUs now) {
    EdgeBatch batch;
    portENTER_CRITICAL(&edgeMux);
    uint8_t count = static_cast<uint8_t>((edgeHead + kEdgeRingSize - edgeTail) % kEdgeRingSize);
    batch.overrun = edgeOverrun;
    batch.total = edgeTotal;
    if (count) batch.latestAtUs = edgeTimes[(edgeHead + kEdgeRingSize - 1u) % kEdgeRingSize];
    if (count && now >= batch.latestAtUs && now - batch.latestAtUs >= kEdgeSettleUs) {
        batch.available = true;
        batch.multiple = count > 1;
        edgeTail = edgeHead; // Select only the latest complete boundary observation.
        edgeOverrun = false;
    }
    portEXIT_CRITICAL(&edgeMux);
    return batch;
}

uint32_t currentEdgeTotal() {
    portENTER_CRITICAL(&edgeMux);
    const uint32_t total = edgeTotal;
    portEXIT_CRITICAL(&edgeMux);
    return total;
}
}

void PicoTimeSource::begin(MonotonicUs now) {
    qualification_.begin(now);
    memset(tx_, 0, sizeof(tx_));
    memset(rx_, 0, sizeof(rx_));

    // Establish inactive CS before configuring the VSPI controller. GPIO26 is
    // intentionally untouched; it remains reserved and high impedance.
    digitalWrite(kPicoCs, HIGH);
    pinMode(kPicoCs, OUTPUT);
    pinMode(kTimeSync, INPUT); // No internal pull: TIME_SYNC is driven by Pico.
    portENTER_CRITICAL(&edgeMux);
    edgeHead = edgeTail = 0;
    edgeOverrun = false;
    edgeTotal = 0;
    portEXIT_CRITICAL(&edgeMux);
    attachInterrupt(digitalPinToInterrupt(kTimeSync), timeSyncRiseIsr, RISING);
    SPI.begin(kPicoSclk, kPicoMiso, kPicoMosi, -1); // Software CS on GPIO27.

    startedAtUs_ = now;
    nextTransactionAtUs_ = now + kFirstRequestDelayUs;
    lastTransactionAtUs_ = 0;
    lastCsHighAtUs_ = now;
}

void PicoTimeSource::poll(MonotonicUs now) {
    qualification_.poll(now);
    const uint32_t total = currentEdgeTotal();
    qualification_.recordCapturedEdges(total);
    if (now < startedAtUs_ || now - startedAtUs_ < kFirstRequestDelayUs) return;
    if (now < nextTransactionAtUs_ || now < lastCsHighAtUs_ || now - lastCsHighAtUs_ < kCsHighMinimumUs) return;
    if (lastTransactionAtUs_ && (now < lastTransactionAtUs_ || now - lastTransactionAtUs_ < kTransactionPeriodUs)) return;

    EdgeBatch batch = takeReadyEdges(now);
    qualification_.recordCapturedEdges(batch.total);
    const bool missedLocalEdge = batch.overrun || batch.multiple;
    if (missedLocalEdge) qualification_.edgeOverflow(now);

    SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE1));
    digitalWrite(kPicoCs, LOW);
    delayMicroseconds(100); // Pico CS IRQ snapshot/reset/preload setup.
    const uint32_t edgeTotalAtStart = currentEdgeTotal();
    SPI.transferBytes(tx_, rx_, static_cast<uint32_t>(sizeof(rx_))); // Exactly 40 clocks, MOSI zeros.
    delayMicroseconds(10); // CS hold after the final trailing SCK.
    digitalWrite(kPicoCs, HIGH);
    SPI.endTransaction();
    const MonotonicUs finishedAt = static_cast<MonotonicUs>(esp_timer_get_time());
    lastTransactionAtUs_ = now;
    lastCsHighAtUs_ = finishedAt;
    nextTransactionAtUs_ = now + kTransactionPeriodUs;

    const bool edgeDidNotRace = batch.total == edgeTotalAtStart && edgeTotalAtStart == currentEdgeTotal();
    const bool associate = batch.available && !missedLocalEdge && edgeDidNotRace &&
                           finishedAt >= batch.latestAtUs && finishedAt - batch.latestAtUs < kPicoEdgeTimeoutUs;
    if (!edgeDidNotRace) qualification_.edgeOverflow(finishedAt);
    qualification_.observePacket(rx_, sizeof(rx_), finishedAt, associate, batch.latestAtUs);
}
} // namespace aac
