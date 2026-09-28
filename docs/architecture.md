# AAC Time Bridge architecture — Round 2

## Product boundary

The appliance exposes trustworthy external authoritative time to the LAN. The
Absurdly Accurate Clock (AAC), currently running on a Pico 2 / RP2350, is the
sole current source. There is no upstream Internet NTP source, fallback,
ESP32 wall-clock substitute, synthetic epoch, or implicit holdover. If no source
qualifies, selection is None, the selected anchor is cleared, and the clock is
UNSYNCHRONIZED. Wi-Fi, configuration, status and normal application execution
continue. AAC Protocol v1 SPI/TIME_SYNC acquisition is implemented; NTP serving
remains future work.

Round 1's upstream stub, source enum, network synchronization quality, fallback
policy, configuration host and unused service contracts were removed. The small
TimeSource interface remains for genuine external sources. No source registry
or speculative alternate source was added. PicoTimeSource implements the v1
source boundary; absent or malformed hardware remains an ordinary source state,
never a watchdog fault.

## Ownership and state

main.cpp only delegates setup/poll and yields. Application owns the source,
ClockCoordinator, ApplianceState, Watchdog, StatusIndicator and NetworkServices.
Acquisition remains separate from pure selectClock policy. Available, valid,
error-free, Locked AAC time needs an anchor, sensible timestamps and an
exclusive, known freshness deadline. Holdover is representable but ineligible.
Contact alone cannot extend timing validity. Source diagnostics retain expired
anchors; selected authoritative time does not. Zero is a legitimate monotonic
timestamp; unknown timestamps have explicit presence tags.

The single application task is the only state writer. ApplianceState combines
ClockState, NetworkState, reset/watchdog diagnostics, initialization and uptime.
LED and web consume it; neither acquires or selects time. currentUtc rechecks
qualification and expiry at consumption, interpolates only the authoritative
anchor, and guards signed overflow. UTC is displayed as signed Unix seconds,
not through ESP32 system time or a narrowing time_t. This is representation,
not an accuracy claim. The web page is a refreshable snapshot. No async consumer
retains state references; future concurrent tasks will require synchronized copies.

## AAC acquisition over Protocol v1

The pin assignment is GPIO23 VSPI MOSI to Pico GP8/SPI1 RX, GPIO27 software CS
to GP9/CSn, GPIO18 SCLK to GP10/SCK, GPIO19 MISO from GP11/TX, and GPIO25 input
from GP12/TIME_SYNC. Connect grounds and use 3.3 V logic only. GPIO27 is active
low and initialized HIGH before SPI setup; an external 10 kOhm pull-up to Pico
3V3 is recommended. GPIO25 is input-only with internal pulls disabled. GPIO26 is
not configured or touched; GPIO16 remains the appliance LED.

The ESP32 uses VSPI mode 1, MSB first, 8-bit words at 100 kHz, software CS and
one 40-byte `SPI.transferBytes` operation with a zero-filled MOSI buffer. The
sequence is bounded: at least 1 second before first request, at least 1 ms CS
high between requests, no more than 10 transactions per second, 100 us after CS
low before clocks, and 10 us CS hold after the final clock. No response wait or
DMA is involved; the ESP32 generates all 40 bytes of SCLK. Source initialization
happens after the appliance's intentional five-second startup delay. This
provides more than the protocol's one-second Pico startup guard when the boards
power up together or the Pico is already running. v1 has no Pico boot signal, so
independently powering the Pico after the ESP32 remains an assumption to check;
do not consider this a hardware-qualified startup detector.

TIME_SYNC rising edges are captured by a GPIO ISR using the ESP timer's
monotonic microsecond counter and a fixed eight-slot ring. The ISR only timestamps
and queues. Main-loop code waits until an edge is at least 1 ms old, then starts
the snapshot read. If another edge races the transaction, it discards that
edge/packet pairing. Overflow or multiple queued edges invalidates alignment and
requires a later clean relationship. No high-rate serial diagnostics run from
the ISR.

