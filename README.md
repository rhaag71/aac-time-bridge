# AAC Time Bridge

Copyright (c) 2026 Rob Haag  
Licensed under the MIT License.

AAC Time Bridge is a network time appliance and companion project for the
[Absurdly Accurate Clock](https://github.com/rhaag71/absurdly-accurate-clock).

It runs on an ESP32 and provides network time services for the RP2350-based
Absurdly Accurate Clock.

The RP2350 remains the authoritative clock. The ESP32 receives UTC and
time-quality information from the RP2350 and serves that time to the LAN
via NTP.

The ESP32 may use upstream NTP as a fallback when authoritative clock data
from the RP2350 is unavailable. Upstream time is never used to discipline
or set the RP2350 clock.

NTP stratum and reported time quality reflect the actual active time source,
including GPS/PPS-disciplined time, future holdover support, upstream NTP
fallback, or an unsynchronized state.

## Hardware

- NodeMCU ESP-32S / ESP-WROOM-32
- RP2350 interface using reserved GP8-GP13 pins

## Development

Built with PlatformIO using the Arduino framework.

## Related Project

- [Absurdly Accurate Clock](https://github.com/rhaag71/absurdly-accurate-clock)

## License

MIT License

Copyright (c) 2026 Rob Haag

See [LICENSE](LICENSE) for the full license text.