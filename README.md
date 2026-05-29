# Desktop Buddy BTTF

Arduino firmware for an interactive desktop companion with WT32-SC01 Plus touch display, MQTT terminal interface, and themed "time circuit" idle screen inspired by the DeLorean from Back to the Future.

This project receives real-time messages via MQTT broker, displays them in a terminal viewport, allows the operator to select and send predefined responses, and transitions to an animated "time circuit" clock display with toggleable LEDs and animated flux capacitor.

## Features at a Glance

- 📨 **Real-time MQTT messaging**: Receive inbound messages and display in terminal
- 🎨 **Dual-screen UI**: Terminal for messaging, "time circuit" clock for idle state
- 🔘 **Touch-responsive controls**: Select actions, send responses, toggle LED displays
- ⚡ **Smooth transitions**: Pixel-art loading animation between screen modes
- 📊 **Device status tracking**: Publish device state, WiFi/MQTT health, and user actions
- 🔐 **Secure TLS**: Configured for secure MQTT brokers with certificate validation
- 🎬 **Custom graphics**: 7-segment displays, animated flux capacitor, brushed steel UI theme

## Architecture

This is a **real-time embedded system** showcasing:
- **State machine design** (Terminal ↔ Clock screen switching)
- **MQTT enterprise patterns** (retained status, QoS levels, JSON payloads)
- **Hardware integration** (ESP32-S3 GPIO, I2C touch controller, parallel LCD bus at 40 MHz)
- **Graphics optimization** (efficient 7-segment rendering, animation frame throttling)
- **Professional error handling** (WiFi reconnection, MQTT fallback modes, message deduplication)

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed state diagram and message flow.

## Hardware