`PicoProtocol.cpp` decodes each byte explicitly and commits no packet fields
unless the exact 40-byte size, ACT1 magic, version/length, reserved bits and
bytes, CRC-32/ISO-HDLC, and v1 field invariants all pass. HOLDOVER and other
reserved flags are rejected. Invalid UTC must carry zero, invalid sync must
carry the FFFFFFFF delay sentinel, valid sync delay is at most 5000 us, and
satellite sentinels/range are checked. The CRC implementation passes the
123456789 check vector and the Pico repository's fixed invalid and beyond-2038
golden packets.

Authority requires UTC_VALID, PPS_PRESENT, PPS_LOCKED and SYNC_VALID plus a
locally captured unambiguous edge. The estimated UTC boundary is the captured
edge timestamp minus sync_delay_us. The first clean edge/packet pair aligns the
sequence counters. Later accepted boundaries require sync_sequence and
boundary_sequence to advance exactly one modulo 2^32 and packet_sequence to move
forward modulo 2^32 without an ambiguous half-range jump. Repeated snapshots
cannot advance the anchor. A discontinuity packet is discarded and the next
clean edge/packet relationship must re-establish alignment. Sequence counters
still have no boot identifier; the v1 edge relationship, continuity and stale
timeout are the conservative recovery mechanism. No protocol change was made.

The phase deadline expires 1.5 seconds after the last accepted TIME_SYNC edge.
Malformed packets, invalid UTC/flags, edge races/overflow, missed boundaries,
transport failures and expiry clear the selected anchor and return the clock to
UNSYNCHRONIZED. A well-formed UTC_VALID packet without adequate PPS/edge proof is
reported as valid-but-unqualified, never selected. Zero with UTC_VALID clear is
only a sentinel. No system, browser, Internet, or stale AAC time substitutes
for the lost source. Wi-Fi and the watchdog continue independently.

AAC acquisition diagnostics in centralized state include transaction/valid-packet counts,
last packet result, packet/boundary/sync sequences, flags, satellites when valid,
age of the last valid packet, age of the last associated edge, association state,
and a bounded qualification reason. The web page and `/telemetry` endpoint read
these from the same application snapshot. While unhealthy, an application-context
Serial reporter emits one bounded human-readable line on rejection/reason
transitions and at most once every thirty seconds. It formats the existing
source/clock state rather than reimplementing qualification. A rejected 40-byte response includes an eight-byte
hex and printable-ASCII preview; validated packet fields are shown as readable
flag names, sequences and satellite count. On acquisition it prints one
authority-acquired transition and suppresses recurring bring-up lines until
authority is lost. Output is copied into a fixed line buffer and queued in
small chunks no larger than the UART's current free capacity, so the ESP32's
128-byte FIFO cannot permanently defer a longer report. The reporter commits
only after the complete line is queued and does no work in the TIME_SYNC ISR.
These are observations for bring-up, not proof of electrical timing.

## Execution watchdog and resets

Startup now captures reset diagnostics separately from watchdog subscription.
After Serial.begin, it prints identity/build/reset details and a service-delay
notice, yields for five seconds, repeats the banner, and prints Initializing.
Only then does it arm/subscribe the application watchdog and initialize clock and
network services. GPIO16 is explicitly LOW throughout startup. The first completed
application pass publishes status and feeds normally; Application ready is printed
immediately afterward, with named network state and LAN/AP addresses. The periodic
interval starts at readiness. A late-attaching serial monitor can see the repeated
banner before any Wi-Fi/NVS work. This sequence still needs hardware observation.

The hardware-specific platform/Watchdog adapter uses ESP-IDF 4's supported
esp_task_wdt_init(30, true), esp_task_wdt_add(nullptr), and esp_task_wdt_reset().
The Timer Group 0 task watchdog observes the Arduino application task as well
as the runtime's existing CPU0 idle subscription. Initialization reconfigures
rather than removes existing subscriptions. The standard interrupt watchdog is
left enabled. The pinned platform uses Arduino ESP32 2.0.17 / IDF 4.4; its panic
configuration reboots. A major IDF change intentionally requires adapter review.

