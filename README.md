# AAC Time Bridge

Copyright (c) 2026 Rob Haag  
Licensed under the MIT License.

AAC Time Bridge is a network time appliance and companion project for the
[Absurdly Accurate Clock](https://github.com/rhaag71/absurdly-accurate-clock).

It runs on an ESP32 as a standalone network appliance that can consume time
from the RP2350-based Absurdly Accurate Clock.

The Pico is the known authoritative external source. The planned appliance
serves its qualified UTC to the LAN via NTP. It does not use Internet NTP or the
ESP32 system clock as a fallback. Without trustworthy external time it remains
operational and UNSYNCHRONIZED; it never manufactures authoritative time.

## Clock Interface Protocol

The RP2350 clock-source interface is defined in
[`docs/clock-network-protocol.md`](docs/clock-network-protocol.md).

The authoritative copy is maintained by the Absurdly Accurate Clock project.
The copy in this repository is kept in sync for development of AAC Time Bridge.
Protocol changes should originate in the clock-source project and require
corresponding review here.

## Hardware

- NodeMCU ESP-32S / ESP-WROOM-32
- RP2350 interface using reserved GP8-GP13 pins
- ESP32 assignments: [pin reference](Paper-Documents/aac-time-bridge-pin-assignment-rev1.pdf)
  (Pico GP numbers above are not ESP32 GPIO numbers).

## Build, upload and monitor

Built with PlatformIO using the Arduino framework.

Production build/upload:

```sh
pio run -e nodemcu-32s -t upload
```

Watchdog bench build/upload:

```sh
pio run -e watchdog-bench -t upload
```

Serial monitor:

```sh
pio device monitor -b 115200
```

Build both environments without uploading and run host tests:

```sh
pio run -e nodemcu-32s -e watchdog-bench
sh scripts/test-host.sh
python3 scripts/check-firmware.py
```

## Startup and serial service delay

After initializing Serial at 115200 baud, firmware prints AAC Time Bridge,
PRODUCTION or WATCHDOG BENCH, firmware/build date/time, reset reason/code, and
`Starting in 5 seconds (serial service delay)...`. It deliberately waits about
five seconds so a monitor can attach. It repeats the banner and prints
`Initializing...` **before** starting Wi-Fi or loading configuration.

The application watchdog is subscribed **after** this delay; the delay yields
to the RTOS and existing idle watchdogs. Once initialization finishes, an immediate
`Application ready` line shows named network state, LAN/AP addresses, clock/source
and synchronization state, and watchdog state/error. Ready means the application
is running; an asynchronous Wi-Fi connection may still be in progress. Status
continues every 60 seconds. Every reset, including a watchdog reset, repeats this
startup sequence and begins with untrusted clock state.

## Normal Wi-Fi setup over the recovery AP

1. With no stored credentials, the appliance starts an **open** setup AP named
   `AAC-Bridge-XXXXXX`, with a short device-unique suffix. No AP password is
   generated, stored or required; normal field setup needs no USB serial access.
2. Connect your phone/computer to that AP. Stay connected even if it reports
   “no Internet.”
3. Open **http://192.168.4.1/** directly; there is no captive portal. `/setup`
   remains a restricted compatibility redirect to the single appliance page.
4. Enter your target **2.4 GHz infrastructure Wi-Fi SSID** and password, then
   select **Save and connect**. Manual SSID entry supports hidden networks. Nearby
   network scanning is deferred to keep AP/STA radio handling bounded in this slice.
   Leave the password blank only for an open network. SSIDs support up to 32 bytes;
   passwords support 8–63 characters or a 64-digit hexadecimal PSK.
5. Credentials are validated and saved to the existing NVS configuration record.
   The same page shows connection feedback while the setup AP is available. It
   does not refresh while you type.
   while you type. If connection takes over 60 seconds, feedback asks you to check
   the credentials/router; retries continue. Storage errors are reported without
   replacing the active configuration.
6. Return to your normal network and browse the reported LAN IP for status.
   Once infrastructure Wi-Fi is connected **and a LAN IP is available**, the
   appliance immediately prints `LAN status: http://<address>/` over Serial and
   shuts down the setup AP. This disconnects the setup browser; the final success
   page may not arrive. Find the LAN address in Serial or your router's DHCP client
   list and reopen status from your normal network. No USB access is required if
   the router provides the address.

The setup network and target network are labeled separately. The SSID may be
shown, but stored/submitted **infrastructure passwords are never returned in
pages or printed over Serial**. The web form uses a plain visible text field for a **new password while typing**;
it is always empty on load and never pre-populated. Enter the password again when
changing settings. There is no show/hide toggle. NVS credentials are not application-encrypted.

`/` is the appliance page on LAN and AP, displaying network/source/clock state,
qualified UTC only, reset/watchdog diagnostics, build and uptime. The Wi-Fi form
appears only on the active recovery AP; `/setup` and POST `/configure` are
restricted to connections through the AP address. Management confirmations are
in-page; actions remain POST-only. No web passwords, logins, API keys or tokens are used. Requests and bodies have fixed limits
and an absolute connection timeout. HTTP has no TLS. A router already using
192.168.4.0/24 may conflict with the setup subnet; verify AP/STA access on your LAN.

## Recovery and emergency USB configuration

With stored credentials, firmware first attempts the infrastructure network.
It retries every 30 seconds; after 60 seconds disconnected it starts the setup
AP so you can repair settings at http://192.168.4.1/. Working stored
credentials do not cause an unnecessary setup AP at boot. Wi-Fi failure never
requests a reboot and does not make the clock valid. If a working LAN connection
is lost, the same 60-second recovery policy can start the AP again. Successful
reconnection shuts it down again without erasing credentials.

Developer/emergency USB commands remain available at 115200 baud:

- `provision` followed by Enter starts the open setup AP immediately when
  disconnected. Serial prints its SSID and setup URL, with no AP password. On a
  healthy LAN connection it reports that the AP stays off; use LAN management.
- `wifi <SSID><TAB><password><ENTER>` uses an **actual tab** separator. This
  fallback validates and saves through the same NVS path as web setup. The
  command and SSID echo normally. After TAB, password characters do not echo
  (not even masking characters); Backspace/Delete safely remove hidden bytes.
  Deleting the empty TAB separator returns to visible SSID editing. Stored
  passwords are never printed back. Disable terminal-side local echo so the
  terminal does not independently display secrets.

All console commands are lowercase; uppercase legacy spellings are rejected.
Ordinary typing echoes, Backspace and Delete erase the previous character, and
CR, LF or CRLF submits once. The console is byte-oriented, uses a 112-byte fixed
buffer (111 input bytes plus terminator), and processes at most 64 bytes per
application pass. Overflow or unsupported controls (including escape sequences)
discard the entire line on Enter; they cannot execute a truncated command. Re-enter
the command after a discard. No input call waits for a complete line.

## Web appliance management

Open the normal status page (`/`) on the LAN or setup AP and use **Management**.
No web authentication is required. Local network/radio access and deliberate
confirmation are the intended boundary; confirmations are not authentication.

- **Reboot** expands an in-page confirmation. Select **Confirm reboot** to submit a
  POST and restart normally. All persistent configuration, including Wi-Fi
  credentials, is preserved. **Cancel and return to status** does nothing.
- **Factory Reset** expands an in-page warning. It explicitly explains that
  all stored AAC Time Bridge configuration, including Wi-Fi credentials, will
  be erased and the appliance will reboot into setup mode. Only selecting the
  separate **Factory Reset** confirmation button submits the destructive POST.
  Merely expanding the panel or cancelling does not erase anything. Confirmation
  stays in the page and does not require a password.

Factory reset clears **all keys in the firmware-owned NVS namespace `aac-bridge`**.
Currently this contains the `wifi-v1` infrastructure SSID/password record. It does
not erase the NVS partition, other namespaces, platform storage, firmware, or
hardware identity. It is a logical configuration reset, not a forensic flash wipe.
Future persistent AAC configuration must stay in that namespace to be covered.

Both operations acknowledge the request and schedule a normal software restart
with about 2.5 seconds for the response to be delivered. No watchdog expiration
is deliberately caused. If configuration erasure cannot be confirmed, factory
reset reports an error and does not schedule a reboot; retry rather than assuming
that erasure succeeded. Configuration writes are blocked while restart is pending.

After factory reset, wait for the normal startup/service delay, connect to the
**open `AAC-Bridge-XXXXXX` AP**, and browse **http://192.168.4.1/**. The suffix
and address are unchanged. No AP password or USB access is required. Both reboot
paths start clock state untrusted/UNSYNCHRONIZED until a qualified external source
establishes time. The reset diagnostics should report a software reset, not a
watchdog failure, subject to the platform's reported reset reason.

## Emergency serial factory recovery

When network/web access is unavailable, open `pio device monitor -b 115200` and
send the exact lowercase line **`factory reset`** followed by Enter. There is no
interactive confirmation. This is an emergency/service mechanism; web Management
is the normal UI. The serial path calls the same erase-and-reboot implementation,
prints an acknowledgement without old credentials, then reboots into the open
setup AP. On storage error it prints a failure and does not schedule a reboot.
This command is available in both production and watchdog-bench firmware.

## Watchdog bench procedure

1. Upload `watchdog-bench` using the command above; open the serial monitor.
2. Verify the startup banner says **WATCHDOG BENCH** and the ready line reports
   `watchdog=armed` after the five-second service delay.
3. Send the exact line **`wedge watchdog`** followed by Enter. It deliberately
   stops application progress while yielding to the scheduler, with no watchdog feeds.
4. Expect a task-watchdog panic/reset around the 30-second timeout. Check the new
   startup banner's reset reason/code, ready line, and status page. The clock must
   remain UNSYNCHRONIZED with UTC unavailable without a qualified external source.
   The deliberate hang does not automatically repeat after reboot.
5. Reflash the production environment after testing. It contains neither the
   command nor the intentional hang function. Test without JTAG/OpenOCD, which
   can disable watchdogs.

Reporting `watchdog=armed` confirms initialization reported success; it does not
validate actual watchdog recovery.

## Implementation and validation status

Round 2 provides centralized state, a 30-second application task watchdog,
reset diagnostics, GPIO16 external status indication, persistent Wi-Fi,
reconnection/recovery AP, web setup and local status. Pico acquisition remains
an unavailable production stub. SPI/TIME_SYNC and NTP serving are not implemented.

Operator bench-verified on production NodeMCU ESP-32S: virgin boot starts the open
AP; web provisioning saves credentials and joins the LAN; Serial immediately
announces the LAN URL; normal web reboot preserves configuration; confirmed web
factory reset erases AAC-owned configuration and returns to setup; reprovisioning
works; clock stays UNSYNCHRONIZED with Pico not implemented. These observations
precede this cleanup pass. Watchdog recovery has not been bench validated.

This pass's lowercase console/editing/password echo, visible new web-password
entry, unconfigured status regression fix, and AP shutdown/recovery lifecycle
still require hardware checks. Status-page rendering now has direct host coverage
from centralized network state; the earlier reported Serial/web mismatch was not
reproduced from the inspected source, so verify both displays on the new build.

The single-page styling pass builds `nodemcu-32s` and `watchdog-bench`, passes all
host tests, and passes production/bench firmware image inspection. It has not
been flashed or browser-tested on hardware. Next, open `/` on both LAN and the
recovery AP, verify the responsive layout and actual SSID/IP, submit valid and
invalid Wi-Fi credentials, confirm the AP stops/returns at the expected times,
and exercise the in-page reboot and factory-reset confirmations on phone and
desktop browsers.

See [architecture and protocol analysis](docs/architecture.md). GPIO16 / P16 /
physical header 27 is intended for an active-high external LED through a series
resistor to GND; add it to pin-sheet Rev 2. GPIO2 is unused. Confirm the WROOM-based
38-pin board variant.

## Related Project

- [Absurdly Accurate Clock](https://github.com/rhaag71/absurdly-accurate-clock)

## License

MIT License

Copyright (c) 2026 Rob Haag

See [LICENSE](LICENSE) for the full license text.
