# Memorial Tecnico de Replicacao - Desktop Buddy BTTF

Este documento serve como guia de replicacao para quem quiser montar o projeto do zero, com foco em hardware WT32-SC01 Plus, pinagem real usada no firmware, configuracao de MQTT/TLS e validacao em campo.

## 1. Visao geral do sistema
O firmware transforma o WT32-SC01 Plus em um desktop buddy touch MQTT com dois modos de tela:
1. `Terminal`: recebe mensagens do broker, permite selecionar acao e publicar resposta.
2. `BTTF`: painel inspirado no relogio do DeLorean, com animacao local, chaves de LED e capacitor de fluxo.

O dispositivo opera como subscriber e publisher:
1. Recebe mensagens de entrada do broker.
2. Publica respostas de operador.
3. Publica status operacional em topico retained.

## 2. Hardware de referencia
1. Placa: WT32-SC01 Plus (ESP32-S3)
2. Display: ST7796, 320x480, barramento paralelo de 8 bits
3. Touch: FT5x06 via I2C
4. Alimentacao: USB-C, 5V estavel, fonte recomendada >= 2A
5. Ambiente de uso: bancada com bom aterramento e cabo USB de dados curto

## 3. Pinagem oficial usada no firmware
Abaixo esta a configuracao exatamente como esta no sketch Arduino atual.

### 3.1 Barramento paralelo do display (Bus_Parallel8)
| Sinal | GPIO |
|---|---|
| WR | 47 |
| RD | -1 (nao usado) |
| RS/DC | 0 |
| D0 | 9 |
| D1 | 46 |
| D2 | 3 |
| D3 | 8 |
| D4 | 18 |
| D5 | 17 |
| D6 | 16 |
| D7 | 15 |

### 3.2 Controle de painel
| Sinal | GPIO |
|---|---|
| CS | -1 (nao usado) |
| RST | 4 |
| BUSY | -1 |

### 3.3 Backlight (PWM)
| Parametro | Valor |
|---|---|
| BL pin | 45 |
| PWM channel | 7 |
| PWM freq | 44100 Hz |
| Invert | false |

### 3.4 Touch FT5x06 (I2C)
| Parametro | Valor |
|---|---|
| I2C port | 1 |
| SDA | 6 |
| SCL | 5 |
| Endereco | 0x38 |
| Frequencia | 400000 Hz |
| X range | 0..319 |
| Y range | 0..479 |

## 4. Stack de software
1. Arduino IDE (com core ESP32 instalado)
2. Biblioteca LovyanGFX
3. Biblioteca PubSubClient
4. Biblioteca ArduinoJson

Arquivos principais do projeto:
1. `desktop_buddy_bttf.ino`: conectividade, MQTT, touch, roteamento de telas e loop principal
2. `BTTF_Screens.h`: renderizacao da tela BTTF (mantida como baseline visual)
3. `_delorean_codegen.h`: sprite embutido usado na transicao de carregamento
4. `secrets.h`: credenciais e CA TLS

Nota de saneamento: este repositorio foi criado como baseline limpo para a identidade `Desktop Buddy BTTF`, sem carregar o nome ou historico do projeto legado.

## 5. Preparacao do Arduino IDE
Checklist minimo antes da compilacao:
1. Instalar o pacote ESP32 no Board Manager.
2. Selecionar uma placa ESP32-S3 equivalente ao WT32-SC01 Plus.
3. Instalar bibliotecas `LovyanGFX`, `PubSubClient` e `ArduinoJson`.
4. Confirmar porta serial correta.
5. Definir baud da serial em 115200 para logs de diagnostico.

## 6. Configuracao de credenciais (`secrets.h`)
O firmware espera as seguintes definicoes:
1. `WIFI_SSID`
2. `WIFI_PASS`
3. `MQTT_HOST`
4. `MQTT_PORT` (TLS tipicamente 8883)
5. `MQTT_USER`
6. `MQTT_PASS`
7. `MQTT_ROOT_CA` (PEM da autoridade certificadora do broker)

Boas praticas:
1. Nunca versionar credencial real.
2. Usar usuario MQTT exclusivo para o terminal.
3. Atualizar o certificado CA em caso de rotacao no broker.

