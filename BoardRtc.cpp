#include "BoardRtc.h"
#include "ClockCalendar.h"

bool BoardRtc::begin() {
    uint8_t control=0;
    present_=board_.readRegisters(Address,0x00,&control,1);
    error_=present_?"Hora ainda nao validada":"RTC nao responde no I2C";
    Serial.println(present_?"[RTC] PCF85063 encontrado em 0x51":"[RTC] PCF85063 indisponivel");
    return present_;
}
bool BoardRtc::readUtc(time_t& epoch) {
    if(!board_.i2cReady()){error_="I2C indisponivel";return false;}
    BoardHardware::Lock lock(board_);
    uint8_t control=0;
    if(!board_.readRegisters(Address,0x00,&control,1)) {present_=false;error_="Falha de leitura I2C";return false;}
    present_=true;
    if(control & 0xA0){error_="RTC parado ou em modo de teste";return false;}
    for(int attempt=0;attempt<3;++attempt) {
        uint8_t data[7]{},seconds=0;
        if(!board_.readRegisters(Address,0x04,data,sizeof(data)) || !board_.readRegisters(Address,0x04,&seconds,1)) {
            error_="Falha de leitura I2C";return false;
        }
        if((data[0] | seconds) & 0x80) {error_="RTC perdeu a referencia (OS=1)";return false;}
        if(seconds!=data[0])continue; // Retry a read crossing a one-second rollover.
        const int second=fromBcd(data[0] & 0x7F),minute=fromBcd(data[1] & 0x7F);
        int hour=fromBcd(data[2] & ((control & 0x02)?0x1F:0x3F));
        if(control & 0x02) {
            if(hour<1 || hour>12){error_="Hora RTC invalida";return false;}
            hour=(hour%12)+((data[2] & 0x20)?12:0);
        }
        const int day=fromBcd(data[3] & 0x3F),month=fromBcd(data[5] & 0x1F),year=fromBcd(data[6]);
        if(year<0 || !ClockCalendar::toUtc(2000+year,month,day,hour,minute,second,epoch)) {
            error_="Calendario RTC invalido";return false;
        }
        error_="OK";return true;
    }
    error_="Hora RTC mudou durante a leitura";return false;
}
bool BoardRtc::writeUtc(time_t epoch) {
    if(!board_.i2cReady()){error_="I2C indisponivel";return false;}
    struct tm utc{};
    if(!gmtime_r(&epoch,&utc) || utc.tm_year<100 || utc.tm_year>199) {error_="RTC suporta 2000 a 2099";return false;}
    BoardHardware::Lock lock(board_);
    uint8_t control=0;
    if(!board_.readRegisters(Address,0x00,&control,1)){error_="RTC nao responde";return false;}
    present_=true;
    const uint8_t run=control & uint8_t(~0xB2); // Clear EXT_TEST, STOP, SR and 12-hour mode; preserve CAP_SEL.
    if(!board_.writeRegister(Address,0x00,run | 0x20)){error_="Falha ao pausar RTC";return false;}
    const uint8_t data[]={toBcd(utc.tm_sec),toBcd(utc.tm_min),toBcd(utc.tm_hour),toBcd(utc.tm_mday),
        uint8_t(utc.tm_wday),toBcd(utc.tm_mon+1),toBcd(utc.tm_year-100)};
    // On an incomplete write, keep STOP set: a partial calendar must not be treated as valid.
    if(!board_.writeRegisters(Address,0x04,data,sizeof(data))){error_="Escrita RTC incompleta; RTC mantido parado";return false;}
    if(!board_.writeRegister(Address,0x00,run)){error_="Falha ao retomar RTC";return false;}
    time_t verified=0;
    if(!readUtc(verified) || difftime(verified,epoch)<-1 || difftime(verified,epoch)>1) {
        error_="Falha ao verificar hora gravada no RTC";return false;
    }
    error_="OK";return true;
}
