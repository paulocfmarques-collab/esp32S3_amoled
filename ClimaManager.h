#pragma once
#include <Arduino.h>
#include "BoardConfig.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <math.h>
#include <stdlib.h>
#include <esp_timer.h>
#include <ctype.h>
#include <esp_heap_caps.h>

struct ClimaSnapshot
{
    float temperatura = 0, minima = 0, maxima = 0;
    float umidade = NAN;
    int codigoCondicao = 0, chuva = 0;
    bool sincronizado = false, previsaoValida = false, ultimaFalhou = false;
    uint32_t ultimaAtualizacao = 0, previsaoAtualizada = 0;
    char data[11] = {0};
    int64_t ultimaAtualizacaoUs = 0;
};

struct ClimaDiagnostics {
    uint32_t attempts = 0, successes = 0;
    int httpCode = 0;
    char detail[96] = "Nenhuma consulta executada";
};

class ClimaManager
{
  public:
    using Snapshot = ClimaSnapshot;

  private:
    Snapshot data;
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    bool busy = false;
    bool pausedForOta = false;
    uint32_t attempted = 0;
    ClimaDiagnostics diagnostics;
    TaskHandle_t workerHandle = nullptr;

    void detail(const String& value) {
        portENTER_CRITICAL(&mux);
        value.toCharArray(diagnostics.detail, sizeof(diagnostics.detail));
        portEXIT_CRITICAL(&mux);
    }

    String objectBlock(const String& payload, const char* key) {
        int pos = payload.indexOf(String("\"") + key + "\"");
        if (pos < 0) return "";
        pos = payload.indexOf(':', pos);
        if (pos < 0) return "";
        ++pos;
        while (pos < int(payload.length()) && isspace((unsigned char)payload[pos])) ++pos;
        if (pos >= int(payload.length()) || payload[pos] != '{') return "";
        int end = payload.indexOf('}', pos);
        return end < 0 ? String("") : payload.substring(pos, end + 1);
    }

    bool number(const String& block, const char* key, bool array, float& result)
    {
        int pos = block.indexOf(String("\"") + key + "\"");
        if (pos < 0)
            return false;
        pos = block.indexOf(':', pos + strlen(key) + 2);
        if (pos < 0) return false;
        ++pos;
        if (array)
        {
            pos = block.indexOf('[', pos);
            if (pos < 0)
                return false;
            pos++;
        }
        int end = pos;
        while (end < int(block.length()) && block[end] != ',' && block[end] != '}' &&
               block[end] != ']')
            end++;
        String token = block.substring(pos, end);
        token.trim();
        char* tail = nullptr;
        float value = strtof(token.c_str(), &tail);
        if (tail == token.c_str() || *tail || !isfinite(value))
            return false;
        result = value;
        return true;
    }

    // Return normally so HTTP/TLS objects and Strings always run their destructors.
    void query()
    {
        Snapshot next = snapshot();
        bool currentOk = false, dailyOk = false, humidityOk = false;
        NetworkClientSecure client;
        client.setInsecure();
        client.setHandshakeTimeout(10);
        HTTPClient http;
        http.setConnectTimeout(8000);
        http.setTimeout(8000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        const char* url = "https://api.open-meteo.com/v1/"
                          "forecast?latitude=-30.03&longitude=-51.23&current=temperature_2m,"
                          "relative_humidity_2m,weather_code&daily=temperature_2m_min,temperature_2m_max,precipitation_"
                          "probability_max&forecast_days=1&timezone=America%2FSao_Paulo";
        detail("Conectando HTTPS ao Open-Meteo");
        const bool opened = http.begin(client, url);
        const int code = opened ? http.GET() : 0;
        portENTER_CRITICAL(&mux);
        diagnostics.httpCode = code;
        portEXIT_CRITICAL(&mux);
        if (opened && code == HTTP_CODE_OK)
        {
            detail("Lendo resposta HTTPS");
            String payload = http.getString();
            detail("Processando resposta JSON");
            String current = objectBlock(payload, "current");
            float t, w;
            if (current.length())
            {
                String block = current;
                float humidity;
                if (number(block, "relative_humidity_2m", false, humidity) &&
                    humidity >= 0 && humidity <= 100) {
                    next.umidade = humidity;
                    humidityOk = true;
                }
                if (number(block, "temperature_2m", false, t) &&
                    number(block, "weather_code", false, w))
                {
                    next.temperatura = t;
                    next.codigoCondicao = int(w);
                    next.sincronizado = true;
                    next.ultimaAtualizacao = millis();
                    currentOk = true;
                    next.ultimaAtualizacaoUs = esp_timer_get_time();
                }
            }
            String daily = objectBlock(payload, "daily");
            float low, high, rain;
            if (daily.length())
            {
                String block = daily;
                int time = block.indexOf("\"time\"");
                int start = time >= 0 ? block.indexOf('[', time) : -1;
                int quote = start >= 0 ? block.indexOf('"', start) : -1;
                if (quote >= 0 && number(block, "temperature_2m_min", true, low) &&
                    number(block, "temperature_2m_max", true, high) &&
                    number(block, "precipitation_probability_max", true, rain) && low <= high &&
                    rain >= 0 && rain <= 100)
                {
                    String date = block.substring(quote + 1, quote + 11);
                    if (date.length() == 10 && date[4] == '-' && date[7] == '-')
                    {
                        next.minima = low;
                        next.maxima = high;
                        next.chuva = int(rain);
                        next.previsaoValida = true;
                        next.previsaoAtualizada = millis();
                        date.toCharArray(next.data, sizeof(next.data));
                        dailyOk = true;
                    }
                }
            }
        }
        String outcome;
        if (!opened) outcome = "Falha ao preparar HTTPS";
        else if (code < 0) outcome = "HTTPS: " + HTTPClient::errorToString(code);
        else if (code != HTTP_CODE_OK) outcome = "HTTP " + String(code) + " recebido da API";
        else outcome = String(currentOk ? "Atual " : "Atual INVALIDO ") +
                       (humidityOk ? "umidade " : "umidade INVALIDA ") +
                       (dailyOk ? "previsao OK" : "previsao INVALIDA");
        detail(outcome);
        http.end();
        next.ultimaFalhou = !(currentOk && dailyOk && humidityOk);
        portENTER_CRITICAL(&mux);
        data = next;
        if (currentOk && dailyOk && humidityOk) ++diagnostics.successes;
        portEXIT_CRITICAL(&mux);
        Serial.printf("[CLIMA] HTTP=%d: %s\n", code, outcome.c_str());
    }

    static void worker(void* context)
    {
        static_cast<ClimaManager*>(context)->work();
    }

    void work()
    {
        for (;;) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            query();
            // query() has returned: all per-request allocations are released first.
            portENTER_CRITICAL(&mux);
            busy = false;
            portEXIT_CRITICAL(&mux);
        }
    }

