#include "MetricsService.h"
#include "BoardConfig.h"
#include "CommandProcessor.h"
#include "ClimaManager.h"
#include "SDUtil.h"
#include "PortalUi.h"
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_ota_ops.h>

bool MetricsService::validHost(const String& host)
{
    if (host.isEmpty() || host.length() > 253) return false;
    unsigned length = 0;
    char previous = 0;
    for (size_t i = 0; i < host.length(); ++i)
    {
        const char c = host[i];
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9');
        if (c == '.')
        {
            if (!length || previous == '-') return false;
            length = 0;
        }
        else if ((!alnum && c != '-') || (!length && c == '-') || ++length > 63) return false;
        previous = c;
    }
    return length && previous != '-';
}

bool MetricsService::parseNumber(const String& text, uint16_t minimum, uint16_t maximum, uint16_t& value)
{
    if (text.isEmpty() || text.length() > 5) return false;
    uint32_t parsed = 0;
    for (size_t i = 0; i < text.length(); ++i)
    {
        if (text[i] < '0' || text[i] > '9') return false;
        parsed = parsed * 10 + text[i] - '0';
    }
    if (parsed < minimum || parsed > maximum) return false;
    value = static_cast<uint16_t>(parsed);
    return true;
}

String MetricsService::escapeLabel(String value)
{
    value.replace("\\", "\\\\"); value.replace("\"", "\\\"");
    value.replace("\n", "\\n"); value.replace("\r", "\\n");
    return value;
}

String MetricsService::escapeHtml(String value)
{
    value.replace("&", "&amp;"); value.replace("<", "&lt;"); value.replace(">", "&gt;");
    value.replace("\"", "&quot;"); value.replace("'", "&#39;");
    return value;
}

void MetricsService::load()
{
    settings.version = 1; settings.port = 9120; settings.interval = 15;
    Preferences prefs;
    if (prefs.begin("telemetry", false))
    {
        Settings saved{};
        const size_t length = prefs.getBytesLength("settings");
        if (length == sizeof(saved) && prefs.getBytes("settings", &saved, sizeof(saved)) == sizeof(saved) &&
            saved.version == 1 && memchr(saved.host, 0, sizeof(saved.host)) &&
            validHost(String(saved.host)) && saved.port && saved.interval >= 5 && saved.interval <= 3600)
            settings = saved;
        else if (length) Serial.println("[METRICS] Configuracao NVS invalida; usando padroes.");
        const size_t bootLength = prefs.getBytesLength("boots");
        bool valid = bootLength == 0;
        if (bootLength == sizeof(boots))
            valid = prefs.getBytes("boots", &boots, sizeof(boots)) == sizeof(boots) && boots.version == 1;
        if (valid)
        {
            boots.version = 1;
            if (boots.boots < UINT32_MAX) ++boots.boots;
            const esp_reset_reason_t reason = esp_reset_reason();
            if ((reason == ESP_RST_WDT || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT) &&
                boots.watchdogs < UINT32_MAX) ++boots.watchdogs;
            countersOk = prefs.putBytes("boots", &boots, sizeof(boots)) == sizeof(boots);
        }
        prefs.end();
    }
    else Serial.println("[METRICS] NVS indisponivel; usando porta 9120 e intervalo 15s.");
    if (!countersOk) Serial.println("[METRICS] Contadores persistentes indisponiveis.");
}

bool MetricsService::resetCounters()
{
    Preferences prefs;
    if (!prefs.begin("telemetry", false))
    {
        countersOk = false;
        return false;
    }
    BootRecord zero{1, 0, 0};
    const bool ok = prefs.putBytes("boots", &zero, sizeof(zero)) == sizeof(zero);
    prefs.end();
    countersOk = ok;
    if (ok)
    {
        boots = zero;
    }
    return ok;
}

void MetricsService::begin(WebServer& web, SDUtil& sd)
{
    if (initialized) return;
    panel = &web; storage = &sd;
    load();
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    char id[18];
    snprintf(id, sizeof(id), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    device = id;
    imageBytes = ESP.getSketchSize();
    const esp_partition_t* partition = esp_ota_get_running_partition();
    partitionBytes = partition ? partition->size : 0;
    WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t) { disconnects.fetch_add(1); },
                 ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#if !configUSE_TRACE_FACILITY || !configGENERATE_RUN_TIME_STATS
    Serial.println("[METRICS] CPU indisponivel: SDK sem estatisticas de runtime.");
#endif
    auto record = [this](WebServer& server, Middleware::Callback next)
    {
        const int64_t startUs = esp_timer_get_time();
        const bool handled = next();
        ++requests;
        if (server.responseCode() >= 400 || server.responseCode() <= 0) ++errors;
        responseMs = (esp_timer_get_time() - startUs) / 1000.0;
        return handled;
    };
    web.addMiddleware(record);
    exporter.addMiddleware(record);
    exporter.on("/metrics", HTTP_GET, [this]() { sendMetrics(exporter); });
    web.on("/metrics", HTTP_GET, [this]()
    {
        if (activePort == 80) sendMetrics(*panel);
        else panel->send(404, "text/plain; charset=utf-8", "Use a porta de metricas configurada.");
    });
    web.on("/prometheus", HTTP_GET, [this]() { show(); });
    web.on("/prometheus", HTTP_POST, [this]() { handleSave(); });
    web.on("/prometheus.yml", HTTP_GET, [this]()
    {
        panel->sendHeader("Cache-Control", "no-store");
        panel->sendHeader("Content-Disposition", "attachment; filename=prometheus.yml");
        panel->send(200, "text/yaml; charset=utf-8", yaml());
    });
    start();
    initialized = true;
}

