# CloudTech — Data Center Guard (Projeto Final CCIDC 2026.2)

Monitoramento inteligente de temperatura de um rack de servidores com **ESP32 + DHT22 + LED** no Wokwi, telemetria no **Adafruit IO** e painel web próprio (`index.html`).

## Arquitetura

- **Wokwi (ESP32)**: leitura local do DHT22 a cada 1 s e publicação de telemetria a cada 4 s (laço `for`, sem `delay` bloqueante).
- **Edge Computing**: o próprio ESP32 liga o exaustor (LED) quando a temperatura atinge **28 °C** e recusa desligamento manual enquanto estiver quente.
- **Adafruit IO**: feeds `temperatura` e `led`, com dashboard (Gauge, Line Chart e Toggle).
- **Painel Web (`index.html`)**: conectado por WebSocket (`wss://io.adafruit.com:443`), com visor, alerta térmico, botões sincronizados, consulta `/get` e gráfico em tempo real.

## Hardware

| Componente | Conexão |
|---|---|
| DHT22 VCC | ESP32 3V3 |
| DHT22 SDA | ESP32 GPIO 15 (D15) |
| DHT22 GND | ESP32 GND |
| LED (ânodo) | Resistor 220 Ω → ESP32 GPIO 2 (D2) |
| LED (cátodo) | ESP32 GND |

## Bibliotecas

- Adafruit MQTT Library
- DHT sensor library for ESPx

## Como executar o painel web

```bash
python3 -m http.server 8000
```

Depois abra: `http://localhost:8000/index.html`

## Credenciais

Por segurança, a chave do Adafruit IO **não** está no repositório. Substitua os placeholders localmente:

- `sketch.ino`: `#define AIO_KEY "SUA_CHAVE_AQUI"`
- `index.html`: `const adafruitKey = 'SUA_CHAVE_AQUI';`

## Estrutura

```
.
├── sketch.ino       # firmware ESP32 (Wokwi)
├── diagram.json     # circuito do Wokwi
├── libraries.txt    # bibliotecas do Wokwi
└── index.html       # painel web (WebSocket + gráfico)
```
