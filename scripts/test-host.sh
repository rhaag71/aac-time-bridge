#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/aac-clock-test.XXXXXX)
trap 'rm -f "$binary"' EXIT HUP INT TERM
for source in test/host/*_test.cpp; do
    if [ "$source" = test/host/reset_adapter_test.cpp ]; then
        "${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
            -Itest/host/fakes -Isrc src/clock/ClockState.cpp src/sources/PicoProtocol.cpp src/sources/PicoSerialDiagnostics.cpp src/platform/ApplianceReset.cpp "$source" -o "$binary"
    else
        "${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
            -Isrc src/clock/ClockState.cpp src/sources/PicoProtocol.cpp src/sources/PicoSerialDiagnostics.cpp "$source" -o "$binary"
    fi
    if [ "$source" = test/host/pico_protocol_test.cpp ] && [ -f ../absurdly-accurate-clock/test/host/network-v1-vectors.json ]; then
        "$binary" ../absurdly-accurate-clock/test/host/network-v1-vectors.json
    else
        "$binary"
    fi
    if [ "$source" = test/host/console_test.cpp ]; then
        "${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
            -DAAC_WATCHDOG_BENCH=1 -Isrc "$source" -o "$binary"
        "$binary"
    fi
done
