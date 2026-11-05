# Setup and Validation Guide

Practical setup notes for **AMOLED Gateway**, targeting the Waveshare ESP32-S3-Touch-AMOLED-1.8. For the GitHub overview, architecture and examples, see [README.md](README.md).

Updated: **2026-11-04**. This guide describes the current source files. Compilation, flashing and physical testing have not been performed for the Waveshare adaptation.

## 1. Select the hardware revision

Open [BoardConfig.h](BoardConfig.h) and match `AMOLED_BOARD_REVISION` to the label on the back of the board.

| Value | Display | Touch | Touch I2C address |
| --- | --- | --- | --- |
| `1` | SH8601 | FT3168 | `0x38` |
| `2` — default for the confirmed board | CO5300 | CST820 | `0x15` |

There is no automatic revision detection. Both code paths require validation on the matching hardware.

## 2. Install libraries and configure the IDE

Use the appropriate **Arduino_GFX** library set from the official Waveshare example package:

- Original board: `examples/arduino/libraries`.
- V2 board: `examples/arduino-v2/libraries`.

Required libraries:

| Library | Used for |
| --- | --- |
| GFX_Library_for_Arduino / Arduino_GFX | QSPI display drivers and canvas |
| BoardIoExpander.h (included) | Local TCA9554 I/O expander driver; no separate installation |
| SensorLib 0.5.1 | Modern ImuDrv.hpp API for the QMI8658 accelerometer |

Use the bundled GFX library to match the display classes and constructors. Arduino-ESP32 supplies WiFi, WebServer, Preferences, ArduinoOTA, SD_MMC and FreeRTOS. M5Unified, M5GFX, SdFat and LVGL are not required.

Open [ESP32S3.ino](ESP32S3.ino), keeping all source files in the same sketch folder. The inherited configuration targets Arduino-ESP32 **3.3.12** and its WebServer middleware API. This is a configuration target, not a verified build matrix.

| IDE option | Setting |
| --- | --- |
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16 MB layout with two application partitions and OTA support |
| USB CDC On Boot | Enabled |
| USB Mode | Hardware CDC and JTAG |
| Arduino Runs On | Core 1 |
| Events Run On | Core 0 |
| Serial Monitor | 115200 baud; newline line ending |

Do not select a single application slot without OTA support. The framebuffer requires **329,728 bytes in PSRAM**. If allocation fails, rendering is disabled and a serial diagnostic is emitted; the network services can continue.


### SensorLib version

Use **SensorLib 0.5.1**, already found in the current Arduino installation. The project uses the modern **ImuDrv.hpp** API through [BoardImu.h](BoardImu.h). Do not install the previously supplied 0.3.3 archive for this revision, and do not include the deprecated root SensorQMI8658.hpp alongside the modern driver. Use the Waveshare package for the matching GFX library, not to replace SensorLib with its older bundled copy.

## 3. Verify the board mapping

| Function | Configuration |
| --- | --- |
| AMOLED QSPI | CS 12, CLK 11, D0 4, D1 5, D2 6, D3 7 |
| Shared I2C | SDA 15, SCL 14 |
| Touch interrupt pin | GPIO21; current touch implementation polls registers |
| SDMMC, 1 bit | CLK 2, CMD 1, DATA0 3 |
| I/O expander | I2C `0x20` |
| Expander outputs 0/1/2 | Display/touch initialization sequence |
| Expander output 7 | SD DAT3/CS held high for SDMMC operation |
| BOOT button | GPIO0 |

PWR is configured through BoardPower and the AXP2101: short press for orderly shutdown, approximately 1 second to power on, and a 6-second hardware emergency hold. GPIO41 and the AtomS3R LP5562 RGB controller are not used.

## 4. Check startup and connectivity