The sole application feed is at the end of a complete Application::poll pass,
after network, clock, indicator, HTTP and diagnostics work return. Arduino's
automatic loop watchdog feeding is not enabled. There are no timer/ISR/helper
feeds. Thus even a yielding hang inside a service stops application progress
and triggers the watchdog. Missing Pico, invalid time, lost edges, Wi-Fi outage,
storage errors, and unsynchronized status never decide whether to feed. Service
failures return normally and are diagnostic states, not reboot requests.

Thirty seconds gives generous margin for network scheduling, NVS and normal
service operations; it is not a network connection deadline. Connection attempts
are asynchronous. Serial processing is capped at 64 bytes/pass. HTTP accepts one
client, at most 128 input bytes/pass, 2047 total header bytes and 512 body bytes, with a two-second
absolute connection lifetime and nonblocking sends of at most 512 bytes/pass.
Slow, malformed, unsupported and disconnected requests are simply closed.
Only fixed-length URL-encoded setup bodies are accepted; chunked transfer,
ambiguous/duplicate framing and unsupported requests are rejected. There are no
socket receive waits, dynamic page allocations or general HTTP framework. SDK operations still require real-board soak testing;
compilation cannot establish their latency or recovery behavior.

esp_reset_reason() is captured at initialization as a label and raw code. Task,
interrupt and other watchdog reasons are distinct from power-on, software,
external, panic, brownout, deep-sleep, SDIO and unknown reasons. Classification
is only as precise as the SDK's reported cause; a generic panic is not guessed
to be a watchdog. Diagnostics also expose watchdog armed state, API error,
timeout and bench-build identity. Initialization failure is visible on serial
and web, without deliberately restarting into a loop. There is no persistent
reset counter or stored trusted UTC. Every reset reconstructs untrusted clock
and association state, irrespective of retained Wi-Fi configuration.

### Single-page appliance UI

The root path is the canonical appliance page for LAN and recovery-AP use. It
renders a centralized snapshot of authoritative clock, source, network,
diagnostics, and management controls. `/telemetry` returns the latest bounded
JSON snapshot from that same state approximately once per second. The browser
does not retain or queue updates: it issues the next request only after the
previous one completes. Each response rechecks `currentUtc()` against the
AAC-derived anchor and its freshness deadline. The browser formats the
returned authoritative UTC second without interpolating it; invalid source
state or a failed/stalled telemetry request clears the readout. Uptime and all
other dynamic status values are re-anchored by each response. The manual
Refresh status button is in the header. Local CSS and vanilla JavaScript are
served from flash; there are no Internet assets or frameworks. The recovery-AP
view includes the Wi-Fi form; it is omitted on LAN. `/setup` and `/setup/result`
are AP-restricted compatibility redirects to `/`. Setup writes remain AP-only;
management remains available on LAN and AP.

Reboot and factory reset use in-page disclosure panels and separate explicit
confirmation buttons. Both actions remain POST-only and use the existing
management handlers. The interface is intentionally unauthenticated and shows
no stored credentials. Provisioning accepts a new password in a visible text
field while typing, then clears that browser field after submission.

The page and local assets are deliberately bounded. CSS and JavaScript are
constant flash assets streamed in chunks; generated status HTML uses a fixed
8 KiB buffer and dynamic state values are HTML-escaped. The browser refresh has
a five-second request deadline; HTTP clients remain bounded by the existing
two-second server lifetime and chunked send path.

### Deliberate hardware bench test

Build/flash `pio run -e watchdog-bench -t upload`. Normal provisioning is the native AP/web flow documented in README. No credentials
starts the open AAC-Bridge-XXXXXX AP at 192.168.4.1. GET / serves the single
appliance page and its AP-only manual SSID/password form; POST /configure validates a bounded URL-encoded body before using the same NVS save path as USB.
The former provisioning token was removed: no web password, token, login, API key
or other application authentication is used.
Setup/progress/write routes require the accepted socket's local address to match
the active AP address. LAN provisioning remains disallowed; status and appliance management are available
on both LAN and AP. Submitted/stored infrastructure
stored passwords are never echoed or placed into HTML; SSIDs are escaped. Request buffers
are cleared after handling, timeout or disconnect. The form has no stored password
value. Its new-password input is intentionally type=text (visible while typing),
with no toggle and no firmware prepopulation. CSP forbids framing and restricts form actions to the same origin.

