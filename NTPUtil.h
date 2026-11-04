#ifndef NTP_UTIL_H
#define NTP_UTIL_H

#include <Arduino.h>

class NTPUtil
{
  private:
    int32_t fusoHora = -3;
    bool dstAtivo = false;

    void aplicarConfiguracaoNTP();

  public:
    bool initNTP();
    void getDateTime(String& dateTime, uint32_t timeoutMs = 0);
    String getDateTime(uint32_t timeoutMs = 0);

    // Declaração explícita das funções exigidas pelo CommandProcessor
    bool isSincronizado();
    String getSomenteHora();
    bool ajustarDataHora(int ano, int mes, int dia, int hora, int minuto, int segundo);
    int32_t getFusoHora() const;
    bool isDstAtivo() const;

    void atualizarConfiguracao(int32_t novoFuso, bool novoDst);
    void carregarConfiguracoes();
};

#endif
