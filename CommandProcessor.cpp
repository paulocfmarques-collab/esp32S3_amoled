#include "CommandProcessor.h"
#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "RGBLed.h"
#include "SDUtil.h"
#include "ClimaManager.h"
#include "CrashDiagnostics.h"

#include <WiFi.h>
#include <esp_system.h>

#include <Preferences.h>

CommandProcessor::CommandProcessor(DisplayUtil& display, ESP32Gateway& gateway, NTPUtil& ntp,
                                   RGBLed& led, SDUtil& sd, ClimaManager& climate)
    : display_(display), gateway_(gateway), ntp_(ntp), led_(led), sd_(sd), climate_(climate)
{
}

bool CommandProcessor::begin()
{
    sdReady_ = sd_.begin();
    Serial.println(sdReady_ ? "SD pronto" : "SD indisponivel");
    return sdReady_;
}

void CommandProcessor::saveLog(String message)
{
    if (sdReady_)
    {
        sd_.appendText("/log.txt", message);
    }
}

void CommandProcessor::answerAll(String message, bool log)
{
    Serial.print(message);
    if (!serialReply_)
        gateway_.sendMessage(message);
    if (log)
    {
        saveLog(message);
    }
}

void CommandProcessor::startBlink(uint16_t pulses, uint32_t interval, bool continuous)
{
    ledManual_ = true;
    breathActive_ = false;
    blinkActive_ = true;
    blinkOn_ = true;
    blinkContinuous_ = continuous;
    pulsesRemaining_ = pulses;
    blinkInterval_ = interval;
    lastBlinkChange_ = millis();
    led_.white();
}