1. Compile and flash over USB.
2. Open the serial monitor and inspect the `[BOARD]` display/IMU result, `[SD]` mount result and task core messages.
3. Join **AMOLED_CONFIG** and open [the Wi-Fi setup page](http://192.168.4.1/wifi).
4. Save up to five Wi-Fi profiles. Once connected, use the LAN IP shown on the display.
5. Send `help` over Serial or UDP port `4210`. Existing command tokens and firmware responses retain their original language.

The HTTP portal runs on port 80. Routes include `/info`, `/wifi`, `/logs`, `/prometheus` and `/prometheus.yml`. Metrics use port 9120 and `/metrics` by default. The device and OTA hostname is **AMOLED-Gateway**.

## 5. Validate display and input

| Test | Expected source behavior |
| --- | --- |
| Swipe up / swipe left | Advance one distinct page |
| Short tap | Wake / keep the current page |
| Swipe down / swipe right | Select the previous distinct page |
| Hold touch for at least 800 ms and release | Open Personalize from a clock; open the customized clock elsewhere |
| First touch after screensaver / zero brightness | Wake without page navigation |
| Short BOOT press and release | Advance one page |
| Hold BOOT for 3 seconds and release | Restart |
| Hold BOOT for 10 seconds and release | Clear saved Wi-Fi profiles and restart |

BOOT held during power-on enters download mode. Exercise the destructive Wi-Fi-reset action only when intended.

Direct page selection preserves the original IDs:

| ID | Page |
| --- | --- |
| 0 | Clock |
| 1 / 2 | NTP aliases |
| 3 / 4 | Network aliases |
| 5 | System |
| 6 | Saved Wi-Fi profiles |
| 7 | SD storage |
| 8 | Network monitor |
| 9 | Weather forecast |
| 10 | Persistent analog clock |

Sequential navigation skips duplicate aliases. Test `rotacao:0`, `rotacao:90`, `rotacao:180` and `rotacao:270`, checking both rendering and touch directions. Rotation defaults to 0° and is not saved between boots.

The customized clock and Personalize do not auto-return; other non-clock pages return to the clock after 20 seconds without navigation. The screensaver starts after 60 seconds of inactivity; after 120 seconds, brightness is reduced to at most 20% of full scale, without exceeding the configured level. Commands temporarily display the console for approximately 10 seconds.

The QMI8658 drives the animated eyes, including a neutral pose, filtering, blinking and expressions. Invalid or missing acceleration data triggers automatic eye movement. BoardRtc supplies hardware-clock fallback and NTPUtil synchronizes it from confirmed network time; BoardPower provides battery telemetry. Audio remains outside the application services.

## 6. Validate microSD

Prefer a FAT32 card. The firmware mounts it at `/sdcard` using **1-bit SDMMC at 20 MHz**, with automatic formatting disabled. Command paths are relative to the card root.

~~~text
sd_status
sd_test
sd_write:/example.txt:Hello from AMOLED Gateway
sd_read:/example.txt
sd_append:/example.txt:Another line
sd_list:/
~~~

`sd_test` creates, verifies and removes a uniquely named temporary file. The examples above create or modify `/example.txt`. No card is formatted by these operations.

| File | Purpose |
| --- | --- |
| `/log.txt` | Command responses with logging enabled |
| `/network_history.log` | Network history, rotated at 64 KiB |
| `/network_history.previous.log` | Previous network-history file |
| `/crashes/crash-*.txt` | Crash report written by `crash_info` when supported |

The portal `/logs` shows application/network log tails, capped at 16 KiB. Text/JSON helper reads also return at most the last 16 KiB; `sd_read` and `sd_log` return up to 1,000 bytes. A directory listing stops after its output exceeds approximately 8,000 bytes; the command response applies a further limit.

If no card was mounted at startup, an SD command retries mounting. Hot-swap detection is not implemented. Restart after replacing an already mounted card, and avoid removing a card during writes. Crash archiving requires core-dump support in the selected SDK and partition layout; an archive failure preserves the internal dump.

## 7. Validate weather, metrics and OTA

- Weather queries target **Porto Alegre, Brazil**, using Open-Meteo, at a normal interval of 15 minutes. Use `clima_status` to inspect attempts and errors, and `clima_sync` to request an update. Busy, offline or OTA-paused requests can be refused.
- Check `http://DEVICE_IP:9120/metrics`. The model label is `Waveshare-ESP32-S3-Touch-AMOLED-1.8`. Weather values are remote API data. CPU-utilization metrics depend on SDK runtime-statistics support.
- Storage snapshots normally refresh every 30 seconds or after cache invalidation by file changes. The clock metric is `sdcard_clock_frequency_hz`.
- Verify the `/prometheus` settings page and YAML download. The downloaded configuration retains the inherited `esp32_c6` job name; edit it if desired.
- After a USB installation, test an ArduinoOTA upload to **AMOLED-Gateway** using an OTA-capable partition layout. Watch the screen and serial transfer summary.

OTA temporarily uses 240 MHz and a higher application-task priority, pauses new weather queries and displays progress. A running weather query may finish. The normal portal and navigation wait while the upload is processed. No transfer-rate improvement has been measured on this board.

## Execution model

`Application` owns the service objects and passes dependencies by reference. The application task, display, portal and commands run on core 1. Touch sampling and the persistent weather worker run as separate tasks on core 0. Touch events cross a FreeRTOS queue, weather snapshots use protected copies, and I2C/SD access uses recursive mutexes. The Arduino loop waits while those tasks run.

`RGBLed` is a compatibility adapter for the status separator. Physical `led_*` commands report that the M5-specific RGB hardware is unavailable.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Missing SH8601/CO5300 class or constructor | Matching Waveshare GFX library set and duplicate library installations |
| WebServer middleware compilation error | Arduino-ESP32 API compatibility with the inherited 3.3.12 configuration |
| No display / framebuffer diagnostic | Revision selection, OPI PSRAM and board initialization result |
| No touch | Matching revision/address and board initialization; use BOOT or `tela_next` as fallback |
| SD mount failure | FAT32 card, seating, startup result and selected board configuration |
| Empty log viewer | A log may not have been created yet; inspect `sd_list:/` |
| No current weather | Wi-Fi and `clima_status` diagnostics; values may still be cached |
| OTA unavailable | Wi-Fi reachability, network discovery and two application partitions |
| No crash report | SDK core-dump support, available dump and writable mounted card |

HTTP/UDP control and OTA do not configure authentication; the setup AP has no password. The HTTPS weather client currently disables certificate verification. Keep the current implementation on a trusted local network.

## References

- [GitHub overview](README.md)
- [Changelog](changelog.md)
- [Waveshare documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8)
- [Official examples and libraries](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8)

## Blank-display startup diagnostics

`BoardConfig::DisplayStartupTest` now defaults to false after the user confirmed that the display works; enable it temporarily if needed. Startup attempts a direct red/green/blue display test before allocating the canvas. If colors appear but the normal UI does not, inspect the PSRAM allocation messages. If no colors appear, inspect the expander readback, selected V1/V2 controller and driver initialization messages; a successful driver return does not prove that the panel rendered pixels. Touch-address ACK probes only suggest a possible revision mismatch and do not automatically switch controllers.

Capture the boot log at 115200 baud and confirm the label on the board. Disable the temporary color test after diagnosis by setting `DisplayStartupTest` to false.

## Confirmed board and startup crash investigation

The supplied product-label photo identifies this device as **V2: CO5300 display, CST820 touch, 16 MB flash and 8 MB PSRAM**. The project now defaults to revision 2.

The reported firmware hash `ca842b338` was matched to the cached ELF. Its captured return addresses resolve to the legacy SensorLib begin path and modern QMI8658 initialization in installed SensorLib **0.5.1**. The deprecated and modern headers declare incompatible class layouts with the same name. The project now includes only the modern `ImuDrv.hpp` API via `BoardImu`, with acceleration converted from m/s² to g for the existing animation. USB startup markers are emitted before constructing Application.

Recompile and upload this revision; the new boot log and visible startup-color test are needed to validate the correction. No successful on-device result is claimed yet.

## Full-screen layout, vertical navigation and PWR

The user confirmed that the V2 display now operates. The updated UI uses larger clock digits, adaptive text and rows distributed across the available display height, with a compact top status bar and a minimal page indicator. The shared ScreenLayout geometry adapts to rotated orientations. The analog face and eyes retain their aspect ratio within the larger available viewport.

Swipe **up** or **left** to advance. Swipe **down** or **right** to go back. A short tap elsewhere wakes the current page. Touch-hold opens Personalize from a clock and the customized clock from other pages, and the first gesture after idle only wakes the screen. BOOT retains its existing navigation/reset behavior.

PWR is handled by the included BoardPower class, without an extra PMU library. A short press requests AXP2101 shutdown after pausing touch/new weather requests and unmounting SD. Hold PWR for approximately 1 second to start from the off state. A 6-second hold provides hardware emergency power-off; it bypasses orderly SD shutdown. Charging currents and supply-rail voltages are preserved. Startup power-key events are ignored briefly to avoid an immediate shutdown after waking.

If the CPU remains powered after the shutdown request, the firmware reports this, restores operation and shows a diagnostic instead of claiming the device is off. USB/external supply and PMIC wake behavior must be tested on the actual board; verify standalone power-off/on with a charged battery and USB disconnected. During an OTA transfer, software polling waits, while the hardware long-press fallback remains available.

Recompile and validate this new layout, both vertical gestures in each rotation and PWR off/on. The display confirmation predates these changes; the new UI and power behavior are not yet physically verified.

## Arduino IDE wireless uploads

See [OTA.md](OTA.md) for the complete USB-first setup, network-port selection and discovery troubleshooting. ArduinoOTA explicitly advertises AMOLED-Gateway through mDNS, listens on port 3232 and restarts after successful firmware uploads.

## Battery command and persistent analog clock

Use **bateria** (alias **battery**) over Serial or UDP to read the AXP2101 battery telemetry:

- Battery detection, USB supply and charging/discharging/standby state.
- Remaining charge percentage reported by the PMIC fuel gauge.
- Used capacity as **100 minus the current charge percentage**, relative to full capacity; this is not a cumulative energy counter.
- Battery voltage when the ADC reading is valid.
- Estimated discharge rate in **percentage points per hour**, after at least 10 minutes of uninterrupted discharge without USB and a reported drop in charge. This estimate comes from fuel-gauge changes, not a measured current or power value. Until there is enough information, the command explicitly reports that it is waiting.

Readings refresh every 5 seconds; a battery command requests a fresh read. Missing battery, I2C failures and invalid percentage/voltage readings are shown as unavailable instead of fabricated values. Charger current, target voltage and gauge calibration are not changed.

Use **relogio** or **tela:10**, or navigate to the new Analog Clock page. The native vector dial is inspired by the inner face of the supplied watch reference: dark textured dial, cyan markers/rings, hour/minute hands, a contrasting seconds hand, date, UTC offset, Wi-Fi state, digital time, battery status and weather in the dial area. Steps/calories or other unmeasured values are not imitated.

This is the ninth distinct page. Existing IDs 0–9 remain unchanged; analog is ID 10. While it is selected:

- The page stays selected; no automatic 20-second return.
- The eye screensaver is disabled.
- Idle brightness reduction is disabled on this page; the configurable night schedule and explicit brightness/off commands still apply.
- Time/seconds update once per second. Without a valid clock, the face shows a placeholder rather than invented time.
- Touch gestures, BOOT, PWR, networking, commands and OTA remain available. Other pages retain their previous inactivity behavior.

The selection is not persisted across reboot. Compilation and physical validation of the new battery readings and dial remain with the user.

## AMOLED visual refresh

The bottom next-page button and its touch target have been removed. Navigation is by swipes (up/left for next, down/right for previous); short taps change cards/charts on Personalize/Monitor and wake other pages; touch-hold opens Personalize from a clock and the customized clock elsewhere. A compact top bar displays the page number, pinned-clock state, battery and Wi-Fi signal. RSSI colors are green at -60 dBm or better, amber down to -75 dBm, orange down to -85 dBm and red below that; AP mode and offline state are indicated separately.

UiPainter supplies bundled proportional FreeSans fonts in several sizes, pixel-measured text fitting, vector icons and colored cards. Main pages include illustrated weather, actual ping history, RAM/PSRAM availability bars and SD utilization. Unmeasured ping samples are omitted, and timeouts are marked separately. The analog clock and animated eyes use the recovered vertical space. One buffered frame is transferred per render.

No new installed graphics library is required. The font headers and upstream notices are included in UiFonts. See [font attribution](UiFonts/README.md). New UI changes still require user compilation and visual validation in portrait/landscape and over OTA.

## Hardware RTC synchronization and offline time

The onboard **PCF85063A** is now integrated through the local BoardRtc driver at I2C address **0x51**, with no extra library dependency. NTPUtil owns the time-source policy and receives the RTC by reference.

- On startup, a valid RTC calendar restores the system clock immediately. The device does not wait for Wi-Fi to show that time.
- An actual SNTP callback confirms network synchronization. A valid system date restored from RTC is never treated as proof that NTP succeeded.
- NTP synchronization is configured every 5 minutes. With a recent confirmed response on the current Wi-Fi connection, the UI shows **NTP**. A disconnect, an initial missing response or a response older than 10 minutes selects **RTC** when its calendar is valid. A fresh response is required after reconnection.
- Every confirmed NTP update, and periodically while network time remains fresh, compares UTC against the RTC. A difference of at least 2 seconds triggers an RTC update; the 1-second tolerance avoids unnecessary writes at a tick/read boundary. An invalid RTC is initialized from trusted NTP time.
- RTC writes use a stopped clock, a single calendar burst and readback verification. A partial calendar write stays stopped until it can be repaired; it is not accepted as valid time.
- The seconds oscillator-stop flag, stop/test modes, BCD fields, month length and leap years are checked before accepting a calendar. The hardware calendar covers 2000–2099.
- The RTC stores **UTC**. The configured timezone and DST offset are applied only for display, so timezone changes do not shift the hardware clock. A pre-existing RTC that was written in local time by another firmware is realigned on the first successful NTP synchronization.

Both digital and analog clocks, plus **time** and **date**, use the selected source. The digital clock has an NTP/RTC source badge and the analog dial shows the source beside the UTC offset. The pinned analog page keeps its existing no-screensaver behavior.

Use **ntp_status** or **rtc_status** to inspect the displayed time, source, RTC validation error, latest NTP age, measured difference and verified RTC-write count. **set_time:YYYY-MM-DD HH:MM:SS** accepts local wall time, stores the equivalent UTC in the RTC when supported and marks the manual source until a new NTP response arrives.

If the RTC is invalid/unreachable, a previously trusted running system clock may continue with the explicit **INTERNO** label; a manual setting uses **MANUAL**. If no valid time exists anywhere, the display shows an unavailable-time placeholder instead of a fabricated date. RTC retention depends on the board keeping the RTC/backup supply powered.

### Validation on the board

1. Connect Wi-Fi and confirm an actual NTP response with **ntp_status**.
2. Confirm a valid hardware RTC and, when needed, a verified correction with **rtc_status**.
3. Disconnect the network and check that the display source becomes RTC while the clock continues.
4. Restart without network, retaining the RTC supply, and check that time appears from RTC before any NTP response.
5. Reconnect and confirm that a fresh NTP response restores the NTP label and corrects any significant drift.
6. Change timezone/DST and confirm that display time changes once, without shifting the stored UTC calendar.

Compilation and physical RTC/NTP validation remain with the user.


## Personal watch faces, night brightness and session histories

The customized clock stays on **page 10** (**relogio**) with seconds updating without Internet, using the same NTP/RTC time authority. All three watch faces remain pinned: no automatic return and no eye screensaver. Night brightness still applies.

### Touch controls and personalization

- Hold a clock view for at least **800 ms**, then release, to open **Personalize** (**personalizar**, page **12**).
- Hold any other page, including Personalize, to open the customized clock.
- Swipe up/left for the next page and down/right for the previous page. The first contact after the screensaver only wakes the display.
- Tap a Personalize card to change its setting: **Instrumental / Minimalist / Digital**, **cyan / green / amber / violet**, automatic night brightness, night hours, night intensity, or motion gestures.
- On the night-hours card, tap the **left half** to advance the starting hour or the **right half** to advance the ending hour. Each tap advances one hour. Equal hours disable the night interval.
- The night intensity cycles through **5%, 12%, 20%, 30%**. Defaults are **22:00–07:00 at 12%**. This is a cap on the requested brightness, so a lower manual setting stays lower and an explicitly off display stays off until touched or turned on by command.
- Preferences are saved and read back from NVS. A save failure is reported on the page; changes then apply only to the current session.

The night schedule uses local clock time, including the configured timezone/DST and RTC fallback. With no valid time it does not guess the hour. It does not require a light sensor, Internet, or a weather response.

### Battery history

Open **historico_bateria** or **tela:11**. The screen shows remaining charge, charge/discharge state, voltage, a **144-slot graph sampled nominally every five minutes** (about 12 hours), and approximate remaining autonomy after sufficient uninterrupted discharge observations. Green segments mean charging; amber segments mean discharge/standby. Invalid or absent-battery readings create gaps rather than fabricated percentages. Autonomy is extrapolated from SOC change, not an instantaneous mA measurement.

The chart is a bounded RAM history for the current boot session. When the SD is mounted, each sample is also appended to **/battery_history.csv**, containing local time, uptime, SOC, voltage, charging and USB flags. Invalid values are left blank. At 128 KiB the current archive rotates to **/battery_history.previous.csv**; only these two archives are kept. Read them with **sd_read:/battery_history.csv** or the existing SD commands. Archives are not reloaded into the graph at boot. Collection and the live graph still work without a card.

### Network monitor

On **page 8**, tap to alternate **gateway latency** and **Wi-Fi RSSI** charts. A 120-slot session ring samples nominally every ten seconds (about 20 minutes), while the existing asynchronous gateway ping continues. The page also shows Wi-Fi drops, cumulative offline seconds, ping loss and average successful RTT. Red marks identify missing/failed readings; gaps are not connected. Latency uses an automatic vertical scale and RSSI uses -100 to -30 dBm. A ping to the local gateway measures LAN reachability, **not Internet availability**. Existing network SD logging and net_monitor/net_history commands remain available.

### Motion gestures

With motion enabled, the QMI8658 is polled without delays at approximately 80 ms intervals. Filtered acceleration rejects shocks. Rest the device flat for at least 800 ms and lift it to wake a dimmed/screensaver display; explicit tela_off remains respected. This wakes the display while the MCU is running, not a powered-off device.

Portrait orientation is learned from the first stable upright pose. Turning the device upside down for at least 1.2 seconds rotates the canvas and touch coordinates by 180 degrees relative to that reference. Automatic landscape is intentionally excluded. Rotation waits for an active touch to finish and is deferred during BOOT holds or console messages. Motion can be disabled on Personalize; manual rotacao commands remain available. Sensor-axis calibration, lift thresholds and behavior in the enclosure still require physical validation.

### Validation on the device

Compilation/flashing remain with the device owner. No firmware build or physical validation is claimed for this change.

1. Open **personalizar**, try all three faces/four colors, reboot and confirm saved choices.
2. Select **relogio**, disconnect Wi-Fi and confirm RTC time continues, seconds update and the screensaver stays disabled.
3. Move the night interval around the current RTC hour; check the night cap, restore 22:00–07:00, and verify tela_off/tela_on.
4. Check battery charge/USB readings, collect several five-minute points, and inspect the CSV with and without an SD card.
5. Tap the monitor to change charts; disconnect/reconnect Wi-Fi and inspect gaps, loss and drop counts.
6. Learn an upright pose, invert for 1.2 seconds, check touch coordinates, rest flat/lift, and verify the disabled-motion setting.
7. Repeat an Arduino IDE OTA upload and verify the same configuration and RTC behavior after reboot.
