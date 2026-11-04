# Changelog

All entries are in English and use ISO dates. Dates record development changes, not published releases. No semantic-version tags are asserted; the firmware's `version` command continues to derive its identifier from the build date and time.

Entries before 2026-11-04 describe the inherited AtomS3R project. Hardware-specific behavior in those entries is historical and does not override the current Waveshare configuration.

## 2026-11-04 — Select confirmed V2 hardware and migrate the IMU API

- Confirmed CO5300/CST820 V2 from the supplied product-label photo and selected revision 2 by default.
- Matched the reported ELF SHA256 prefix `ca842b338` to the cached build without recompiling. Captured return addresses `0x42009afa` and `0x4201c6ef` decode to SensorLib device initialization and QMI8658 `initImpl`.
- Found SensorLib 0.5.1 installed while the sketch included its deprecated root header, which declares an incompatible class layout sharing the modern driver name.
- Added `BoardImu` using only the modern `ImuDrv.hpp` API, checked accelerometer configuration/enabling and converted m/s² readings to g.
- Added USB startup messages before and after constructing Application.
- Updated dependency instructions to SensorLib 0.5.1 and withdrew the obsolete 0.3.3 installation archive.
- Compilation, flashing and hardware confirmation remain with the user.

## 2026-11-04 — Diagnose a blank display

- Added an optional direct red/green/blue startup test before allocating the PSRAM framebuffer (`BoardConfig::DisplayStartupTest`, enabled by default).
- Added I2C startup checks, expander output/configuration readback and touch-address probes to highlight a possible revision mismatch.
- Separated driver initialization from framebuffer-allocation diagnostics; driver success does not prove visible panel output.
- Added a direct on-screen error when framebuffer allocation fails, plus PSRAM size/free-memory diagnostics.
- No root cause has been confirmed for the reported blank panel. Awaiting the selected board revision and boot log; physical verification remains pending.

## 2026-11-04 — Package the matching SensorLib dependency

- Added `SensorLib-Waveshare.zip`, an installable SensorLib 0.3.3 archive extracted from the supplied official Waveshare package.
- Verified that the archive contains `library.properties` and `src/SensorQMI8658.hpp`; included the library sources and bundled license files.
- Added Arduino IDE ZIP-installation instructions. Firmware compilation and hardware testing remain pending with the user.

## 2026-11-04 — Remove the external expander dependency

### Fixed

- Resolved the reported missing `Adafruit_XCA9554.h` include by adding the local `BoardIoExpander` class using Arduino Wire.
- Preserved unrelated expander bits, preloaded output latches before enabling outputs and checked every initialization write.
- Kept all expander transactions inside the board initialization I2C lock.
- Removed Adafruit XCA9554/BusIO installation requirements from the setup guides.

### Validation

- Source references reviewed; recompilation and physical verification remain with the user.

## 2026-11-04 — Waveshare AMOLED adaptation and GitHub documentation

### Added

- Board support classes for the ESP32-S3-Touch-AMOLED-1.8, with an explicit revision selector in `BoardConfig.h`: SH8601/FT3168 for V1 and CO5300/CST820 for V2.
- Native 368 × 448 AMOLED rendering and a 329,728-byte RGB565 framebuffer allocated in PSRAM.
- A dedicated touch task on core 0, 20 ms polling, gesture events through a FreeRTOS queue and rotation-aware touch coordinates.
- Tap/left-swipe navigation, right-swipe reverse navigation and touch-hold return to the clock.
- Integrated microSD access through 1-bit SDMMC at 20 MHz, with recursive locking and no automatic formatting.
- File operations, command logs, rotating network-history files, bounded reads and storage-metric snapshots.
- A `/logs` portal page for the application and current/previous network-history tails.
- SD crash-report archiving when an internal core dump is available and supported by the SDK configuration.
- QMI8658 integration for the eye animation, retaining expressions, blinking, relative movement and automatic fallback.
- A GitHub-facing `README.md` with feature tables, architecture diagram, setup instructions, command examples, observability notes and validation status.
- An English setup/validation guide in `LEIA-ME.md` and this English changelog.

### Changed

- Made `Application` the owner of the board and service objects, with explicit reference-based dependencies.
- Converted weather and OTA services to instances rather than shared static service state.
- Kept the application task on core 1 and placed touch/weather in separate tasks on core 0.
- Protected shared I2C access with a recursive mutex and SD operations with a separate recursive mutex.
- Adapted display pages, console, OTA progress and animation to the larger AMOLED screen.
- Included the SD page in sequential navigation, while retaining `tela:0..9` and duplicate NTP/network aliases.
- Moved the user-button behavior from AtomS3R GPIO41 to BOOT GPIO0.
- Set the default display rotation to 0° and applied rotation to touch mapping.
- Renamed the setup AP to `AMOLED_CONFIG` and the device/OTA hostname to `AMOLED-Gateway`.
- Updated the Prometheus model label to `Waveshare-ESP32-S3-Touch-AMOLED-1.8`.
- Replaced the SPI-specific storage-frequency metric with `sdcard_clock_frequency_hz` and updated the button metric to GPIO0.
- Updated command and Prometheus notes for the new board, storage support and service identity.

