#include "DisplayUtil.h"
#include "ClimaManager.h"
#include <WiFi.h>
#include <math.h>

DisplayUtil::DisplayUtil(BoardHardware& board,TouchInput& touch,ClimaManager& climate)
    : board_(board),touch_(touch),frame_(board.screen()),climate_(climate),
      textColor(UiPainter::Ink),backgroundColor(0),textSize(2) {}
void DisplayUtil::begin() {
    settings_.begin();
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

void DisplayUtil::centered(const String& value,int y,uint8_t size,uint16_t color) {
    const auto font=size<=1?UiPainter::Font::Small:size==2?UiPainter::Font::Body:
        size==3?UiPainter::Font::Title:size==4?UiPainter::Font::Large:UiPainter::Font::Hero;
    ui_.text(value,frame_.width()/2,y,font,color,frame_.width()-32,UiPainter::Align::Center);
}
void DisplayUtil::header(const String& title,UiPainter::Icon icon) {
    ui_.icon(icon,16,44,26,UiPainter::Cyan);
    ui_.text(title,54,45,UiPainter::Font::Title,UiPainter::Ink,frame_.width()-72);
}
void DisplayUtil::footer() {
    ui_.statusBar(pageIndex_,ScreenLayout::VisiblePages,battery_,continuousClock_);
    const int y=frame_.height()-8;
    frame_.fillRect(0,ScreenLayout::footerTop(frame_.height()),frame_.width(),ScreenLayout::FooterHeight,0);
    for(int i=0;i<ScreenLayout::VisiblePages;++i)
        frame_.fillRoundRect(frame_.width()/2-(ScreenLayout::VisiblePages*10)/2+i*10,y,i==pageIndex_?8:3,3,1,
            i==pageIndex_?UiPainter::Cyan:UiPainter::Track);
}
void DisplayUtil::present() { footer();frame_.flush(); }
void DisplayUtil::page(const String& title,const String& body) {
    if(!ready_)return;
    frame_.fillScreen(backgroundColor);header(title,title=="Console"?UiPainter::Icon::Console:UiPainter::Icon::Sync);
    std::vector<String> rows;int start=0;
    while(start<int(body.length())) {
        int end=body.indexOf('\n',start);if(end<0)end=body.length();
        if(end>start)rows.push_back(body.substring(start,end));start=end+1;
    }
    const int top=ScreenLayout::HeaderHeight+8,bottom=ScreenLayout::footerTop(frame_.height())-8;
    const int capacity=max(1,(bottom-top)/28),first=max(0,int(rows.size())-capacity);
    const int count=int(rows.size())-first,slot=count?(bottom-top)/count:0;
    for(int i=0;i<count;++i) {
        const int y=top+i*slot;
        ui_.card(12,y,frame_.width()-24,max(22,slot-6),UiPainter::Cyan);
        const auto font=slot<40?UiPainter::Font::Small:UiPainter::Font::Body;
        ui_.text(rows[first+i],26,y+max(5,(slot-(slot<40?14:22))/2)-3,font,textColor,frame_.width()-52);
    }
    present();
}
void DisplayUtil::showOtaProgress(uint8_t percent,const String& status) {
    if(!ready_)return;percent=min(uint8_t(100),percent);
    frame_.fillScreen(0);header("Atualizacao OTA",UiPainter::Icon::Sync);
    const int cx=frame_.width()/2,cy=frame_.height()/2-10,r=min(105,(int(frame_.height())-180)/2);
    for(int i=0;i<60;++i) {
        const float a=i*0.1047198f;
        frame_.drawLine(cx+int(sinf(a)*r),cy-int(cosf(a)*r),cx+int(sinf(a)*(r+8)),cy-int(cosf(a)*(r+8)),
            i*100<percent*60?UiPainter::Cyan:UiPainter::Track);
    }
    ui_.text(String(percent)+"%",cx,cy-28,UiPainter::Font::Large,UiPainter::Ink,2*r-16,UiPainter::Align::Center);
    centered(status,cy+r+30,2,UiPainter::Muted);present();
}
void DisplayUtil::setBacklight(uint8_t p) { brightness_ = min(uint8_t(100), p); wake(); }
void DisplayUtil::wake() { lastActivity_ = millis(); updatePower(); }
void DisplayUtil::updatePower() {
    const uint8_t scheduled=nightActive_?min(brightness_,settings_.nightBrightness):brightness_;
    const uint8_t percent = !continuousClock_ && idleFor(120000) ? min(uint8_t(20), scheduled) : scheduled;
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
    if(!ready_)return;
    const auto weather=climate_.snapshot();frame_.fillScreen(0);header("Agora",UiPainter::Icon::Clock);
    ui_.text(clockSource_,frame_.width()-18,52,UiPainter::Font::Small,
        clockSource_=="NTP"?UiPainter::Green:UiPainter::Amber,90,UiPainter::Align::Right);
    const bool valid=date.length()==19 && date[10]==' ';
    const int top=ScreenLayout::HeaderHeight+12;
    ui_.text(valid?date.substring(11,16):String("--:--"),frame_.width()/2,top,UiPainter::Font::Hero,
        UiPainter::Ink,frame_.width()-32,UiPainter::Align::Center);
    ui_.text(valid?date.substring(0,10):String("Aguardando NTP"),20,top+80,UiPainter::Font::Body,UiPainter::Muted,frame_.width()-100);
    frame_.fillCircle(frame_.width()-73,top+88,3,UiPainter::Cyan);
    ui_.text(valid?date.substring(17,19):String("--"),frame_.width()-59,top+79,UiPainter::Font::Body,UiPainter::Cyan,42);
    const int y=top+124,gap=10;
    const int height=(ScreenLayout::footerTop(frame_.height())-y-10-gap)/2;
    ui_.card(12,y,frame_.width()-24,height,UiPainter::Amber);
    ui_.icon(weather.sincronizado && weather.codigoCondicao<=1?UiPainter::Icon::Sun:UiPainter::Icon::Cloud,
        26,y+(height-42)/2,42,UiPainter::Amber);
    ui_.text("PORTO ALEGRE",84,y+12,UiPainter::Font::Small,UiPainter::Muted,frame_.width()-108);
    ui_.text(weather.sincronizado?String(weather.temperatura,1)+" C":String("Aguardando clima"),84,y+(height<70?31:35),
        height>=90?UiPainter::Font::Title:height<70?UiPainter::Font::Small:UiPainter::Font::Body,UiPainter::Ink,frame_.width()-108);
    const int ny=y+height+gap;
    ui_.card(12,ny,frame_.width()-24,height,WiFi.status()==WL_CONNECTED?UiPainter::Green:UiPainter::Amber);
    ui_.icon(UiPainter::Icon::Wifi,28,ny+(height-34)/2,34,UiPainter::Cyan);
    ui_.text(WiFi.status()==WL_CONNECTED?WiFi.SSID():String("AMOLED_CONFIG"),84,ny+12,UiPainter::Font::Small,UiPainter::Muted,frame_.width()-108);
    ui_.text(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String("192.168.4.1"),84,ny+(height<70?31:35),
        height<70?UiPainter::Font::Small:UiPainter::Font::Body,UiPainter::Ink,frame_.width()-108);
    present();
}
void DisplayUtil::setContinuousClock(bool enabled) { if(continuousClock_!=enabled){continuousClock_=enabled;wake();} }
void DisplayUtil::showAnalogClock(const String& timestamp,const BatterySnapshot& battery,int32_t timezone,bool dst) {
    if(!ready_)return;const auto weather=climate_.snapshot();battery_=battery;
    watch_.draw(frame_,timestamp,battery,WiFi.status()==WL_CONNECTED,weather.sincronizado,weather.temperatura,timezone,dst,clockSource_,settings_.face,settings_.color());
    present();
}
void DisplayUtil::showPowerOff() {
    if(!ready_)return;frame_.fillScreen(0);
    ui_.icon(UiPainter::Icon::Power,frame_.width()/2-30,frame_.height()/2-90,60,UiPainter::Cyan);
    centered("Desligando",frame_.height()/2,3,UiPainter::Ink);
    centered("Segure PWR para ligar",frame_.height()/2+54,1,UiPainter::Muted);frame_.flush();
}
void DisplayUtil::showStatusPage(bool sdReady,bool sync,const String& date,int32_t zone,bool dst,uint8_t) {
    page("Hora e sincronismo",String("Fonte: ")+clockSource_+"\n"+String(sync?"NTP confirmado":"Sem resposta NTP recente")+"\n"+date+
        "\nUTC "+String(zone)+"  |  DST "+String(dst?"ON":"OFF")+"\nCartao SD: "+String(sdReady?"pronto":"ausente"));
}
void DisplayUtil::showNetworkPage(uint8_t) {
    if(!ready_)return;frame_.fillScreen(0);header("Conectividade",UiPainter::Icon::Wifi);
    const bool online=WiFi.status()==WL_CONNECTED;const int top=ScreenLayout::HeaderHeight+8;
    const int width=frame_.width()-24,height=ScreenLayout::footerTop(frame_.height())-top-8;
    ui_.card(12,top,width,90,online?UiPainter::Green:UiPainter::Amber);
    ui_.icon(UiPainter::Icon::Wifi,28,top+25,38,online?UiPainter::Green:UiPainter::Amber);
    ui_.text(online?WiFi.SSID():String(BoardConfig::AccessPoint),84,top+16,UiPainter::Font::Body,UiPainter::Ink,width-96);
    ui_.text(online?WiFi.localIP().toString():String("192.168.4.1"),84,top+51,UiPainter::Font::Body,UiPainter::Cyan,width-96);
    const int y=top+100,h=height-100;
    ui_.card(12,y,width,h,UiPainter::Cyan);
    ui_.text(online?String("SINAL WI-FI"):String("MODO CONFIGURACAO"),28,y+16,UiPainter::Font::Small,UiPainter::Muted,width-32);
    if(online) {
        ui_.text(String(WiFi.RSSI())+" dBm",28,y+44,UiPainter::Font::Large,UiPainter::Ink,width-40);
        ui_.meter(28,y+100,width-32,9,constrain((WiFi.RSSI()+100)*2,0,100),WiFi.RSSI()>=-70?UiPainter::Green:UiPainter::Amber);
        ui_.text("Gateway  "+WiFi.gatewayIP().toString(),28,y+h-34,UiPainter::Font::Small,UiPainter::Muted,width-32);
    } else {
        ui_.text("Configure o Wi-Fi no portal",28,y+53,UiPainter::Font::Body,UiPainter::Ink,width-32);
        ui_.text(connecting_.length()?String("Buscando: ")+connecting_:String("Aguardando uma rede salva"),28,y+h-38,UiPainter::Font::Small,UiPainter::Cyan,width-32);
    }present();
}
void DisplayUtil::showSystemPage() {
    if(!ready_)return;frame_.fillScreen(0);header("Dispositivo",UiPainter::Icon::Chip);
    const int top=ScreenLayout::HeaderHeight+8,w=frame_.width()-24;
    ui_.card(12,top,w,68);ui_.icon(UiPainter::Icon::Chip,28,top+16,36,UiPainter::Cyan);
    ui_.text(String(ESP.getChipModel()),84,top+10,UiPainter::Font::Body,UiPainter::Ink,w-96);
    ui_.text(String(ESP.getChipCores())+" cores  /  "+String(ESP.getCpuFreqMHz())+" MHz",84,top+42,UiPainter::Font::Small,UiPainter::Muted,w-96);
    const int y=top+80,h=(ScreenLayout::footerTop(frame_.height())-y-28)/2;
    const uint64_t totals[]={ESP.getHeapSize(),ESP.getPsramSize()};const uint64_t frees[]={ESP.getFreeHeap(),ESP.getFreePsram()};
    for(int i=0;i<2;++i) {
        const int yy=y+i*(h+10);ui_.card(12,yy,w,h,i?UiPainter::Cyan:UiPainter::Green);
        ui_.text(i?"PSRAM LIVRE":"MEMORIA LIVRE",28,yy+12,UiPainter::Font::Small,UiPainter::Muted,w-32);
        ui_.text(String(frees[i]/1024)+" KB",28,yy+36,UiPainter::Font::Body,UiPainter::Ink,w-32);
        ui_.meter(28,yy+h-17,w-32,6,totals[i]?100.0f*frees[i]/totals[i]:0,i?UiPainter::Cyan:UiPainter::Green);
    }present();
}
void DisplayUtil::showSavedWifiPage(const String ssids[5],int slot,uint8_t) {
    if(!ready_)return;frame_.fillScreen(0);header("Redes salvas",UiPainter::Icon::Wifi);
    const int top=ScreenLayout::HeaderHeight+8,h=(ScreenLayout::footerTop(frame_.height())-top-6)/5;
    for(int i=0;i<5;++i) {
        int y=top+i*h;const bool active=i==slot;
        ui_.card(12,y,frame_.width()-24,h-6,active?UiPainter::Green:UiPainter::Edge);
        ui_.icon(UiPainter::Icon::Wifi,26,y+(h-28)/2,24,active?UiPainter::Green:UiPainter::Muted);
        ui_.text(ssids[i].length()?ssids[i]:String("Slot ")+String(i+1)+" vazio",64,y+(h-22)/2,UiPainter::Font::Body,active?UiPainter::Ink:UiPainter::Muted,frame_.width()-88);
    }present();
}
void DisplayUtil::showHoldMessage(const String& a,const String& b,uint16_t color) {
    const uint16_t previous=textColor;textColor=color;page(a,b);textColor=previous;
}
void DisplayUtil::showSdPage(bool ready,const String& type,uint64_t total,uint64_t used) {
    if(!ready){page("Armazenamento","Cartao nao montado\nInsira um cartao FAT32\nUse sd_status para tentar");return;}
    if(!ready_)return;frame_.fillScreen(0);header("Armazenamento",UiPainter::Icon::Storage);
    const int top=ScreenLayout::HeaderHeight+8,w=frame_.width()-24;
    ui_.card(12,top,w,104);ui_.icon(UiPainter::Icon::Storage,28,top+20,54,UiPainter::Cyan);
    ui_.text(type,104,top+19,UiPainter::Font::Title,UiPainter::Ink,w-116);
    ui_.text(String(total/(1024*1024))+" MiB",104,top+65,UiPainter::Font::Body,UiPainter::Muted,w-116);
    const int y=top+116,h=ScreenLayout::footerTop(frame_.height())-y-8;
    ui_.card(12,y,w,h);ui_.text("ESPACO UTILIZADO",28,y+16,UiPainter::Font::Small,UiPainter::Muted,w-32);
    const float percent=total?min(100.0f,100.0f*used/total):0;
    ui_.text(String(percent,1)+"%",28,y+45,UiPainter::Font::Large,UiPainter::Ink,w-32);
    ui_.meter(28,y+h-48,w-32,12,percent,percent>90?UiPainter::Amber:UiPainter::Cyan);
    ui_.text(String(used/(1024*1024))+" MiB usados",28,y+h-26,UiPainter::Font::Small,UiPainter::Muted,w-32);present();
}
void DisplayUtil::showSdLoading(uint8_t) {page("Armazenamento","Preparando SDMMC...");}
void DisplayUtil::showMonitorPage() {
    if(!monitor_){page("Monitor","Aguardando resultados...");return;}
    if(!ready_)return;const auto& m=*monitor_;frame_.fillScreen(0);header("Monitor de rede",UiPainter::Icon::Chart);
    const int top=ScreenLayout::HeaderHeight+8,w=frame_.width()-24,bottom=ScreenLayout::footerTop(frame_.height());
    const bool signal=monitorView_==1;
    ui_.card(12,top,w,82,m.connected?UiPainter::Green:UiPainter::Amber);
    ui_.text(signal?"SINAL WI-FI":"PING AO GATEWAY",28,top+12,UiPainter::Font::Small,UiPainter::Muted,w-32);
    const String value=signal?(m.connected?String(WiFi.RSSI())+" dBm":String("Offline")):
        !m.connected?String("Offline"):!m.hasResult?String("Aguardando"):m.replied?String(m.rtt)+" ms":String("Timeout");
    ui_.text(value,28,top+40,UiPainter::Font::Title,UiPainter::Ink,w-32);
    const int y=top+92,h=bottom-y-109;
    ui_.card(12,y,w,h,UiPainter::Cyan);
    ui_.text(signal?"RSSI / -100 A -30 dBm":"LATENCIA / ESCALA AUTOMATICA",26,y+9,UiPainter::Font::Small,UiPainter::Muted,w-28);
    int maximum=1;for(int i=0;i<m.trendCount;++i)maximum=max(maximum,int(m.trendAt(i).latency));
    const int gx=27,gy=y+32,gw=w-30,gh=h-47;
    for(int i=0;i<3;++i)frame_.drawFastHLine(gx,gy+i*gh/2,gw,UiPainter::Track);
    bool previous=false;int px=0,py=0;
    for(int i=0;i<m.trendCount;++i) {
        const auto& trend=m.trendAt(i);const int xx=gx+(NetworkMonitor::TrendCapacity-m.trendCount+i)*gw/(NetworkMonitor::TrendCapacity-1);
        const bool missing=signal?trend.rssi==-127:trend.latency<0;
        if(missing){frame_.fillCircle(xx,gy+gh,2,UiPainter::Red);previous=false;continue;}
        const int normalized=signal?constrain(int(trend.rssi)+100,0,70):int(trend.latency);
        const int yy=gy+gh-normalized*gh/(signal?70:maximum);
        if(previous)frame_.drawLine(px,py,xx,yy,signal?UiPainter::Green:UiPainter::Cyan);
        frame_.fillCircle(xx,yy,1,signal?UiPainter::Green:UiPainter::Cyan);px=xx;py=yy;previous=true;
    }
    ui_.text("Quedas "+String(m.drops)+" / Offline "+String((unsigned long)m.offlineSeconds())+"s",22,bottom-98,UiPainter::Font::Small,UiPainter::Amber,w-24);
    ui_.text("Perda "+String(m.attempts?100.0f*m.failures/m.attempts:0,1)+"% / Media "+(m.received?String((unsigned long)(m.totalRtt/m.received))+"ms":String("--")),22,bottom-74,UiPainter::Font::Small,UiPainter::Muted,w-24);
    ui_.text("Ate 20min / passo 10s / toque: grafico",22,bottom-48,UiPainter::Font::Small,UiPainter::Cyan,w-24);
    ui_.text("Gateway local: nao testa a Internet",22,bottom-24,UiPainter::Font::Small,UiPainter::Muted,w-24);present();
}
void DisplayUtil::showForecastPage() {
    const auto c=climate_.snapshot();
    if(!c.previsaoValida){page("Previsao","Porto Alegre\nAguardando atualizacao do clima");return;}
    if(!ready_)return;frame_.fillScreen(0);header("Porto Alegre",UiPainter::Icon::Cloud);
    const int top=ScreenLayout::HeaderHeight+8,w=frame_.width()-24;
    ui_.card(12,top,w,100,UiPainter::Amber);
    ui_.icon(c.codigoCondicao<=1?UiPainter::Icon::Sun:UiPainter::Icon::Cloud,28,top+18,62,c.codigoCondicao<=1?UiPainter::Amber:UiPainter::Cyan);
    ui_.text(c.sincronizado?String(c.temperatura,1)+" C":String(c.data),110,top+15,UiPainter::Font::Large,UiPainter::Ink,w-122);
    ui_.text("Hoje  /  "+String(c.data),110,top+70,UiPainter::Font::Small,UiPainter::Muted,w-122);
    const int y=top+112,h=(ScreenLayout::footerTop(frame_.height())-y-24)/2,half=(w-10)/2;
    ui_.card(12,y,half,h);ui_.card(22+half,y,half,h,UiPainter::Amber);
    ui_.text("MINIMA",26,y+12,UiPainter::Font::Small,UiPainter::Muted,half-28);
    ui_.text(String(c.minima,1)+" C",26,y+37,UiPainter::Font::Body,UiPainter::Cyan,half-28);
    ui_.text("MAXIMA",36+half,y+12,UiPainter::Font::Small,UiPainter::Muted,half-28);
    ui_.text(String(c.maxima,1)+" C",36+half,y+37,UiPainter::Font::Body,UiPainter::Amber,half-28);
    const int yy=y+h+10;ui_.card(12,yy,w,h);
    ui_.text("CHUVA  "+String(c.chuva)+"%",28,yy+12,UiPainter::Font::Small,UiPainter::Muted,w-32);
    ui_.meter(28,yy+h-20,w-32,8,c.chuva,UiPainter::Cyan);present();
}
void DisplayUtil::desenharOlhosScreensaver() {if(ready_ && eyes_.draw(frame_,rotation_))present();}
void DisplayUtil::dispararAnimacaoGravacaoSD() {fimAnimacaoSD=millis()+300;}
void DisplayUtil::print(String text) {println(text);}
void DisplayUtil::println(String text) {
    text.replace("\r","");
    const int limit=ready_?frame_.width()-56:280;
    while(text.length()) {
        int end=text.indexOf('\n');if(end<0)end=text.length();
        String line=text.substring(0,end);text.remove(0,end+(end<int(text.length())?1:0));
        while(line.length()) {
            int n=line.length();while(n>1 && ui_.textWidth(line.substring(0,n),UiPainter::Font::Small)>limit)--n;
            addLine(line.substring(0,n));line.remove(0,n);
        }
    }redraw();
}
void DisplayUtil::addLine(String line) {lines.push_back(line);while(lines.size()>10)lines.erase(lines.begin());}
void DisplayUtil::redraw() {String body;for(const auto& line:lines)body+=line+"\n";page("Console",body);}

void DisplayUtil::updateLocalHour(const String& timestamp) {
    const bool valid=timestamp.length()==19 && timestamp[10]==' ' && isDigit(timestamp[11]) && isDigit(timestamp[12]);
    nightActive_=valid && settings_.nightAt(timestamp.substring(11,13).toInt());
}
void DisplayUtil::settingsTap(int x,int y) {
    const int top=ScreenLayout::HeaderHeight+8;
    const int h=(ScreenLayout::footerTop(frame_.height())-top-24)/6;
    if(x<12 || x>=frame_.width()-12 || y<top || y>=top+6*h)return;
    const int row=(y-top)/h;if((y-top)%h>=h-5)return;
    switch(row) {
    case 0:settings_.face=(settings_.face+1)%3;break;
    case 1:settings_.accent=(settings_.accent+1)%4;break;
    case 2:settings_.nightEnabled=!settings_.nightEnabled;break;
    case 3:if(x<frame_.width()/2)settings_.nightStart=(settings_.nightStart+1)%24;
           else settings_.nightEnd=(settings_.nightEnd+1)%24;break;
    case 4:settings_.nightBrightness=settings_.nightBrightness<12?12:settings_.nightBrightness<20?20:settings_.nightBrightness<30?30:5;break;
    case 5:settings_.motionEnabled=!settings_.motionEnabled;break;
    }
    settingsSaved_=settings_.save();
    if(!settingsSaved_)Serial.println("[UI] Falha ao salvar preferencias; alteracoes apenas nesta sessao");
    wake();
}
void DisplayUtil::showSettingsPage() {
    if(!ready_)return;frame_.fillScreen(0);header("Personalizar",UiPainter::Icon::Sun);
    const int top=ScreenLayout::HeaderHeight+8,w=frame_.width()-24;
    const int h=(ScreenLayout::footerTop(frame_.height())-top-24)/6;
    char interval[32];snprintf(interval,sizeof(interval),"%02dh > %02dh",settings_.nightStart,settings_.nightEnd);
    const String titles[]={"MOSTRADOR","COR DO RELOGIO","BRILHO NOTURNO","INICIO < TOQUE > FIM","BRILHO A NOITE","MOVIMENTOS"};
    const String values[]={settings_.faceName(),String(settings_.accent==0?"Ciano":settings_.accent==1?"Verde":settings_.accent==2?"Ambar":"Violeta"),
        String(settings_.nightEnabled?"Automatico":"Desativado"),String(interval),String(settings_.nightBrightness)+"%",String(settings_.motionEnabled?"Levantar + girar":"Desativados")};
    for(int i=0;i<6;++i) {
        const int y=top+i*h;ui_.card(12,y,w,h-5,settings_.color());
        if(h<45)ui_.text(titles[i]+"  "+values[i],25,y+9,UiPainter::Font::Small,UiPainter::Ink,w-26);
        else {
            ui_.text(titles[i],25,y+5,UiPainter::Font::Small,UiPainter::Muted,w-26);
            ui_.text(values[i],25,y+23,UiPainter::Font::Body,UiPainter::Ink,w-26);
        }
    }
    ui_.text(settingsSaved_?"Toque para mudar / deslize para sair":"Falha ao salvar: somente nesta sessao",frame_.width()/2,ScreenLayout::footerTop(frame_.height())-20,
        UiPainter::Font::Small,settingsSaved_?UiPainter::Muted:UiPainter::Red,w,UiPainter::Align::Center);
    present();
}
void DisplayUtil::showBatteryHistory(const BatteryHistory& history) {
    if(!ready_)return;frame_.fillScreen(0);header("Energia",UiPainter::Icon::Battery);
    const int top=ScreenLayout::HeaderHeight+8,w=frame_.width()-24,bottom=ScreenLayout::footerTop(frame_.height());
    ui_.card(12,top,w,88,battery_.charging?UiPainter::Green:UiPainter::Amber);
    ui_.text(battery_.valid && battery_.connected && battery_.percentValid?String(battery_.percent)+"%":String("--%"),28,top+12,UiPainter::Font::Large,UiPainter::Ink,w-100);
    ui_.icon(UiPainter::Icon::Battery,frame_.width()-78,top+19,44,battery_.charging?UiPainter::Green:UiPainter::Amber);
    ui_.text(!battery_.valid?"Telemetria indisponivel":!battery_.connected?"Sem bateria":battery_.charging?"Carregando":battery_.discharging?"Em descarga":"Em espera",28,top+65,UiPainter::Font::Small,UiPainter::Muted,w-32);
    const int y=top+98,h=bottom-y-90;
    ui_.card(12,y,w,h,UiPainter::Green);
    ui_.text("CARGA % / ATE 12h / PASSO 5min",26,y+10,UiPainter::Font::Small,UiPainter::Muted,w-28);
    const int gx=44,gy=y+36,gw=w-48,gh=h-54;
    for(int i=0;i<3;++i){const int yy=gy+i*gh/2;frame_.drawFastHLine(gx,yy,gw,UiPainter::Track);ui_.text(String(100-i*50),18,yy-6,UiPainter::Font::Small,UiPainter::Muted,28);}
    bool previous=false;int px=0,py=0;
    for(int i=0;i<history.count();++i) {
        const auto& value=history.at(i);if(value.percent<0){previous=false;continue;}
        const int xx=gx+(BatteryHistory::Capacity-history.count()+i)*gw/(BatteryHistory::Capacity-1),yy=gy+gh-value.percent*gh/100;
        const uint16_t color=value.charging?UiPainter::Green:UiPainter::Amber;
        if(previous)frame_.drawLine(px,py,xx,yy,color);
        frame_.fillCircle(xx,yy,2,color);px=xx;py=yy;previous=true;
    }
    const float rate=battery_.dischargePercentPerHour;
    const bool estimate=battery_.valid && battery_.connected && battery_.percentValid && battery_.discharging && isfinite(rate) && rate>0;
    ui_.text(battery_.valid && battery_.voltageValid?String(battery_.millivolts/1000.0f,2)+" V":String("Tensao --"),22,bottom-78,UiPainter::Font::Body,UiPainter::Cyan,w-24);
    ui_.text(estimate?String("Autonomia estimada: ")+String(battery_.percent/rate,1)+" h":String("Autonomia: aguardando descarga"),22,bottom-43,UiPainter::Font::Small,UiPainter::Muted,w-24);
    ui_.text("Historico desta sessao / verde = carga",22,bottom-23,UiPainter::Font::Small,UiPainter::Muted,w-24);present();
}
