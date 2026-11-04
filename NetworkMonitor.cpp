#include "NetworkMonitor.h"
#include "lwip/ip_addr.h"

void NetworkMonitor::begin()
{
    for (int& value : history)
        value = -2;
    tickAt = offlineAt = millis();
    connected = WiFi.status() == WL_CONNECTED;
    initialized = true;
    event(connected ? "WiFi conectado" : "Sem conexao");
}

void NetworkMonitor::event(const String& text)
{
    events[2] = events[1];
    events[1] = events[0];
    events[0] = String(millis() / 60000) + "m " + text;
}

void NetworkMonitor::onSuccess(esp_ping_handle_t h, void* arg)
{
    auto* self = static_cast<NetworkMonitor*>(arg);
    uint32_t ms = 0;
    esp_ping_get_profile(h, ESP_PING_PROF_TIMEGAP, &ms, sizeof(ms));
    portENTER_CRITICAL(&self->mux);
    self->success = true;
    self->elapsed = ms;
    portEXIT_CRITICAL(&self->mux);
}

void NetworkMonitor::onEnd(esp_ping_handle_t, void* arg)
{
    auto* self = static_cast<NetworkMonitor*>(arg);
    portENTER_CRITICAL(&self->mux);
    self->done = true;
    portEXIT_CRITICAL(&self->mux);
}

void NetworkMonitor::update()
{
    if (!initialized)
        begin();
    uint32_t now = millis();
    bool online = WiFi.status() == WL_CONNECTED;
    // Incremental accounting also handles millis() rollover.
    if (!connected)
        offlineMs += uint32_t(now - tickAt);
    tickAt = now;
    offlineAt = now;
    if (online != connected)
    {
        if (!online)
        {
            drops++;
            lastDown = now;
            event("WiFi caiu");
        }
        else
            event("WiFi voltou");
        epoch++;
        connected = online;
        hasResult = false;
        nextAt = now;
    }
    bool finished, ok;
    uint32_t ms;
    portENTER_CRITICAL(&mux);
    finished = done;
    ok = success;
    ms = elapsed;
    portEXIT_CRITICAL(&mux);
    if (session && finished)
    {
        esp_ping_delete_session(session);
        session = nullptr;
        // Ignore replies belonging to the previous network/gateway.
        if (online && epoch == sessionEpoch && target == WiFi.gatewayIP())
        {
            hasResult = true;
            replied = ok;
            rtt = ms;
            measuredAt = now;
            attempts++;
            if (!ok)
                failures++;
            else
            {
                if (!received || ms < minimum)
                    minimum = ms;
                if (!received || ms > maximum)
                    maximum = ms;
                received++;
                totalRtt += ms;
            }
            history[head] = ok ? int(ms) : -1;
            head = (head + 1) % 24;
        }
        nextAt = now + 5000;
    }
    if (!online || session || int32_t(now - nextAt) < 0)
        return;
    target = WiFi.gatewayIP();
    nextAt = now + 5000;
    if (target == IPAddress(0, 0, 0, 0))
        return;
    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.count = 1;
    cfg.timeout_ms = 1000;
    IP_ADDR4(&cfg.target_addr, target[0], target[1], target[2], target[3]);
    esp_ping_callbacks_t callbacks = {};
    callbacks.cb_args = this;
    callbacks.on_ping_success = onSuccess;
    callbacks.on_ping_end = onEnd;
    portENTER_CRITICAL(&mux);
    done = false;
    success = false;
    elapsed = 0;
    portEXIT_CRITICAL(&mux);
    sessionEpoch = epoch;
    if (esp_ping_new_session(&cfg, &callbacks, &session) != ESP_OK)
    {
        session = nullptr;
        return;
    }
    if (esp_ping_start(session) != ESP_OK)
    {
        esp_ping_delete_session(session);
        session = nullptr;
    }
}
