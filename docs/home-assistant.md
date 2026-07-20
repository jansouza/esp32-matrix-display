# Home Assistant Integration

The display's REST API (`GET /api/display`) integrates with
[Home Assistant](https://www.home-assistant.io/) without any custom component —
the built-in `rest_command`.

All examples assume:

- Display IP: `192.168.1.50` (set a DHCP reservation or static IP so it does
  not change).
- API authentication enabled, with the key stored in `secrets.yaml`:

```yaml
# secrets.yaml
matrix_display_key: "3f8a1c9b2e7d4f60a5b8c1d2"
```

If you have not enabled authentication in the **API** tab of the web
interface, just omit the `X-API-Key` header from the examples.

## rest_command

`rest_command` lets you pass every API parameter (speed, brightness, mode,
clock settings), not just the message:

```yaml
# configuration.yaml
rest_command:
  matrix_message:
    url: >-
      http://192.168.1.50/api/display?msg={{ message | urlencode
      }}&spd={{ speed | default(100) }}&brt={{ brightness | default(5)
      }}&mode={{ mode | default(0) }}
    headers:
      X-API-Key: !secret matrix_display_key

  matrix_alert:
    # Shows the message for `duration` seconds, then the display restores
    # the previous state (clock/message, brightness, speed) by itself
    url: >-
      http://192.168.1.50/api/display?msg={{ message | urlencode
      }}&alert={{ duration | default(30) }}&brt={{ brightness | default(15)
      }}&mode={{ mode | default(0) }}
    headers:
      X-API-Key: !secret matrix_display_key

  matrix_clock:
    url: >-
      http://192.168.1.50/api/display?display=clock&tz={{ tz | default(-180)
      }}&date={{ date | default(1) }}&dateint={{ dateint | default(30) }}&brt={{ brightness | default(4) }}
    headers:
      X-API-Key: !secret matrix_display_key

  matrix_life:
    url: "http://192.168.1.50/api/display?display=life"
    headers:
      X-API-Key: !secret matrix_display_key

  matrix_brightness:
    url: "http://192.168.1.50/api/display?brt={{ brightness }}"
    headers:
      X-API-Key: !secret matrix_display_key

```

Usage:

```yaml
actions:
  - action: rest_command.matrix_message
    data:
      message: "[warn] Laundry done"
      speed: 60
      brightness: 10
      mode: 0        # 0 = scroll, 1 = static/blink
```

Sending `msg` automatically switches the display back to Message mode; call
`rest_command.matrix_clock` to return to the clock.

## Recipes

### Show a notification, then return to the clock

The firmware handles this natively via the `alert` parameter — one call, no
delay/restore choreography, and no race if two automations fire at once:

```yaml
actions:
  - action: rest_command.matrix_alert
    data:
      message: "[bell] Someone at the door"
      duration: 30
```

After 30 seconds the display returns to whatever it was showing before
(clock or the previous message), including the previous brightness/speed.

### Dim the display at night

```yaml
automation:
  - alias: "Matrix night brightness"
    triggers:
      - trigger: time
        at: "22:30:00"
    actions:
      - action: rest_command.matrix_brightness
        data:
          brightness: 0

  - alias: "Matrix day brightness"
    triggers:
      - trigger: time
        at: "07:00:00"
    actions:
      - action: rest_command.matrix_brightness
        data:
          brightness: 8
```

### Temperature from a sensor

```yaml
automation:
  - alias: "Matrix outdoor temperature"
    triggers:
      - trigger: time_pattern
        minutes: "/15"
    actions:
      - action: notify.matrix_display
        data:
          message: >-
            [sun] {{ states('sensor.outdoor_temperature') | round(0) }}C
```

### Alert with attention-grabbing blink

```yaml
actions:
  - action: rest_command.matrix_message
    data:
      message: "[fire] SMOKE DETECTED"
      mode: 1          # static + blink
      brightness: 15
```

### Screensaver: Game of Life when idle

Switch to the Game of Life mode when nobody is home, and back to the clock
when someone returns:

```yaml
automation:
  - alias: "Matrix screensaver when away"
    triggers:
      - trigger: state
        entity_id: zone.home
        to: "0"
    actions:
      - action: rest_command.matrix_life

  - alias: "Matrix clock when someone is home"
    triggers:
      - trigger: state
        entity_id: zone.home
        from: "0"
    actions:
      - action: rest_command.matrix_clock
```

## Lovelace button

A quick dashboard button to switch modes:

```yaml
type: button
name: Matrix Clock
icon: mdi:clock-outline
tap_action:
  action: perform-action
  perform_action: rest_command.matrix_clock
```

```yaml
type: button
name: Matrix Game of Life
icon: mdi:grid
tap_action:
  action: perform-action
  perform_action: rest_command.matrix_life
```

## API reference

See the [REST API section of the README](../docs/rest-api.md) for the full
parameter list (`msg`, `spd`, `brt`, `mode`, `display`, `tz`, `date`,
`dateint`) and the available icon tags.

## Troubleshooting

- **`{"ok":false,"error":"unauthorized"}`** — the `X-API-Key` header is
  missing or wrong; check the key in the **API** tab of the web interface.
- **Message truncated** — the display accepts up to 255 characters
  (icon tags count in their `[tag]` form until expanded).
- **Nothing happens** — confirm the display IP (shown on the matrix at boot)
  and that Home Assistant can reach it (`curl` from the HA host is the
  quickest test).
