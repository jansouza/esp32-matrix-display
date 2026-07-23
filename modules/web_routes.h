// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

AsyncWebServer server(80);

inline bool paramValue(AsyncWebServerRequest *request, const char *name, char *out, size_t maxLen)
{
  if (!request->hasParam(name)) return false;
  strncpy(out, request->getParam(name)->value().c_str(), maxLen - 1);
  out[maxLen - 1] = '\0';
  return true;
}

inline bool apiKeyAuthorized(AsyncWebServerRequest *request)
{
  if (!apiAuthEnabled) return true;
  if (!request->hasHeader("X-API-Key")) return false;
  return request->getHeader("X-API-Key")->value() == apiKey;
}

inline void sendJson(AsyncWebServerRequest *request, int code, JsonDocument &doc)
{
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->addHeader("Cache-Control", "no-store");
  response->setCode(code);
  serializeJson(doc, *response);
  request->send(response);
}

inline void sendJsonOk(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(request, 200, doc);
}

// Shared "apply these settings fields from a request" logic, used by
// both /api/display (REST API, X-API-Key gated) and /api/settings
// (the local web UI's own save action, ungated) - both take the same
// standard query-string param names.
inline bool applySettingsFromRequest(AsyncWebServerRequest *request, bool defaultToMessageMode = false)
{
  char szParam[8];
  bool settingsChanged = false;

  if (paramValue(request, "spd", szParam, sizeof(szParam)))
  {
    uint8_t v = (uint8_t)constrain(atoi(szParam), MIN_SCROLL_DELAY, MAX_SCROLL_DELAY);
    if (v != scrollDelay) { scrollDelay = v; settingsChanged = true; }
  }
  if (paramValue(request, "brt", szParam, sizeof(szParam)))
  {
    uint8_t v = (uint8_t)constrain(atoi(szParam), 0, MAX_INTENSITY);
    if (v != brightness) { brightness = v; mx.control(MD_MAX72XX::INTENSITY, brightness); settingsChanged = true; }
  }
  if (paramValue(request, "mode", szParam, sizeof(szParam)))
  {
    uint8_t v = (uint8_t)constrain(atoi(szParam), 0, 2);
    if (v != displayMode)
    {
      displayMode = v;
      mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
      resetScrollSource();
      if (displayMode == 0) newMessageAvailable = true;
      else if (displayMode == 1) showStatic();
      else resetBlinkScroll();
      settingsChanged = true;
    }
  }
  {
    char szTz[48];
    if (paramValue(request, "tzname", szTz, sizeof(szTz)) ||
        paramValue(request, "tz", szTz, sizeof(szTz)))
    {
      if (isKnownTimezone(szTz) && (strcmp(szTz, tzName) != 0))
      {
        strncpy(tzName, szTz, sizeof(tzName) - 1);
        tzName[sizeof(tzName) - 1] = '\0';
        if (!apMode) applyTimeConfig();
        settingsChanged = true;
      }
    }
  }
  if (paramValue(request, "lang", szParam, sizeof(szParam)))
  {
    if (isSupportedLanguage(szParam) && (strcmp(szParam, language) != 0))
    {
      strncpy(language, szParam, sizeof(language) - 1);
      language[sizeof(language) - 1] = '\0';
      settingsChanged = true;
    }
  }
  if (paramValue(request, "date", szParam, sizeof(szParam)) ||
      paramValue(request, "dateon", szParam, sizeof(szParam)))
  {
    bool v = (szParam[0] == '1');
    if (v != dateEnabled) { dateEnabled = v; settingsChanged = true; }
  }
  if (paramValue(request, "dateint", szParam, sizeof(szParam)) ||
      paramValue(request, "dateiv", szParam, sizeof(szParam)))
  {
    uint16_t v = (uint16_t)constrain(atoi(szParam), MIN_DATE_EVERY_S, MAX_DATE_EVERY_S);
    if (v != dateEveryS) { dateEveryS = v; settingsChanged = true; }
  }
  if (paramValue(request, "dateus", szParam, sizeof(szParam)))
  {
    bool v = (szParam[0] == '1');
    if (v != dateUsFormat) { dateUsFormat = v; settingsChanged = true; }
  }
  // Top-level mode: display=clock|life|gol|message (REST) or dmode=0|1|2 (web UI)
  {
    uint8_t v = appMode;
    bool modeParamPresent = false;
    if (paramValue(request, "display", szParam, sizeof(szParam)))
    {
      if (strcmp(szParam, "clock") == 0) v = APP_MODE_CLOCK;
      else if (strcmp(szParam, "life") == 0 || strcmp(szParam, "gol") == 0) v = APP_MODE_LIFE;
      else v = APP_MODE_MESSAGE;
      modeParamPresent = true;
    }
    else if (paramValue(request, "dmode", szParam, sizeof(szParam)))
    {
      v = (uint8_t)constrain(atoi(szParam), APP_MODE_MESSAGE, APP_MODE_LIFE);
      modeParamPresent = true;
    }
    else if (defaultToMessageMode)
    {
      // A plain message without an explicit display= always switches back
      // to message mode, so a bare ?msg=... ends up visible.
      v = APP_MODE_MESSAGE;
    }
    if (modeParamPresent) alertActive = false;  // explicit mode choice wins over a pending revert
    if (v != appMode)
    {
      setAppMode(v);
      settingsChanged = true;
    }
  }

  return settingsChanged;
}

