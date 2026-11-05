## Personalization and histories (2026-11-04)

- **personalizar** / **tela:12**: touch settings for watch face, accent, night schedule/intensity and motion. Hold a clock for 800 ms to open it; hold another page to return to the customized clock.
- **relogio** / **tela:10**: selected Instrumental, Minimalist or Digital clock. Stays visible without Internet using RTC fallback, with no screensaver.
- **historico_bateria** / **tela:11**: session battery graph, voltage and estimated autonomy. Five-minute SD archive: **sd_read:/battery_history.csv**.
- **tela:8**: tap to switch latency/RSSI graphs; local gateway ping does not prove Internet access.
- **tela:0..12**: page IDs 0..10 remain compatible. Night brightness defaults to 22:00–07:00, capped at 12%; edit hours/intensity by touch.
- Motion: rest flat then lift to wake; hold an inverted portrait pose for 1.2 seconds to rotate 180 degrees. Disable through Personalize. Explicit tela_off is respected.

# RTC and network time update

- `ntp_status` / `rtc_status`: selected time source, valid displayed time, hardware RTC status/error, latest NTP response age and RTC corrections.
- `time` and `date`: confirmed network time when available, otherwise the valid hardware RTC.
- `set_time:YYYY-MM-DD HH:MM:SS`: set local wall time and persist its UTC equivalent to RTC.
- The digital/analog clocks label NTP or RTC. A valid RTC value alone no longer reports NTP synchronization. See README.md for freshness, validation and backup-power behavior.

# Battery and analog clock update

- `bateria` / `battery`: charge %, used capacity %, voltage, power state and an observed discharge-rate estimate. No instantaneous mA/mW measurement is claimed.
- `relogio` / `tela:10`: select the analog clock; this page stays selected with no screensaver or idle brightness reduction; the night schedule still applies. Swipe to leave.
- Direct selection now accepts `tela:0..12`. Existing IDs remain unchanged.

# Current navigation and power update

Swipe up/left for the next page; the former bottom button and its hit area have been removed. Swipe down/right for the previous page. A tap elsewhere only wakes the screen. Hold touch to return to the clock. Short PWR requests orderly shutdown; hold PWR about 1 second to power on, or 6 seconds for emergency power-off. See README.md for supply-related behavior and validation status. This update supersedes older navigation descriptions below.

# Adaptação de navegação

Toque curto/deslize à esquerda: próxima página. Deslize à direita: anterior. Segure o touch por 800 ms e solte: relógio. BOOT GPIO0 mantém as ações de 3/10 segundos. Consulte LEIA-ME.md para revisão V1/V2 e configuração.

# Comandos do AMOLED Gateway Gateway

Documentação do firmware atual, revisada em 31/10/2026.
Inclui as alterações de navegação e intervalo do clima do commit `5c56bcf`.
Histórico local: [changelog.md](changelog.md); visão de todos os
firmwares: [catálogo geral](../../ESP32_Documents/COMANDOS.md).

## Como enviar

- **USB serial:** 115200 baud. Envie um comando por linha, terminando com Enter (LF, CR ou CR+LF).
- **UDP:** envie o comando ao IP da placa, porta **4210**. A resposta volta ao endereço e à porta do remetente.
- Respostas longas podem chegar em vários datagramas de até 1200 bytes. O cliente deve continuar recebendo até um período de silêncio; UDP não garante entrega nem ordenação. Em falha local de envio, o firmware reabre a porta e repete o bloco uma vez, sem executar novamente o comando.
- Limite de entrada: **255 bytes** por comando. No serial, linhas maiores ou com NUL são descartadas até a terminação; UDP acima do limite é recusado com erro, sem executar trecho truncado.
- O nome do comando é convertido para minúsculas. Os parâmetros preservam maiúsculas, minúsculas e espaços.
- Use dois-pontos para separar parâmetros. Não acrescente aspas aos exemplos.

Exemplos para o Monitor Serial:

```text
help
info
wifi_list
brilho:80
tela:0
clima_sync
```

`help` responde com `COMANDOS` e um item por linha. Os argumentos aparecem
após `:`; descrições ficam neste guia, fora da saída parseável.

## Sistema e diagnóstico

