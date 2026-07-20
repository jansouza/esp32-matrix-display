# REST API Reference

The ESP32 Matrix Display exposes a single REST endpoint on port 80:

```
GET /api/display
```

All parameters are passed as query string key-value pairs. No request body is used. The device always responds with JSON.

---

## Authentication

Authentication is **optional** and controlled from the **API** tab of the web interface.

When enabled, every request must include the `X-API-Key` header with the key shown in the web UI:

```
X-API-Key: <your-key>
```

An unauthorized request returns HTTP 200 with:

```json
{"ok": false, "error": "unauthorized"}
```

> The key can be regenerated at any time from the web interface. Regeneration does not require a reboot.

---

## Parameters

| Parameter | Type | Range / Values | Description |
|---|---|---|---|
| `msg` | string | up to 255 chars | Message text to display. Icon tags (e.g. `[heart]`) are expanded automatically. Sending `msg` without `display=` always switches to Message mode. |
| `spd` | integer | 20 – 250 | Scroll speed in milliseconds per step. Lower = faster. Saved to NVS (unless sent with `alert`). |
| `brt` | integer | 0 – 15 | Display brightness. 0 = minimum, 15 = maximum. Saved to NVS (unless sent with `alert`). |
| `mode` | integer | `0` / `1` / `2` | Message sub-mode: `0` = scroll, `1` = static with blink, `2` = blink+scroll. Saved to NVS (unless sent with `alert`). |
| `alert` | integer | 1 – 3600 | Show `msg` for N seconds, then automatically restore the previous state (mode, message, brightness, speed). Requires `msg`. Settings sent along with `alert` are also temporary. |
| `display` | string | `clock` / `message` | Switches the top-level display mode. `message` shows the current message; `clock` shows the NTP clock. Saved to NVS. |
| `tz` | integer | -720 – 840 | Timezone as a UTC offset in **minutes** (e.g. `-180` for UTC-3, `60` for UTC+1, `330` for UTC+5:30). Saved to NVS. |
| `date` | integer | `0` / `1` | Clock mode only. `1` = periodically show the date (`DD/MM`) for a few seconds. Saved to NVS. |
| `dateint` | integer | 5 – 3600 | Clock mode only. How often (in seconds) to show the date. Requires `date=1`. Saved to NVS. |

All parameters are optional. A request with no parameters is valid (returns `{"ok":true}`) but changes nothing.

### Message sub-modes (`mode`)

- **`0` — Scroll**: the message scrolls continuously across the display.
- **`1` — Blink**: the message is shown statically (truncated to what fits the display) and the whole panel blinks every 500 ms.
- **`2` — Blink+Scroll**: the part of the message that fits the display blinks for 5 seconds, then the text scrolls on once to reveal the hidden rest (at the normal `spd` speed), and the cycle repeats. A message that fits the display entirely just keeps blinking.

---

## Responses

All responses use `Content-Type: application/json`.

### Success

```json
{"ok": true}
```

When `msg` is included:

```json
{"ok": true, "msg": "Hello World"}
```

### Error — missing `msg` when required

```json
{"ok": false, "error": "missing msg parameter"}
```

### Error — unauthorized

```json
{"ok": false, "error": "unauthorized"}
```

---

## Icons

Icon tags can be embedded anywhere in a `msg` string. They are expanded to 8×8 pixel bitmaps on the display.

| Tag | Description |
|---|---|
| `[heart]` | Heart |
| `[wifi]` | WiFi symbol |
| `[smile]` | Smiley face |
| `[star]` | Star |
| `[sun]` | Sun |
| `[rain]` | Rain cloud |
| `[bell]` | Bell |
| `[music]` | Music note |
| `[clock]` | Clock face |
| `[warn]` | Warning triangle |
| `[bolt]` | Lightning bolt |
| `[fire]` | Flame |
| `[ok]` | Checkmark |
| `[x]` | Cross / X |
| `[pin]` | Location pin |
| `[plus]` | Plus sign |
| `[up]` | Up arrow |
| `[down]` | Down arrow |
| `[left]` | Left arrow |
| `[right]` | Right arrow |

> **curl note**: when a message contains `[` or `]`, use the `-g` (`--globoff`) flag to prevent curl from interpreting them as URL range syntax. Alternatively, URL-encode the brackets: `%5Bheart%5D`.

---

## Examples

Replace `192.168.1.50` with your device's IP (shown on the display at boot) and `<key>` with your API key.

### Send a plain message

```bash
curl "http://192.168.1.50/api/display?msg=Hello%20World"
```

### Send a message with authentication

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=Hello%20World"
```

### Send a message with icons

```bash
curl -g -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=Temp%2023C%20[sun]"
```

### Adjust speed, brightness and display mode

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=Alert!&spd=40&brt=15&mode=1"
```

- `spd=40` → fast scroll
- `brt=15` → maximum brightness
- `mode=1` → static blinking text

### Blink+Scroll mode

The visible part of the message blinks for 5 seconds, then the text scrolls on once to show the rest, repeating:

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=Meeting%20at%203pm%20room%20B12&mode=2"
```

### Temporary alert (auto-restore)

Show a message for 30 seconds, then the display automatically reverts to whatever it was showing before (clock or previous message), including the previous brightness and speed:

```bash
curl -g -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=[warn]%20Door%20open&alert=30&brt=15&mode=1"
```

A new alert while one is active replaces the text and resets the timer. The original pre-alert state is preserved. Sending a plain message or switching modes cancels any pending revert.

### Switch to clock mode

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?display=clock"
```

### Switch to clock mode with full configuration

Timezone UTC-3, date shown every 60 seconds:

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?display=clock&tz=-180&date=1&dateint=60"
```

### Switch back to message mode

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?display=message"
```

### Change brightness only (without changing the message)

```bash
curl -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?brt=3"
```

### Change brightness only during an alert (temporary)

```bash
curl -g -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=[moon]%20Night%20mode&alert=3600&brt=1"
```

### Multi-parameter: message + speed + brightness

```bash
curl -g -H "X-API-Key: <key>" \
  "http://192.168.1.50/api/display?msg=[bell]%20Meeting%20in%205%20min&spd=60&brt=12&mode=0"
```

---

## Behaviour notes

- **Persistence**: `spd`, `brt`, `mode`, `display`, `tz`, `date`, and `dateint` are saved to NVS and survive reboots, **except** when sent as part of an `alert` request (those are temporary).
- **`msg` auto-switches mode**: sending `msg` without `display=clock` always switches the device to Message mode. You do not need to send `display=message` explicitly.
- **Clock requires NTP**: the NTP client only runs in station mode (not in the setup AP). Until the first NTP sync, the clock shows `--:--`.
- **Timezone granularity**: `tz` accepts any integer in minutes, so half-hour offsets such as UTC+5:30 (`tz=330`) and UTC+9:30 (`tz=570`) are fully supported.
- **No rate limit**: requests are handled cooperatively from `loop()`; sending many requests in rapid succession may cause brief display glitches.