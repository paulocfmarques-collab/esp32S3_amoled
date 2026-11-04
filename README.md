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

The onboard RTC, audio hardware and battery telemetry are not exposed as new application services. Time comes from NTP. The M5-specific physical RGB LED is unavailable; `led_*` commands report this, while a screen separator provides network status colors.

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
| [ClimaManager](ClimaManager.h) / [NTPUtil](NTPUtil.h) | Weather cache and network time |
| [CommandProcessor](CommandProcessor.h) / [SerialConsole](SerialConsole.h) | Shared command handling and serial input |
| [MetricsService](MetricsService.h) / [OTAManager](OTAManager.h) | Monitoring and firmware updates |
| [CrashDiagnostics](CrashDiagnostics.h) | Conditional core-dump inspection and archiving |

## Controls

| Input | Action |
| --- | --- |
| Short tap / swipe left | Next page |
| Swipe right | Previous page |
| Hold touch for at least 800 ms, then release | Return to the clock |
| First touch after the screensaver or with zero brightness | Wake the display without changing pages |
| Short BOOT press | Next page on release |
| Hold BOOT for 3 seconds, then release | Restart |
| Hold BOOT for 10 seconds, then release | Clear saved Wi-Fi profiles and restart |

Holding BOOT during power-on enters the ESP32 download mode. PWR remains under the board's power management.

The page sequence is **Clock → NTP → Network → System → Saved Wi-Fi → SD → Monitor → Forecast**. Direct selection remains `tela:0..9`; NTP pages 1/2 and network pages 3/4 are aliases. The interface returns to the clock after 20 seconds without navigation, activates the eyes after 60 seconds of inactivity, and reduces brightness after 120 seconds.

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