void MetricsService::start()
{
    exporter.stop();
    activePort = settings.port;
    if (activePort != 80) exporter.begin(activePort);
    pending = false;
    Serial.printf("[METRICS] Porta %u; painel web na porta 80.\n", activePort);
}

void MetricsService::process()
{
    if (!initialized || !storage) return;
    if (pending && millis() - savedAt >= 1000UL) start();
    sampleCpu();
    storage->processMetrics();
    if (activePort != 80) exporter.handleClient();
}

bool MetricsService::save(const String& host, uint16_t port, uint16_t interval)
{
    Settings next{};
    next.version = 1; next.port = port; next.interval = interval;
    host.toCharArray(next.host, sizeof(next.host));
    Preferences prefs;
    if (!prefs.begin("telemetry", false)) return false;
    const bool ok = prefs.putBytes("settings", &next, sizeof(next)) == sizeof(next);
    prefs.end();
    if (ok) settings = next;
    return ok;
}

void MetricsService::handleSave()
{
    String host = panel->arg("host"); host.trim();
    uint16_t port, interval;
    if (!validHost(host) || !parseNumber(panel->arg("port"), 1, 65535, port) ||
        !parseNumber(panel->arg("interval"), 5, 3600, interval))
    {
        show(400, "Use IPv4/DNS sem protocolo, porta 1..65535 e intervalo 5..3600 segundos.");
        return;
    }
    if (!save(host, port, interval))
    {
        Serial.println("[METRICS] Falha ao salvar preferencias.");
        show(503, "Falha na NVS. Configuracao ativa preservada.");
        return;
    }
    pending = port != activePort; savedAt = millis();
    show(200, "Preferencias salvas. Painel na porta 80. Aplique o YAML no Prometheus.");
}

String MetricsService::yaml() const
{
    const String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    return "scrape_configs:\n  - job_name: 'esp32_c6'\n    scrape_interval: " + String(settings.interval) +
           "s\n    metrics_path: /metrics\n    static_configs:\n      - targets: ['" +
           ip + ":" + String(settings.port) + "']\n";
}

void MetricsService::show(int status, const String& notice)
{
    String h = portalStart("Prometheus", false, false, false, true);
    h += F("<style>input[type=number]{width:100%;background:#0d1728;border:1px solid #34465f;"
           "color:var(--ink);padding:11px 12px;border-radius:9px}</style>");
    h += F("<h1>Prometheus</h1><p class='muted'>Coleta HTTP de metricas reais.</p>");
    if (notice.length()) h += "<div class='notice' role='status'>" + escapeHtml(notice) + "</div>";
    h += F("<section class='card'><h2>Preferencias persistentes</h2><form action='/prometheus' method='post'>"
           "<label class='field' for='host'>Endereco do servidor Prometheus (IPv4 ou DNS)</label>"
           "<input id='host' name='host' type='text' maxlength='253' required value='");
    h += escapeHtml(String(settings.host));
    h += "'><label class='field' for='port'>Porta de metricas do ESP32</label>"
         "<input id='port' name='port' type='number' min='1' max='65535' required value='" + String(settings.port);
    h += "'><label class='field' for='interval'>Intervalo sugerido (segundos)</label>"
         "<input id='interval' name='interval' type='number' min='5' max='3600' required value='" + String(settings.interval);
    h += F("'><div class='actions'><button>Salvar nas preferencias</button></div></form>"
           "<p>Servidor sem protocolo, caminho ou porta. O endereco salvo e uma referencia: nao configura "
           "envio ativo nem restringe clientes. A porta acima pertence ao endpoint /metrics deste ESP32.</p>"
           "<p>Painel, logs e configuracao permanecem na porta 80. Mudancas de porta sao aplicadas em cerca "
           "de um segundo, sem reboot. A porta 80 pode ser compartilhada.</p></section>"
           "<section class='card section'><h2>Configuracao do servidor</h2>"
           "<p>Adicione o job ao Prometheus e recarregue a configuracao. Salvar aqui nao altera o servidor.</p>"
           "<pre style='white-space:pre-wrap;overflow-wrap:anywhere'>");
    h += escapeHtml(yaml());
    const String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    h += "</pre><a class='button' href='/prometheus.yml'>Baixar YAML</a> <a class='button secondary' href='http://" +
         escapeHtml(ip) + ":" + String(settings.port) + "/metrics' target='_blank' rel='noopener'>Ver metricas</a></section>";
    h += F("<section class='card section'><h2>Fontes e limites</h2>"
           "<p>Sistema, Wi-Fi, CPU, tarefas, memoria, flash, botao BOOT GPIO0, HTTP e SD. "
           "Temperatura interna nao e ambiente. Temperatura e umidade relativa de Porto Alegre "
           "vem do Open-Meteo, com source=open_meteo e location=porto_alegre. "
           "As series de clima so aparecem apos dados validos.</p>"
           "<p>Energia, bateria, sensores ambientais locais, reles, ADC e MQTT nao estao integrados; "
           "essas series sao omitidas. SD usa cache atualizado em tarefa a cada 30 segundos, sem ler por scrape.</p>"
           "<p>Boot/watchdog persistem; HTTP, desconexoes e operacoes SD reiniciam no boot. "
           "Use up do Prometheus para detectar dispositivo desligado. Sem autenticacao: use rede confiavel.</p></section>");
    if (!countersOk) h += F("<div class='notice section' role='alert'>Contadores persistentes indisponiveis. Consulte a Serial.</div>");
    h += portalEnd();
    panel->sendHeader("Cache-Control", "no-store");
    panel->send(status, "text/html; charset=utf-8", h);
}