After saving, the page receives an acknowledgement and checks status while the
AP is reachable. The actual connection change is queued until the current HTTP client closes, to
avoid changing the AP/STA radio channel before sending the acknowledgement.
The main poll loop updates centralized setup feedback, named network state and
IP addresses. Joining, connected, 60-second failure and storage failure are distinct
messages. The editable form never auto-refreshes. Wi-Fi failures may mean bad
credentials, an absent router or other network problems; no false certainty about
password correctness is claimed. Failed saves retain active configuration.
Manual entry supports hidden networks; asynchronous scanning is deferred because
it adds AP/STA radio scheduling and scan-result handling to this first setup slice.

STA connects asynchronously and retries every 30 seconds. Stored credentials try
STA first; successful startup does not enable the AP. After 60 seconds disconnected,
an open recovery AP is started. USB command provision also starts it. The
short device-unique SSID and setup URL are printed when the AP starts. No recovery
AP password is generated, stored or printed. Users can connect directly to the
open AP and browse to http://192.168.4.1/ without USB serial access.
A usable infrastructure connection (association plus nonzero LAN IP) shuts down
only the AP interface via WiFi.mode(WIFI_STA). Pending AP HTTP connections close;
LAN status/management remain active. A subsequent 60-second infrastructure outage
can start the AP again. Credentials are not erased. AP shutdown failures are
reported and retried every 30 seconds, not treated as watchdog faults. The existing
provision command starts recovery early only while disconnected; it does not
keep a second radio network active on a healthy LAN connection.
AP startup failures are reported and retried every 30 seconds. No captive portal
or DNS interception is used. A target LAN overlapping 192.168.4.0/24 needs review.

The developer/emergency USB command wifi <SSID><TAB><password><ENTER> remains,
using an actual tab. Commands and SSIDs echo; the password after TAB never does.
Its schema and NVS record are unchanged. LineEditor owns a fixed 112-byte input
buffer and produces bounded echo output. Backspace/Delete remove visible bytes
with backspace-space-backspace; hidden bytes emit nothing. Removing an empty TAB
separator restores visible SSID editing. Everything after the first TAB is hidden,
even in malformed commands. CR/LF/CRLF submit once. Overflow and unsupported control
input invalidate the entire line until Enter, never executing a truncated command.
NetworkServices consumes at most 64 input bytes per pass and checks UART write
space before echoing (at most three echo bytes per input byte); there is no wait
for Enter. Error text never includes submitted input. Terminal local echo must be
disabled separately. Exact lowercase factory reset and (bench only) wedge watchdog
replace uppercase spellings without aliases.
The local HTTP page at / still consumes centralized appliance state and reports
network/AP/storage, AAC availability/validity/quality, synchronization, qualified
UTC, reset/watchdog diagnostics, firmware/build and uptime. No Internet connection
is needed. There is no TLS, OTA, remote LAN configuration, or NTP listener.

### Central network state and AP lifecycle

NetworkPolicy is the hardware-independent credential-first state producer. No
configured credentials always means Unconfigured, even if lower-level link flags
are misleading. Configured link readiness also requires a usable IP; pending
connection changes cannot masquerade as success. NetworkServices publishes the
result into ApplianceState::network before web/LED/serial consumers run. The web
formatter was extracted without a visual redesign into StatusPage.h so tests
render the actual HTML from that snapshot, not a stand-in string. Serial uses the
same networkStatusName mapping. Regression coverage verifies the production
state policy and exact HTML together.