inline void handleApiDisplay(AsyncWebServerRequest *request)
{
  if (!apiKeyAuthorized(request))
  {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "unauthorized";
    sendJson(request, 401, doc);
    return;
  }

  bool apiMessageOk = false;
  if (request->hasParam("msg"))
  {
    strncpy(newMessage, request->getParam("msg")->value().c_str(), MESG_SIZE - 1);
    newMessage[MESG_SIZE - 1] = '\0';
    decodeUtf8Latin1(newMessage);
    expandIcons(newMessage);
    newMessageAvailable = true;
    apiMessageOk = true;
  }

  // Temporary alert: ?msg=...&alert=<seconds> shows the message for that
  // long, then loop() restores the state snapshotted here - before the
  // other settings below can touch it.
  uint16_t apiAlertSecs = 0;
  char szParam[8];
  if (apiMessageOk && paramValue(request, "alert", szParam, sizeof(szParam)))
  {
    apiAlertSecs = (uint16_t)constrain(atoi(szParam), 1, 3600);
    if (!alertActive)
    {
      alertPrevAppMode = appMode;
      alertPrevDisplayMode = displayMode;
      alertPrevScrollDelay = scrollDelay;
      alertPrevBrightness = brightness;
      strcpy(alertPrevMessage, curMessage);
      alertActive = true;
    }
    alertRevertTime = millis() + (uint32_t)apiAlertSecs * 1000;
  }
  else if (apiMessageOk)
    alertActive = false;

  bool settingsChanged = applySettingsFromRequest(request, apiMessageOk);

  // Alert state is temporary by definition - don't persist it
  if (settingsChanged && (apiAlertSecs == 0)) saveSettings();

  JsonDocument doc;
  if (apiMessageOk)
  {
    char szTags[MESG_SIZE * 2];
    char szUtf8[MESG_SIZE * 3];
    collapseIcons(newMessage, szTags, sizeof(szTags));
    encodeLatin1Utf8(szTags, szUtf8, sizeof(szUtf8));
    doc["ok"] = true;
    doc["msg"] = szUtf8;
    if (apiAlertSecs > 0) doc["alert"] = apiAlertSecs;
  }
  else if (settingsChanged)
  {
    doc["ok"] = true;
  }
  else
  {
    doc["ok"] = false;
    doc["error"] = "missing msg parameter";
  }
  sendJson(request, 200, doc);
}

inline void handleApiSettingsSave(AsyncWebServerRequest *request)
// The local web UI's own save action (message + mode + tz + date + lang),
// same param names and logic as /api/display, minus the message/alert
// handling (the UI sends the message via this same route's "msg" param).
{
  if (request->hasParam("msg"))
  {
    strncpy(newMessage, request->getParam("msg")->value().c_str(), MESG_SIZE - 1);
    newMessage[MESG_SIZE - 1] = '\0';
    decodeUtf8Latin1(newMessage);
    expandIcons(newMessage);
    newMessageAvailable = true;
    alertActive = false;  // explicit user message cancels a pending alert revert
  }

  bool settingsChanged = applySettingsFromRequest(request);
  if (request->hasParam("msg")) settingsChanged = true;
  if (settingsChanged) saveSettings();

  sendJsonOk(request);
}

