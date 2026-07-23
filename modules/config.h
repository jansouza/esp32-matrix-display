// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef CONFIG_H
#define CONFIG_H

// Debug / build-time toggles ------------------------------------------
#define PRINT_CALLBACK  0
#define DEBUG 1
#define LED_HEARTBEAT 0

#if DEBUG
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }
#define PRINTS(s)   { Serial.print(F(s)); }
#else
#define PRINT(s, v)
#define PRINTS(s)
#endif

#if LED_HEARTBEAT
#define HB_LED  D2
#define HB_LED_TIME 500 // in milliseconds
#endif

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// GPIO pins
#define CLK_PIN   18 // VSPI_SCK
#define DATA_PIN  23 // VSPI_MOSI
#define CS_PIN    5  // VSPI_SS
#define FACTORY_RESET_PIN 0 // BOOT button on most ESP32 dev boards

// WiFi login parameters - factory default network name and password,
// used if no credentials have been saved yet (or as a last resort)
const char factorySsid[] = "";
const char factoryPassword[] = "";

// Access Point used for emergency setup when the configured network
// cannot be reached
const char setupApSsid[] = "MD-Display-Setup";
const uint32_t WIFI_CONNECT_TIMEOUT = 15000; // ms

// Global message buffers shared by Wifi and Scrolling functions
const uint8_t MESG_SIZE = 255;
const uint8_t CHAR_SPACING = 1;

// Defaults used the first time the device boots (no saved preferences yet)
const uint8_t DEFAULT_SCROLL_DELAY = 75;  // ms between scroll steps
const uint8_t DEFAULT_BRIGHTNESS   = 8;   // 0..MAX_INTENSITY (15)
const uint8_t DEFAULT_MODE         = 0;   // 0 = scroll, 1 = static blink, 2 = blink+scroll

const uint8_t MIN_SCROLL_DELAY = 20;
const uint8_t MAX_SCROLL_DELAY = 250;
const uint16_t BLINK_INTERVAL  = 500;  // ms, static blink mode on/off period
const uint32_t BLINK_PHASE_MS  = 5000; // blink+scroll mode: blink this long between scroll passes

// Top-level device mode: what the display shows. Message mode keeps the
// scroll/blink sub-modes above (displayMode); clock mode shows an NTP
// clock instead.
#define APP_MODE_MESSAGE 0
#define APP_MODE_CLOCK   1
#define APP_MODE_LIFE    2
const uint8_t DEFAULT_APP_MODE = APP_MODE_MESSAGE;

// Clock mode configuration
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";
const char *const DEFAULT_TZ_NAME = "America/Sao_Paulo";  // matches the old UTC-3 Brasilia default
const bool DEFAULT_DATE_ENABLED = true;       // periodically show the date
const bool DEFAULT_DATE_US_FORMAT = false;    // false = day/month order, true = month/day order
const uint16_t DEFAULT_DATE_EVERY_S = 30;     // show the date every this many seconds
const uint16_t MIN_DATE_EVERY_S = 5;
const uint16_t MAX_DATE_EVERY_S = 3600;
#define CLOCK_DATE_SHOW_S 5    // how long the date stays on screen

// Weekday-name language: restricted to the codes whose day abbreviations
// render correctly in the 3x5 small font (see display_clock.h's
// smallLetterGlyph()). Decoupled from dateUsFormat, which now only
// controls day/month vs month/day digit ordering.
const char *const DEFAULT_LANGUAGE = "en";
const char *const SUPPORTED_LANGUAGES[] = { "en", "pt", "de", "es", "fr", "it" };
const uint8_t SUPPORTED_LANGUAGES_COUNT = sizeof(SUPPORTED_LANGUAGES) / sizeof(SUPPORTED_LANGUAGES[0]);

// Game of Life mode configuration
const uint16_t GOL_TICK_MS = 200;           // ms between generations (fixed for v1)
const uint8_t  GOL_STAGNANT_LIMIT = 8;      // generations with no visible change before reseeding
const uint16_t GOL_REVIVE_DENSITY_PCT = 35; // percent of cells alive on reseed
const uint16_t GOL_BLANK_PAUSE_MS = 400;    // blank pause shown before a reseed

// REST API (/api/display) authentication
const uint8_t API_KEY_LEN = 24;  // characters, not counting the null terminator

// NVS namespace and key names (Preferences) --------------------------
#define NVS_NAMESPACE   "mdmax"
#define NVS_KEY_SPD     "spd"
#define NVS_KEY_BRT     "brt"
#define NVS_KEY_MODE    "mode"
#define NVS_KEY_AMODE   "amode"
#define NVS_KEY_TZNAME  "tzname"
#define NVS_KEY_LANG    "lang"
#define NVS_KEY_DATEON  "dateon"
#define NVS_KEY_DATEUS  "dateus"
#define NVS_KEY_DATEIV  "dateiv"
#define NVS_KEY_APIAUTH "apiauth"
#define NVS_KEY_APIKEY  "apikey"
#define NVS_KEY_LASTMSG "lastmsg"
#define NVS_KEY_NETSSID "net_ssid"
#define NVS_KEY_NETPASS "net_pass"

#endif // CONFIG_H