- **Board**: WT32-SC01 Plus (ESP32-S3 dual-core @ 240 MHz, 8 MB PSRAM)
- **Display**: ST7796 ILI9488 (3.5" TFT, 320×480 pixels, 8-bit parallel interface)
- **Touch Controller**: FT5x06 (I2C @ 400 kHz)
- **Connectivity**: WiFi 802.11 b/g/n, 2 UART serial, GPIO for LED control (future)
- **Framework**: Arduino IDE (PlatformIO compatible)

## Tech Stack

| Layer | Component | Role |
|-------|-----------|------|
| **Graphics** | [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Fast, low-level LCD driver abstraction |
| **Messaging** | [PubSubClient](https://github.com/knolleary/pubsubclient) | MQTT 3.1.1 client with TLS support |
| **Serialization** | [ArduinoJson](https://arduinojson.org/) | Lightweight JSON parsing (statically allocated) |
| **MCU** | ESP32-S3 IDF (Arduino core) | FreeRTOS, WiFi stack, NTP/TLS |

## Project Structure

```
desktop_buddy_bttf/
├── desktop_buddy_bttf.ino       # Main firmware: MQTT, touch, state machine, event loop
├── BTTF_Screens.h               # TimeCircuitPanel class: rendering & animation
├── _delorean_codegen.h          # Pixel-art transition asset (loading animation)
├── _relogio_codegen.h           # Additional visual assets
├── secrets.example.h            # Template for credentials (NOT tracked)
├── secrets.h                    # Local credentials (Git-ignored)
├── README.md                    # This file
├── ARCHITECTURE.md              # State diagram, MQTT flow, design rationale
└── LICENSE                      # MIT License
```

## Getting Started

### 1. Install Dependencies

Open Arduino IDE → **Sketch → Include Library → Manage Libraries**, then install:

- **LovyanGFX** (v1.1.0+) — Graphics driver for ST7796
- **PubSubClient** (v2.8.0+) — MQTT client
- **ArduinoJson** (v7.0.0+) — JSON parsing

### 2. Configure Secrets

Copy `secrets.example.h` → `secrets.h` and fill in your credentials:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// WiFi Configuration: Dual Network with Automatic Failover
// Primary: preferred network (e.g., home WiFi, 5 GHz)
const char* WIFI_SSID_PRIMARY = "your-primary-network";
const char* WIFI_PASS_PRIMARY = "your-primary-password";

// Secondary: fallback network (e.g., guest WiFi, 2.4 GHz, mobile hotspot)
// Leave as "" if you don't have a secondary network
const char* WIFI_SSID_SECONDARY = "your-secondary-network";
const char* WIFI_PASS_SECONDARY = "your-secondary-password";

// WiFi Failover Timings
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;  // Timeout per attempt
const unsigned long WIFI_PRIMARY_RETRY_MS = 30000;    // Check primary every 30 sec

// MQTT Broker (TLS)
const char* MQTT_HOST = "your-broker-hostname.example.com";
const int   MQTT_PORT = 8883;  // TLS port
const char* MQTT_USER = "your-mqtt-username";
const char* MQTT_PASS = "your-mqtt-password";

// CA Certificate (get from your broker)
const char* MQTT_ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"MIIBkjCB+wIJAKHHCgVFH5NCMA0GCSqGSIb3DQEBBQUAMBMxETAPBgNVBAMMCCpz\n"
"...(base64-encoded certificate)...\n"
"-----END CERTIFICATE-----\n";

#endif
```

### 3. Select Hardware Target

- **Board**: Select `ESP32S3 Dev Module` (or `WT32-SC01 Plus` if available in your board manager)
- **Port**: Select your USB serial port
- **Upload Speed**: 921600 baud

### 4. Upload Firmware

```bash
Sketch → Upload (Ctrl+U)
```

Open **Serial Monitor** (115200 baud) to watch boot progress:
```
Conectando Wi-Fi...
Wi-Fi OK: 192.168.1.100
MQTT: CONECTADO
```

### 5. Test with Sample Messages

Publish a test message to `home/llm/inbound`:

```bash
mosquitto_pub -h your-broker.example.com \
  -p 8883 \
  --cafile ca.crt \
  -u your-mqtt-user \
  -P your-mqtt-pass \
  -t home/llm/inbound \
  -m '{"source":"demo","message":"Hello from MQTT!"}'
```

You should see the message appear in the **Terminal** screen on the device.

## MQTT Contract

### Topics Subscribed

| Topic | QoS | Purpose |
|-------|-----|---------|
| `home/llm/inbound` | 1 | Inbound messages displayed in terminal |
| `home/llm/command` | 1 | Command directives (reserved for future use) |

### Topics Published

| Topic | Retain | Purpose |
|-------|--------|---------|
| `home/llm/response` | No | User-selected action (`SIM`, `NAO`, `REVERTA`, `PARE`) |
| `home/llm/status` | **Yes** | Device health & telemetry (heartbeat every 45 sec) |

### Example Payloads

**Inbound message** (`home/llm/inbound`):
```json
{
  "source": "n8n",
  "message": "Deploy to production?",
  "timestamp": 1234567890
}
```

**User response** (`home/llm/response`):
```json
{
  "action": "SIM",
  "source": "esp32_touch",
  "device": "DESKTOP-BUDDY-BTTF",
  "ts_ms": 45821933
}
```

**Device status** (`home/llm/status`, retained):
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

## Dual Network Failover

The device automatically switches between primary and secondary WiFi networks:

```
┌─────────────────────┐
│  Primary WiFi       │  ◄─── Preferred (5 GHz, home network)
│  (e.g., home AP)    │
└─────────────────────┘
         │
         ├─ Connected ──► Use primary
         │
         └─ Unavailable ──┐
                          ▼
                  ┌─────────────────────┐
                  │  Secondary WiFi     │  ◄─── Fallback (2.4 GHz, guest)
                  │  (e.g., guest AP)   │
                  └─────────────────────┘
                          │
                          └─ Used until primary returns
                                    │
                  ┌─────────────────┴──────────────┐
                  │  Primary detected (every 30s)  │
                  └─────────────────┬──────────────┘
                                    │
                                    └─► Switch back to primary
```

**Status bar shows**: `NET: PRIMARY` (green) / `NET: SECOND` (amber) / `NET: OFF` (red)

If you only have one WiFi network, leave `WIFI_SSID_SECONDARY = ""` and the device will just use the primary network.

## Message Flow

```
┌─────────────────────────────────────────────────────┐
│          MQTT Broker (e.g., Mosquitto)              │
└────────────┬────────────┬────────────────────────────┘
             │            │
         [inbound]    [response]
             │            │
    ┌────────▼──────┐     │
    │   Terminal    │     │
    │   Viewport    │     │
    │   + Selector  │────►│ Publish action
    │   + [SEND]    │     │
    └─────────────────────┘

    ┌──────────────────────┐
    │  Status Heartbeat    │
    │  (every 45 seconds)  │────► [status] (retained)
    └──────────────────────┘
```

**State Transitions**:
- **Terminal → Clock**: Tap the "LLM TRANSMISSION" label at top
- **Clock → Terminal**: Tap the flux capacitor or envelope icon
- **Status Update**: Automatic after any action or every 45 seconds

## User Interface

### Terminal Screen

- **Top panel**: Status bar (WiFi OK/OFF, MQTT CONNECTED/DISCONNECTED, MQTT return code)
- **Viewport**: 20-line scrolling message history, right-aligned source labels
- **Selector**: Rotates through actions: `SIM` → `NAO` → `REVERTA` → `PARE`
- **Send button**: Publishes selected action and updates device status

**Colors**: Green text on black background (retro CRT aesthetic)

### Time Circuit Clock Screen

- **4 time rows** (each 82×300 pixels, with 7-segment displays):
  1. **DESTINATION TIME** (red LEDs)
  2. **LAST TIME DEPARTED** (green LEDs)
  3. **SYNC POINT** (amber LEDs)
  4. **PRESENT TIME** (green, updates every second from NTP)

- **Animated flux capacitor**: Pulsing energy effect (Y-shaped circuit with glowing core)
- **Control buttons** (right side):
  - **Flux button**: Toggle flux capacitor animation
  - **Panel button**: Toggle LED brightness

**Colors**: Brushed steel background, red/green/amber 7-segment displays

## Build and Test

### Compile

1. Open `desktop_buddy_bttf.ino` in Arduino IDE
2. **Sketch → Verify** to check syntax (no hardware needed)

### Flash to Device

1. Connect WT32-SC01 Plus via USB-C
2. **Sketch → Upload**
3. Wait ~10 seconds for upload to complete

### Validate on Device

1. Open **Tools → Serial Monitor** (115200 baud)
2. Watch for boot messages:
   ```
   Conectando Wi-Fi...
   Wi-Fi OK: 192.168.x.x
   MQTT: CONECTADO
   ```

3. Publish test message to `home/llm/inbound`:
   ```bash
   mosquitto_pub -h <broker> -p 8883 \
     --cafile ca.crt -u <user> -P <pass> \
     -t home/llm/inbound \
     -m '{"source":"test","message":"Hello device!"}'
   ```

4. **Verify on screen**:
   - Message appears in terminal
   - Tap selector to choose action
   - Tap SEND button
   - Check `home/llm/response` topic for published action
   - Check `home/llm/status` topic for updated device state

5. **Verify screen transitions**:
   - Tap "LLM TRANSMISSION" label → goes to clock
   - Tap flux capacitor → loading animation → back to terminal

### Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|------|---|
| **No boot messages in Serial Monitor** | Wrong baud rate | Set to 115200 |
| **WiFi connects but MQTT fails** | Missing/incorrect CA cert | Verify `MQTT_ROOT_CA` in `secrets.h` |
| **MQTT fails 3 times, then works** | TLS cert issue → fallback mode | Check broker certificate is current |
| **Messages not displaying** | Wrong topic subscription | Verify sending to `home/llm/inbound` |
| **Screen stays blank** | Hardware not initialized | Check pin config matches WT32-SC01 Plus schematic |

## What You'll Learn

This project demonstrates real-world embedded systems skills:

- **ESP32 development**: GPIO, WiFi stack, I2C, UART, memory management
- **Networking**: Secure TLS/MQTT, JSON serialization, connection pooling
- **Real-time graphics**: Display driver control, high-speed parallel bus, animation timing
- **Touch input**: Capacitive sensor calibration, debouncing, hit detection
- **State machines**: Clean screen switching with initialization/cleanup
- **Low-level optimization**: 7-segment rendering, color quantization (RGB565), frame timing
- **Professional practices**: Secrets management, Git hygiene, hardware abstraction

## License

MIT License — See [LICENSE](LICENSE) file for details.

## Contributing & Support

This is a personal portfolio project. For questions or to adapt it for your use case, see the code comments or review [ARCHITECTURE.md](ARCHITECTURE.md).

