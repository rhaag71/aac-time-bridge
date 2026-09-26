# AAC Time Bridge

Copyright (c) 2026 Rob Haag  
Licensed under the MIT License.

Network time appliance for the Absurdly Accurate Clock project.

AAC Time Bridge runs on an ESP32 and provides network services for the
RP2350-based Absurdly Accurate Clock.

The RP2350 remains the authoritative clock. The ESP32 receives UTC and
time-quality information from the RP2350 and serves that time to the LAN
via NTP.

The ESP32 may use upstream NTP as a fallback when authoritative clock data
is unavailable. Upstream time is never used to discipline or set the
RP2350 clock.

NTP stratum and reported time quality reflect the actual active time source.

## Hardware

- NodeMCU ESP-32S / ESP-WROOM-32
- RP2350 interface using reserved GP8-GP13 pins

## Development

Built with PlatformIO using the Arduino framework.

## License

MIT