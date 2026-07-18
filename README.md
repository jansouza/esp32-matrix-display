# ESP32 Matrix Display

Firmware for ESP32 that controls an LED matrix (MAX7219/MAX72XX, via
[MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX)) and turns it into a
WiFi-connected scrolling display, controllable via a web interface and a simple
REST API. Designed to grow beyond text messages — next steps include a clock
mode and other widgets.

## Features

- **Web interface** to type and send scrolling messages to the display;
  the message box comes prefilled with the last message sent.
- **REST API** (`GET /api/message?msg=...`) to send messages programmatically.
- **Optional API key authentication** (`X-API-Key` header), generated and managed via the web interface.
- **WiFi setup portal**: if the configured network is not found, the device
  starts an Access Point (`MD-Display-Setup`) so you can configure the
  SSID/password from your browser.
- **Persistent settings** (scroll speed, brightness, display mode) saved to
  NVS via `Preferences`, surviving reboots.
- **Built-in custom icons** inserted into messages with tags such as
  `[heart]`, `[wifi]`, `[smile]`, `[clock]`, `[star]`, and others.
- **Static mode with blink**, in addition to the default scroll mode.
- **Factory reset**: hold the BOOT button while powering on/resetting to erase
  all saved settings and credentials.

## Hardware

Tested with ESP32 + FC16 LED matrix module (MAX7219), 4 modules chained.

| Matrix (MAX7219) | ESP32 (hardware SPI / VSPI) |
|---|---|
| VCC      | VIN |
| GND      | GND |
| DIN      | D23 |
| CS       | D5  |
| CLK      | D18 |

Pins can be adjusted at the top of the `.ino` file (`CLK_PIN`, `DATA_PIN`, `CS_PIN`).

![ESP32 MATRIX MAX7219](assets/esp32_matrix_max7219.png)

## Dependencies

- [Arduino core for ESP32](https://github.com/espressif/arduino-esp32)
- [MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX) (matrix control library)

## Getting Started

1. Compile and flash the firmware to the ESP32 (Arduino IDE or `arduino-cli`).
2. On first boot (no WiFi configured), the device starts the Access Point
   **MD-Display-Setup**. Connect to it and open the IP shown on the display
   (or `192.168.4.1`) in your browser.
3. Configure your network SSID/password via the web interface. The device
   restarts and attempts to connect; if it fails, it returns to setup mode.
4. Once connected, the display shows the assigned IP — open it in your browser
   to send messages, adjust brightness/speed, and manage the API key.

## REST API

```
GET /api/message?msg=Hello%20World
```

Optional parameters: `spd` (scroll speed, 20–250 ms), `brt` (brightness,
0–15), and `mode` (`0` = scroll, `1` = static with blink). Icon tags like
`[heart]` work normally in the message.

### curl Examples

Send a message (no authentication):

```bash
curl "http://192.168.1.50/api/message?msg=Hello%20World"
```

With authentication enabled, send the API key (generated in the **API** tab of
the web interface) via the `X-API-Key` header:

```bash
curl -g -H "X-API-Key: 3f8a1c9b2e7d4f60a5b8c1d2" \
  "http://192.168.1.50/api/message?msg=Temperature%2023C%20[sun]"
```

> The `-g` (`--globoff`) flag is required when the message contains icon tags:
> without it, curl interprets `[` and `]` as range syntax and fails with
> `bad range in URL`. Alternative: URL-encode the brackets (`%5Bsun%5D`).

Adjusting speed, brightness, and mode as well:

```bash
curl -H "X-API-Key: 3f8a1c9b2e7d4f60a5b8c1d2" \
  "http://192.168.1.50/api/message?msg=Alert!&spd=40&brt=15&mode=1"
```

JSON responses:

```json
{"ok":true,"msg":"Hello World"}
{"ok":false,"error":"missing msg parameter"}
{"ok":false,"error":"unauthorized"}
```

> Replace `192.168.1.50` with the IP shown on the display and the example key
> with yours, displayed in the web interface.

## Factory Reset

Hold the **BOOT** button on the ESP32 during power-on/reset to erase all saved
settings (WiFi, brightness, speed, API key) and return to factory defaults.

## Roadmap

- [ ] Clock mode (NTP)
- [ ] Additional widgets/display modes

## License

Define the project license here (e.g., MIT).
