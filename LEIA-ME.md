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

PWR remains controlled by the board power-management hardware. GPIO41 and the AtomS3R LP5562 RGB controller are not used.

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
| Short tap or swipe left | Advance one distinct page |
| Swipe right | Select the previous distinct page |
| Hold touch for at least 800 ms and release | Return to the clock |
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

Sequential navigation skips duplicate aliases. Test `rotacao:0`, `rotacao:90`, `rotacao:180` and `rotacao:270`, checking both rendering and touch directions. Rotation defaults to 0° and is not saved between boots.

The interface returns to the clock after 20 seconds without navigation. The screensaver starts after 60 seconds of inactivity; after 120 seconds, brightness is reduced to at most 20% of full scale, without exceeding the configured level. Commands temporarily display the console for approximately 10 seconds.

The QMI8658 drives the animated eyes, including a neutral pose, filtering, blinking and expressions. Invalid or missing acceleration data triggers automatic eye movement. The firmware does not add RTC, audio or battery-telemetry services. The clock uses NTP.

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

`BoardConfig::DisplayStartupTest` defaults to true. Startup attempts a direct red/green/blue display test before allocating the canvas. If colors appear but the normal UI does not, inspect the PSRAM allocation messages. If no colors appear, inspect the expander readback, selected V1/V2 controller and driver initialization messages; a successful driver return does not prove that the panel rendered pixels. Touch-address ACK probes only suggest a possible revision mismatch and do not automatically switch controllers.

Capture the boot log at 115200 baud and confirm the label on the board. Disable the temporary color test after diagnosis by setting `DisplayStartupTest` to false.

## Confirmed board and startup crash investigation

The supplied product-label photo identifies this device as **V2: CO5300 display, CST820 touch, 16 MB flash and 8 MB PSRAM**. The project now defaults to revision 2.

The reported firmware hash `ca842b338` was matched to the cached ELF. Its captured return addresses resolve to the legacy SensorLib begin path and modern QMI8658 initialization in installed SensorLib **0.5.1**. The deprecated and modern headers declare incompatible class layouts with the same name. The project now includes only the modern `ImuDrv.hpp` API via `BoardImu`, with acceleration converted from m/s² to g for the existing animation. USB startup markers are emitted before constructing Application.

Recompile and upload this revision; the new boot log and visible startup-color test are needed to validate the correction. No successful on-device result is claimed yet.