void CommandProcessor::executeCommand(String command, bool fromSerial)
{
    serialReply_ = fromSerial;
    // Preserve spaces inside credentials and SD payloads. Strip transport line endings only.
    while (command.endsWith("\r") || command.endsWith("\n"))
        command.remove(command.length() - 1);
    if (command.indexOf(':') < 0)
        command.trim();
    if (command.length() == 0)
    {
        return;
    }

    int separator = command.indexOf(':');
    String name = separator < 0 ? command : command.substring(0, separator);
    name.trim();
    name.toLowerCase();
    command = name + (separator < 0 ? String("") : command.substring(separator));
    if (name.startsWith("led_") && !led_.available())
    {
        answerAll("Esta placa nao possui LED RGB integrado.\n", false);
        return;
    }
    String safeCommand = name == "wifi_add" ? "wifi_add:[credenciais ocultas]" : command;
    String commandLog = "> " + safeCommand + "\n";
    String message = "";
    Serial.print(commandLog);
    display_.println("> " + safeCommand);

    if (command == "help")
    {
        message = "COMANDOS\n"
                  "help\n"
                  "info\n"
                  "status\n"
                  "uptime\n"
                  "reason\n"
                  "crash_info\n"
                  "version\n"
                  "build\n"
                  "alive\n"
                  "reboot\n"
                  "temp\n"
                  "cpu\n"
                  "ram\n"
                  "flash\n"
                  "init\n"
                  "heap\n"
                  "heap_min\n"
                  "chip_info\n"
                  "flash_info\n"
                  "health\n"
                  "selftest\n"
                  "name\n"
                  "reset_counters\n"
                  "wifi_add:SSID:senha\n"
                  "wifi_list\n"
                  "net_monitor\n"
                  "net_history\n"
                  "tela:0..9\n"
                  "tela_next\n"
                  "rotacao:0|90|180|270\n"
                  "brilho:0..100\n"
                  "tela_on\n"
                  "tela_off\n"
                  "net_info\n"
                  "mac\n"
                  "reset_wifi\n"
                  "rssi\n"
                  "ip\n"
                  "ssid\n"
                  "channel\n"
                  "wifi_status\n"
                  "time\n"
                  "date\n"
                  "ntp_status\n"
                  "set_fuso:X\n"
                  "dst_on\n"
                  "dst_off\n"
                  "set_time:[AAAA-MM-DD HH:MM:SS]\n"
                  "led_on\n"
                  "led_off\n"
                  "led_breath\n"
                  "led_pisca:P:I\n"
                  "led_blink:I\n"
                  "led_rgb:R:G:B\n"
                  "sd_status\n"
                  "sd_list:[pasta]\n"
                  "sd_read:arquivo\n"
                  "sd_write:arquivo:texto\n"
                  "sd_append:arquivo:texto\n"
                  "sd_del:arquivo\n"
                  "sd_mkdir:pasta\n"
                  "sd_rmdir:pasta\n"
                  "sd_log\n"
                  "sd_clear_log\n"
                  "sd_test\n"
                  "clima\n"
                  "clima_age\n"
                  "clima_status\n"
                  "previsao\n"
                  "clima_sync\n";
        answerAll(message);
    }
    else if (command == "reset_counters")
        answerAll(gateway_.resetCounters()
                      ? "Contadores NVS de boot/watchdog zerados.\n"
                      : "Erro ao zerar contadores NVS.\n");
    else if (command == "name")
        answerAll(gateway_.getDeviceName() + "\n", false);
    else if (command == "wifi_list")
        answerAll(gateway_.listarSSIDs(), false);
    else if (command.startsWith("wifi_add:"))
    {
        String args = command.substring(9);
        int split = args.indexOf(':');
        String error;
        if (split < 0)
            answerAll("Uso: wifi_add:SSID:senha (senha vazia para rede aberta)\n", false);
        else
            answerAll(
                gateway_.adicionarRede(args.substring(0, split), args.substring(split + 1), error)
                    ? "Rede gravada e verificada. Aplicada na proxima reconexao.\n"
                    : "Falha: " + error + "\n",
                false);
    }
    else if (command == "net_monitor")
        answerAll(gateway_.monitorReport(), false);
    else if (command == "net_history")
        answerAll(sdReady_ ? sd_.readTail("/network_history.log", 2000) : "SD indisponivel\n",
                  false);
    else if (command == "tela_next")
    {
        display_.nextPage();
        answerAll("Proxima pagina\n", false);
    }
    else if (command.startsWith("tela:"))
    {
        String arg = command.substring(5);
        bool valid = arg.length() == 1 && arg[0] >= '0' && arg[0] <= '9';
        if (valid)
        {
            display_.selectPage(arg.toInt());
            answerAll("Pagina selecionada\n", false);
        }
        else
            answerAll("Uso: tela:0..9\n", false);
    }
    else if (name == "rotacao")
    {
        const int colon = command.indexOf(':');
        const String angle = colon < 0 ? String("") : command.substring(colon + 1);
        if (angle != "0" && angle != "90" && angle != "180" && angle != "270") {
            answerAll("Uso: rotacao:0|90|180|270\n", false);
            return;
        }
        display_.setRotation(angle.toInt() / 90);
        display_.wake();
        display_.redraw();
        answerAll("Rotacao do display: " + angle + " graus\n", false);
    }
    else if (command.startsWith("brilho:"))
    {
        String arg = command.substring(7);
        bool valid = arg.length() > 0 && arg.length() <= 3;
        for (size_t i = 0; i < arg.length(); i++)
            if (!isDigit(arg[i]))
                valid = false;
        if (valid && arg.toInt() <= 100)
        {
            display_.setBacklight(arg.toInt());
            display_.wake();
            answerAll("Brilho ajustado\n", false);
        }
        else
            answerAll("Uso: brilho:0..100\n", false);
    }
    else if (command == "tela_off")
    {
        display_.setBacklight(0);
        answerAll("Tela apagada; rede ativa\n", false);
    }
    else if (command == "tela_on")
    {
        display_.setBacklight(80);
        display_.wake();
        answerAll("Brilho AMOLED em 80%\n", false);
    }
    else if (command == "previsao")
    {
        auto weather = climate_.snapshot();
        if (!weather.previsaoValida)
            answerAll("Previsao ainda indisponivel\n", false);
        else
            answerAll("Porto Alegre " + String(weather.data) + "\nMin: " +
                          String(weather.minima, 1) + " C; Max: " + String(weather.maxima, 1) +
                          " C\nChuva: " + String(weather.chuva) + "%\nIdade: " +
                          String((millis() - weather.previsaoAtualizada) / 60000) + " min\n" +
                          (weather.ultimaFalhou ? "Ultima consulta falhou; exibindo cache\n" : ""),
                      false);
    }
    else if (command == "reboot")
    {
        answerAll("Reiniciando o sistema remotamente via UDP...\n");
        delay(500);
        ESP.restart();
    }
    else if (command == "clima_sync")
    {
        const bool scheduled = climate_.atualizar(true);
        answerAll(scheduled
                      ? "Consulta de clima solicitada em segundo plano. Use clima/previsao.\n"
                      : "Consulta nao iniciada. Verifique o diagnostico abaixo.\n");
        answerAll(climate_.status(), false);
    }

    else if (command == "info")
    {
        String dataHoraCompleta;
        ntp_.getDateTime(dataHoraCompleta, 100);
        String dataStr = "Sem Sinc.", horaStr = "--:--:--";
        if (dataHoraCompleta.length() >= 19)
        {
            dataStr = dataHoraCompleta.substring(0, 10);
            horaStr = dataHoraCompleta.substring(11, 19);
        }
        uint32_t heapLivre = ESP.getFreeHeap() / 1024;
        float flashLivre = (float)ESP.getFreeSketchSpace() / (1024.0 * 1024.0);

        message = "===== DEVICE INFO =====\n"
                  "Hostname: AMOLED-Gateway\n"
                  "Firmware: " +
                  obterVersaoAutomatica() +
                  "\n"
                  "Build: " +
                  String(__DATE__) + " " + String(__TIME__) + "\n" + "SSID: " + WiFi.SSID() + "\n" +
                  "IP: " + WiFi.localIP().toString() + "\n" + "MAC: " + WiFi.macAddress() + "\n" +
                  "RSSI: " + String(WiFi.RSSI()) + " dBm\n" + "Heap Livre: " + String(heapLivre) +
                  " KB\n" + "Flash Livre: " + String(flashLivre, 1) + " MB\n" + "Data: " + dataStr +
                  "\n" + "Hora: " + horaStr + "\n" + "Uptime: " + String(millis()) + " ms\n" +
                  "Reset: " + obterMotivoReset() + "\n" + "=======================\n";
        answerAll(message);
    }
    else if (command == "crash_info")
    {
        answerAll(CrashDiagnostics::lerArquivarEApagar(sd_, obterMotivoReset()), false);
    }
    else if (command == "reason")
    {
        message = "===== ULTIMO RESET =====\n"
                  "Motivo: " +
                  obterMotivoReset() +
                  "\n"
                  "Uptime Atual: " +
                  String(millis() / 1000) +
                  " s\n"
                  "========================\n";
        answerAll(message);
    }
    else if (command == "reset_wifi")
    {
        answerAll("Limpando configuracao Wi-Fi...\n");
        gateway_.clearConfig();
    }
    else if (command == "version")
    {
        message = "Versao Firmware: " + obterVersaoAutomatica() + "\n";
        answerAll(message);
    }
    else if (command == "build")
    {
        message = "Build:\nData: " + String(__DATE__) + "\nHora: " + String(__TIME__) + "\n";
        answerAll(message);
    }
    else if (command == "status")
    {
        String statusWifi = (WiFi.status() == WL_CONNECTED) ? "OK" : "FALHA";
        uint32_t heapKB = ESP.getFreeHeap() / 1024;
        message = "AMOLED\nWiFi: " + statusWifi +
                  "\nNTP: " + (ntp_.isSincronizado() ? "OK" : "FALHA") +
                  "\nHeap: " + String(heapKB) + " KB\n";
        answerAll(message);
    }
    else if (name == "led_rgb")
    {
        int values[3] = {};
        String args =
            command.indexOf(':') < 0 ? String("") : command.substring(command.indexOf(':') + 1);
        bool valid = true;
        for (int i = 0; i < 3; ++i)
        {
            int colon = args.indexOf(':');
            String value = colon < 0 ? args : args.substring(0, colon);
            if (value.length() == 0 || value.length() > 3 || (i < 2 ? colon < 0 : colon >= 0))
                valid = false;
            for (size_t j = 0; j < value.length(); ++j)
                if (!isDigit(value[j]))
                    valid = false;
            values[i] = value.toInt();
            if (values[i] > 255)
                valid = false;
            args = colon < 0 ? String("") : args.substring(colon + 1);
        }
        if (!valid)
        {
            answerAll("Uso: led_rgb:R:G:B (0..255)\n", false);
            return;
        }
        ledManual_ = true;
        blinkActive_ = false;
        breathActive_ = false;
        led_.setColor(values[0], values[1], values[2]);
        answerAll(led_.available() ? "Cor RGB ajustada\n" : "Falha ao atualizar RGB\n", false);
    }
    else if (command == "led_on")
    {
        ledManual_ = true;
        blinkActive_ = false;
        breathActive_ =
            false; // Desativa o modo de respiraÃ§Ã£o se outro comando de LED for recebido
        led_.white();
        answerAll("LED ligado\n");
    }
    else if (command == "led_off")
    {
        ledManual_ = true;
        blinkActive_ = false;
        breathActive_ =
            false; // Desativa o modo de respiraÃ§Ã£o se outro comando de LED for recebido
        led_.off();
        answerAll("LED desligado\n");
    }
    else if (command.startsWith("led_pisca"))
    {
        uint16_t pulses = 10;
        uint32_t interval = 250;
        int firstColon = command.indexOf(':');
        int secondColon = command.indexOf(':', firstColon + 1);

        if (firstColon >= 0 && secondColon > firstColon)
        {
            int requestedPulses = command.substring(firstColon + 1, secondColon).toInt();
            long requestedInterval = command.substring(secondColon + 1).toInt();
            if (requestedPulses < 1 || requestedPulses > 100 || requestedInterval < 1 ||
                requestedInterval > 5000)
            {
                answerAll("Parametros LED_PISCA invalidos\n");
                return;
            }
            pulses = static_cast<uint16_t>(requestedPulses);
            interval = static_cast<uint32_t>(requestedInterval);
        }

        startBlink(pulses, interval, false);
        answerAll("LED piscando " + String(pulses) + " vezes\n");
    }
    else if (command.startsWith("led_blink"))
    {
        uint32_t interval = 250;
        int separator = command.indexOf(':');
        if (separator >= 0)
        {
            long requestedInterval = command.substring(separator + 1).toInt();
            if (requestedInterval < 50 || requestedInterval > 60000)
            {
                answerAll("Intervalo LED_BLINK invalido (50 a 60000 ms)\n");
                return;
            }
            interval = static_cast<uint32_t>(requestedInterval);
        }

        startBlink(0, interval, true);
        answerAll("Blink iniciado (" + String(interval) + " ms)\n");
    }
    else if (command == "dst_on")
    {
        Preferences prefs;
        prefs.begin("ntp_cfg", true);
        int currentFuso = prefs.getInt("fuso", -3);
        prefs.end();

        ntp_.atualizarConfiguracao(currentFuso, true);
        answerAll("HorÃ¡rio de VerÃ£o ativado (DST ON)\n");
    }
    else if (command == "dst_off")
    {
        Preferences prefs;
        prefs.begin("ntp_cfg", true);
        int currentFuso = prefs.getInt("fuso", -3);
        prefs.end();

        ntp_.atualizarConfiguracao(currentFuso, false);
        answerAll("HorÃ¡rio de VerÃ£o desativado (DST OFF)\n");
    }
    else if (command.startsWith("set_fuso:"))
    {
        int separator = command.indexOf(':');
        if (separator >= 0)
        {
            int novoFuso = command.substring(separator + 1).toInt();
            if (novoFuso < -12 || novoFuso > 14)
            {
                answerAll("Erro: Fuso invÃ¡lido. Escolha de -12 a +14.\n");
                return;
            }

            Preferences prefs;
            prefs.begin("ntp_cfg", true);
            bool currentDst = prefs.getBool("dst");
            prefs.end();

            ntp_.atualizarConfiguracao(novoFuso, currentDst);
            answerAll("Novo fuso configurado: " + String(novoFuso) + "\n");
        }
    }
    else if (command.startsWith("set_time:"))
    {
        String v = command.substring(9);
        v.trim();
        int ano, mes, dia, hora, min, seg;
        if (v.length() != 19 ||
            sscanf(v.c_str(), "%d-%d-%d %d:%d:%d", &ano, &mes, &dia, &hora, &min, &seg) != 6 ||
            !ntp_.ajustarDataHora(ano, mes, dia, hora, min, seg))
        {
            answerAll("Erro: use set_time:AAAA-MM-DD HH:MM:SS\n");
        }
        else
        {
            answerAll("Data/hora ajustada: " + v + "\n");
        }
    }
    else if (command == "temp")
    {
        message = "CPU Temp: " + String(temperatureRead()) + " C";
        answerAll(message);
    }
    else if (command == "cpu")
    {
        message = "CPU:\nModelo: " + String(ESP.getChipModel()) +
                  "\nRevisao: " + String(ESP.getChipRevision()) +
                  "\nNucleos: " + String(ESP.getChipCores()) +
                  "\nCPU: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
        answerAll(message);
    }
    else if (command == "ram")
    {
        message = "RAM:\nHeap Total: " + String(ESP.getHeapSize() / 1024.0) + " KB\n" +
                  "Heap Livre: " + String(ESP.getFreeHeap() / 1024.0) + " KB\n" +
                  "Menor Bloco Livre: " + String(ESP.getMinFreeHeap() / 1024.0) + " KB\n" +
                  "Maior Bloco Livre: " + String(ESP.getMaxAllocHeap() / 1024.0) + " KB\n" +
                  "RAM Utilizada: " +
                  String(100.0 * (ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize()) +
                  " %\n";
        answerAll(message);
    }
    else if (command == "flash")
    {
        message = "FLASH:\nTamanho: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB\n" +
                  "Velocidade: " + String(ESP.getFlashChipSpeed() / 1000 / 1000) + " MHz\n" +
                  "Flash Mode: " + String(ESP.getFlashChipMode()) + "\n" +
                  "Sketch Size: " + String(ESP.getSketchSize() / 1024 / 1024) + " MB\n" +
                  "Free Sketch Space: " + String(ESP.getFreeSketchSpace() / 1024 / 1024) + " MB\n" +
                  "Flash livre: " +
                  String(100.0 * (ESP.getFlashChipSize() - ESP.getFreeSketchSpace()) /
                         ESP.getFlashChipSize()) +
                  " %\n";
        answerAll(message);
    }
    else if (command == "init")
    {
        answerAll("Motivo reset: " + obterMotivoReset() + "\n");
    }
    else if (command == "uptime")
    {
        uint64_t totalSegundos = millis() / 1000ULL;

        uint32_t dias = totalSegundos / 86400ULL;
        totalSegundos %= 86400ULL;

        uint8_t horas = totalSegundos / 3600ULL;
        totalSegundos %= 3600ULL;

        uint8_t minutos = totalSegundos / 60ULL;
        uint8_t segundos = totalSegundos % 60ULL;

        char buffer[100];

        snprintf(buffer, sizeof(buffer),
                 "===== UPTIME =====\n"
                 "%lu dias\n"
                 "%02u:%02u:%02u\n"
                 "==================\n",
                 (unsigned long)dias, horas, minutos, segundos);

        answerAll(String(buffer));
    }
    else if (command == "ntp_status")
    {
        bool sincronizado = ntp_.isSincronizado();

        message = "===== NTP STATUS =====\n"
                  "Estado: ";

        message += sincronizado ? "SINCRONIZADO\n" : "SEM SINCRONISMO\n";

        if (sincronizado)
        {
            String dataHora;
            ntp_.getDateTime(dataHora, 100);

            message += "Data/Hora: " + dataHora + "\n";
        }

        message += "======================\n";

        answerAll(message);
    }
    else if (command == "mac")
    {
        answerAll("MAC: " + WiFi.macAddress() + "\n");
    }
    else if (command == "net_info")
    {
        message = "NET:\nSSID: " + WiFi.SSID() + "\n" + "IP: " + WiFi.localIP().toString() + "\n" +
                  "Gateway: " + WiFi.gatewayIP().toString() + "\n" +
                  "Subnet: " + WiFi.subnetMask().toString() + "\n" +
                  "DNS1: " + WiFi.dnsIP(0).toString() + "\n" + "DNS2: " + WiFi.dnsIP(1).toString() +
                  "\n" + "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
        answerAll(message);
    }
    else if (command == "time")
    {
        answerAll("Hora:\n" + ntp_.getSomenteHora() + "\n");
    }
    else if (command == "date")
    {
        String dataHoraCompleta;
        ntp_.getDateTime(dataHoraCompleta, 100);
        String data =
            (dataHoraCompleta.length() >= 10) ? dataHoraCompleta.substring(0, 10) : "Erro NTP";
        answerAll("Data:\n" + data + "\n");
    }
    else if (command == "led_breath")
    {
        ledManual_ = true;
        blinkActive_ = false;
        breathActive_ = true; // Ativa o modo de respiraÃ§Ã£o do LED
        answerAll("Modo do LED alterado para Pulsar (Breathing)\n");
    }
    else if (command == "alive")
    {
        answerAll("alive\n");
    }
    else if (command == "heap")
    {
        uint32_t heapLivre = ESP.getFreeHeap();

        message = "===== MEMORIA RAM =====\n"
                  "Heap livre: " +
                  String(heapLivre) +
                  " bytes\n"
                  "Heap livre: " +
                  String(heapLivre / 1024.0, 1) +
                  " KB\n"
                  "=======================\n";

        answerAll(message);
    }
    else if (command == "heap_min")
    {
        uint32_t heapMinimo = ESP.getMinFreeHeap();

        message = "===== HEAP MINIMO =====\n"
                  "Minimo livre: " +
                  String(heapMinimo) +
                  " bytes\n"
                  "Minimo livre: " +
                  String(heapMinimo / 1024.0, 1) +
                  " KB\n"
                  "=======================\n";

        answerAll(message);
    }

    else if (command == "chip_info")
    {
        message = "===== CHIP INFO =====\n"
                  "Modelo: " +
                  String(ESP.getChipModel()) +
                  "\n"
                  "Revisao: " +
                  String(ESP.getChipRevision()) +
                  "\n"
                  "Cores: " +
                  String(ESP.getChipCores()) +
                  "\n"
                  "CPU: " +
                  String(getCpuFrequencyMhz()) +
                  " MHz\n"
                  "SDK: " +
                  String(ESP.getSdkVersion()) +
                  "\n"
                  "=====================\n";

        answerAll(message);
    }
    else if (command == "flash_info")
    {
        uint32_t flashSize = ESP.getFlashChipSize();
        uint32_t sketchSize = ESP.getSketchSize();
        uint32_t sketchLivre = ESP.getFreeSketchSpace();

        message = "===== FLASH INFO =====\n"
                  "Flash total: " +
                  String(flashSize / (1024.0 * 1024.0), 2) +
                  " MB\n"
                  "Firmware: " +
                  String(sketchSize / 1024.0, 1) +
                  " KB\n"
                  "OTA livre: " +
                  String(sketchLivre / 1024.0, 1) +
                  " KB\n"
                  "======================\n";

        answerAll(message);
    }
    else if (command == "rssi" || command == "ip" || command == "ssid" || command == "channel" ||
             command == "wifi_status")
    {
        if (WiFi.status() != WL_CONNECTED)
            message = "WiFi desconectado.\n";
        else if (command == "rssi")
            message = "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
        else if (command == "ip")
            message = "IP: " + WiFi.localIP().toString() + "\n";
        else if (command == "ssid")
            message = "SSID: " + WiFi.SSID() + "\n";
        else if (command == "channel")
            message = "Canal: " + String(WiFi.channel()) + "\n";
        else
            message = "WiFi conectado\nSSID: " + WiFi.SSID() +
                      "\nIP: " + WiFi.localIP().toString() + "\nRSSI: " + String(WiFi.RSSI()) +
                      " dBm\n";
        answerAll(message);
    }
    else if (command == "clima")
    {
        message = "===== CLIMA =====\n"
                  "Temperatura: " +
                  String(climate_.snapshot().temperatura, 1) +
                  " C\n"
                  "Condicao: " +
                  climate_.obterTextoCondicao() +
                  "\n"
                  "Codigo WMO: " +
                  String(climate_.snapshot().codigoCondicao) +
                  "\n"
                  "=================\n";

        answerAll(message);
    }
    else if (command == "clima_status")
    {
        answerAll(climate_.status(), false);
    }
    else if (command == "clima_age")
    {
        if (climate_.snapshot().ultimaAtualizacao == 0)
        {
            answerAll("Clima ainda nao foi atualizado.\n");
        }
        else
        {
            uint32_t segundos = (millis() - climate_.snapshot().ultimaAtualizacao) / 1000UL;

            uint32_t minutos = segundos / 60;
            segundos %= 60;

            message = "===== CLIMA AGE =====\n"
                      "Ultima atualizacao:\n" +
                      String(minutos) + " min " + String(segundos) +
                      " s atras\n"
                      "=====================\n";

            answerAll(message);
        }
    }
    else if (command.startsWith("sd_"))
    {
        executarSd(command);
    }
    else if (command == "health")
    {
        executarHealth();
    }
    else if (command == "selftest")
    {
        executarSelfTest();
    }

    else
    {
        answerAll("Comando invalido\n");
    }
}

void CommandProcessor::executarSd(const String& command)
{
    const size_t maxBytes = 1000;
    String name = command;
    String arg = "";
    int sep = command.indexOf(":");
    if (sep >= 0)
    {
        name = command.substring(0, sep);
        arg = command.substring(sep + 1);
    }

    if (!sdReady_)
    {
        sdReady_ = sd_.begin();
        if (!sdReady_)
        {
            answerAll("SD indisponivel\n", false);
            return;
        }
    }

    if (name == "sd_status")
    {
        const uint64_t mib = 1024ULL * 1024ULL;
        uint64_t total = sd_.getCardSizeBytes();
        uint64_t used = sd_.getUsedBytes();
        answerAll("SD: pronto\nTipo: " + sd_.getCardType() +
                      "\nTotal: " + String((unsigned long)(total / mib)) + " MiB" +
                      "\nUsado: " + String((unsigned long)(used / mib)) + " MiB" + "\nLivre: " +
                      String((unsigned long)((total > used ? total - used : 0) / mib)) + " MiB\n",
                  false);
        return;
    }
    if (name == "sd_test")
    {
        answerAll(sd_.test() ? "SD teste: OK\n" : "SD teste: FALHOU\n", false);
        return;
    }
    if (name == "sd_log")
    {
        String log = sd_.readTail("/log.txt", maxBytes);
        answerAll(log.length() ? log : String("Log vazio\n"), false);
        return;
    }
    if (name == "sd_clear_log")
    {
        answerAll(sd_.removeFile("/log.txt") ? "Log apagado\n" : "Log inexistente\n", false);
        return;
    }

    String text = "";
    if (name == "sd_write" || name == "sd_append")
    {
        int sep2 = arg.indexOf(":");
        if (sep2 < 0)
        {
            answerAll("Uso: " + name + ":/arquivo:texto\n", false);
            return;
        }
        text = arg.substring(sep2 + 1);
        arg = arg.substring(0, sep2);
    }

    if (name == "sd_list" && arg.length() == 0)
    {
        arg = "/";
    }
    if (arg.length() == 0 || arg[0] != 0x2F || arg.indexOf("..") >= 0)
    {
        answerAll("Caminho invalido (use /caminho)\n", false);
        return;
    }

    if (name == "sd_list")
    {
        String list = sd_.listFiles(arg);
        if (list.length() > maxBytes)
            list = list.substring(0, maxBytes) + "\n...\n";
        answerAll(list.length() ? list : String("(vazio)\n"), false);
    }
    else if (name == "sd_read")
    {
        if (!sd_.exists(arg))
        {
            answerAll("Arquivo nao encontrado\n", false);
            return;
        }
        String content = sd_.readTail(arg, maxBytes);
        if (sd_.getFileSize(arg) > maxBytes)
            content = "[ultimos " + String((unsigned)maxBytes) + " bytes]\n" + content;
        answerAll(content.length() ? content : String("(arquivo vazio)\n"), false);
    }
    else if (name == "sd_write")
    {
        answerAll(sd_.writeText(arg, text) ? "Gravado\n" : "Falha ao gravar\n", false);
    }
    else if (name == "sd_append")
    {
        answerAll(sd_.appendText(arg, text + "\n") ? "Adicionado\n" : "Falha ao gravar\n", false);
    }
    else if (name == "sd_del")
    {
        answerAll(sd_.removeFile(arg) ? "Apagado\n" : "Falha ao apagar\n", false);
    }
    else if (name == "sd_mkdir")
    {
        answerAll(sd_.createDir(arg) ? "Pasta criada\n" : "Falha ao criar pasta\n", false);
    }
    else if (name == "sd_rmdir")
    {
        answerAll(sd_.removeDir(arg) ? "Pasta removida\n" : "Falha (pasta nao vazia?)\n", false);
    }
    else
    {
        answerAll("Comando SD invalido\n", false);
    }
}

String CommandProcessor::obterMotivoReset()
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason)
    {
    case ESP_RST_UNKNOWN:
        return "DESCONHECIDO";
    case ESP_RST_POWERON:
        return "POWER-ON (Tomada/VCC)";
    case ESP_RST_EXT:
        return "PINO RESET (Botao EN)";
    case ESP_RST_SW:
        return "SOFTWARE / OUTROS";
    case ESP_RST_PANIC:
        return "CRASH / PANIC (Exception)";
    case ESP_RST_INT_WDT:
        return "WATCHDOG INTERNO (Core Travado)";
    case ESP_RST_TASK_WDT:
        return "TASK WATCHDOG (Loop Travado)";
    case ESP_RST_WDT:
        return "OUTROS WATCHDOGS";
    case ESP_RST_DEEPSLEEP:
        return "ACORDOU DO DEEP SLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT (Queda de Tensao)";
    case ESP_RST_SDIO:
        return "RESET VIA SDIO";
    default:
        return "CODIGO NAO MAPEADO";
    }
}

