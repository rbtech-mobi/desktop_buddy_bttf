# Desktop Buddy BTTF

Firmware Arduino para um desktop buddy com tela touch WT32-SC01 Plus, terminal MQTT e tela de descanso inspirada nos circuitos de tempo do DeLorean.

O projeto recebe mensagens do broker, mostra no terminal, permite responder com acoes predefinidas e alterna para uma tela de relogio BTTF com botoes de liga/desliga dos LEDs, capacitor de fluxo animado e transicao com pixel art.

## O que ele faz

- Recebe mensagens MQTT em tempo real.
- Mostra as mensagens em uma tela terminal.
- Permite selecionar e enviar `REVERTA`, `SIM`, `NAO` ou `PARE`.
- Publica status operacional retained para outros consumidores.
- Exibe uma tela BTTF com relogio, LEDs e capacitor de fluxo.
- Volta da tela BTTF para o terminal pelo capacitor de fluxo ou pelo icone de mensagem.

## Hardware

- Board: WT32-SC01 Plus (ESP32-S3 + LCD touch 3.5")
- Display: ST7796, 320x480, paralelo 8 bits
- Touch: FT5x06 via I2C
- Framework: Arduino

## Dependencias

Instale pelo Arduino Library Manager:

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [ArduinoJson](https://arduinojson.org/)

## Estrutura

```text
desktop_buddy_bttf/
|-- desktop_buddy_bttf.ino       # Firmware principal: MQTT, touch, telas e loop
|-- BTTF_Screens.h               # Tela BTTF/relogio/capacitor de fluxo
|-- _delorean_codegen.h          # Pixel art da transicao de carregamento
|-- _relogio_codegen.h           # Assets visuais extras
|-- secrets.example.h            # Modelo de credenciais
`-- secrets.h                    # Credenciais locais, ignorado pelo Git
```

## Configuracao

Copie `secrets.example.h` para `secrets.h` e preencha os dados locais:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";

const char* MQTT_HOST = "your-broker-host";
const int   MQTT_PORT = 8883;
const char* MQTT_USER = "your-user";
const char* MQTT_PASS = "your-password";

const char* MQTT_ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"...\n"
"-----END CERTIFICATE-----\n";

#endif
```

## Contrato MQTT

### Subscribed

| Topic              | QoS |
|--------------------|-----|
| `home/llm/inbound` | 1   |
| `home/llm/command` | 1   |

### Published

| Topic               | Retain | Descricao |
|---------------------|--------|-----------|
| `home/llm/response` | No     | Acao selecionada no touch |
| `home/llm/status`   | Yes    | Heartbeat/status do dispositivo |

### Status payload

```json
{
  "device": "DESKTOP-BUDDY-BTTF",
  "project": "Desktop Buddy BTTF",
  "screen": "terminal",
  "wifi": true,
  "mqtt": true,
  "last_action": "SIM",
  "selected_action": "SIM",
  "last_source": "n8n",
  "last_preview": "Deploy to production?",
  "uptime_s": 3600
}
```

## Fluxo

```text
broker -> home/llm/inbound -> mensagem renderizada no terminal
operador -> seletor de acao -> publish home/llm/response
device -> heartbeat/status -> publish retained home/llm/status
```

## Interface

- Terminal: viewport de mensagens, seletor de acao e botao `ENVIAR`.
- BTTF: relogio inspirado no DeLorean, chaves de LED e capacitor de fluxo.
- Terminal -> BTTF: toque na etiqueta superior do terminal.
- BTTF -> Terminal: toque no capacitor de fluxo ou no icone de envelope.

## Build e teste

1. Instale as dependencias.
2. Configure `secrets.h`.
3. Selecione o target ESP32-S3 / WT32-SC01 Plus no Arduino IDE.
4. Grave o firmware.
5. Publique payloads de teste em `home/llm/inbound`.
6. Valide renderizacao, envio em `home/llm/response` e status retained em `home/llm/status`.