Policy emits link gained/lost and recovery/AP decisions. The usable-link transition
immediately formats and prints LAN status: http://<IP>/ before stopping the AP;
it is independent of the periodic diagnostic interval. The previous 30-second
retry / 60-second recovery timing is retained. AP shutdown means the browser's
page may disappear before showing success; user instructions direct
them to the normal LAN and the Serial/router DHCP address. Live status uses
bounded HTTP JSON polling rather than a persistent WebSocket; no NTP or
authentication was added.

## Appliance management

GET /manage/reboot and GET /manage/factory-reset redirect to the canonical page.
GET never schedules an action. In-page disclosure panels provide a separate
explicit submit button and cancel control. POST to the corresponding path requires exactly
confirm=reboot or confirm=factory-reset. These static confirmation values describe
user intent; they are not credentials or authentication tokens. Factory Reset's
page clearly states that all stored appliance configuration, including Wi-Fi
credentials, will be erased and the device will reboot into open setup mode.
Management is available from both LAN and AP, without authentication, under the
specified local network/radio access policy. Provisioning stays AP-only.

The exact lowercase emergency serial line factory reset and the web confirmation both call
NetworkServices::requestManagement -> Management::request. The hardware-independent
Management coordinator invokes the same ResetPlatform operation, rejects duplicate
requests while pending, and schedules a normal restart after a bounded 2.5-second
acknowledgement grace period. It does not block the application loop or add watchdog
feeds. Additional configuration/management writes are rejected while a restart is
pending, so erased credentials cannot be inadvertently saved again during the grace
period. Network activity cannot postpone the restart indefinitely.

platform/ApplianceReset.cpp implements the ESP32 boundary. Factory reset opens only
the dedicated aac-bridge NVS namespace, calls Preferences::clear (namespace-scoped
erasure plus commit in the pinned Arduino implementation), closes the handle, and
returns success/failure. This erases every owned key, currently wifi-v1; it does not
use partition erasure, esp_wifi_restore, or global platform resets. Configuration
load/save share the same namespace/key constants. Future persistent application
settings must remain in this namespace. This is logical erasure, not a secure
physical overwrite of flash. There are no stored trusted clock anchors or AP secrets.

If open/erase/commit fails, report the error without rebooting or claiming success;
erasure might be incomplete, so a retry is necessary. Successful reset also clears
the in-memory configuration and stops pending connection changes before restarting.
Reboot skips storage operations entirely. Both ultimately call esp_restart, not a
watchdog trigger; esp_reset_reason should classify the result as software reset.
No persistent special reset marker is introduced. Clock state and GPIO16 startup
remain as before: untrusted state, LOW indicator, serial identity/reset diagnostics,
yielding five-second service delay, application watchdog subscription, initialization,
and immediate readiness. No stored Wi-Fi credentials after factory reset causes
the open setup AP and /setup to be available again.

Host tests cover GET/POST route separation, exact confirmations, truncated requests,
exact serial command matching, equivalent web/serial coordination, no storage call
on reboot, duplicate requests, grace timing, and erase failure. The actual hardware
adapter is also compiled against a small fake SDK to check namespace targeting,
all-owned-key erasure, preservation of unrelated fake data, and software restart
calls. Those tests validate code contracts only, not real NVS durability or reset
hardware. Bench tests must verify persistence across reboot, factory erasure through
both entry points, setup AP return, response delivery, and software reset diagnostics.

## Protocol v1 reboot / sequence review

Clock Network Protocol v1 is unchanged. Resettable sequence counters and no boot
identifier mean an ESP32 cannot *prove detection of every Pico restart* from
counter values alone. That observation is real, but is not by itself a correctness
blocker for consuming time. Sequences are association aids, not globally unique
boot identities and not permission to extrapolate old UTC indefinitely.