void MetricsService::sampleCpu()
{
#if configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS
    if (millis() - sampledAt < 5000UL) return;
    sampledAt = millis();
    if (uxTaskGetNumberOfTasks() > 64)
    {
        cpuUsage = NAN; baseline = false;
        Serial.println("[METRICS] CPU indisponivel: mais de 64 tarefas.");
        return;
    }
    configRUN_TIME_COUNTER_TYPE total = 0;
    const UBaseType_t count = uxTaskGetSystemState(tasks, 64, &total);
    const int64_t now = esp_timer_get_time();
    uint64_t idleDelta = 0;
    bool foundAll = count != 0;
    for (BaseType_t core = 0; core < portNUM_PROCESSORS; ++core)
    {
        const TaskHandle_t idleTask = xTaskGetIdleTaskHandleForCore(core);
        bool found = false;
        for (UBaseType_t i = 0; i < count; ++i) if (tasks[i].xHandle == idleTask)
        {
            const auto idle = tasks[i].ulRunTimeCounter;
            idleDelta += static_cast<configRUN_TIME_COUNTER_TYPE>(idle - previousIdle[core]);
            previousIdle[core] = idle; found = true; break;
        }
        foundAll = foundAll && found;
    }
    // O runtime ESP-IDF e em microssegundos; normaliza pela capacidade dos nucleos.
    const uint64_t capacity = (now - previousTime) * portNUM_PROCESSORS;
    cpuUsage = baseline && foundAll && capacity && idleDelta <= capacity
        ? 100.0 * (1.0 - static_cast<double>(idleDelta) / capacity) : NAN;
    previousTime = now; baseline = foundAll;
#endif
}

void MetricsService::sendMetrics(WebServer& web)
{
    web.sendHeader("Cache-Control", "no-store");
    web.send(200, "text/plain; version=0.0.4; charset=utf-8", exposition());
}