void CommandProcessor::update()
{
    if (breathActive_)
    {
        led_.breathingTask(0, 255, 255); // Roda a tarefa nÃ£o bloqueante do LED
    }

    // 2. Manutencao do Blink do LED (Mantenha o seu codigo original abaixo)
    if (!blinkActive_ || millis() - lastBlinkChange_ < blinkInterval_)
    {
        return;
    }

    lastBlinkChange_ = millis();
    blinkOn_ = !blinkOn_;
    if (blinkOn_)
    {
        led_.white();
    }
    else
    {
        led_.off();
        if (!blinkContinuous_ && pulsesRemaining_ > 0 && --pulsesRemaining_ == 0)
        {
            blinkActive_ = false;
        }
    }
}

void CommandProcessor::executarHealth()
{
    int totalTestes = 0;
    int testesOK = 0;

    String resultado;

    resultado = "\n===== SYSTEM HEALTH =====\n\n";

    // =========================================================
    // WI-FI
    // =========================================================

    totalTestes++;

    bool wifiOK = (WiFi.status() == WL_CONNECTED);

    if (wifiOK)
    {
        testesOK++;

        int32_t rssi = WiFi.RSSI();

        resultado += "WiFi....... OK   ";
        resultado += String(rssi);
        resultado += " dBm\n";
    }
    else
    {
        resultado += "WiFi....... FAIL\n";
    }

    // =========================================================
    // NTP
    // =========================================================

    totalTestes++;

    bool ntpOK = ntp_.isSincronizado();

    if (ntpOK)
    {
        testesOK++;
        resultado += "NTP........ OK\n";
    }
    else
    {
        resultado += "NTP........ FAIL\n";
    }

    // =========================================================
    // MEMORIA RAM
    // =========================================================

    totalTestes++;

    uint32_t heapLivre = ESP.getFreeHeap();
    uint32_t heapMinimo = ESP.getMinFreeHeap();

    bool ramOK = heapLivre >= (40 * 1024);

    if (ramOK)
    {
        testesOK++;
        resultado += "RAM........ OK   ";
    }
    else
    {
        resultado += "RAM........ LOW  ";
    }

    resultado += String(heapLivre / 1024);
    resultado += " KB\n";

    // =========================================================
    // CLIMA
    // =========================================================

    totalTestes++;

    bool climaOK = false;

    if (climate_.snapshot().ultimaAtualizacao != 0)
    {
        uint32_t idadeClima = millis() - climate_.snapshot().ultimaAtualizacao;

        // Consideramos os dados validos por ate 30 minutos.
        climaOK = idadeClima <= 1800000UL;
    }

    if (climaOK)
    {
        testesOK++;

        resultado += "Clima...... OK   ";
        resultado += String(climate_.snapshot().temperatura, 1);
        resultado += " C\n";
    }
    else
    {
        resultado += "Clima...... STALE\n";
    }

    // =========================================================
    // CALCULO DO HEALTH
    // =========================================================

    int health = 0;

    if (totalTestes > 0)
    {
        health = (testesOK * 100) / totalTestes;
    }

    // =========================================================
    // STATUS GERAL
    // =========================================================

    String statusGeral;

    if (health == 100)
    {
        statusGeral = "EXCELENTE";
    }
    else if (health >= 80)
    {
        statusGeral = "BOM";
    }
    else if (health >= 60)
    {
        statusGeral = "ATENCAO";
    }
    else
    {
        statusGeral = "CRITICO";
    }

    // =========================================================
    // RESULTADO
    // =========================================================

    resultado += "\n-------------------------\n";

    resultado += "Health..... ";
    resultado += String(health);
    resultado += "%\n";

    resultado += "Status..... ";
    resultado += statusGeral;
    resultado += "\n";

    resultado += "Heap min... ";
    resultado += String(heapMinimo / 1024);
    resultado += " KB\n";

    resultado += "\n=========================\n";

    answerAll(resultado);
}