## 7. Contrato MQTT atual
Topicos de subscribe:
1. `home/llm/inbound` (QoS 1)
2. `home/llm/command` (QoS 1)

Topicos de publish:
1. `home/llm/response` (acao selecionada, sem retain)
2. `home/llm/status` (status operacional, com retain)

Observacao importante sobre QoS:
1. O `PubSubClient` usa QoS 0 para publish.
2. O firmware usa QoS 1 no subscribe para reduzir perda na recepcao.
3. Para confirmacao estrita ponta a ponta, tratar idempotencia no consumidor.

## 8. Payloads esperados
Entrada aceita em `home/llm/inbound` e `home/llm/command`:
1. Texto bruto.
2. JSON com campos como `message`, `text`, `prompt` ou `question`.

Saida em `home/llm/response`:
1. `action`
2. `source`
3. `device`
4. `ts_ms`

Saida em `home/llm/status` (retained):
1. `device`
2. `project`
3. `screen`
4. `wifi`
5. `mqtt`
6. `last_action`
7. `selected_action`
8. `last_source`
9. `last_preview`
10. `uptime_s`

## 9. Fluxo de interface e touch
Hotspots atuais no `.ino`:
1. `clockFluxHotspot = {112, 386, 96, 82}`
2. `clockMsgIconHotspot = {282, 362, 24, 20}`
3. `terminalClockHotspot = {94, 46, 132, 18}`

Navegacao:
1. Terminal -> Relogio: toque na area da etiqueta superior.
2. Relogio -> Terminal: toque no capacitor de fluxo ou no icone de envelope.
3. Acoes de terminal: seletor de acao (`REVERTA`, `SIM`, `NAO`, `PARE`) e botao `ENVIAR`.

## 10. Sequencia de bring-up recomendada
1. Compilar e gravar firmware.
2. Abrir serial monitor e validar conexao Wi-Fi.
3. Confirmar handshake MQTT/TLS.
4. Verificar barra superior `MQTT: CONECTADO`.
5. Publicar mensagem de teste em `home/llm/inbound`.
6. Validar render da mensagem no terminal.
7. Selecionar acao local e tocar `ENVIAR`.
8. Confirmar recebimento no broker em `home/llm/response`.
9. Verificar `home/llm/status` retained ao reconectar um cliente monitor.

## 11. Testes de smoke para quem vai copiar
1. Teste de reconnect: desligar Wi-Fi e religar, validar reconexao automatica.
2. Teste de broker offline: parar broker, subir novamente, validar subscribe apos retorno.
3. Teste de touch: alternar telas e acionar `ENVIAR` repetidamente sem travamentos.
4. Teste de deduplicacao: enviar payload igual em menos de 1,5s e validar filtro local.

## 12. Troubleshooting rapido
Erro de compilacao `LovyanGFX.hpp`:
1. Biblioteca LovyanGFX nao instalada ou instalada em pasta errada.

Erro de compilacao `PubSubClient.h`:
1. Biblioteca PubSubClient ausente no Library Manager.

Wi-Fi conecta mas MQTT nao:
1. Conferir host, porta, usuario, senha e CA.
2. Validar relogio de sistema via NTP para TLS.
3. Verificar firewall/rede bloqueando 8883.

MQTT conecta mas nao recebe mensagens:
1. Confirmar topico exato.
2. Confirmar publish no mesmo broker/tenant.
3. Confirmar que payload nao extrapola buffer MQTT.

Touch impreciso:
1. Ajustar ranges `x_min/x_max/y_min/y_max`.
2. Revalidar hotspots com a orientacao vertical (`setRotation(0)`).

## 13. Boas praticas para evolucao
1. Introduzir versionamento de schema nos payloads (`schema_version`).
2. Criar namespace por ambiente (`dev/stg/prod`) nos topicos.
3. Adicionar telemetria de latencia (tempo entre RX e render na tela).
4. Extrair modulo de UI e modulo de MQTT para reduzir acoplamento.
5. Manter a tela BTTF como modulo isolado para proteger a identidade visual.

## 14. Nota de contexto visual
A tela de descanso e inspirada no painel do DeLorean, mas as datas fixas utilizadas no layout sao pessoais e deliberadas, sem dependencia funcional com o firmware MQTT.