### Removed

- Dependencies on M5Unified, M5GFX and SdFat in the adapted firmware.
- The obsolete shared-SPI helper and global `extern` references to display, SD and RGB service objects.
- Direct access to the AtomS3R LP5562 RGB hardware. Physical `led_*` commands now report unavailability; the compatibility adapter colors the screen status separator.

### Retained

- Five Wi-Fi profiles in NVS, serial commands at 115200 baud, UDP port 4210, the local HTTP portal, NTP, network monitoring, weather caching, OTA and Prometheus.
- Clock return after 20 seconds, screensaver activation after 60 seconds and idle brightness reduction after 120 seconds.
- Conditional preservation of an internal crash dump if SD archiving fails.
- The original source project in its separate folder.

### Validation and known boundaries

- Compilation, flashing and physical tests remain pending and are assigned to the user. Neither hardware revision is claimed as validated by this adaptation.
- Board revision selection is manual; SD hot-swap detection is not implemented.
- RTC, audio and battery telemetry were not added as application services.
- Existing firmware messages/command tokens and the extended command/Prometheus reference files retain their original language.
- The generated Prometheus YAML retains the inherited `esp32_c6` job name.

## 2026-10-31 — Persistent counter reset

### Added

- `reset_counters` in Serial/UDP handling and the command menu.
- Reset of NVS boot/watchdog counters while preserving Prometheus settings.

### Documentation

- Updated the local command guide and consolidated command catalog.

## 2026-10-27 — Persistent weather worker and memory lifecycle

### Fixed

- Removed task deletion while HTTP/TLS objects and strings were still in scope, which prevented their destructors from running.
- Moved each query into a normally returning function so request resources are released before clearing the busy state.

### Changed

- Reserved a persistent weather task at startup on core 0. The task waits for notifications between 15-minute query cycles instead of repeatedly allocating a task stack/control block.
- Added worker-reservation status, free internal heap and largest internal-memory block to `clima_status`.
- Made `clima_sync` report refusal when scheduling is prevented by memory pressure, an active query, OTA or missing connectivity.

### Validation

- Reviewed object lifetimes in source; compilation and extended on-device memory testing remained pending.

## 2026-10-26 — Stale-weather diagnostics

### Added

- `clima_status` with connectivity, worker activity, OTA pause state, attempt/success counts, attempt age and the latest HTTP/parsing result.
- Diagnostics in `clima_sync` and a serial `[CLIMA]` summary after each completed query.

### Changed

- Set the TLS handshake timeout to 10 seconds and HTTP connection/read timeouts to 8 seconds.
- Allowed whitespace between JSON keys and colons when extracting objects/fields.
- Retained the 15-minute automatic interval and prevention of duplicate active query tasks.

### Validation

- The reported 302-minute stale reading did not establish a root cause. Device validation was still required.

## 2026-10-21 — Parseable command help

### Changed

- Standardized `help` output with the `COMANDOS` header and one command per line, without explanatory prose in the command list.
- Updated the command guide.

## 2026-10-19 — UDP reliability and Prometheus coverage

### Added

- `esp_udp_listener_up`, `esp_udp_packets_received_total`, `esp_udp_replies_sent_total` and `esp_udp_send_errors_total`.
- Separate accounting for complete replies and transmitted blocks; a reply counts only after every block is accepted by the local send path.
- Open-Meteo relative humidity in the protected cache, with 0–100% validation.
- `sensor_temperature_celsius` and `sensor_humidity_percent` with `source="open_meteo"` and `location="porto_alegre"`; unavailable values are omitted.
- Prometheus guidance describing cache limits and dashboard queries.

### Fixed

- Captured and preserved the UDP sender address/port through processing and socket recovery.
- Checked packet opening, writing and sending; failures report the destination and errno through Serial.
- Added one socket-recovery retry per failed block without re-executing the command.
- Reopened the listener during station reconnection and AP/station transitions.
- Split long replies into explicitly checked datagrams of up to 1,200 bytes.

### Changed

- Used 64-bit UDP counters, preserving them across socket recovery and resetting them at boot.
- Passed UDP status to the exporter from the application task without opening sockets during scrapes.
- Included UDP counters in network-monitor reports.
- Retained a 15-minute weather interval with no weather API requests during scrapes.
- Retained the then-current `M5Stack-AtomS3R` model label for dashboard compatibility.

### Validation

- Long-duration UDP reliability and server/device verification remained pending.

## 2026-10-18 — Navigation, rotation and animated UI

### Added

