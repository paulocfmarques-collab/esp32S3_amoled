<div align="center">

# AMOLED Gateway

### A touch-enabled network dashboard for the Waveshare ESP32-S3-Touch-AMOLED-1.8

**Dual-core firmware · Native AMOLED UI · microSD logging · Local web portal · Prometheus**

![Platform](https://img.shields.io/badge/platform-ESP32--S3-0891b2?style=flat-square)
![Framework](https://img.shields.io/badge/framework-Arduino-00878f?style=flat-square)
![Architecture](https://img.shields.io/badge/architecture-C%2B%2B%20%2F%20FreeRTOS-2563eb?style=flat-square)
![Display](https://img.shields.io/badge/display-368%20%C3%97%20448%20AMOLED-a855f7?style=flat-square)
![Validation](https://img.shields.io/badge/hardware%20validation-pending-d97706?style=flat-square)

[Quick start](#quick-start) · [Architecture](#architecture) · [Controls](#controls) · [Observability](#observability) · [Changelog](changelog.md)

</div>

---

AMOLED Gateway turns the Waveshare ESP32-S3-Touch-AMOLED-1.8 into a compact status display and network monitor. Check connectivity, time, system information, weather and storage from the touchscreen; manage Wi-Fi profiles in a local web portal; inspect logs on microSD; and export device metrics to Prometheus.

The firmware is adapted from an M5AtomS3/AtomS3R gateway project. Its application services are owned by an `Application` object, with explicit dependencies and FreeRTOS tasks pinned to both ESP32-S3 cores.

> **Project status:** the Waveshare adaptation is implemented in source. Compilation, flashing and physical validation remain pending. The badges describe the project configuration, not a passing build or hardware certification. Firmware messages and command names retain their original language.

## Highlights

| Capability | Implementation |
| --- | --- |
| Native AMOLED interface | 368 × 448 RGB565 rendering with a 329,728-byte PSRAM framebuffer |
| Touch navigation | Tap, horizontal swipe and hold gestures; rotation-aware coordinates |
| Dual-core execution | Application on core 1; separate touch and weather tasks on core 0 |
| Local network management | Five Wi-Fi profiles in NVS, setup AP and HTTP portal |
| Persistent storage | Built-in microSD slot over 1-bit SDMMC at 20 MHz |
| Diagnostics | Serial/UDP commands, network monitoring, reset information and conditional crash archiving |
| Observability | Prometheus exporter, UDP counters, system metrics and cached weather/storage data |
| OTA updates | ArduinoOTA with on-screen progress and temporary transfer prioritization |
| Animated screensaver | QMI8658-driven eyes with blinking, expressions and automatic motion fallback |

## Hardware compatibility

Select the board revision in [BoardConfig.h](BoardConfig.h) **before compiling**. Revision detection is not automatic.

| Configuration | Display controller | Touch controller |
| --- | --- | --- |
| `AMOLED_BOARD_REVISION 1` | SH8601 | FT3168 |
| `AMOLED_BOARD_REVISION 2` — default for the confirmed board | CO5300 | CST820 |

The target configuration uses 16 MB flash and 8 MB OPI PSRAM. These are source-level targets; both board revisions still require physical testing with this firmware.

The hardware RTC provides offline time and is corrected from confirmed NTP updates. Audio is not exposed as an application service. Battery telemetry is available through BoardPower. The M5-specific physical RGB LED is unavailable; `led_*` commands report this, while a screen separator provides network status colors.

## Quick start

### 1. Install the dependencies

Use the matching Waveshare example library set for **Arduino_GFX**:

- V1: `examples/arduino/libraries`.
- V2: `examples/arduino-v2/libraries`.

Install **GFX_Library_for_Arduino (Arduino_GFX)** and **SensorLib 0.5.1**. The I/O expander uses the included `BoardIoExpander.h` driver; no Adafruit expander library is required. Use Waveshare's bundled GFX library to match the SH8601/CO5300 drivers used here.

WiFi, WebServer, Preferences, ArduinoOTA, SD_MMC and FreeRTOS are supplied by Arduino-ESP32. M5Unified, M5GFX, SdFat and LVGL are not required.


### SensorLib version

Use **SensorLib 0.5.1**, already found in the current Arduino installation. The project uses the modern **ImuDrv.hpp** API through [BoardImu.h](BoardImu.h). Do not install the previously supplied 0.3.3 archive for this revision, and do not include the deprecated root SensorQMI8658.hpp alongside the modern driver. Use the Waveshare package for the matching GFX library, not to replace SensorLib with its older bundled copy.

### 2. Configure the Arduino IDE

Open [ESP32S3.ino](ESP32S3.ino) in this folder. The configuration carried forward from the project targets **esp32 by Espressif Systems 3.3.12**, including its WebServer middleware API; it has not been compiled in this adaptation.

| Setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | A 16 MB layout with two application slots and OTA support |
| USB CDC On Boot | Enabled |
| USB Mode | Hardware CDC and JTAG |
| Arduino Runs On | Core 1 |
| Events Run On | Core 0 |
| Serial Monitor | 115200 baud; newline-terminated commands |

Avoid a single-application partition layout without OTA support. If PSRAM allocation fails, the display reports a serial diagnostic and network services can continue without rendering.

### 3. Select the revision and flash

~~~cpp
// BoardConfig.h: choose 1 or 2 to match the board label.
#define AMOLED_BOARD_REVISION 2
~~~

Compile and upload over USB, then inspect the serial output for board initialization, SD mounting and task core assignments.

### 4. Connect and explore

1. Join **AMOLED_CONFIG**.
2. Open [the setup portal](http://192.168.4.1/wifi) and save your Wi-Fi profiles.
3. Once connected, use the device's LAN IP to open the portal.
4. Send `help` over Serial or UDP port **4210** to discover the command interface.

| Service | Default address or setting |
| --- | --- |
| Device / OTA hostname | `AMOLED-Gateway` |
| Setup access point | `AMOLED_CONFIG` |
| HTTP portal | Port `80`: `/`, `/info`, `/wifi` |
| SD log viewer | `/logs` |
| Prometheus settings | `/prometheus` |
| Prometheus configuration download | `/prometheus.yml` |
| Metrics exporter | Port `9120`, path `/metrics` |
| UDP command listener | Port `4210` |

## Architecture

~~~mermaid
flowchart LR
    App[Application owns the services]
    subgraph Core1[Core 1]
        Main[Application task]
        UI[Display and animation]
        Net[HTTP / UDP / OTA]
        SD[SDMMC files and logs]
    end
    subgraph Core0[Core 0]
        Touch[Touch task]
        Weather[Weather task]
    end
    App --> Main
    Main --> UI
    Main --> Net
    Main --> SD
    Touch -->|Gesture queue| Main
    Main -->|Task notification| Weather
    Weather -->|Protected snapshot| Main
~~~

Touch polling runs every 20 ms. Weather requests use a persistent worker, normally scheduled every 15 minutes while connected. I2C access is protected by a recursive mutex; SD operations have their own recursive mutex. The touch task sends events and does not draw or modify application state directly.

| Source | Responsibility |
| --- | --- |
| [Application](Application.h) | Object ownership, initialization, navigation and main task |
| [BoardConfig](BoardConfig.h) / [BoardHardware](BoardHardware.h) | Board revision, pin mapping, QSPI, local TCA9554 driver and IMU |
| [TouchInput](TouchInput.h) | Touch sampling, gesture detection and event queue |
| [DisplayUtil](DisplayUtil.h) / [EyesAnimation](EyesAnimation.h) | PSRAM framebuffer, pages, brightness, rotation and expressions |
| [SDUtil](SDUtil.h) | SDMMC mounting, bounded reads, file operations and storage snapshots |
| [ESP32Gateway](ESP32Gateway.h) / [NetworkMonitor](NetworkMonitor.h) | Wi-Fi profiles, portal, UDP and connection monitoring |
| [ClimaManager](ClimaManager.h) / [NTPUtil](NTPUtil.h) | Weather cache and NTP/RTC time selection |
| [CommandProcessor](CommandProcessor.h) / [SerialConsole](SerialConsole.h) | Shared command handling and serial input |
| [MetricsService](MetricsService.h) / [OTAManager](OTAManager.h) | Monitoring and firmware updates |
| [CrashDiagnostics](CrashDiagnostics.h) | Conditional core-dump inspection and archiving |

## Controls

| Input | Action |
| --- | --- |
| Swipe up / swipe left | Next page |
| Short tap | Wake / keep the current page |
| Swipe down / swipe right | Previous page |
| Hold touch for at least 800 ms, then release | Open Personalize from a clock; open the customized clock elsewhere |
| First touch after the screensaver or with zero brightness | Wake the display without changing pages |
| Short BOOT press | Next page on release |
| Hold BOOT for 3 seconds, then release | Restart |
| Hold BOOT for 10 seconds, then release | Clear saved Wi-Fi profiles and restart |

Holding BOOT during power-on enters the ESP32 download mode. PWR now uses the AXP2101 through the included BoardPower class: a short press requests orderly shutdown; hold for about 1 second to power on; a 6-second hold is the hardware emergency power-off.

The page sequence is **Clock → NTP → Network → System → Saved Wi-Fi → SD → Monitor → Forecast → Customized Clock → Battery History → Personalize**. Direct selection remains `tela:0..12`; NTP pages 1/2 and network pages 3/4 are aliases. The customized clock and Personalize do not auto-return; other non-clock pages return to the clock after 20 seconds without navigation, activates the eyes after 60 seconds of inactivity, and reduces brightness after 120 seconds.

### Useful commands

Command tokens are part of the existing firmware API and are intentionally preserved.

~~~text
help
status
net_monitor
clima_status
clima_sync
tela:7
rotacao:90
brilho:80
sd_status
sd_test
sd_write:/example.txt:Hello from AMOLED Gateway
sd_read:/example.txt
sd_list:/
~~~

Serial input is limited to 255 bytes per command. Display rotation supports 0°, 90°, 180° and 270°, defaults to 0° and is not persisted across restarts. Weather is currently configured for **Porto Alegre, Brazil**.

## Storage and logs

The card is mounted at `/sdcard` using **1-bit SDMMC at 20 MHz**, without automatic formatting. Prefer a FAT32 card. Commands use card-relative paths such as `/example.txt`, not `/sdcard/example.txt`.

| Path | Purpose |
| --- | --- |
| `/log.txt` | Command responses for which logging is enabled |
| `/network_history.log` | Network history; rotated at 64 KiB |
| `/network_history.previous.log` | Previous network history file |
| `/crashes/crash-*.txt` | Crash reports created by `crash_info` when core-dump support is available |

The web viewer and storage text helpers read up to the last 16 KiB. `sd_read` and `sd_log` return up to 1,000 bytes. Directory listings are bounded. `sd_test` creates, verifies and removes a uniquely named temporary file.

An SD command retries mounting if no card was mounted at startup. Hot-swap detection is not implemented; restart after removing or replacing an already mounted card. `crash_info` preserves the internal dump if archiving fails.

## Observability

The exporter provides system, Wi-Fi, UDP, HTTP, memory, storage and weather metrics. Weather values come from **Open-Meteo**, not a local environmental sensor. CPU utilization is exposed only when the SDK provides the required runtime statistics.

~~~yaml
scrape_configs:
  - job_name: amoled_gateway
    scrape_interval: 15s
    static_configs:
      - targets: ["192.168.1.50:9120"]
~~~

Replace the example IP with your device's LAN address. This is a manual configuration example; the firmware-generated YAML currently retains the inherited `esp32_c6` job name.

~~~promql
esp_info{model="Waveshare-ESP32-S3-Touch-AMOLED-1.8"}
rate(esp_udp_packets_received_total[5m])
sdcard_mounted
esp_weather_data_age_seconds
~~~

Storage metrics refresh approximately every 30 seconds, or after file changes invalidate the cache. The SD clock metric is `sdcard_clock_frequency_hz`, replacing the former SPI-specific name.

## OTA and deployment boundaries

The first installation uses USB. Subsequent ArduinoOTA updates use **AMOLED-Gateway** when discoverable on the network. During transfer, the screen shows progress, the CPU is set to 240 MHz, task priority is temporarily raised and new weather queries are paused. An already running query may finish. The portal and normal navigation wait while the transfer is handled.

Use a trusted local network: the setup AP has no configured password, HTTP/UDP control is unauthenticated, and no ArduinoOTA password is configured. The weather client currently uses `setInsecure()` for HTTPS. These are current implementation limits, not hardened remote-management features.

## Validation and contribution

No successful Waveshare build, physical test or performance benchmark is claimed for this revision. Before relying on the device, verify both the selected board revision and the intended features using the [setup and validation guide](LEIA-ME.md).

When reporting an issue, include the board revision, Arduino-ESP32 and library versions, IDE settings, relevant serial output and reproduction steps. Changes should preserve explicit object ownership, the single application-task owner of the display, and synchronization between cores.

## Documentation and references

- [Setup and validation guide](LEIA-ME.md) — English.
- [Changelog](changelog.md) — English, including inherited project history.
- [Extended command reference](comandos.md) — existing Portuguese reference.
- [Prometheus notes](PROMETHEUS.md) — existing Portuguese reference.
- [Waveshare documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8).
- [Waveshare examples and libraries](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8).

**License:** no project license file is currently included. Dependency licenses remain applicable; add an appropriate project license before distributing the project under a declared license.

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
