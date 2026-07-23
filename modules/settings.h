// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef SETTINGS_H
#define SETTINGS_H

// Persistence (NVS via Preferences) --------------------------------

inline bool isSupportedLanguage(const char *lang)
{
  for (uint8_t i = 0; i < SUPPORTED_LANGUAGES_COUNT; i++)
    if (strcmp(lang, SUPPORTED_LANGUAGES[i]) == 0) return true;
  return false;
}

inline bool isKnownTimezone(const char *iana)
{
  for (size_t i = 0; i < TZ_MAPPINGS_COUNT; i++)
    if (strcmp(iana, tz_mappings[i].iana) == 0) return true;
  return false;
}

inline void loadSettings(void)
{
  prefs.begin(NVS_NAMESPACE, false);
  scrollDelay = prefs.getUChar(NVS_KEY_SPD, DEFAULT_SCROLL_DELAY);
  brightness  = prefs.getUChar(NVS_KEY_BRT, DEFAULT_BRIGHTNESS);
  displayMode = prefs.getUChar(NVS_KEY_MODE, DEFAULT_MODE);
  appMode     = prefs.getUChar(NVS_KEY_AMODE, DEFAULT_APP_MODE);
  if (appMode > APP_MODE_LIFE) appMode = DEFAULT_APP_MODE;

  prefs.getString(NVS_KEY_TZNAME, tzName, sizeof(tzName));
  if ((tzName[0] == '\0') || !isKnownTimezone(tzName))
    strncpy(tzName, DEFAULT_TZ_NAME, sizeof(tzName) - 1);

  prefs.getString(NVS_KEY_LANG, language, sizeof(language));
  if ((language[0] == '\0') || !isSupportedLanguage(language))
    strncpy(language, DEFAULT_LANGUAGE, sizeof(language) - 1);

  dateEnabled  = prefs.getBool(NVS_KEY_DATEON, DEFAULT_DATE_ENABLED);
  dateUsFormat = prefs.getBool(NVS_KEY_DATEUS, DEFAULT_DATE_US_FORMAT);
  dateEveryS   = prefs.getUShort(NVS_KEY_DATEIV, DEFAULT_DATE_EVERY_S);
  dateEveryS  = constrain(dateEveryS, MIN_DATE_EVERY_S, MAX_DATE_EVERY_S);
  apiAuthEnabled = prefs.getBool(NVS_KEY_APIAUTH, false);
  prefs.getString(NVS_KEY_APIKEY, apiKey, sizeof(apiKey));
}

inline void saveSettings(void)
{
  prefs.putUChar(NVS_KEY_SPD, scrollDelay);
  prefs.putUChar(NVS_KEY_BRT, brightness);
  prefs.putUChar(NVS_KEY_MODE, displayMode);
  prefs.putUChar(NVS_KEY_AMODE, appMode);
  prefs.putString(NVS_KEY_TZNAME, tzName);
  prefs.putString(NVS_KEY_LANG, language);
  prefs.putBool(NVS_KEY_DATEON, dateEnabled);
  prefs.putBool(NVS_KEY_DATEUS, dateUsFormat);
  prefs.putUShort(NVS_KEY_DATEIV, dateEveryS);
}

inline void applyTimeConfig(void)
// (Re)start SNTP using the IANA zone's POSIX TZ string, so DST (where
// the zone observes it) is handled automatically by libc. Non-blocking:
// the ESP32 SNTP client syncs in the background; until then
// getLocalTime() fails and the clock face shows "--:--".
{
  const char *posix = ianaToPosix(tzName);
  setenv("TZ", posix, 1);
  tzset();
  configTzTime(posix, NTP_SERVER_1, NTP_SERVER_2);
}

inline void setAppMode(uint8_t v)
// Switch between the top-level Message and Clock modes, restoring the
// display state the new mode expects (blink mode may have left the
// matrix shut down, scroll mode needs its state machine reset).
{
  appMode = v;
  mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
  resetScrollSource();
  if (appMode == APP_MODE_CLOCK)
    clockForceRedraw();
  else if (appMode == APP_MODE_LIFE)
  {
    golSeed();
    golForceRedraw();
  }
  else if (displayMode == 0)
    newMessageAvailable = true;  // restart scrolling from the last message
  else if (displayMode == 1)
    showStatic();                // redraw immediately in static/blink mode
  else
    resetBlinkScroll();          // restart blink+scroll from its blink phase
}

inline void generateApiKey(void)
// Fill apiKey with a new random hex string (using the hardware RNG) and
// persist it, so a freshly-enabled API doesn't start with a blank/guessable
// key.
{
  const char hexDigits[] = "0123456789abcdef";
  for (uint8_t i = 0; i < API_KEY_LEN; i++)
    apiKey[i] = hexDigits[esp_random() % 16];
  apiKey[API_KEY_LEN] = '\0';
  prefs.putString(NVS_KEY_APIKEY, apiKey);
}

inline void saveApiAuthEnabled(bool enabled)
{
  apiAuthEnabled = enabled;
  prefs.putBool(NVS_KEY_APIAUTH, apiAuthEnabled);
}

inline void saveLastMessage(const char *szMesg)
{
  prefs.putString(NVS_KEY_LASTMSG, szMesg);
}

inline boolean loadLastMessage(char *pszOut, uint16_t maxLen)
// Fill pszOut with the last saved message. Returns false (and leaves
// pszOut untouched) if there is none saved yet.
{
  if (prefs.getString(NVS_KEY_LASTMSG, pszOut, maxLen) == 0) return(false);
  return(pszOut[0] != '\0');
}

inline void saveNetworkCreds(const char *newSsid, const char *newPass)
{
  prefs.putString(NVS_KEY_NETSSID, newSsid);
  prefs.putString(NVS_KEY_NETPASS, newPass);
}

inline const char *err2Str(wl_status_t code)
{
  switch (code)
  {
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("CONNECT_FAILED"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}

inline void connectWiFi(void)
// Try the saved network credentials (falling back to the factory
// defaults if none are saved). If the connection can't be made within
// WIFI_CONNECT_TIMEOUT, fall back to an open emergency setup Access
// Point so the device is never left unreachable.
{
  char savedSsid[64], savedPass[64];

  prefs.getString(NVS_KEY_NETSSID, savedSsid, sizeof(savedSsid));
  prefs.getString(NVS_KEY_NETPASS, savedPass, sizeof(savedPass));

  const char *useSsid = (savedSsid[0] != '\0') ? savedSsid : factorySsid;
  const char *usePass = (savedSsid[0] != '\0') ? savedPass : factoryPassword;
  strncpy(currentSsid, useSsid, sizeof(currentSsid) - 1);
  currentSsid[sizeof(currentSsid) - 1] = '\0';

  PRINT("\nConnecting to ", useSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(useSsid, usePass);

  uint32_t t = millis();
  while ((WiFi.status() != WL_CONNECTED) && (millis() - t < WIFI_CONNECT_TIMEOUT))
  {
    PRINT("\n", err2Str(WiFi.status()));
    uint32_t t2 = millis();
    while (millis() - t2 <= 500) yield();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    apMode = false;
    PRINTS("\nWiFi connected");
  }
  else
  {
    apMode = true;
    PRINTS("\nCould not connect - starting setup AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(setupApSsid);
  }
}

#endif // SETTINGS_H