void CommandProcessor::executarSelfTest()
{
    int totalTestes = 0;
    int testesOK = 0;

    String resultado;

    resultado.reserve(700);

    resultado = "\n===== SELF TEST =====\n\n";

    // =========================================================
    // 1. WI-FI
    // =========================================================

    totalTestes++;

    bool wifiOK = (WiFi.status() == WL_CONNECTED);

    if (wifiOK)
    {
        testesOK++;

        resultado += "WiFi.......... PASS  ";
        resultado += String(WiFi.RSSI());
        resultado += " dBm\n";
    }
    else
    {
        resultado += "WiFi.......... FAIL\n";
    }

    // =========================================================
    // 2. NTP
    // =========================================================

    totalTestes++;

    bool ntpOK = ntp_.isSincronizado();

    if (ntpOK)
    {
        testesOK++;
        resultado += "NTP........... PASS\n";
    }
    else
    {
        resultado += "NTP........... FAIL\n";
    }

    // =========================================================
    // 5. RAM
    // =========================================================

    totalTestes++;

    uint32_t heapLivre = ESP.getFreeHeap();
    uint32_t heapMinimo = ESP.getMinFreeHeap();

    bool ramOK = heapLivre >= (40 * 1024);

    if (ramOK)
    {
        testesOK++;

        resultado += "RAM........... PASS  ";
        resultado += String(heapLivre / 1024);
        resultado += " KB\n";
    }
    else
    {
        resultado += "RAM........... FAIL  ";
        resultado += String(heapLivre / 1024);
        resultado += " KB\n";
    }

    // =========================================================
    // 6. CLIMA
    // =========================================================

    totalTestes++;

    bool climaOK = false;

    if (climate_.snapshot().ultimaAtualizacao != 0)
    {
        uint32_t idade = millis() - climate_.snapshot().ultimaAtualizacao;

        climaOK = idade <= 1800000UL;
    }

    if (climaOK)
    {
        testesOK++;

        resultado += "Clima......... PASS  ";
        resultado += String(climate_.snapshot().temperatura, 1);
        resultado += " C\n";
    }
    else
    {
        resultado += "Clima......... FAIL\n";
    }

    // =========================================================
    // 7. CPU
    // =========================================================

    totalTestes++;

    uint32_t cpuMHz = getCpuFrequencyMhz();

    bool cpuOK = cpuMHz > 0;

    if (cpuOK)
    {
        testesOK++;

        resultado += "CPU........... PASS  ";
        resultado += String(cpuMHz);
        resultado += " MHz\n";
    }
    else
    {
        resultado += "CPU........... FAIL\n";
    }

    // =========================================================
    // RESULTADO FINAL
    // =========================================================

    resultado += "\n-------------------------\n";

    if (testesOK == totalTestes)
    {
        resultado += "RESULTADO: PASS\n";
    }
    else
    {
        resultado += "RESULTADO: FAIL\n";
    }

    resultado += String(testesOK);
    resultado += " / ";
    resultado += String(totalTestes);
    resultado += " testes OK\n";

    resultado += "Heap min: ";
    resultado += String(heapMinimo / 1024);
    resultado += " KB\n";

    resultado += "=========================\n";

    answerAll(resultado);
}