  public:
    bool begin()
    {
        if (workerHandle) return true;
        if (xTaskCreatePinnedToCore(worker, "weather", 8192, this, 1, &workerHandle, BoardConfig::InputCore) == pdPASS)
            return true;
        workerHandle = nullptr;
        detail("Sem memoria para criar tarefa de clima");
        Serial.printf("[CLIMA] Falha ao reservar tarefa: heap interno=%u; maior bloco=%u\n",
            unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
        return false;
    }

    void pauseForOta(bool paused)
    {
        portENTER_CRITICAL(&mux);
        pausedForOta = paused;
        portEXIT_CRITICAL(&mux);
    }

    String status() {
        ClimaDiagnostics d; bool running, paused; uint32_t lastAttempt;
        portENTER_CRITICAL(&mux);
        d = diagnostics; running = busy; paused = pausedForOta; lastAttempt = attempted;
        portEXIT_CRITICAL(&mux);
        String result = "===== CLIMA STATUS =====\nWiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "offline") +
            "\nTarefa: " + String(running ? "ocupada" : "livre") + "\nPausa OTA: " + String(paused ? "SIM" : "NAO") +
            "\nTentativas: " + String(d.attempts) + " | Sucessos completos: " + String(d.successes) +
            "\nUltimo HTTP: " + String(d.httpCode) + "\nDetalhe: " + String(d.detail) + "\n";
        if (d.attempts) result += "Ultima tentativa ha: " + String((millis() - lastAttempt) / 1000) + " s\n";
        if (running && millis() - lastAttempt > 30000) result += "AVISO: consulta ocupada ha mais de 30 s\n";
        result += "Worker reservado: " + String(workerHandle ? "SIM" : "NAO") +
            "\nHeap interno livre: " + String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) +
            "\nMaior bloco interno: " + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) + " bytes\n";
        return result;
    }

    Snapshot snapshot()
    {
        portENTER_CRITICAL(&mux);
        Snapshot copy = data;
        portEXIT_CRITICAL(&mux);
        return copy;
    }

    bool atualizar(bool force = false)
    {
        if (WiFi.status() != WL_CONNECTED)
            return false;
        uint32_t now = millis();
        portENTER_CRITICAL(&mux);
        constexpr uint32_t interval = 15UL * 60UL * 1000UL;
        bool start = !pausedForOta && !busy && (force || diagnostics.attempts == 0 || now - attempted >= interval);
        if (start)
        {
            busy = true;
            attempted = now;
            ++diagnostics.attempts;
            diagnostics.httpCode = 0;
        }
        portEXIT_CRITICAL(&mux);
        if (!start) return false;
        if (!begin())
        {
            portENTER_CRITICAL(&mux);
            busy = false;
            data.ultimaFalhou = true;
            portEXIT_CRITICAL(&mux);
            return false;
        }
        xTaskNotifyGive(workerHandle);
        return true;
    }

    String obterTextoCondicao()
    {
        int code = snapshot().codigoCondicao;
        if (code == 0)
            return "Ceu limpo";
        if (code <= 3)
            return "Parc. nublado";
        if (code >= 45 && code <= 48)
            return "Nevoeiro";
        if (code >= 51 && code <= 67)
            return "Chuva / garoa";
        if (code >= 71 && code <= 77)
            return "Neve";
        if (code >= 80 && code <= 82)
            return "Pancadas";
        if (code >= 95)
            return "Tempestade";
        return "Nublado";
    }
};