inline void handleApiNetwork(AsyncWebServerRequest *request)
{
  char newSsid[64], newPass[64];
  if (!paramValue(request, "ssid", newSsid, sizeof(newSsid)) || (newSsid[0] == '\0'))
  {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "missing ssid parameter";
    sendJson(request, 400, doc);
    return;
  }
  newPass[0] = '\0';
  paramValue(request, "pass", newPass, sizeof(newPass));
  if (newPass[0] == '\0') prefs.getString(NVS_KEY_NETPASS, newPass, sizeof(newPass));
  saveNetworkCreds(newSsid, newPass);
  restartPending = true;
  restartTime = millis() + 1000; // let the HTTP response go out first

  sendJsonOk(request);
}

inline void handleApiAuth(AsyncWebServerRequest *request)
{
  char szParam[8];
  if (paramValue(request, "enabled", szParam, sizeof(szParam)))
    saveApiAuthEnabled(szParam[0] == '1');
  sendJsonOk(request);
}

inline void handleApiRegen(AsyncWebServerRequest *request)
{
  generateApiKey();
  sendJsonOk(request);
}

inline void handleApiNetInfo(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc["ssid"] = currentSsid;
  doc["rssi"] = apMode ? 0 : WiFi.RSSI();
  sendJson(request, 200, doc);
}

inline void handleApiSettingsGet(AsyncWebServerRequest *request)
{
  char szLast[MESG_SIZE];
  char szTags[MESG_SIZE * 2];
  char szUtf8[MESG_SIZE * 3];

  if (!loadLastMessage(szLast, sizeof(szLast))) szLast[0] = '\0';
  collapseIcons(szLast, szTags, sizeof(szTags));
  encodeLatin1Utf8(szTags, szUtf8, sizeof(szUtf8));

  JsonDocument doc;
  doc["spd"] = scrollDelay;
  doc["brt"] = brightness;
  doc["mode"] = displayMode;
  doc["appMode"] = appMode;
  doc["tzname"] = tzName;
  doc["lang"] = language;
  doc["dateon"] = dateEnabled ? 1 : 0;
  doc["dateiv"] = dateEveryS;
  doc["dateus"] = dateUsFormat ? 1 : 0;
  doc["lastmsg"] = szUtf8;
  sendJson(request, 200, doc);
}

inline void handleApiApiSettings(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  doc["enabled"] = apiAuthEnabled ? 1 : 0;
  doc["key"] = apiKey;
  sendJson(request, 200, doc);
}

inline void handleApiTimezones(AsyncWebServerRequest *request)
{
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (size_t i = 0; i < TZ_MAPPINGS_COUNT; i++)
    arr.add(tz_mappings[i].iana);
  sendJson(request, 200, doc);
}

inline void handleRoot(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", WebPage);
  response->addHeader("Cache-Control", "max-age=3600");
  request->send(response);
}

inline void setupWebRoutes(void)
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/display", HTTP_GET, handleApiDisplay);
  // GET /api/settings reads the current settings (used on page load);
  // POST /api/settings is the web UI's own save action. Distinguished by
  // HTTP method rather than by which query params are present, which
  // ESPAsyncWebServer routes cleanly without any parameter sniffing.
  server.on("/api/settings", HTTP_GET, handleApiSettingsGet);
  server.on("/api/settings", HTTP_POST, handleApiSettingsSave);
  server.on("/api/netinfo", HTTP_GET, handleApiNetInfo);
  server.on("/api/apisettings", HTTP_GET, handleApiApiSettings);
  server.on("/api/timezones", HTTP_GET, handleApiTimezones);
  server.on("/api/network", HTTP_GET, handleApiNetwork);
  server.on("/api/apiauth", HTTP_GET, handleApiAuth);
  server.on("/api/apiregen", HTTP_GET, handleApiRegen);
  server.begin();
}

#endif // WEB_ROUTES_H