- Strict `rotacao:0|90|180|270` command validation for Serial, UDP and help.
- OTA progress bar and percentage, with writing, verification, completion and error states.
- Cyan mouth/eyebrows and happy, curious, surprised and sleepy expressions in the screensaver.
- Expression changes every 3.5–6.5 seconds, smooth eyelid transitions and an approximately two-second surprise reaction to strong movement, with a minimum four-second cooldown.

### Changed

- Applied absolute rotation to pages, console, animation and OTA; aligned accelerometer mapping with display rotation.
- Set the AtomS3R default orientation to 180° at that time, without persisting user rotation across boots. The Waveshare adaptation later changed this default to 0°.
- Limited OTA screen updates to meaningful advances of at least five percentage points and at least 250 ms, with an immediate final 100% frame.
- Advanced one page per debounced short button release. Skipped duplicate NTP/network pages and the then-disabled SD page.
- Returned to the clock after 20 seconds without navigation; holding the button suppressed the return. Direct page commands used the same timer.
- Kept automatic clock return separate from the 60-second screensaver inactivity timer.
- Retained a fixed 15-minute weather interval after success or failure, initial querying after connection and manual updates through `clima_sync`.

### Documentation

- Consolidated the setup, command and change guides.
- Documented `name`, the 255-byte command limit, `reset_wifi`, RGB behavior and OTA/core-dump boundaries.
- Corrected the historical command catalog: LP5562 RGB was implemented on AtomS3R, while integrated SD and card-based history were unavailable there.
- Preserved earlier navigation/weather changes attributed by the source history to commit `5c56bcf`.

### Validation

- Compilation, flashing and visual validation remained pending for these revisions.

## 2026-10-17 — Initial AtomS3R migration and improvements

### Added

- Migration of ESP32-C6 gateway services to a dual-core ESP32-S3 application without changing the source project.
- Application task pinned to core 1, weather task pinned to core 0, protected weather cache, task-creation checks and single-core configuration rejection.
- Five Wi-Fi profiles in NVS, USB Serial at 115200 baud, UDP port 4210, NTP, network monitoring, weather and Prometheus.
- `comandos.md` and the initial change history.
- A parent-folder `Compilar.ps1` for the original board configuration; this script is not part of the current Waveshare sketch folder.

### Display and animation

- Corrected the initial AtomS3 Lite hardware assumption to AtomS3R after the actual board was identified.
- Enabled M5Unified/M5GFX rendering and LP5562 backlight control for the original board.
- Added compact clock, NTP, network, system, saved-Wi-Fi, monitor and forecast pages.
- Introduced a 128 × 128 RGB565 framebuffer, allocation checks and complete-frame transfers.
- Increased text sizes, adjusted console wrapping and Wi-Fi labels, and separated clock time/date/IP rows.
- Limited brightness writes to actual changes.
- Added a screensaver after 60 seconds, with smooth gaze changes, blinking and button/command exit. Initially mixed eye colors were later changed to cyan.
- Integrated the BMI270 accelerometer with a neutral pose, rotation mapping, filtering, a dead zone and eye-boundary limits. Automatic motion remained the fallback after missing/invalid readings.

### OTA and RGB

- Prioritized OTA handling before portal/rendering work and handled invitation/start in consecutive calls.
- Disabled Wi-Fi power saving for OTA availability, temporarily used 240 MHz and raised task priority during transfer.
- Paused new weather queries during OTA while allowing an active request to finish.
- Added Serial progress and final bytes, duration and KiB/s reporting; a screen progress bar followed on 2026-10-18.
- Corrected the original RGB hardware interpretation: U5 RGB connects to LP5562 U4; the green reset/download LED is separate.
- Replaced RGB stubs with direct LP5562 channel access without changing the white backlight channel.
- Restored RGB commands and effects, validated `led_rgb:R:G:B` values and retained manual-command priority over automatic network indication.

### Board and portal corrections

- Configured AtomS3R GPIO41 for the front button, 8 MB flash, 8 MB OPI PSRAM and an OTA-capable layout.
- Used the historical `AtomS3R-Gateway` identity.
- Removed the earlier WS2812/GPIO35 assumption and Adafruit NeoPixel dependency.
- Left SD unavailable on the AtomS3R, which has no integrated card reader.
- Removed the earlier SD log menu, `/logs` and `/api/logs` routes, `LogPortal.h` and SD references in portal information/footers. The Waveshare adaptation later restored a `/logs` viewer.
- Added the explicit `SharedSpi.h` include after a reported compilation error in the original migration.

### Validation

- The source history recorded a user-supplied boot log confirming the original application's core-1 execution. Later animation/display changes still required physical verification.
- No complete build, hardware validation or measured OTA speed improvement was claimed for the latest inherited revisions.
- Gyroscope and magnetometer data were not used; the original AtomS3R revision did not write microSD logs.
