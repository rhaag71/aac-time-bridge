/*
 * AAC Time Bridge
 * Copyright (c) 2026 Rob Haag
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include "Application.h"

namespace {
aac::Application application;
}

void setup()
{
    application.begin();
}

void loop()
{
    application.poll();
    delay(10); // Yield to the Arduino/RTOS runtime.
}