| Comando | Função |
| --- | --- |
| `help` | Lista os comandos do firmware. Os comandos SD estão habilitados. |
| `name` | Retorna o nome configurado no dispositivo. |
| `alive` | Responde `alive`. |
| `info` | Resumo de firmware, rede, memória, data/hora, uptime e reset. |
| `status` | Estado de Wi-Fi, NTP e heap livre. |
| `version` | Versão calculada pelo firmware. |
| `build` | Data e hora da compilação. |
| `uptime` | Tempo desde a inicialização em dias, horas, minutos e segundos. |
| `reason` | Motivo do último reset e uptime atual. |
| `init` | Consulta o motivo do reset; não reinicializa o dispositivo. |
| `crash_info` | Consulta o diagnóstico de crash, se core dump estiver habilitado na compilação. Com SD, arquiva em /crashes antes de apagar o registro interno; sem cartão, preserva o registro. |
| `cpu` | Modelo, revisão, número de núcleos e frequência da CPU. |
| `chip_info` | Informações do chip e versão do SDK. |
| `temp` | Temperatura interna do chip; não é a temperatura ambiente. |
| `ram` | Estatísticas de heap e uso de RAM. |
| `heap` | Heap livre em bytes e KB. |
| `heap_min` | Menor heap livre registrado desde o boot. |
| `flash` | Informações de flash e espaço do firmware. |
| `flash_info` | Tamanho da flash, firmware e espaço para OTA. |
| `health` | Resumo de Wi-Fi, NTP, RAM e atualização do clima. |
| `selftest` | Verificações de Wi-Fi, NTP, RAM, clima e frequência da CPU. |
| `reboot` | Reinicia a placa após aproximadamente 500 ms. |
| `reset_counters` | Zera somente os contadores NVS persistentes de boot/watchdog; preserva as configurações Prometheus. |

`health` e `selftest` consideram RAM suficiente a partir de 40 KB de heap livre e clima recente até 30 minutos. Não são testes físicos completos dos periféricos.

## Wi-Fi e rede

| Comando | Função |
| --- | --- |
| `wifi_add:SSID:senha` | Salva ou atualiza uma rede, com confirmação de leitura em NVS. Aplicada na próxima reconexão. |
| `wifi_list` | Lista os SSIDs salvos, sem as senhas. |
| `wifi_status` | Estado da conexão, SSID, IP e RSSI. |
| `net_info` | SSID, IP, gateway, máscara, DNS e RSSI. |
| `net_monitor` | Estatísticas do monitor de rede e ping ao gateway. |
| `mac` | Endereço MAC da interface Wi-Fi. |
| `ip` | IP da conexão Wi-Fi. |
| `ssid` | Nome da rede conectada. |
| `rssi` | Intensidade do sinal em dBm. |
| `channel` | Canal Wi-Fi atual. |
| `reset_wifi` | Apaga os perfis Wi-Fi salvos e reinicia. |

`reset_wifi` limpa o namespace `wifi`, incluindo o cursor dos perfis, e agenda
o reinício para aproximadamente um segundo depois. Preserva os namespaces
`ntp_cfg` (fuso/DST) e `telemetry` (Prometheus). Não apaga o firmware.

Há até cinco perfis de rede, com atualização circular. Para rede aberta, mantenha o último dois-pontos:

```text
wifi_add:MinhaRede:MinhaSenha
wifi_add:RedeAberta:
```

O primeiro dois-pontos após `wifi_add:` separa SSID e senha: esse formato não permite dois-pontos no SSID. A senha pode conter dois-pontos. Espaços fazem parte das credenciais.

Ping ao gateway mede a resposta do roteador; não comprova acesso à Internet.

## Tela e brilho

| Comando | Função |
| --- | --- |
| `tela:N` | Seleciona uma página, de `0` a `9`. |
| `rotacao:G` | Gira o display para `0`, `90`, `180` ou `270` graus. Exemplo: `rotacao:90`. |
| `tela_next` | Avança à próxima tela distinta, pulando apenas páginas duplicadas. |
| `brilho:P` | Define o brilho de `0` a `100` por cento. |
| `tela_on` | Liga a iluminação com brilho de 80%. |
| `tela_off` | Define brilho zero; os serviços de rede continuam ativos. |

| Página | Conteúdo |
| --- | --- |
| 0 | Relógio, IP e temperatura do clima |
| 1 e 2 | NTP, data/hora, fuso e DST |
| 3 e 4 | Informações de rede |
| 5 | Sistema, heap, PSRAM e uptime |
| 6 | Cinco redes salvas |
| 7 | Estado do cartão SDMMC |
| 8 | Monitor de rede |
| 9 | Previsão de Porto Alegre |

As páginas 1/2 e 3/4 usam o mesmo conteúdo nesta adaptação. O console ocupa a tela por cerca de 10 segundos após um comando; comandos de navegação selecionam a página imediatamente.

