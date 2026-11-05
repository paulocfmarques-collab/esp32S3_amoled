# Upload Firmware from Arduino IDE over Wi-Fi

The project uses **ArduinoOTA**, with mDNS discovery enabled for Arduino IDE, hostname **AMOLED-Gateway**, upload invitation port **3232** and automatic restart after a successful update. Progress appears on the display and in Serial output.

## First installation

1. Open ESP32S3.ino in Arduino IDE.
2. Keep ESP32S3 Dev Module, 16 MB flash and OPI PSRAM selected.
3. Select an OTA-capable partition scheme. The previously used app3M_fat9M_16MB scheme provides app0 and app1, each 3 MiB, and is suitable when the firmware fits that size. Do not use Huge APP / No OTA.
4. Upload the current sketch over USB. This installs the running firmware that will receive future wireless uploads.
5. Configure Wi-Fi through the device portal. The ESP32 must connect to a Wi-Fi network; the current OTA integration starts when the station connection is established.

Changing the partition layout requires a USB upload. An ordinary ArduinoOTA application upload does not install a new partition table.

## Subsequent wireless uploads

1. Connect the computer and ESP32 to the same reachable local network.
2. Keep the device powered and wait for Wi-Fi to connect.
3. In Arduino IDE, open **Tools → Port** and select the network entry **AMOLED-Gateway at DEVICE_IP**. Depending on IDE language/version, the wording can vary.
4. Keep the same board, flash, PSRAM and compatible partition settings.
5. Click **Upload**. Do not select the USB COM port for a wireless upload.
6. Watch the on-device percentage; after a successful upload, the ESP32 restarts into the new application.

OTA does not require a USB data connection after initial installation. It does require continuous power. No OTA password is configured in this project.

## Serial diagnostics

After Wi-Fi connects, startup logs report:

~~~text
[OTA] Destino: app1, limite 3145728 bytes
[OTA] IDE Arduino: AMOLED-Gateway em DEVICE_IP, porta 3232; anuncio mDNS habilitado
~~~

The target alternates between app0 and app1. The actual IP and partition size depend on the running configuration. The message records service initialization and the discovery request; a successful upload must still be verified from the IDE and device.

## If the network port does not appear

- Confirm that the ESP32 is connected to the router, not only running its setup AP.
- Confirm that the computer can reach the displayed device IP.
- Reopen the IDE port menu or restart Arduino IDE after the device connects.
- Avoid a guest network or access-point client isolation, which can prevent devices from reaching one another.
- On Windows, allow Arduino IDE and the ESP32 upload helper/Python through the firewall on the private network. Discovery uses mDNS (UDP 5353); ArduinoOTA receives invitations on UDP 3232 and connects back to the uploader over TCP for the firmware transfer.
- If the IDE finds the device but upload fails, retain the complete IDE error and the device Serial output. Check destination partition size and firmware size before changing settings.

The USB Serial Monitor is not automatically transported over the OTA network port. Use USB to capture boot diagnostics when needed.

Compilation and the first actual Arduino IDE wireless upload remain to be validated by the user.

## References

- [Project overview](README.md)
- [Setup guide](LEIA-ME.md)
- [Espressif ArduinoOTA source](https://github.com/espressif/arduino-esp32/tree/master/libraries/ArduinoOTA)
- [Espressif partition-table guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/partition_table.html)
