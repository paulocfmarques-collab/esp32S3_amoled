#include "DisplayUtil.h"
#include "ClimaManager.h"
#include <WiFi.h>
#include <math.h>

DisplayUtil::DisplayUtil(BoardHardware& board, TouchInput& touch, ClimaManager& climate)
    : board_(board), touch_(touch), frame_(board.screen()), climate_(climate),
      textColor(0xFFFF), backgroundColor(0), textSize(2) {}
void DisplayUtil::begin() {
    if (!board_.displayReady()) {
        Serial.println("[LCD] Interface desativada: inicializacao da placa/driver falhou"); return;
    }
    Serial.printf("[LCD] Buffer RGB565: precisa %u bytes; PSRAM total=%u livre=%u\n",
        unsigned(BoardConfig::Width * BoardConfig::Height * sizeof(uint16_t)),
        unsigned(ESP.getPsramSize()), unsigned(ESP.getFreePsram()));
    ready_ = frame_.begin(GFX_SKIP_OUTPUT_BEGIN);
    if (!ready_) {
        Serial.println("[LCD] Falha ao alocar buffer na PSRAM; selecione OPI PSRAM na IDE");
        auto& screen = board_.screen();
        screen.fillScreen(0x7800); screen.setTextColor(0xFFFF, 0x7800);
        screen.setTextSize(2); screen.setCursor(20, 70);
        screen.println("Framebuffer failed");
        screen.setCursor(20, 110); screen.println("Check OPI PSRAM");
        return;
    }
    Serial.println("[LCD] Buffer PSRAM pronto; iniciando interface");
    frame_.setTextWrap(false); wake();
    page("AMOLED Gateway", "Inicializando...\nAplicacao: core 1\nTouch e clima: core 0");
}
void DisplayUtil::centered(const String& value, int y, uint8_t size, uint16_t color) {
    frame_.setTextSize(size); frame_.setTextColor(color, backgroundColor);
    frame_.setCursor(max(0, (int(frame_.width()) - int(value.length()) * 6 * size) / 2), y);
    frame_.print(value);
}
void DisplayUtil::footer() {
    const int y = frame_.height() - 34;
    frame_.drawFastHLine(12, y, frame_.width() - 24, statusColor_);
    centered("Toque: proxima | Segure: inicio", y + 12, 1, 0xBDF7);
}
void DisplayUtil::page(const String& title, const String& body) {
    if (!ready_) return;
    frame_.fillScreen(backgroundColor); frame_.setTextWrap(false);
    centered(title, 18, 2, 0x07FF);
    frame_.drawFastHLine(12, 48, frame_.width() - 24, statusColor_);
    int start = 0, y = 65;
    while (start < int(body.length()) && y < frame_.height() - 50) {
        int end = body.indexOf('\n', start);
        if (end < 0) end = body.length();
        String line = body.substring(start, end);
        const int maxChars = (frame_.width() - 32) / 12;
        do {
            frame_.setTextSize(2); frame_.setTextColor(textColor, backgroundColor);
            frame_.setCursor(16, y); frame_.print(line.substring(0, maxChars));
            line.remove(0, min(maxChars, int(line.length()))); y += 25;
        } while (line.length() && y < frame_.height() - 50);
        start = end + 1;
    }
    footer(); frame_.flush();
}
void DisplayUtil::showOtaProgress(uint8_t percent, const String& status) {
    if (!ready_) return;
    percent = min(uint8_t(100), percent);
    frame_.fillScreen(0);
    centered("Atualizacao OTA", 30, 2, 0x07FF);
    centered(String(percent) + "%", frame_.height()/2 - 60, 5, 0xFFFF);
    const int y = frame_.height()/2 + 5, width = frame_.width() - 48;
    frame_.drawRoundRect(24, y, width, 28, 6, 0x07FF);
    frame_.fillRect(28, y+4, uint32_t(percent)*(width-8)/100, 20, 0x07FF);
    centered(status, y+55, 2, 0xBDF7); frame_.flush();
}
void DisplayUtil::setBacklight(uint8_t p) { brightness_ = min(uint8_t(100), p); wake(); }
void DisplayUtil::wake() { lastActivity_ = millis(); updatePower(); }
void DisplayUtil::updatePower() {
    const uint8_t percent = idleFor(120000) ? min(uint8_t(20), brightness_) : brightness_;
    const uint8_t level = uint32_t(percent)*255/100;
    if (ready_ && level != appliedBrightness_) { board_.setBrightness(level); appliedBrightness_ = level; }
}
void DisplayUtil::clear() { lines.clear(); if (ready_) { frame_.fillScreen(backgroundColor); frame_.flush(); } }
void DisplayUtil::setTextColor(uint16_t color) { textColor = color; }
void DisplayUtil::setBackgroundColor(uint16_t color) { backgroundColor = color; }
void DisplayUtil::setTextSize(uint8_t size) { textSize = constrain(size, 1, 5); }
void DisplayUtil::setRotation(uint8_t rotation) {
    rotation_ = rotation & 3; touch_.setRotation(rotation_);
    if (ready_) { frame_.setRotation(rotation_); clear(); }
}
void DisplayUtil::showClock(String date) {
    if (!ready_) return;
    const auto weather = climate_.snapshot();
    frame_.fillScreen(backgroundColor);
    centered("AMOLED Gateway", 22, 2, 0x07FF);
    const int middle = frame_.height()/2;
    centered(date.length() >= 19 ? date.substring(11,19) : String("--:--:--"), middle-74, 4, textColor);
    centered(date.length() >= 19 ? date.substring(0,10) : String("Sem hora"), middle-22, 3, textColor);
    centered(WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : String("AP: 192.168.4.1"), middle+30, 2, 0xBDF7);
    centered(weather.sincronizado ? String(weather.temperatura,1)+" C" : String("Clima aguardando"), middle+64, 2, 0x07FF);
    footer(); frame_.flush();
}
void DisplayUtil::showStatusPage(bool sdReady, bool sync, const String& date, int32_t zone, bool dst, uint8_t) {
    page("NTP", String(sync?"Sincronizado":"Aguardando")+"\n"+date+
        "\nFuso: "+String(zone)+"\nDST: "+String(dst?"ON":"OFF")+"\nSD: "+String(sdReady?"OK":"ausente"));
}
void DisplayUtil::showNetworkPage(uint8_t) {
    page("Rede", WiFi.status()==WL_CONNECTED ? WiFi.SSID()+"\n"+WiFi.localIP().toString()+
        "\nRSSI: "+String(WiFi.RSSI())+" dBm\nGateway: "+WiFi.gatewayIP().toString() :
        String("AP: ")+BoardConfig::AccessPoint+"\n192.168.4.1\nBuscando: "+connecting_);
}
void DisplayUtil::showSystemPage() {
    page("Sistema", String(ESP.getChipModel())+"\nCores: "+String(ESP.getChipCores())+
        "\nHeap: "+String(ESP.getFreeHeap()/1024)+" KB\nPSRAM: "+String(ESP.getPsramSize()/1024/1024)+
        " MB\nUptime: "+String(millis()/1000)+" s\nPlaca: V"+String(AMOLED_BOARD_REVISION));
}
void DisplayUtil::showSavedWifiPage(const String ssids[5], int slot, uint8_t) {
    String body;
    for (int i=0;i<5;++i) body+=String(i==slot?"> ":"  ")+String(i+1)+": "+ssids[i]+"\n";
    page("Redes salvas", body);
}
void DisplayUtil::showHoldMessage(const String& a, const String& b, uint16_t color) {
    const uint16_t previous = textColor; textColor = color; page(a,b); textColor = previous;
}
void DisplayUtil::showSdPage(bool ready, const String& type, uint64_t total, uint64_t used) {
    page("Cartao microSD", ready ? String("SDMMC 1 bit\nTipo: ")+type+
        "\nCapacidade: "+String(total/(1024*1024))+" MiB\nUsado: "+String(used/(1024*1024))+" MiB\nLogs habilitados" :
        String("Cartao nao montado\nInsira um cartao FAT32\nUse sd_status para tentar"));
}
void DisplayUtil::showSdLoading(uint8_t) { page("Cartao microSD", "Montando..."); }
void DisplayUtil::showMonitorPage() {
    if (!monitor_) { page("Monitor", "Aguardando..."); return; }
    const auto& m = *monitor_;
    page("Monitor de rede", String(m.connected?"WiFi conectado":"WiFi desconectado")+
        "\nPing: "+(!m.hasResult?String("aguardando"):m.replied?String(m.rtt)+" ms":String("timeout"))+
        "\nQuedas: "+String(m.drops)+"\nFalhas: "+String(m.failures));
}
void DisplayUtil::showForecastPage() {
    const auto c = climate_.snapshot();
    page("Porto Alegre", c.previsaoValida ? String(c.data)+"\nMin: "+String(c.minima,1)+
        " C\nMax: "+String(c.maxima,1)+" C\nChuva: "+String(c.chuva)+
        "%\nIdade: "+String((millis()-c.previsaoAtualizada)/60000)+" min" : String("Aguardando clima"));
}
void DisplayUtil::desenharOlhosScreensaver() { if (ready_) eyes_.draw(frame_,rotation_); }
void DisplayUtil::dispararAnimacaoGravacaoSD() { fimAnimacaoSD=millis()+300; }
void DisplayUtil::print(String text) { println(text); }
void DisplayUtil::println(String text) {
    text.replace("\r", "");
    const int maxChars = ready_ ? (frame_.width()-32)/12 : 28;
    while (text.length()) {
        const int nl=text.indexOf('\n');
        const int n=min(maxChars,nl<0?int(text.length()):nl);
        addLine(text.substring(0,n)); text.remove(0,n+(nl==n?1:0));
    }
    redraw();
}
void DisplayUtil::addLine(String line) { lines.push_back(line); while (lines.size()>10) lines.erase(lines.begin()); }
void DisplayUtil::redraw() { String body; for (const auto& line:lines) body+=line+"\n"; page("Console",body); }