### Navegação pelo botão (18/10/2026)

Cada toque curto avança uma tela ao soltar: relógio → NTP → rede → sistema → redes salvas → SD → monitor → previsão → relógio. Apenas as páginas duplicadas são puladas nessa sequência, inclusive por `tela_next`; `tela:N` mantém a numeração da tabela.

Após 20 segundos sem novo toque, a página volta ao relógio. Cada toque reinicia esse prazo; segurar o botão mantém as ações de 3/10 segundos. Selecionar uma página por comando também concede 20 segundos de visualização. O retorno automático não conta como interação para o protetor, que continua entrando após 60 segundos.

Após **60 segundos sem interação**, aparecem dois olhos ciano com movimento suave e piscadas. Botão ou comando sai do protetor. Após dois minutos, o brilho cai para no máximo 20%; uma interação restaura o valor configurado. Se o brilho configurado for zero, use `tela_on` ou `brilho:P` para voltar a iluminar.

## Data e hora

| Comando | Função |
| --- | --- |
| `time` | Consulta a hora local. |
| `date` | Consulta a data local. |
| `ntp_status` | Estado de sincronização e data/hora quando disponíveis. |
| `set_fuso:X` | Define o fuso UTC inteiro entre `-12` e `14`. Exemplo: `set_fuso:-3`. |
| `dst_on` | Ativa o ajuste de horário de verão. |
| `dst_off` | Desativa o ajuste de horário de verão. |
| `set_time:AAAA-MM-DD HH:MM:SS` | Ajusta manualmente a data e hora local. |

Exemplo: `set_time:2026-10-17 14:30:00`. O fuso e o ajuste de horário de verão ficam salvos em NVS. O NTP pode atualizar posteriormente a hora ajustada manualmente.

## LED RGB

Esta placa não possui o RGB LP5562 do Atom. Os comandos led_* retornam indisponibilidade. O separador da tela indica o estado da rede.

## Clima

| Comando | Função |
| --- | --- |
| `clima` | Temperatura, condição e código WMO do cache atual. |
| `clima_age` | Tempo desde a última atualização válida. |
| `clima_status` | Diagnóstico de Wi-Fi, tarefa ocupada, pausa OTA, tentativas, sucessos, código HTTP e resultado da última consulta. |
| `previsao` | Data, mínima, máxima, chance de chuva e idade da previsão diária. |
| `clima_sync` | Solicita uma consulta em segundo plano, se houver Wi-Fi. |

`clima_status` também informa se a tarefa permanente está reservada, o heap interno livre e o maior bloco livre. Compare os valores após consultas concluídas, com a tarefa livre. A consulta reutiliza a tarefa criada no boot; `clima_sync` informa quando não consegue agendar, em vez de confirmar uma solicitação que falhou.

Os dados são do Open-Meteo para **Porto Alegre**, herdados do projeto de origem. A primeira consulta ocorre assim que houver Wi-Fi; as consultas automáticas seguintes ocorrem a cada **15 minutos**, inclusive após falha, contados desde a última tentativa. Sem Wi-Fi ou durante OTA, a consulta aguarda a disponibilidade do serviço. `clima_sync` permite antecipar uma consulta e confirma a solicitação, não a conclusão. Consulte `clima_age` e `previsao` para verificar a atualização; `clima` isoladamente não comprova que o cache já foi sincronizado.

## Cartão SD habilitado

Use caminhos iniciados em /. Não há formatação automática.

| Comando | Ação |
| --- | --- |
| sd_status | Montagem, tipo, capacidade e uso |
| sd_test | Grava, lê e remove arquivo temporário próprio |
| sd_list:/ | Lista arquivos |
| sd_write:/arquivo.txt:texto | Substitui/cria arquivo |
| sd_append:/arquivo.txt:texto | Acrescenta texto e quebra de linha |
| sd_read:/arquivo.txt | Últimos 1000 bytes |
| sd_del:/arquivo.txt | Remove arquivo |
| sd_mkdir:/pasta | Cria diretório |
| sd_rmdir:/pasta | Remove diretório vazio |
| sd_log | Últimos 1000 bytes de /log.txt |
| sd_clear_log | Apaga /log.txt |

net_history lê o histórico do cartão. /logs mostra aplicação e rede. Sem cartão no boot, um comando SD tenta montá-lo novamente. Para trocar cartão já montado, reinicie a placa.

## Botão e endereços

