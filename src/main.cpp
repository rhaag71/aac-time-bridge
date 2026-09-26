/*
 * AAC Time Bridge
 * Copyright (c) 2026 Rob Haag
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("AAC Time Bridge");
    Serial.println("NodeMCU ESP-32S");
}

void loop()
{
    delay(1000);
}