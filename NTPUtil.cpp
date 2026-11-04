#include <Preferences.h>
#include "NTPUtil.h"
#include <time.h>
#include <sys/time.h>

namespace
{
bool anoBissexto(int ano)
{
    return (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
}

int diasNoMes(int ano, int mes)
{
    static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return mes == 2 && anoBissexto(ano) ? 29 : dias[mes - 1];
}
} // namespace

void NTPUtil::carregarConfiguracoes()
{
    Preferences prefs;
    prefs.begin("ntp_cfg", true);
    fusoHora = prefs.getInt("fuso", -3); // Padrão travado em UTC-3
    dstAtivo = prefs.getBool("dst", false);
    prefs.end();
}

void NTPUtil::aplicarConfiguracaoNTP()
{
    int32_t deslocamentoSegundos = fusoHora * 3600;
    int32_t deslocamentoDST = dstAtivo ? 3600 : 0;

    configTime(deslocamentoSegundos, deslocamentoDST, "a.st1.ntp.br", "pool.ntp.org",
               "time.nist.gov");
    Serial.printf("[NTP] Configuracao aplicada -> Fuso: %d | DST: %s\n", fusoHora,
                  dstAtivo ? "ON" : "OFF");
}

bool NTPUtil::initNTP()
{
    Serial.println("--- Inicializando NTP para AMOLED ---");
    carregarConfiguracoes();
    aplicarConfiguracaoNTP();

    struct tm timeinfo;
    return getLocalTime(&timeinfo, 10); // Synchronization continues in the background.
}

void NTPUtil::atualizarConfiguracao(int32_t novoFuso, bool novoDst)
{
    fusoHora = novoFuso;
    dstAtivo = novoDst;

    Preferences prefs;
    prefs.begin("ntp_cfg", false);
    prefs.putInt("fuso", fusoHora);
    prefs.putBool("dst", dstAtivo);
    prefs.end();

    aplicarConfiguracaoNTP();
}

void NTPUtil::getDateTime(String& dateTime, uint32_t timeoutMs)
{
    struct tm timeinfo;
    uint32_t timeoutSeguro = (timeoutMs < 50) ? 50 : timeoutMs;

    if (getLocalTime(&timeinfo, timeoutSeguro))
    {
        char buffer[30]; // Tamanho seguro alocado explicitamente
        strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
        dateTime = String(buffer);
    }
    else
    {
        dateTime = "Erro ao obter data e hora";
    }
}

String NTPUtil::getDateTime(uint32_t timeoutMs)
{
    String dateTime;
    getDateTime(dateTime, timeoutMs);
    return dateTime;
}

bool NTPUtil::isSincronizado()
{
    struct tm timeinfo;
    return getLocalTime(&timeinfo, 50); // Timeout rápido e seguro para diagnóstico
}

bool NTPUtil::ajustarDataHora(int ano, int mes, int dia, int hora, int minuto, int segundo)
{
    if (ano < 1970 || ano > 2099 || mes < 1 || mes > 12 || dia < 1 || dia > diasNoMes(ano, mes) ||
        hora < 0 || hora > 23 || minuto < 0 || minuto > 59 || segundo < 0 || segundo > 59)
    {
        return false;
    }

    aplicarConfiguracaoNTP();

    int64_t diasDesdeEpoch = 0;
    for (int anoAtual = 1970; anoAtual < ano; anoAtual++)
    {
        diasDesdeEpoch += anoBissexto(anoAtual) ? 366 : 365;
    }
    for (int mesAtual = 1; mesAtual < mes; mesAtual++)
    {
        diasDesdeEpoch += diasNoMes(ano, mesAtual);
    }
    diasDesdeEpoch += dia - 1;

    int64_t epochUtc = diasDesdeEpoch * 86400LL + hora * 3600LL + minuto * 60LL + segundo;
    epochUtc -= static_cast<int64_t>(fusoHora) * 3600LL;
    if (dstAtivo)
    {
        epochUtc -= 3600LL;
    }

    struct timeval timeValue = {};
    timeValue.tv_sec = static_cast<time_t>(epochUtc);
    return settimeofday(&timeValue, nullptr) == 0;
}

int32_t NTPUtil::getFusoHora() const
{
    return fusoHora;
}

bool NTPUtil::isDstAtivo() const
{
    return dstAtivo;
}

String NTPUtil::getSomenteHora()
{
    String dateTime;
    getDateTime(dateTime, 50);
    int separator = dateTime.indexOf(' ');
    if (separator >= 0)
    {
        return dateTime.substring(separator + 1);
    }
    return "--:--:--";
}
