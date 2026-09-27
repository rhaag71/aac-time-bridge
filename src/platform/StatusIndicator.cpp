#include "StatusIndicator.h"
#include <Arduino.h>
namespace aac {
void StatusIndicator::begin() {
    digitalWrite(kStatusLedGpio, LOW);
    pinMode(kStatusLedGpio, OUTPUT);
    digitalWrite(kStatusLedGpio, LOW);
}
void StatusIndicator::render(const ApplianceState& state) {
    digitalWrite(kStatusLedGpio, statusLedOn(state) ? HIGH : LOW);
}
}
