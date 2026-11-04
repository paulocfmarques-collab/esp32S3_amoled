#include "CrashDiagnostics.h"
#include "SDUtil.h"
#include <esp_core_dump.h>

String CrashDiagnostics::lerArquivarEApagar(SDUtil& sd, const String& motivoReset)
{
    String resultado = "Ultimo reset: " + motivoReset + "\n";
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    esp_err_t erro = esp_core_dump_image_check();
    if (erro != ESP_OK)
        return resultado + "Sem core dump valido: " + esp_err_to_name(erro) + "\n";
    char motivo[200] = {};
    if (esp_core_dump_get_panic_reason(motivo, sizeof(motivo)) == ESP_OK)
        resultado += "Panic salvo: " + String(motivo) + "\n";
    esp_core_dump_summary_t resumo = {};
    erro = esp_core_dump_get_summary(&resumo);
    if (erro != ESP_OK)
        return resultado + "Falha ao ler resumo: " + esp_err_to_name(erro) + "\n";
    char linha[96];
    snprintf(linha, sizeof(linha), "\nPC: 0x%08lx\n", (unsigned long)resumo.exc_pc);
    resultado += "Tarefa: " + String(resumo.exc_task) + linha;
#if CONFIG_IDF_TARGET_ARCH_RISCV
    snprintf(linha, sizeof(linha), "RA: 0x%08lx\nSP: 0x%08lx\nMCAUSE: 0x%08lx\nMTVAL: 0x%08lx\n",
             (unsigned long)resumo.ex_info.ra, (unsigned long)resumo.ex_info.sp,
             (unsigned long)resumo.ex_info.mcause, (unsigned long)resumo.ex_info.mtval);
    resultado += linha;
    resultado += "Pilha RISC-V (decodificar com o ELF deste firmware):\n";
    size_t bytes = resumo.exc_bt_info.dump_size;
    if (bytes > sizeof(resumo.exc_bt_info.stackdump)) bytes = sizeof(resumo.exc_bt_info.stackdump);
    for (size_t i = 0; i < bytes; ++i) {
        if (i % 16 == 0) {
            snprintf(linha, sizeof(linha), "0x%08lx:", (unsigned long)(resumo.ex_info.sp + i));
            resultado += linha;
        }
        snprintf(linha, sizeof(linha), " %02x", resumo.exc_bt_info.stackdump[i]);
        resultado += linha;
        if (i % 16 == 15 || i + 1 == bytes) resultado += '\n';
    }
#else
    resultado += "Backtrace:";
    for (uint32_t i = 0; i < resumo.exc_bt_info.depth &&
         i < sizeof(resumo.exc_bt_info.bt) / sizeof(resumo.exc_bt_info.bt[0]); ++i) {
        snprintf(linha, sizeof(linha), " 0x%08lx", (unsigned long)resumo.exc_bt_info.bt[i]);
        resultado += linha;
    }
    resultado += "\nBacktrace corrompido: " + String(resumo.exc_bt_info.corrupted ? "SIM" : "NAO") + "\n";
#endif
    char hash[sizeof(resumo.app_elf_sha256) + 1] = {};
    memcpy(hash, resumo.app_elf_sha256, sizeof(resumo.app_elf_sha256));
    resultado += "ELF SHA256: " + String(hash) + "\n";
    String caminho, erroSd;
    if (!sd.salvarCrash(resultado, caminho, erroSd))
        return resultado + "Falha ao arquivar crash: " + erroSd + "\nRegistro interno preservado.\n";
    resultado += "Crash salvo no SD: " + caminho + "\n";
    erro = esp_core_dump_image_erase();
    resultado += erro == ESP_OK ? "Registro de crash apagado apos leitura.\n"
        : "Falha ao apagar registro de crash: " + String(esp_err_to_name(erro)) + "\n";
#else
    resultado += "Core dump nao habilitado neste firmware.\n";
#endif
    return resultado;
}