- Pressão curta no botão BOOT GPIO0: próxima tela distinta ao soltar e saída do protetor; retorno ao relógio após 20 segundos sem novo toque.
- Segurar por 3 segundos e soltar: reinicia.
- Segurar por 10 segundos e soltar: apaga os perfis Wi-Fi e reinicia.
- AP de configuração: **AMOLED_CONFIG**; página Wi-Fi: `http://192.168.4.1/wifi`.
- Portal conectado: `http://IP_DA_PLACA/`, com `/info`, `/wifi`, `/logs` e `/prometheus` na porta 80.
- Métricas: `http://IP_DA_PLACA:9120/metrics` por padrão; a porta pode ser alterada no portal.
- Hostname OTA: **AMOLED-Gateway**.

Referência de configuração e bibliotecas: [LEIA-ME.md](LEIA-ME.md).


## Gravacao OTA

Selecione a porta de rede AMOLED-Gateway na Arduino IDE para atualizar o firmware. A primeira instalacao desta otimizacao ainda usa o firmware anterior; as otimizacoes passam a valer no boot seguinte.

A OTA recebe atendimento antes do portal e das animacoes. Durante a transferencia, a biblioteca atende exclusivamente a gravacao na tarefa principal: a tela mostra barra e porcentagem de progresso; botao e portal ficam temporariamente sem atualizacao. Novas consultas de clima sao impedidas; uma consulta em andamento pode terminar. O Wi-Fi fica sem economia de energia, a CPU usa 240 MHz e a prioridade da tarefa aumenta temporariamente. Frequencia e prioridade retornam aos valores anteriores em caso de erro; o Wi-Fi continua sem economia de energia para permitir nova tentativa rapida.

Na tela, a barra avança em passos de pelo menos 5 pontos percentuais, com intervalo mínimo de 250 ms; a chegada a 100% é exibida imediatamente. Durante a validação aparece “Verificando...”; o sucesso mostra “Concluido!” e falhas mostram o código de erro. O serial informa progresso a cada aproximadamente 10% e termina com bytes, segundos e KiB/s. Compare esse registro entre gravacoes para medir a taxa real. O tempo depende do sinal Wi-Fi, roteador, computador e tamanho do firmware; nao foi medida uma aceleracao na placa nesta revisao.

## Olhos e acelerometro

A carinha possui boca e sobrancelhas ciano, com expressões feliz, curiosa, surpresa e sonolenta. Elas mudam automaticamente a cada 3,5 a 6,5 segundos; um movimento forte da placa provoca surpresa por cerca de dois segundos. Não é necessário comando adicional.

No protetor, os olhos acompanham a inclinação lida pelo QMI8658, com piscadas automáticas. Sem leitura válida, o olhar usa movimento automático. Touch, botão ou comando acordam a tela.

O serial informa o resultado do QMI8658 na linha [BOARD]. O sensor nao cria um comando novo; basta aguardar os 60 segundos de inatividade e inclinar a placa.

## Limites e cuidados

- UDP e configuração web não oferecem autenticação; use somente rede confiável.
- O firmware atual não configura senha com `ArduinoOTA.setPassword()`.
  Não presuma que a senha usada em outro projeto ESP32 também se aplica aqui.
- `crash_info` depende do suporte de core dump da compilação. Sem SD, a falha
  de arquivamento preserva o dump; guarde o ELF correspondente ao crash.
- `init` não reinicia, `clima_sync` não espera a consulta terminar e
  `tela_off` não coloca a CPU em deep sleep.
- Esta revisão documental não confirma compilação completa, gravação ou teste
  físico das alterações locais.

## AMOLED visual refresh

The bottom next-page button and its touch target have been removed. Navigation is by swipes (up/left for next, down/right for previous); short taps only wake the screen and touch-hold returns to the clock. A compact top bar displays the page number, pinned-clock state, battery and Wi-Fi signal. RSSI colors are green at -60 dBm or better, amber down to -75 dBm, orange down to -85 dBm and red below that; AP mode and offline state are indicated separately.

UiPainter supplies bundled proportional FreeSans fonts in several sizes, pixel-measured text fitting, vector icons and colored cards. Main pages include illustrated weather, actual ping history, RAM/PSRAM availability bars and SD utilization. Unmeasured ping samples are omitted, and timeouts are marked separately. The analog clock and animated eyes use the recovered vertical space. One buffered frame is transferred per render.

No new installed graphics library is required. The font headers and upstream notices are included in UiFonts. See [font attribution](UiFonts/README.md). New UI changes still require user compilation and visual validation in portrait/landscape and over OTA.