A correct future consumer starts unaligned, uses a locally captured edge and a
qualified immutable packet (after publication wait), and establishes alignment
from that pair. It compares subsequent modulo-2^32 increments to captured edge
counts and boundary progression, checks UTC continuity, and rejects ambiguity
across CS/next-edge boundaries. Packet sequence can advance multiple times per
second as quality changes; it must not be assumed to increment exactly once per
edge. Repeated packets/reads are not new time and never extend the edge deadline.
Missed edges, sequence discontinuity, invalid flags, a detected reboot or 1.5 s
without an edge invalidate association. Reacquisition needs a subsequent fresh
qualified edge/packet. It must not attach a pre-reset queued edge or previously
read packet to a new epoch merely because a counter value matches.

Pico reset starts TIME_SYNC low and without valid authority. In ordinary resets,
the invalid period, missing edges or restarted counters reveal loss of association.
A rapid reset with coincident counter values is not universally distinguishable;
a boot ID would improve that diagnostic. Nevertheless, a fresh, unambiguous,
qualified packet tied to a newly captured edge supplies the current authoritative
UTC and measured delay even if the reset was not separately identified. If such
association or continuity cannot be established, reject and re-align. An unseen
source loss may leave the previous anchor eligible until the existing freshness
deadline, just as an unplugged source does; a session ID would not eliminate that
observation latency. No protocol extension is needed for this round.

Optional future proposal, only if explicit session identity is required: a new
protocol version carrying a boot/session identifier with defined uniqueness and
consumer behavior. Do not repurpose v1 reserved bytes. Future consumer tests must
cover quick/slow Pico resets, coincident/reset/wrapped counters, queued old edges,
old packets, invalidation/recovery, missed edges, suppressed edges and reads that
straddle a new edge. This conclusion is design analysis, not transport validation.

## Future transport obligations and validation

Keep v1 full 40-byte validation, explicit little-endian decoding, signature,
version/length/reserved-field/CRC checks and source-specific sequence diagnostics.
Locked requires UTC_VALID, PPS_PRESENT, PPS_LOCKED, SYNC_VALID and unambiguous
edge association; GPS_VALID/satellites are informational. Estimate boundary at
captured TIME_SYNC minus sync_delay_us. The deadline is bounded by 1.5 s from the
edge and never extended by repeated reads. Follow startup/post-edge waits, mode 1,
100 kHz maximum, CS setup/hold, exact length and transaction spacing/rate. Leave
GPIO26 unwired/undriven. Obtain authoritative Pico golden vectors before decoding.

`sh scripts/test-host.sh` exercises policy, invalidation/expiry, recovery,
reset-to-untrusted stub behavior, unknown/future timestamps, quality/identity,
2038/64-bit monotonic cases, UTC interpolation/overflow, LED initialization and
expiry, credential validation, and bounded HTTP request parsing. The provisioning host suite additionally checks
POST framing/completion, body limits, duplicate/ambiguous headers, URL decoding,
field duplication, credential limits and HTML escaping.
`pio run -e nodemcu-32s -e watchdog-bench` compiles both hardware adapters/builds.
Host tests do not validate watchdog hardware, reset classification, GPIO behavior,
NVS power-fail durability, Wi-Fi reconnection or sockets on actual ESP32 hardware.

Next bench: boot with no Pico/no credentials, persist/reboot/reconfigure Wi-Fi,
wrong credentials/router outage/recovery, recovery AP, interrupted/oversized/slow
HTTP, storage failure, power/software/watchdog resets, and LED reset behavior.
Then implement AAC packet decoding and conservative edge association with host
vectors before wiring/qualifying SPI and TIME_SYNC. NTP serving should be a later
focused slice with request-time validity, uncertainty, leap/stratum/root quality
and refusal/unsynchronized behavior defined before any authoritative replies.

Round 2 validation performed: original Round 1 host suite passed before changes;
updated C++11 host suite passed with -Wall -Wextra -Werror -pedantic; final
production and bench PlatformIO builds both passed (Espressif32 7.0.1, Arduino
2.0.17). Firmware binary inspection confirmed wedge watchdog absent in production
and present in the bench image. git diff --check passed. Protocol v1 has no diff.
At the time of that implementation, no device had been flashed. PlatformIO reported an
existing multiple-Core-version advisory; it did not prevent either build.

