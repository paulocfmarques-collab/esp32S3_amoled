#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <Arduino.h>

class DisplayUtil;
class BoardPower;
class ClimaManager;
class ESP32Gateway;
class NTPUtil;
class RGBLed;
class SDUtil; // Encapsula o uso interno, sem herdar cabeçalhos antigos do SD.h

class CommandProcessor
{
public:

    static String obterVersaoAutomatica() {
        // Extração matemática da Data (AAMMDD)
        int ano = ((__DATE__[9] - '0') * 10) + (__DATE__[10] - '0');
        
        int mes = (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n') ? 1 :
                  (__DATE__[0] == 'F')                                             ? 2 :
                  (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r') ? 3 :
                  (__DATE__[0] == 'A' && __DATE__[1] == 'p')                       ? 4 :
                  (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y') ? 5 :
                  (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n') ? 6 :
                  (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l') ? 7 :
                  (__DATE__[0] == 'A' && __DATE__[1] == 'u')                       ? 8 :
                  (__DATE__[0] == 'S')                                             ? 9 :
                  (__DATE__[0] == 'O')                                             ? 10 :
                  (__DATE__[0] == 'N')                                             ? 11 :
                  (__DATE__[0] == 'D')                                             ? 12 : 0;
                  
        int dia = (__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10 + (__DATE__[5] - '0');

        // Extração matemática do Horário (HHMM)
        int hora   = ((__TIME__[0] - '0') * 10) + (__TIME__[1] - '0');
        int minuto = ((__TIME__[3] - '0') * 10) + (__TIME__[4] - '0');

        // Monta a string de forma segura usando buffers de formatação estáveis
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%02d%02d%02d.%02d.%02d", ano, mes, dia, hora, minuto);
        
        return String(buffer);
    }

    CommandProcessor(DisplayUtil& display, ESP32Gateway& gateway, NTPUtil& ntp,
                     RGBLed& led, SDUtil& sd, ClimaManager& climate, BoardPower& power);

    bool begin();
    bool sdReady() const { return sdReady_; }
    void executeCommand(String command, bool fromSerial = false);
    void update();
    void executarSd(const String& command);
    bool ledBusy() const { return ledManual_ || blinkActive_ || breathActive_; }

private:
    void answerAll(String message, bool log = true);
    void saveLog(String message);
    void startBlink(uint16_t pulses, uint32_t interval, bool continuous);
    void executarHealth();
    void executarSelfTest();
    void executarDiagnostico();    
    String obterMotivoReset();

    DisplayUtil& display_;
    ESP32Gateway& gateway_;
    NTPUtil& ntp_;
    RGBLed& led_;
    SDUtil& sd_;
    ClimaManager& climate_;
    BoardPower& power_;

    bool sdReady_ = false;
    bool serialReply_ = false;
    bool blinkActive_ = false;
    bool blinkOn_ = false;
    bool blinkContinuous_ = false;
    uint16_t pulsesRemaining_ = 0;
    uint32_t blinkInterval_ = 250;
    uint32_t lastBlinkChange_ = 0;
    bool scanningWifi_ = false;
    bool breathActive_ = false;
    bool ledManual_ = false;
};

#endif
