# Changelog

All entries are in English and use ISO dates. Dates record development changes, not published releases. No semantic-version tags are asserted; the firmware's `version` command continues to derive its identifier from the build date and time.

Entries before 2026-11-04 describe the inherited AtomS3R project. Hardware-specific behavior in those entries is historical and does not override the current Waveshare configuration.

## 2026-11-04 — Personal watch faces, night mode, histories and motion

- Added three native vector watch faces, four accents and a touch Personalize page with verified NVS persistence.
- Added local-time night brightness with editable start/end hours and intensity; RTC fallback keeps the schedule and clocks working offline.
- Added a bounded battery-session graph, honest SOC-based autonomy and rotating five-minute SD CSV archives without requiring a card for operation.
- Expanded the network monitor with latency/RSSI trend charts, loss, average RTT and cumulative offline time; gateway reachability is labeled as LAN-only.
- Added filtered lift-to-wake and stable, mounting-relative portrait inversion; automatic rotation is deferred during active touch/console/BOOT interactions.
- Added personalizar and historico_bateria commands; extended direct page selection to 0..12 while preserving earlier IDs.
- Firmware compilation, OTA upload and physical validation remain with the device owner.

## 2026-11-04 — PCF85063 RTC fallback and verified NTP synchronization

- Added BoardRtc at 0x51 with UTC calendar storage, coherent burst reads, BCD/calendar validation, oscillator-stop checking and verified calendar writes.
- Added ClockCalendar utilities and guarded I2C burst writes without a new library dependency.
- Restored valid RTC time at startup, before network synchronization is available.
- Changed NTP validity from a plausible system date to an actual recent SNTP callback; callbacks only publish timestamps and all RTC access remains on the application task.
- Compared RTC/network UTC on confirmed updates and once per minute while authority is fresh; differences of 2 seconds or more repair the RTC. NTP refresh is 5 minutes, freshness 10 minutes, with RTC fallback immediately when disconnected.
- Added source badges to both clock views, unified command time selection and expanded ntp_status with the rtc_status alias.
- Applied timezone/DST only to display; manual time also attempts RTC persistence and is labeled explicitly.
- Invalid/missing RTC data is never accepted as a fabricated clock or proof of NTP success. Partial calendar writes remain stopped until repaired.
- Reviewed the installed Arduino-ESP32 3.3.12 SNTP API and NXP register documentation. Compilation and physical retention/reconnection tests remain with the user.

## 2026-11-04 — Gesture-only AMOLED interface and proportional typography

- Removed the bottom Next Screen button and its hit area; kept swipe navigation and wake-only taps.
- Added a compact status bar with color-coded RSSI bars, AP/offline states, battery level, page count and a pinned-clock marker.
- Added UiPainter for bundled proportional fonts, measured text fitting, vector icons, accented cards and progress meters. Included upstream font notices in UiFonts.
- Redesigned digital clock, network, system, saved profiles, SD, network monitor, forecast and OTA pages. Added real ping-history plots and memory/storage meters; unmeasured ping slots are not rendered as data.
- Expanded the analog-face and eye-animation viewports and retained the analog page no-screensaver behavior.
- Reviewed includes, removed stale button references and kept a single framebuffer flush per rendered frame. Compilation and physical visual verification remain with the user.

## 2026-11-04 — Battery telemetry and persistent analog watch face

### Added

- BatterySnapshot and BoardPower sampling for battery presence, charging/discharging state, USB supply, native fuel-gauge percentage and battery-voltage ADC readings.
- `bateria` / `battery` commands, reporting remaining charge, used capacity relative to 100%, voltage and estimated percentage-point discharge rate after a sufficient observation interval. No instantaneous current/power measurement is fabricated.
- A native WatchFace class inspired by the supplied inner-dial reference, with hour/minute/second hands and central time/date/battery/weather information.
- `relogio` and `tela:10` to select the ninth distinct display page; existing page IDs remain unchanged.

### Changed

- The analog clock stays selected without automatic return, screensaver or idle dimming. Manual brightness, page navigation and OTA remain functional.
- Updated direct page selection to accept IDs 0–10 and the footer to show nine page indicators.
- Enabled battery detection and the voltage ADC without modifying charging-current, voltage-target or fuel-gauge calibration settings.

### Validation

- Source integration reviewed; compilation, on-device gauge validity and the dial appearance remain for user validation.

## 2026-11-04 — Arduino IDE wireless upload configuration

- Explicitly enabled ArduinoOTA mDNS advertising, port 3232 and reboot after successful uploads.
- Set the OTA transfer timeout to 8 seconds.
- Added startup diagnostics for the destination partition, available image size, hostname and station IP.
- Added OTA.md with the USB-first installation flow and Arduino IDE network-port instructions. The last cached build used app3M_fat9M_16MB, with two 3 MiB application partitions.
- Actual compilation/upload validation remains with the user.

## 2026-11-04 — Full-screen UI, vertical navigation and power key

### Added

- Shared ScreenLayout geometry for the bottom navigation button and rotation-aware touch hit testing.
- Up-swipe/left-swipe next-page navigation and down-swipe/right-swipe previous-page navigation. Body taps wake without switching pages.
- Persistent bottom Next Screen button, eight page indicators, larger clock digits and adaptive rows using the available content height.
- BoardPower for AXP2101 power-key polling, checked register updates, a 1-second power-on threshold and a 6-second emergency hardware-off threshold. No additional library dependency or charge/rail-voltage changes.
- Orderly short-PWR shutdown that pauses touch/new weather work and unmounts SD before requesting PMIC power-off; startup key-event suppression and a diagnostic/recovery path if the CPU remains supplied.

### Changed

- Reserved navigation-bar space below the screensaver and retained one framebuffer transfer per animation frame.
- Disabled the temporary startup color test by default.

### Validation

- User confirmed that the V2 display now works. The new layout, gestures and PMU power-off/on still require compilation and physical testing by the user, including battery-only operation.

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