Single-page UI pass: strict host tests passed, both PlatformIO environments built,
firmware inspection confirmed the UI assets/POST forms and bench-only command
separation, `node --check` accepted the embedded JavaScript, and `git diff --check`
passed. CSS/JS assets occupy about 9.4 KiB of flash constants; generated page HTML
uses an 8 KiB fixed RAM buffer. This UI pass has not been flashed or browser-tested
on hardware. Check LAN/AP rendering, phone layout, the manual Wi-Fi form, and the
in-page management actions on real browsers before treating the presentation as
bench-verified.


First hardware update (operator-reported): NodeMCU-32S boots and remains running,
periodic serial status reports unsynchronized / AAC not implemented / network=0 /
watchdog armed. This establishes first boot behavior only, not watchdog recovery,
Wi-Fi/AP/NVS/web or LED validation. The subsequent startup delay and web provisioning
changes require bench validation: observe both banners and immediate readiness;
exercise fresh setup, successful saved boot without AP, invalid credentials,
router outage, repair, power-cycle persistence, slow/aborted POSTs and rejected LAN
writes. Then deliberately invoke wedge watchdog in the bench build and verify the
reported reset cause and untrusted clock after reboot.

Startup/web follow-up validation: both production and watchdog-bench builds passed;
both host suites passed with strict compiler warnings; the provisioning parser
suite passed AddressSanitizer/UndefinedBehaviorSanitizer (including leak checking,
run outside sandbox tracing because LeakSanitizer cannot run under ptrace).
Binary inspection again confirmed the deliberate watchdog command absent from
production and present only in the bench build. Protocol v1 remains unchanged.
No additional hardware validation was performed by this implementation task.

Management follow-up validation: production and watchdog-bench PlatformIO builds
passed; all four host suites passed with strict compiler warnings; git diff --check
passed; Protocol v1 has no diff. Firmware binary and ELF-symbol inspection confirmed
factory reset available in both builds and the wedge watchdog command/function
absent from production and present only in watchdog-bench. Source review confirmed
one namespace-scoped erasure adapter, no storage operation in normal restart, and
no AP password or application authentication. No device was flashed or bench-tested
as part of this change.


Latest operator bench feedback (before this cleanup): open AP on virgin boot,
web provisioning and immediate Serial LAN URL, web reboot preserving credentials,
confirmed web factory reset returning to setup, successful reprovisioning, and
UNSYNCHRONIZED/not-implemented clock state are verified on NodeMCU ESP-32S. This does
not validate the new console editor, lowercase commands, visible web input, AP
shutdown/recovery changes, or actual watchdog recovery. Those need another bench
pass. Use scripts/check-firmware.py after both builds to inspect command strings,
visible password input type, and the presence/absence of the deliberate watchdog
hang symbol in the bench/production ELF images.

Cleanup software validation: both PlatformIO environments passed; six host suites
passed, with console tests additionally compiled/run in watchdog-bench mode. Tests
cover lowercase-only commands, all Enter variants, editing/overflow/control input,
password suppression/error privacy, the production HTML's unconfigured status,
AP stop/recovery decisions and immediate LAN URL, plus existing reset/namespace
contracts. scripts/check-firmware.py passed binary and ELF isolation checks for
both environments. git diff --check passed and Protocol v1 has no diff. These are
software results only; no hardware was flashed or newly bench-validated in this pass.

Pico acquisition milestone software validation: the v1 decoder/qualifier host
suite consumes the Pico repository's unchanged `network-v1-vectors.json`, verifies
the CRC check vector, structural/semantic rejection, edge association, continuity,
reboot-like sequence restart, timeout and unsynchronized recovery. Production and
watchdog-bench builds include the hardware adapter; firmware inspection verifies
the acquisition symbols and existing bench-only hang separation. `docs/clock-
network-protocol.md` compares byte-for-byte equal to the Pico project's copy and
was not modified. No connected-Pico hardware test has been performed. Follow the
first-bench procedure in README before treating SPI mode, CS timing, edge phase,
loss or requalification as validated.