String MetricsService::exposition() const
{
    String out; out.reserve(12000);
    auto label = [](const char* key, const String& value) { return String(key) + "=\"" + escapeLabel(value) + "\""; };
    auto sample = [&](const char* name, const String& value, const String& labels = "",
                      const char* type = "gauge", bool describe = true)
    {
        if (describe)
        {
            out += "# HELP " + String(name) + " AMOLED: " + name + ".\n";
            out += "# TYPE " + String(name) + " " + type + "\n";
        }
        out += String(name) + "{device=\"" + device + "\"";
        if (labels.length()) out += "," + labels;
        out += "} " + value + "\n";
    };
    const bool online = WiFi.status() == WL_CONNECTED;
    sample("esp_info", "1", label("chip", ESP.getChipModel()) + ",model=\"Waveshare-ESP32-S3-Touch-AMOLED-1.8\"");
    sample("esp_firmware_version", "1", label("version", CommandProcessor::obterVersaoAutomatica()) + "," +
           label("build", String(__DATE__) + " " + __TIME__));
    if (online) sample("esp_ip_address", "1", label("ip", WiFi.localIP().toString()) + ",interface=\"sta\"");
    if ((WiFi.getMode() & WIFI_AP) && WiFi.softAPIP())
        sample("esp_ip_address", "1", label("ip", WiFi.softAPIP().toString()) + ",interface=\"ap\"", "gauge", !online);
    sample("esp_mac_address", "1", label("mac", device));
    sample("esp_status_online", online ? "1" : "0");
    sample("esp_uptime_seconds", String(esp_timer_get_time() / 1000000.0, 3));
    sample("esp_last_restart_reason", "1", label("reason", String(static_cast<int>(esp_reset_reason()))));
    if (countersOk)
    {
        sample("esp_boot_count", String(boots.boots));
        sample("esp_reboot_count", String(boots.boots ? boots.boots - 1 : 0));
        sample("esp_watchdog_resets", String(boots.watchdogs));
    }
    if (online)
    {
        sample("esp_wifi_rssi_dbm", String(WiFi.RSSI()));
        sample("esp_wifi_channel", String(WiFi.channel()));
        int8_t power;
        if (esp_wifi_get_max_tx_power(&power) == ESP_OK) sample("esp_wifi_tx_power", String(power / 4.0f, 2));
    }
    sample("esp_wifi_disconnect_count", String(disconnects.load()));
    sample("esp_udp_listener_up", udp.listenerUp ? "1" : "0");
    sample("esp_udp_packets_received_total", String(udp.received), "", "counter");
    sample("esp_udp_replies_sent_total", String(udp.replies), "", "counter");
    sample("esp_udp_send_errors_total", String(udp.sendErrors), "", "counter");
    sample("esp_cpu_frequency_mhz", String(ESP.getCpuFreqMHz()));
    if (isfinite(cpuUsage)) sample("esp_cpu_usage_percent", String(cpuUsage, 3));
    const float temperature = temperatureRead();
    if (isfinite(temperature)) sample("esp_cpu_temperature_celsius", String(temperature, 2));
    const auto weather = climate_.snapshot();
    const String weatherLabels = "source=\"open_meteo\",location=\"porto_alegre\"";
    if (weather.sincronizado && isfinite(weather.temperatura))
        sample("sensor_temperature_celsius", String(weather.temperatura, 2), weatherLabels);
    if (isfinite(weather.umidade) && weather.umidade >= 0 && weather.umidade <= 100)
        sample("sensor_humidity_percent", String(weather.umidade, 2), weatherLabels);
    if (weather.sincronizado)
        sample("esp_weather_data_age_seconds", String((esp_timer_get_time() - weather.ultimaAtualizacaoUs) / 1000000.0, 3));
    sample("esp_task_count", String(uxTaskGetNumberOfTasks()));
    sample("esp_free_rtos_tasks", String(uxTaskGetNumberOfTasks()));
    sample("esp_heap_free_bytes", String(ESP.getFreeHeap()));
    sample("esp_heap_total_bytes", String(ESP.getHeapSize()));
    sample("esp_heap_minimum_free_bytes", String(ESP.getMinFreeHeap()));
    sample("esp_psram_free_bytes", String(ESP.getFreePsram()));
    sample("esp_psram_total_bytes", String(ESP.getPsramSize()));
    if (partitionBytes) sample("esp_flash_usage_percent", String(100.0 * imageBytes / partitionBytes, 3));
    sample("gpio_digital_input", digitalRead(BoardConfig::Button) == HIGH ? "1" : "0", "pin=\"0\"");
    sample("application_requests_total", String(requests), "", "counter");
    sample("application_errors_total", String(errors), "", "counter");
    sample("application_response_time_ms", String(responseMs, 3));
    const auto card = storage->metricsSnapshot();
    sample("sdcard_mounted", card.mounted ? "1" : "0");
    sample("sdcard_metrics_available", card.valid ? "1" : "0");
    sample("sdcard_append_operations_total", String(card.appends), "", "counter");
    sample("sdcard_append_errors_total", String(card.appendErrors), "", "counter");
    if (card.valid)
    {
        sample("sdcard_capacity_bytes", String(card.capacity));
        sample("sdcard_total_bytes", String(card.total));
        sample("sdcard_used_bytes", String(card.used));
        sample("sdcard_free_bytes", String(card.total - card.used));
        if (card.total) sample("sdcard_usage_percent", String(100.0 * card.used / card.total, 3));
        sample("sdcard_clock_frequency_hz", "20000000");
        sample("sdcard_data_age_seconds", String((esp_timer_get_time() - card.updatedUs) / 1000000.0, 3));
        bool describeLog = true;
        for (size_t i = 0; i < 3; ++i)
            if (card.logValid[i])
            {
                sample("sdcard_log_size_bytes", String(card.logBytes[i]),
                       label("file", SDUtil::METRIC_LOG_PATHS[i]), "gauge", describeLog);
                describeLog = false;
            }
    }
    return out;
}
