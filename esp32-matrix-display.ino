// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>   // pulls in AsyncTCP transitively on ESP32
#include <ArduinoJson.h>
#include <MD_MAX72xx.h>
#include <Preferences.h>
#include <esp_system.h>  // esp_random(), used to generate the REST API key
#include <time.h>        // NTP clock mode (configTzTime/getLocalTime)

#include "modules/config.h"
#include "modules/icons.h"
#include "modules/text_encoding.h"
#include "modules/days_lookup.h"
#include "modules/months_lookup.h"       // included for future-readiness only, unused by drawDateFace()
#include "modules/tz_lookup.h"
#include "modules/globals.h"
#include "modules/display_message.h"
#include "modules/display_clock.h"
#include "modules/display_life.h"
#include "modules/settings.h"
#include "modules/web_page.h"
#include "modules/web_routes.h"

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

bool apMode = false;       // true when running the emergency setup AP
bool restartPending = false;
uint32_t restartTime = 0;
char currentSsid[64] = "";  // SSID currently in use (saved or factory default)

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

// Runtime configuration, persisted in NVS via Preferences
Preferences prefs;
uint8_t scrollDelay = DEFAULT_SCROLL_DELAY;
uint8_t brightness  = DEFAULT_BRIGHTNESS;
uint8_t displayMode = DEFAULT_MODE;
uint8_t appMode     = DEFAULT_APP_MODE;
char tzName[48];
char language[3];
bool dateEnabled    = DEFAULT_DATE_ENABLED;
bool dateUsFormat   = DEFAULT_DATE_US_FORMAT;
uint16_t dateEveryS = DEFAULT_DATE_EVERY_S;

// Temporary alert message (?msg=...&alert=<seconds> on the REST API):
// snapshot of the display state to restore when the alert expires,
// checked in loop() like the deferred restart. Never persisted to NVS.
bool alertActive = false;
uint32_t alertRevertTime = 0;         // millis() deadline for the revert
uint8_t alertPrevAppMode = DEFAULT_APP_MODE;
uint8_t alertPrevDisplayMode = DEFAULT_MODE;
uint8_t alertPrevScrollDelay = DEFAULT_SCROLL_DELAY;
uint8_t alertPrevBrightness = DEFAULT_BRIGHTNESS;
char alertPrevMessage[MESG_SIZE];

// REST API (/api/display) authentication - off by default so the API
// works out of the box; the web UI lets the user enable it and generate
// a key, which callers must then send as an "X-API-Key" header.
bool apiAuthEnabled = false;
char apiKey[API_KEY_LEN + 1] = "";

void setup(void)
{
#if DEBUG
  Serial.begin(115200);
  PRINTS("\n[MD_MAX72XX WiFi Message Display]\nType a message for the scrolling display from your internet browser");
#endif

#if LED_HEARTBEAT
  pinMode(HB_LED, OUTPUT);
  digitalWrite(HB_LED, LOW);
#endif

  // Factory reset: hold the BOOT button while powering on / resetting to
  // wipe all saved settings and WiFi credentials and start clean.
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
  if (digitalRead(FACTORY_RESET_PIN) == LOW)
  {
    Preferences resetPrefs;
    resetPrefs.begin(NVS_NAMESPACE, false);
    resetPrefs.clear();
    resetPrefs.end();
  }

  // Load saved settings (scroll speed, brightness, display mode)
  loadSettings();
  if (apiKey[0] == '\0') generateApiKey();  // first boot: seed a key even if auth is off yet

  // Display initialization
  PRINTS("\nInitializing Display");
  mx.begin();
  mx.setShiftDataInCallback(scrollDataSource);
  mx.setShiftDataOutCallback(scrollDataSink);
  mx.control(MD_MAX72XX::INTENSITY, brightness);

  curMessage[0] = newMessage[0] = '\0';

  // Connect to the configured WiFi network, or fall back to an
  // emergency setup Access Point if that fails
  connectWiFi();

  // Start background NTP sync for the clock mode (station mode only -
  // there is no internet to reach from the setup AP)
  if (!apMode) applyTimeConfig();

  // Start the server
  PRINTS("\nStarting Server");
  setupWebRoutes();

  // Show the last message sent (if any); otherwise fall back to the IP
  // address. In AP setup mode always show the IP so it can be configured.
  if (apMode || !loadLastMessage(curMessage, MESG_SIZE))
  {
    IPAddress ip = apMode ? WiFi.softAPIP() : WiFi.localIP();
    sprintf(curMessage, "%s%d.%d.%d.%d", apMode ? "CONFIG " : "", ip[0], ip[1], ip[2], ip[3]);
  }
  PRINT("\nStartup message ", curMessage);

  if (appMode == APP_MODE_CLOCK) clockForceRedraw();  // paint the clock right away
  else if (appMode == APP_MODE_LIFE) { golSeed(); golForceRedraw(); }  // seed the board
  else if (displayMode == 1) showStatic();            // static mode: render immediately
  else if (displayMode == 2) resetBlinkScroll();      // blink+scroll renders on the first loop()
}

void loop(void)
{
#if LED_HEARTBEAT
  static uint32_t timeLast = 0;

  if (millis() - timeLast >= HB_LED_TIME)
  {
    digitalWrite(HB_LED, digitalRead(HB_LED) == LOW ? HIGH : LOW);
    timeLast = millis();
  }
#endif

  if (restartPending && (millis() >= restartTime))
  {
    ESP.restart();
    return;
  }

  // Alert expired: restore the display state snapshotted when it was
  // accepted (mode, message, brightness, speed, sub-mode)
  if (alertActive && ((int32_t)(millis() - alertRevertTime) >= 0))
  {
    alertActive = false;
    scrollDelay = alertPrevScrollDelay;
    brightness = alertPrevBrightness;
    mx.control(MD_MAX72XX::INTENSITY, brightness);
    displayMode = alertPrevDisplayMode;
    strcpy(newMessage, alertPrevMessage);
    newMessageAvailable = true;
    setAppMode(alertPrevAppMode);  // handles the clock redraw / scroll reset
  }

  // No handleWiFi() call here: ESPAsyncWebServer handlers run from the
  // TCP stack's own event context via AsyncTCP, independent of this
  // cooperative loop().
  if (appMode == APP_MODE_CLOCK)
    clockTick();
  else if (appMode == APP_MODE_LIFE)
    golTick();
  else if (displayMode == 0)
    scrollText();
  else if (displayMode == 2)
    blinkScroll();
  else
    staticBlink();
}
