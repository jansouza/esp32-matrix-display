// Use the MD_MAX72XX library to scroll text on the display
// received through the ESP32 WiFi interface.
//
// Demonstrates the use of the callback function to control what
// is scrolled on the display text. User can enter text through
// a web browser and this will display as a scrolling message on
// the display.
//
// IP address for the ESP32 is displayed on the scrolling display
// after startup initialization and connected to the WiFi network.
//
// Connections for ESP32 hardware SPI are:
// Vcc       3.3V - A few matrices seem to work at 3.3V
// GND       GND
// DIN       VSPI_MOSI
// CS or LD  VSPI_CS
// CLK       VSPI_SCK
//

#include <WiFi.h>
#include <WiFiServer.h>
#include <MD_MAX72xx.h>
#include <Preferences.h>
#include <esp_system.h>  // esp_random(), used to generate the REST API key
#include <strings.h>     // strncasecmp(), used for case-insensitive header matching
#include <time.h>        // NTP clock mode (configTime/getLocalTime)

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

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// WiFi login parameters - factory default network name and password,
// used if no credentials have been saved yet (or as a last resort)
const char ssid[] = "";
const char password[] = "";

// Access Point used for emergency setup when the configured network
// cannot be reached
const char setupApSsid[] = "MD-Display-Setup";
const uint32_t WIFI_CONNECT_TIMEOUT = 15000; // ms

// WiFi Server object and parameters
WiFiServer server(80);
bool apMode = false;       // true when running the emergency setup AP
bool restartPending = false;
uint32_t restartTime = 0;
char currentSsid[64] = "";  // SSID currently in use (saved or factory default)

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
const int16_t DEFAULT_TZ_OFFSET_MIN = -180;    // UTC-3 (Brasilia)
const int16_t MIN_TZ_OFFSET_MIN = -720;        // UTC-12
const int16_t MAX_TZ_OFFSET_MIN = 840;         // UTC+14
const char *NTP_SERVER_1 = "pool.ntp.org";
const char *NTP_SERVER_2 = "time.nist.gov";
const bool DEFAULT_DATE_ENABLED = true;       // periodically show the date
const uint16_t DEFAULT_DATE_EVERY_S = 30;     // show the date every this many seconds
const uint16_t MIN_DATE_EVERY_S = 5;
const uint16_t MAX_DATE_EVERY_S = 3600;
#define CLOCK_DATE_SHOW_S 3    // how long the date stays on screen

// Game of Life mode configuration
const uint16_t GOL_TICK_MS = 200;           // ms between generations (fixed for v1)
const uint8_t  GOL_STAGNANT_LIMIT = 8;      // generations with no visible change before reseeding
const uint16_t GOL_REVIVE_DENSITY_PCT = 35; // percent of cells alive on reseed
const uint16_t GOL_BLANK_PAUSE_MS = 400;    // blank pause shown before a reseed

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

// Runtime configuration, persisted in NVS via Preferences
Preferences prefs;
uint8_t scrollDelay = DEFAULT_SCROLL_DELAY;
uint8_t brightness  = DEFAULT_BRIGHTNESS;
uint8_t displayMode = DEFAULT_MODE;
uint8_t appMode     = DEFAULT_APP_MODE;
int16_t tzOffsetMin = DEFAULT_TZ_OFFSET_MIN;
bool dateEnabled    = DEFAULT_DATE_ENABLED;
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
const uint8_t API_KEY_LEN = 24;  // characters, not counting the null terminator
bool apiAuthEnabled = false;
char apiKey[API_KEY_LEN + 1] = "";

// Custom icons -----------------------------------------------------
// Icons are encoded in the message as control bytes 0x01-0x07 (values
// that never occur in normal printable text) after being expanded from
// their [tag] form by expandIcons(). Each icon is an 8x8 bitmap, one
// byte per column.
#define ICON_HEART  '\x01'
#define ICON_WIFI   '\x02'
#define ICON_SMILE  '\x03'
#define ICON_UP     '\x04'
#define ICON_DOWN   '\x05'
#define ICON_LEFT   '\x06'
#define ICON_RIGHT  '\x07'
#define ICON_STAR   '\x08'
#define ICON_MUSIC  '\x09'
#define ICON_BELL   '\x0a'
#define ICON_CLOCK  '\x0b'
#define ICON_OK     '\x0c'
#define ICON_X      '\x0d'
#define ICON_SUN    '\x0e'
#define ICON_RAIN   '\x0f'
#define ICON_PIN    '\x10'
#define ICON_PLUS   '\x11'
#define ICON_WARN   '\x12'
#define ICON_BOLT   '\x13'
#define ICON_FIRE   '\x14'

typedef struct
{
  const char *tag;   // text tag, e.g. "[heart]"
  char code;         // internal control byte substituted for the tag
  uint8_t width;      // number of valid columns in bitmap
  uint8_t bitmap[8];  // column data, LSB = top row
} icon_t;

const icon_t iconTable[] =
{
  { "[heart]", ICON_HEART, 8, { 0x0c, 0x1e, 0x3e, 0x7c, 0x7c, 0x3e, 0x1e, 0x0c } },
  { "[wifi]",  ICON_WIFI,  8, { 0x08, 0x04, 0x1a, 0xaa, 0xaa, 0x1a, 0x04, 0x08 } },
  { "[smile]", ICON_SMILE, 8, { 0x3c, 0x42, 0x95, 0xa1, 0xa1, 0x95, 0x42, 0x3c } },
  { "[up]",    ICON_UP,    8, { 0x08, 0x0c, 0x0e, 0x7f, 0x7f, 0x0e, 0x0c, 0x08 } },
  { "[down]",  ICON_DOWN,  8, { 0x10, 0x30, 0x70, 0xfe, 0xfe, 0x70, 0x30, 0x10 } },
  { "[left]",  ICON_LEFT,  8, { 0x08, 0x1c, 0x3e, 0x7f, 0x1c, 0x1c, 0x1c, 0x1c } },
  { "[right]", ICON_RIGHT, 8, { 0x1c, 0x1c, 0x1c, 0x1c, 0x7f, 0x3e, 0x1c, 0x08 } },
  { "[star]",  ICON_STAR,  8, { 0x18, 0x1c, 0x7e, 0x3c, 0x3c, 0x7e, 0x1c, 0x18 } },
  { "[music]", ICON_MUSIC, 6, { 0x60, 0x90, 0x90, 0x50, 0x60, 0x3f, 0x00, 0x00 } },
  { "[bell]",  ICON_BELL,  7, { 0x20, 0x30, 0x3e, 0x7f, 0x3e, 0x30, 0x20, 0x00 } },
  { "[clock]", ICON_CLOCK, 8, { 0x3c, 0x42, 0x91, 0x9d, 0x91, 0x81, 0x42, 0x3c } },
  { "[ok]",    ICON_OK,    8, { 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03 } },
  { "[x]",     ICON_X,     8, { 0x42, 0x66, 0x3c, 0x18, 0x18, 0x3c, 0x66, 0x42 } },
  { "[sun]",   ICON_SUN,   8, { 0x24, 0x18, 0x5a, 0x3c, 0x3c, 0x5a, 0x18, 0x24 } },
  { "[rain]",  ICON_RAIN,  6, { 0x45, 0x2f, 0x4e, 0x2e, 0x0e, 0x04, 0x00, 0x00 } },
  { "[pin]",   ICON_PIN,   7, { 0x04, 0x0e, 0x1f, 0x3f, 0x1f, 0x0e, 0x04, 0x00 } },
  { "[plus]",  ICON_PLUS,  6, { 0x18, 0x18, 0x7e, 0x7e, 0x18, 0x18, 0x00, 0x00 } },
  { "[warn]",  ICON_WARN,  7, { 0x60, 0x58, 0x46, 0x6d, 0x46, 0x58, 0x60, 0x00 } },
  { "[bolt]",  ICON_BOLT,  5, { 0x68, 0x3c, 0x1e, 0x0f, 0x05, 0x00, 0x00, 0x00 } },
  { "[fire]",  ICON_FIRE,  5, { 0x38, 0x7c, 0x5f, 0x7e, 0x38, 0x00, 0x00, 0x00 } },
};

const uint8_t ICON_TABLE_SIZE = sizeof(iconTable) / sizeof(iconTable[0]);

const icon_t *findIconByCode(char c)
// Return pointer to the icon_t matching a control byte, or NULL
{
  for (uint8_t i = 0; i < ICON_TABLE_SIZE; i++)
    if (iconTable[i].code == c) return(&iconTable[i]);
  return(NULL);
}

void expandIcons(char *szMesg)
// Scan the message in place, replacing any [tag] occurrences with
// their single-byte icon code. The buffer shrinks in place so this
// is always safe to do without a second buffer.
{
  char *pRead = szMesg, *pWrite = szMesg;

  while (*pRead != '\0')
  {
    bool matched = false;

    if (*pRead == '[')
    {
      for (uint8_t i = 0; i < ICON_TABLE_SIZE; i++)
      {
        size_t len = strlen(iconTable[i].tag);
        if (strncmp(pRead, iconTable[i].tag, len) == 0)
        {
          *pWrite++ = iconTable[i].code;
          pRead += len;
          matched = true;
          break;
        }
      }
    }

    if (!matched)
      *pWrite++ = *pRead++;
  }

  *pWrite = '\0';
}

void collapseIcons(const char *szIn, char *szOut, uint16_t maxLen)
// Inverse of expandIcons(): rewrite icon control bytes back into their
// [tag] text form so a stored message can be shown and edited in the
// web UI. Output is truncated (but always null-terminated) if the
// expanded form does not fit in maxLen.
{
  uint16_t idx = 0;

  for (; *szIn != '\0'; szIn++)
  {
    const icon_t *pIcon = findIconByCode(*szIn);

    if (pIcon != NULL)
    {
      size_t len = strlen(pIcon->tag);
      if (idx + len >= maxLen) break;
      memcpy(&szOut[idx], pIcon->tag, len);
      idx += len;
    }
    else
    {
      if (idx + 1 >= maxLen) break;
      szOut[idx++] = *szIn;
    }
  }
  szOut[idx] = '\0';
}

// Persistence (NVS via Preferences) --------------------------------

void loadSettings(void)
{
  prefs.begin("mdmax", false);
  scrollDelay = prefs.getUChar("spd", DEFAULT_SCROLL_DELAY);
  brightness  = prefs.getUChar("brt", DEFAULT_BRIGHTNESS);
  displayMode = prefs.getUChar("mode", DEFAULT_MODE);
  appMode     = prefs.getUChar("amode", DEFAULT_APP_MODE);
  if (appMode > APP_MODE_LIFE) appMode = DEFAULT_APP_MODE;
  tzOffsetMin = prefs.getShort("tzofs", DEFAULT_TZ_OFFSET_MIN);
  tzOffsetMin = constrain(tzOffsetMin, MIN_TZ_OFFSET_MIN, MAX_TZ_OFFSET_MIN);
  dateEnabled = prefs.getBool("dateon", DEFAULT_DATE_ENABLED);
  dateEveryS  = prefs.getUShort("dateiv", DEFAULT_DATE_EVERY_S);
  dateEveryS  = constrain(dateEveryS, MIN_DATE_EVERY_S, MAX_DATE_EVERY_S);
  apiAuthEnabled = prefs.getBool("apiauth", false);
  prefs.getString("apikey", apiKey, sizeof(apiKey));
}

void saveSettings(void)
{
  prefs.putUChar("spd", scrollDelay);
  prefs.putUChar("brt", brightness);
  prefs.putUChar("mode", displayMode);
  prefs.putUChar("amode", appMode);
  prefs.putShort("tzofs", tzOffsetMin);
  prefs.putBool("dateon", dateEnabled);
  prefs.putUShort("dateiv", dateEveryS);
}

void applyTimeConfig(void)
// (Re)start SNTP with the configured UTC offset. Non-blocking: the ESP32
// SNTP client syncs in the background; until then getLocalTime() fails
// and the clock face shows "--:--".
{
  configTime((long)tzOffsetMin * 60, 0, NTP_SERVER_1, NTP_SERVER_2);
}

void setAppMode(uint8_t v)
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

void generateApiKey(void)
// Fill apiKey with a new random hex string (using the hardware RNG) and
// persist it, so a freshly-enabled API doesn't start with a blank/guessable
// key.
{
  const char hexDigits[] = "0123456789abcdef";
  for (uint8_t i = 0; i < API_KEY_LEN; i++)
    apiKey[i] = hexDigits[esp_random() % 16];
  apiKey[API_KEY_LEN] = '\0';
  prefs.putString("apikey", apiKey);
}

void saveApiAuthEnabled(bool enabled)
{
  apiAuthEnabled = enabled;
  prefs.putBool("apiauth", apiAuthEnabled);
}

void saveLastMessage(const char *szMesg)
{
  prefs.putString("lastmsg", szMesg);
}

boolean loadLastMessage(char *pszOut, uint16_t maxLen)
// Fill pszOut with the last saved message. Returns false (and leaves
// pszOut untouched) if there is none saved yet.
{
  if (prefs.getString("lastmsg", pszOut, maxLen) == 0) return(false);
  return(pszOut[0] != '\0');
}

void saveNetworkCreds(const char *newSsid, const char *newPass)
{
  prefs.putString("net_ssid", newSsid);
  prefs.putString("net_pass", newPass);
}

void connectWiFi(void)
// Try the saved network credentials (falling back to the factory
// defaults if none are saved). If the connection can't be made within
// WIFI_CONNECT_TIMEOUT, fall back to an open emergency setup Access
// Point so the device is never left unreachable.
{
  char savedSsid[64], savedPass[64];

  prefs.getString("net_ssid", savedSsid, sizeof(savedSsid));
  prefs.getString("net_pass", savedPass, sizeof(savedPass));

  const char *useSsid = (savedSsid[0] != '\0') ? savedSsid : ssid;
  const char *usePass = (savedSsid[0] != '\0') ? savedPass : password;
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

void sendResponseHeader(WiFiClient &client, size_t contentLength, bool cacheable, const char *contentType = "text/html", const char *status = "200 OK")
// Send HTTP status/headers with an accurate Content-Length so the browser
// knows exactly when the response body ends, instead of relying on the
// connection closing. Cacheable responses (the static page) also get a
// Cache-Control header so the ~15KB page isn't re-downloaded on every visit.
{
  client.print("HTTP/1.1 ");
  client.print(status);
  client.print("\r\n"
               "Content-Type: ");
  client.print(contentType);
  client.print("\r\n"
               "Content-Length: ");
  client.print(contentLength);
  client.print("\r\n");
  if (cacheable)
    client.print("Cache-Control: max-age=3600\r\n");
  else
    client.print("Cache-Control: no-store\r\n");
  client.print("Connection: close\r\n\r\n");
}

void sendChunked(WiFiClient &client, const char *data)
// Send a (potentially large) string to the client in small chunks,
// yielding between writes so the network stack can drain the socket
// and the watchdog timer never sees us block for too long in one go.
// client.write()'s return value (bytes actually accepted) is honoured
// instead of assumed, since the TCP send buffer can fill up and only
// take a partial chunk.
{
  const size_t CHUNK = 1024;
  size_t len = strlen(data);
  size_t sent = 0;

  while (sent < len)
  {
    if (!client.connected()) break;
    size_t n = min(CHUNK, len - sent);
    size_t written = client.write((const uint8_t *)(data + sent), n);
    if (written == 0)
    {
      // send buffer full - give the stack time to drain before retrying
      delay(1);
      continue;
    }
    sent += written;
    yield();
  }
}

const char WebPage[] = \
"<!DOCTYPE html>\n" \
"<html>\n" \
"<head>\n" \
"<title>ESP32 Matrix Display</title>\n" \
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n" \
"<style>\n" \
"  :root{\n" \
"    --bg:#16140f; --inset:#0e0d0a; --card:#211e17; --border:#3a3527;\n" \
"    --text:#f1ede3; --muted:#948d78; --accent:#ff9d2e; --accent-ink:#1a1206;\n" \
"    --ok:#5ecb7a; --err:#e3625a;\n" \
"    --font-display:'Courier New',monospace;\n" \
"    --font-body:-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;\n" \
"  }\n" \
"  @media (prefers-color-scheme: light){\n" \
"    :root{\n" \
"      --bg:#f4f1e8; --inset:#ffffff; --card:#ffffff; --border:#ddd6c2;\n" \
"      --text:#241f14; --muted:#7a7261; --accent:#d97a06; --accent-ink:#fff8ec;\n" \
"      --ok:#2f8f4e; --err:#c53a32;\n" \
"    }\n" \
"  }\n" \
"  :root[data-theme=\"dark\"]{\n" \
"    --bg:#16140f; --inset:#0e0d0a; --card:#211e17; --border:#3a3527;\n" \
"    --text:#f1ede3; --muted:#948d78; --accent:#ff9d2e; --accent-ink:#1a1206;\n" \
"    --ok:#5ecb7a; --err:#e3625a;\n" \
"  }\n" \
"  :root[data-theme=\"light\"]{\n" \
"    --bg:#f4f1e8; --inset:#ffffff; --card:#ffffff; --border:#ddd6c2;\n" \
"    --text:#241f14; --muted:#7a7261; --accent:#d97a06; --accent-ink:#fff8ec;\n" \
"    --ok:#2f8f4e; --err:#c53a32;\n" \
"  }\n" \
"  *{box-sizing:border-box;}\n" \
"  body{\n" \
"    margin:0; padding:28px 16px; background:var(--bg); color:var(--text);\n" \
"    font-family:var(--font-body);\n" \
"    display:flex; justify-content:center;\n" \
"  }\n" \
"  .wrap{width:100%; max-width:460px;}\n" \
"  .eyebrow{\n" \
"    font-family:var(--font-display); font-size:.68rem; letter-spacing:.16em;\n" \
"    text-transform:uppercase; color:var(--accent); margin:0 0 4px;\n" \
"  }\n" \
"  h1{\n" \
"    font-size:1.3rem; margin:0 0 20px; font-weight:600; letter-spacing:-.01em;\n" \
"    text-wrap:balance;\n" \
"  }\n" \
"  .card{\n" \
"    background:var(--card); border:1px solid var(--border); border-radius:14px;\n" \
"    padding:18px; margin-bottom:14px;\n" \
"  }\n" \
"  .card h2{\n" \
"    font-size:.72rem; text-transform:uppercase; letter-spacing:.1em;\n" \
"    color:var(--muted); margin:0 0 12px; font-weight:600;\n" \
"  }\n" \
"  input[type=text],input[type=number]{\n" \
"    width:100%; padding:11px 13px; border-radius:9px; border:1px solid var(--border);\n" \
"    background:var(--inset); color:var(--text); font-size:1rem; font-family:var(--font-body);\n" \
"  }\n" \
"  input[type=text]:focus,input[type=number]:focus{outline:2px solid var(--accent); outline-offset:1px; border-color:transparent;}\n" \
"  .preview{\n" \
"    margin-top:10px; padding:12px 14px; border-radius:9px; background:var(--inset);\n" \
"    border:1px solid var(--border); font-family:'Courier New',monospace; font-size:1.05rem;\n" \
"    letter-spacing:.03em; min-height:1.4em; word-break:break-all; color:var(--accent);\n" \
"  }\n" \
"  .icons{display:flex; flex-wrap:wrap; gap:6px; margin-top:12px;}\n" \
"  .icons button{\n" \
"    border:1px solid var(--border); background:var(--inset); color:var(--text);\n" \
"    border-radius:8px; width:36px; height:36px; font-size:1rem; cursor:pointer;\n" \
"    display:flex; align-items:center; justify-content:center;\n" \
"  }\n" \
"  .icons button:hover{border-color:var(--accent);}\n" \
"  .icons button:focus-visible{outline:2px solid var(--accent); outline-offset:1px;}\n" \
"  .row{display:flex; align-items:center; gap:12px; margin-bottom:12px;}\n" \
"  .row label{flex:0 0 84px; color:var(--muted); font-size:.82rem;}\n" \
"  .row input[type=range]{\n" \
"    flex:1; accent-color:var(--accent); height:4px;\n" \
"  }\n" \
"  .row .val{\n" \
"    width:30px; text-align:right; font-variant-numeric:tabular-nums;\n" \
"    font-size:.82rem; color:var(--muted);\n" \
"  }\n" \
"  .modes{display:flex; gap:8px;}\n" \
"  .modes label{\n" \
"    flex:1; text-align:center; padding:9px; border-radius:9px; border:1px solid var(--border);\n" \
"    cursor:pointer; font-size:.82rem; color:var(--muted); background:var(--inset);\n" \
"  }\n" \
"  .modes input{position:absolute; opacity:0; width:0; height:0;}\n" \
"  .modes input:focus-visible + span{outline:2px solid var(--accent); outline-offset:2px;}\n" \
"  .modes label:has(input:checked){\n" \
"    border-color:var(--accent); background:var(--accent); color:var(--accent-ink); font-weight:600;\n" \
"  }\n" \
"  .send{\n" \
"    width:100%; padding:13px; border:none; border-radius:9px; background:var(--accent);\n" \
"    color:var(--accent-ink); font-size:1rem; font-weight:600; cursor:pointer; margin-top:2px;\n" \
"  }\n" \
"  .send:hover{filter:brightness(1.08);}\n" \
"  .send:focus-visible{outline:2px solid var(--text); outline-offset:2px;}\n" \
"  .status{\n" \
"    margin-top:10px; font-size:.82rem; text-align:center; min-height:1.2em; color:var(--ok);\n" \
"    opacity:0; transition:opacity .3s;\n" \
"  }\n" \
"  .status.show{opacity:1;}\n" \
"  .status.err{color:var(--err);}\n" \
"  .tabs{display:flex; gap:4px; margin-bottom:14px; border-bottom:1px solid var(--border);}\n" \
"  .tabs button{\n" \
"    flex:1; padding:10px; border:none; background:none; color:var(--muted);\n" \
"    font-size:.85rem; font-weight:600; cursor:pointer; border-bottom:2px solid transparent;\n" \
"    margin-bottom:-1px; font-family:var(--font-body);\n" \
"  }\n" \
"  .tabs button.active{color:var(--accent); border-bottom-color:var(--accent);}\n" \
"  .tabs button:focus-visible{outline:2px solid var(--accent); outline-offset:-2px;}\n" \
"  .panel{display:none;}\n" \
"  .panel.active{display:block;}\n" \
"  .hint{color:var(--muted); font-size:.78rem; margin:0 0 12px; line-height:1.5;}\n" \
"  .field{margin-bottom:12px;}\n" \
"  .field label{display:block; font-size:.78rem; color:var(--muted); margin-bottom:6px;}\n" \
"  .net-status{\n" \
"    display:flex; align-items:center; gap:8px; font-size:.82rem; color:var(--muted);\n" \
"    margin-bottom:14px; padding:10px 12px; background:var(--inset); border-radius:9px;\n" \
"    border:1px solid var(--border);\n" \
"  }\n" \
"  .net-status .dot{width:8px; height:8px; border-radius:50%; background:var(--ok); flex:0 0 auto;}\n" \
"  .net-status.warn .dot{background:var(--accent);}\n" \
"  .signal{display:flex; align-items:flex-end; gap:2px; height:12px; margin-left:auto;}\n" \
"  .signal span{width:4px; background:var(--border); border-radius:1px;}\n" \
"  .signal span:nth-child(1){height:25%;}\n" \
"  .signal span:nth-child(2){height:50%;}\n" \
"  .signal span:nth-child(3){height:75%;}\n" \
"  .signal span:nth-child(4){height:100%;}\n" \
"  .signal.lvl1 span:nth-child(1),\n" \
"  .signal.lvl2 span:nth-child(-n+2),\n" \
"  .signal.lvl3 span:nth-child(-n+3),\n" \
"  .signal.lvl4 span:nth-child(-n+4){background:var(--ok);}\n" \
"</style>\n" \
"</head>\n" \
"\n" \
"<body>\n" \
"<div class=\"wrap\">\n" \
"  <p class=\"eyebrow\">MD_MAX72xx &middot; 4x8x8 matrix</p>\n" \
"  <h1>Control panel</h1>\n" \
"\n" \
"  <div class=\"tabs\">\n" \
"    <button type=\"button\" class=\"active\" id=\"tabBtnSettings\" onclick=\"showTab('settings')\">Settings</button>\n" \
"    <button type=\"button\" id=\"tabBtnNet\" onclick=\"showTab('net')\">Network</button>\n" \
"    <button type=\"button\" id=\"tabBtnApi\" onclick=\"showTab('api')\">API</button>\n" \
"  </div>\n" \
"\n" \
"  <div class=\"panel active\" id=\"panelSettings\">\n" \
"    <div class=\"card\">\n" \
"      <h2>Mode</h2>\n" \
"      <div class=\"modes\">\n" \
"        <label><input type=\"radio\" name=\"appmode\" value=\"0\" checked onchange=\"onAppModeChange()\"><span>Message</span></label>\n" \
"        <label><input type=\"radio\" name=\"appmode\" value=\"1\" onchange=\"onAppModeChange()\"><span>Clock</span></label>\n" \
"        <label><input type=\"radio\" name=\"appmode\" value=\"2\" onchange=\"onAppModeChange()\"><span>Game of Life</span></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <div class=\"card\" id=\"msgCard\">\n" \
"      <h2>Message</h2>\n" \
"      <form id=\"txt_form\" onsubmit=\"return false;\">\n" \
"        <input type=\"text\" id=\"Message\" maxlength=\"255\" placeholder=\"Type your message...\">\n" \
"      </form>\n" \
"      <div class=\"preview\" id=\"preview\">&nbsp;</div>\n" \
"      <div class=\"icons\">\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[heart]')\">&hearts;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[wifi]')\">&#128225;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[smile]')\">&#128578;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[star]')\">&#9733;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[music]')\">&#9834;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[bell]')\">&#128276;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[clock]')\">&#128340;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[ok]')\">&#10003;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[x]')\">&#10007;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[sun]')\">&#9728;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[rain]')\">&#127783;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[pin]')\">&#128205;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[plus]')\">&#10133;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[warn]')\">&#9888;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[bolt]')\">&#9889;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[fire]')\">&#128293;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[up]')\">&uarr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[down]')\">&darr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[left]')\">&larr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[right]')\">&rarr;</button>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <div class=\"card\">\n" \
"      <h2>Display</h2>\n" \
"      <div class=\"row\" id=\"spdRow\">\n" \
"        <label>Speed</label>\n" \
"        <input type=\"range\" id=\"spd\" min=\"20\" max=\"250\" value=\"75\" oninput=\"onSpdInput()\">\n" \
"        <span class=\"val\" id=\"spdVal\">75</span>\n" \
"      </div>\n" \
"      <div class=\"row\">\n" \
"        <label>Brightness</label>\n" \
"        <input type=\"range\" id=\"brt\" min=\"0\" max=\"15\" value=\"8\" oninput=\"onBrtInput()\">\n" \
"        <span class=\"val\" id=\"brtVal\">8</span>\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"tzRow\" style=\"display:none;\">\n" \
"        <label>Timezone (UTC)</label>\n" \
"        <input type=\"number\" id=\"tz\" min=\"-12\" max=\"14\" step=\"0.5\" value=\"-3\" onchange=\"SendText()\">\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"dateRow\" style=\"display:none;\">\n" \
"        <label style=\"flex:1 1 auto;\">Show date</label>\n" \
"        <input type=\"checkbox\" id=\"dateOn\" checked onchange=\"onDateToggle()\" style=\"width:20px; height:20px; flex:0 0 auto;\">\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"dateIvRow\" style=\"display:none;\">\n" \
"        <label>Date every (s)</label>\n" \
"        <input type=\"number\" id=\"dateIv\" min=\"5\" max=\"3600\" step=\"5\" value=\"30\" onchange=\"SendText()\">\n" \
"      </div>\n" \
"      <div class=\"modes\" id=\"msgModes\">\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"0\" checked onchange=\"SendText()\"><span>Scroll</span></label>\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"1\" onchange=\"SendText()\"><span>Blink</span></label>\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"2\" onchange=\"SendText()\"><span>Blink+Scroll</span></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <button class=\"send\" onclick=\"SendText()\">Send</button>\n" \
"    <div class=\"status\" id=\"status\">&nbsp;</div>\n" \
"  </div>\n" \
"\n" \
"  <div class=\"panel\" id=\"panelNet\">\n" \
"    <div class=\"net-status\" id=\"netStatus\">\n" \
"      <span class=\"dot\"></span>\n" \
"      <span id=\"netStatusText\">Checking...</span>\n" \
"      <span class=\"signal\" id=\"signalBars\"></span>\n" \
"    </div>\n" \
"    <div class=\"card\">\n" \
"      <h2>WiFi network</h2>\n" \
"      <p class=\"hint\">Enter the name (SSID) and password of the network the display should use. Saving reboots the device and it will try to connect. If it fails, it falls back to the <strong>MD-Display-Setup</strong> network so you can try again.</p>\n" \
"      <div class=\"field\">\n" \
"        <label for=\"nssid\">Network name (SSID)</label>\n" \
"        <input type=\"text\" id=\"nssid\" maxlength=\"63\" placeholder=\"My WiFi network\">\n" \
"      </div>\n" \
"      <div class=\"field\">\n" \
"        <label for=\"npass\">Password</label>\n" \
"        <input type=\"text\" id=\"npass\" maxlength=\"63\" placeholder=\"Leave blank to keep current password\">\n" \
"      </div>\n" \
"      <button class=\"send\" onclick=\"SaveNetwork()\">Save and reboot</button>\n" \
"      <div class=\"status\" id=\"netSaveStatus\">&nbsp;</div>\n" \
"    </div>\n" \
"  </div>\n" \
"\n" \
"  <div class=\"panel\" id=\"panelApi\">\n" \
"    <div class=\"card\">\n" \
"      <h2>REST API</h2>\n" \
"      <p class=\"hint\">Set the display message remotely (Home Assistant, Node-RED, curl, etc) with:</p>\n" \
"      <div class=\"preview\" id=\"apiExample\" style=\"word-break:break-all; font-size:.85rem;\">GET /api/display?msg=Hello</div>\n" \
"      <div class=\"row\" style=\"margin-top:14px;\">\n" \
"        <label style=\"flex:1 1 auto;\">Require API key</label>\n" \
"        <input type=\"checkbox\" id=\"apiAuthToggle\" onchange=\"onApiAuthToggle()\" style=\"width:20px; height:20px; flex:0 0 auto;\">\n" \
"      </div>\n" \
"      <div class=\"field\" id=\"apiKeyField\" style=\"display:none;\">\n" \
"        <label for=\"apiKeyValue\">API key (send as header <code>X-API-Key</code>)</label>\n" \
"        <input type=\"text\" id=\"apiKeyValue\" readonly onclick=\"this.select()\">\n" \
"        <button class=\"send\" style=\"margin-top:8px;\" onclick=\"RegenerateApiKey()\">Generate new key</button>\n" \
"        <p class=\"hint\" style=\"margin-top:8px;\">Generating a new key immediately invalidates the old one - update any automations using it.</p>\n" \
"      </div>\n" \
"      <div class=\"status\" id=\"apiStatus\">&nbsp;</div>\n" \
"    </div>\n" \
"  </div>\n" \
"</div>\n" \
"\n" \
"<script>\n" \
"var iconPreview = {\n" \
"  \"[heart]\":\"\\u2764\", \"[wifi]\":\"\\u{1F4E1}\", \"[smile]\":\"\\u{1F642}\",\n" \
"  \"[star]\":\"\\u2b50\", \"[music]\":\"\\u266a\", \"[bell]\":\"\\u{1F514}\", \"[clock]\":\"\\u{1F550}\",\n" \
"  \"[ok]\":\"\\u2713\", \"[x]\":\"\\u2717\", \"[sun]\":\"\\u2600\", \"[rain]\":\"\\u{1F327}\",\n" \
"  \"[pin]\":\"\\u{1F4CD}\", \"[plus]\":\"\\u2795\",\n" \
"  \"[warn]\":\"\\u26a0\", \"[bolt]\":\"\\u26a1\", \"[fire]\":\"\\u{1F525}\",\n" \
"  \"[up]\":\"\\u2191\", \"[down]\":\"\\u2193\", \"[left]\":\"\\u2190\", \"[right]\":\"\\u2192\"\n" \
"};\n" \
"\n" \
"function InsertIcon(tag){\n" \
"  var f = document.getElementById(\"Message\");\n" \
"  f.value += tag;\n" \
"  f.focus();\n" \
"  updatePreview();\n" \
"}\n" \
"\n" \
"function updatePreview(){\n" \
"  var txt = document.getElementById(\"Message\").value;\n" \
"  var out = txt.replace(/\\[[a-z]+\\]/g, function(m){ return iconPreview[m] || m; });\n" \
"  document.getElementById(\"preview\").textContent = out || \" \";\n" \
"}\n" \
"\n" \
"function onSpdInput(){\n" \
"  var v = document.getElementById(\"spd\").value;\n" \
"  document.getElementById(\"spdVal\").textContent = v;\n" \
"}\n" \
"function onBrtInput(){\n" \
"  var v = document.getElementById(\"brt\").value;\n" \
"  document.getElementById(\"brtVal\").textContent = v;\n" \
"}\n" \
"\n" \
"function currentMsgMode(){\n" \
"  var r = document.getElementsByName(\"msgmode\");\n" \
"  for (var i=0;i<r.length;i++) if (r[i].checked) return r[i].value;\n" \
"  return \"0\";\n" \
"}\n" \
"\n" \
"function currentAppMode(){\n" \
"  var r = document.getElementsByName(\"appmode\");\n" \
"  for (var i=0;i<r.length;i++) if (r[i].checked) return r[i].value;\n" \
"  return \"0\";\n" \
"}\n" \
"\n" \
"function updateModeUI(){\n" \
"  var mode = currentAppMode();\n" \
"  var clock = mode === \"1\";\n" \
"  var life = mode === \"2\";\n" \
"  var msgMode = !clock && !life;\n" \
"  var dateOn = document.getElementById(\"dateOn\").checked;\n" \
"  document.getElementById(\"msgCard\").style.display = msgMode ? \"block\" : \"none\";\n" \
"  document.getElementById(\"spdRow\").style.display = msgMode ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"msgModes\").style.display = msgMode ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"tzRow\").style.display = clock ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"dateRow\").style.display = clock ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"dateIvRow\").style.display = (clock && dateOn) ? \"flex\" : \"none\";\n" \
"}\n" \
"\n" \
"function onAppModeChange(){\n" \
"  updateModeUI();\n" \
"  SendText();\n" \
"}\n" \
"\n" \
"function onDateToggle(){\n" \
"  updateModeUI();\n" \
"  SendText();\n" \
"}\n" \
"\n" \
"function showStatus(msg, isError){\n" \
"  var s = document.getElementById(\"status\");\n" \
"  s.textContent = msg;\n" \
"  s.classList.toggle(\"err\", !!isError);\n" \
"  s.classList.add(\"show\");\n" \
"  setTimeout(function(){ s.classList.remove(\"show\"); }, 2000);\n" \
"}\n" \
"\n" \
"function SendText(){\n" \
"  var msg = document.getElementById(\"Message\").value;\n" \
"  var spd = document.getElementById(\"spd\").value;\n" \
"  var brt = document.getElementById(\"brt\").value;\n" \
"  var mode = currentMsgMode();\n" \
"  var tzMin = Math.round((parseFloat(document.getElementById(\"tz\").value) || 0) * 60);\n" \
"  var dateIv = parseInt(document.getElementById(\"dateIv\").value, 10) || 30;\n" \
"  var qs = \"&MSG=\" + encodeURIComponent(msg) +\n" \
"           \"/&SPD=\" + spd +\n" \
"           \"/&BRT=\" + brt +\n" \
"           \"/&MODE=\" + mode +\n" \
"           \"/&DMODE=\" + currentAppMode() +\n" \
"           \"/&TZ=\" + tzMin +\n" \
"           \"/&DATEON=\" + (document.getElementById(\"dateOn\").checked ? \"1\" : \"0\") +\n" \
"           \"/&DATEIV=\" + dateIv +\n" \
"           \"/&nocache=\" + Math.random();\n" \
"\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", qs, true);\n" \
"  request.onload = function(){\n" \
"    showStatus(\"Sent!\");\n" \
"  };\n" \
"  request.onerror = function(){ showStatus(\"Failed to send\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function showTab(name){\n" \
"  var panels = { settings: \"panelSettings\", net: \"panelNet\", api: \"panelApi\" };\n" \
"  var buttons = { settings: \"tabBtnSettings\", net: \"tabBtnNet\", api: \"tabBtnApi\" };\n" \
"  for (var key in panels){\n" \
"    document.getElementById(panels[key]).classList.toggle(\"active\", key === name);\n" \
"    document.getElementById(buttons[key]).classList.toggle(\"active\", key === name);\n" \
"  }\n" \
"}\n" \
"\n" \
"function SaveNetwork(){\n" \
"  var nssid = document.getElementById(\"nssid\").value;\n" \
"  var npass = document.getElementById(\"npass\").value;\n" \
"  if (!nssid){\n" \
"    showNetSaveStatus(\"Enter the network name\", true);\n" \
"    return;\n" \
"  }\n" \
"  var qs = \"&NSSID=\" + encodeURIComponent(nssid) +\n" \
"           \"/&NPASS=\" + encodeURIComponent(npass) +\n" \
"           \"/&nocache=\" + Math.random();\n" \
"\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", qs, true);\n" \
"  request.onload = function(){ showNetSaveStatus(\"Saved! Rebooting...\"); };\n" \
"  request.onerror = function(){ showNetSaveStatus(\"Failed to save\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function showNetSaveStatus(msg, isError){\n" \
"  var s = document.getElementById(\"netSaveStatus\");\n" \
"  s.textContent = msg;\n" \
"  s.classList.toggle(\"err\", !!isError);\n" \
"  s.classList.add(\"show\");\n" \
"  setTimeout(function(){ s.classList.remove(\"show\"); }, 3000);\n" \
"}\n" \
"\n" \
"function showApiStatus(msg, isError){\n" \
"  var s = document.getElementById(\"apiStatus\");\n" \
"  s.textContent = msg;\n" \
"  s.classList.toggle(\"err\", !!isError);\n" \
"  s.classList.add(\"show\");\n" \
"  setTimeout(function(){ s.classList.remove(\"show\"); }, 3000);\n" \
"}\n" \
"\n" \
"function renderApiSettings(enabled, key){\n" \
"  document.getElementById(\"apiAuthToggle\").checked = enabled;\n" \
"  document.getElementById(\"apiKeyField\").style.display = enabled ? \"block\" : \"none\";\n" \
"  document.getElementById(\"apiKeyValue\").value = key || \"\";\n" \
"  document.getElementById(\"apiExample\").textContent =\n" \
"    \"GET /api/display?msg=Hello\" + (enabled ? \"  (header X-API-Key required)\" : \"\");\n" \
"}\n" \
"\n" \
"function loadApiSettings(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/&APISETTINGS=1/&nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    if (request.responseText){\n" \
"      var parts = request.responseText.split(\",\");\n" \
"      renderApiSettings(parts[0] === \"1\", parts[1] || \"\");\n" \
"    }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function onApiAuthToggle(){\n" \
"  var enabled = document.getElementById(\"apiAuthToggle\").checked;\n" \
"  var qs = \"&APIAUTH=\" + (enabled ? \"1\" : \"0\") + \"/&nocache=\" + Math.random();\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", qs, true);\n" \
"  request.onload = function(){\n" \
"    showApiStatus(enabled ? \"API key required\" : \"API key not required\");\n" \
"    loadApiSettings();\n" \
"  };\n" \
"  request.onerror = function(){ showApiStatus(\"Failed to save\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function RegenerateApiKey(){\n" \
"  var qs = \"&APIREGEN=1/&nocache=\" + Math.random();\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", qs, true);\n" \
"  request.onload = function(){\n" \
"    showApiStatus(\"New key generated\");\n" \
"    loadApiSettings();\n" \
"  };\n" \
"  request.onerror = function(){ showApiStatus(\"Failed to generate key\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function checkNetMode(){\n" \
"  var el = document.getElementById(\"netStatus\");\n" \
"  var txt = document.getElementById(\"netStatusText\");\n" \
"  if (location.hostname === \"192.168.4.1\"){\n" \
"    el.classList.add(\"warn\");\n" \
"    txt.textContent = \"Setup mode (AP) - enter your network below\";\n" \
"  } else {\n" \
"    txt.textContent = \"Connected\";\n" \
"  }\n" \
"}\n" \
"\n" \
"function renderSignal(rssi){\n" \
"  var el = document.getElementById(\"signalBars\");\n" \
"  el.innerHTML = \"\";\n" \
"  for (var i = 0; i < 4; i++) el.appendChild(document.createElement(\"span\"));\n" \
"  el.className = \"signal\";\n" \
"  if (!rssi) return;\n" \
"  var lvl = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;\n" \
"  el.classList.add(\"lvl\" + lvl);\n" \
"  el.title = rssi + \" dBm\";\n" \
"}\n" \
"\n" \
"function loadNetInfo(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/&NETINFO=1/&nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    if (request.responseText){\n" \
"      var parts = request.responseText.split(\",\");\n" \
"      document.getElementById(\"nssid\").value = parts[0];\n" \
"      renderSignal(parts.length > 1 ? parseInt(parts[1], 10) : 0);\n" \
"    }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function loadCurrentSettings(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/&SETTINGS=1/&nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    // scrollDelay, brightness, displayMode, appMode, tzOffsetMin,\n" \
"    // dateEnabled, dateEveryS, last message\n" \
"    var parts = request.responseText.split(\",\");\n" \
"    if (parts.length < 7) return;\n" \
"    var displayMode = parts[2];\n" \
"\n" \
"    document.getElementById(\"spd\").value = parts[0];\n" \
"    document.getElementById(\"spdVal\").textContent = parts[0];\n" \
"    document.getElementById(\"brt\").value = parts[1];\n" \
"    document.getElementById(\"brtVal\").textContent = parts[1];\n" \
"\n" \
"    var msgModeInput = document.querySelector(\"input[name=msgmode][value='\" + displayMode + \"']\");\n" \
"    if (msgModeInput) msgModeInput.checked = true;\n" \
"\n" \
"    var appModeInput = document.querySelector(\"input[name=appmode][value='\" + parts[3] + \"']\");\n" \
"    if (appModeInput) appModeInput.checked = true;\n" \
"    document.getElementById(\"tz\").value = parseInt(parts[4], 10) / 60;\n" \
"    document.getElementById(\"dateOn\").checked = parts[5] === \"1\";\n" \
"    document.getElementById(\"dateIv\").value = parseInt(parts[6], 10) || 30;\n" \
"    updateModeUI();\n" \
"\n" \
"    // Prefill the message box with the last message sent (the message\n" \
"    // may contain commas, so rejoin everything after the 7th field),\n" \
"    // unless the user already started typing.\n" \
"    var lastMsg = parts.slice(7).join(\",\");\n" \
"    var msgEl = document.getElementById(\"Message\");\n" \
"    if (lastMsg && !msgEl.value){ msgEl.value = lastMsg; updatePreview(); }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"document.getElementById(\"Message\").addEventListener(\"input\", updatePreview);\n" \
"updatePreview();\n" \
"checkNetMode();\n" \
"// Run these one at a time - the device only handles one HTTP\n" \
"// connection at a time, so firing them all in parallel makes the\n" \
"// browser queue up requests and can trip its connection timeout.\n" \
"loadNetInfo(function(){\n" \
"  loadCurrentSettings(function(){\n" \
"    loadApiSettings();\n" \
"  });\n" \
"});\n" \
"</script>\n" \
"</body>\n" \
"</html>\n" \
"\n";

const char *err2Str(wl_status_t code)
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

uint8_t htoi(char c)
{
  c = toupper(c);
  if ((c >= '0') && (c <= '9')) return(c - '0');
  if ((c >= 'A') && (c <= 'F')) return(c - 'A' + 0xa);
  return(0);
}

boolean getParam(char *szMesg, const char *key, char *psz, uint8_t len)
// Extract the value of "/&KEY=...value.../&" from szMesg into psz,
// URL-decoding %xx escapes along the way.
{
  boolean isValid = false;  // text received flag
  char *pStart, *pEnd;      // pointer to start and end of text
  char szKey[16];           // must fit "/&" + longest key ("APIREGEN") + "=" + null

  sprintf(szKey, "/&%s=", key);
  pStart = strstr(szMesg, szKey);

  if (pStart != NULL)
  {
    pStart += strlen(szKey);  // skip to start of data
    pEnd = strstr(pStart, "/&");
    // Never scan past the end of the request line (a '\n' now follows it,
    // since szBuf holds the full request incl. headers, not just one line).
    char *pLineEnd = strchr(pStart, '\n');
    if ((pLineEnd != NULL) && (pEnd == NULL || pLineEnd < pEnd)) pEnd = NULL;

    if (pEnd != NULL)
    {
      uint8_t written = 0;

      while ((pStart < pEnd) && (written + 1 < len))
      {
        if ((*pStart == '%') && isxdigit(*(pStart+1)))
        {
          // replace %xx hex code with the ASCII character
          char c = 0;
          pStart++;
          c += (htoi(*pStart++) << 4);
          c += htoi(*pStart++);
          *psz++ = c;
        }
        else
          *psz++ = *pStart++;
        written++;
      }

      *psz = '\0'; // terminate the string
      isValid = true;
    }
  }

  return(isValid);
}

boolean getQueryParam(const char *szUrl, const char *key, char *psz, uint8_t len)
// Extract the value of "key=...value..." from a standard HTTP query string
// (the part of szUrl after '?', with params separated by '&'), URL-decoding
// %xx escapes and '+' (space) along the way. Used by the REST API, which
// takes ordinary ?key=value&key2=value2 query strings rather than the
// "/&KEY=.../&" scheme the control panel's own requests use.
{
  boolean isValid = false;
  const char *pQuery = strchr(szUrl, '?');
  char szKey[12];

  if (pQuery == NULL) return(false);
  pQuery++;  // skip '?'

  sprintf(szKey, "%s=", key);
  size_t keyLen = strlen(szKey);

  const char *pParam = pQuery;
  while (pParam != NULL && *pParam != '\0' && *pParam != ' ' && *pParam != '\n')
  {
    if (strncmp(pParam, szKey, keyLen) == 0)
    {
      const char *pStart = pParam + keyLen;
      const char *pEnd = pStart;
      while ((*pEnd != '&') && (*pEnd != '\0') && (*pEnd != ' ') && (*pEnd != '\n')) pEnd++;

      uint8_t written = 0;
      while ((pStart < pEnd) && (written + 1 < len))
      {
        if ((*pStart == '%') && isxdigit(*(pStart + 1)) && isxdigit(*(pStart + 2)))
        {
          char c = (htoi(*(pStart + 1)) << 4) + htoi(*(pStart + 2));
          *psz++ = c;
          pStart += 3;
        }
        else if (*pStart == '+')
        {
          *psz++ = ' ';
          pStart++;
        }
        else
          *psz++ = *pStart++;
        written++;
      }
      *psz = '\0';
      isValid = true;
      break;
    }
    pParam = strchr(pParam, '&');
    if (pParam != NULL) pParam++;
  }

  return(isValid);
}

boolean getHeader(const char *szReq, const char *headerName, char *psz, uint8_t len)
// Find "HeaderName: value" as one of the '\n'-separated header lines in
// szReq (everything after the first line) and copy value into psz. Header
// name matching is case-insensitive, per HTTP semantics.
{
  char szKey[24];
  sprintf(szKey, "%s:", headerName);
  size_t keyLen = strlen(szKey);

  const char *pLine = strchr(szReq, '\n');  // skip the request line itself
  while (pLine != NULL)
  {
    pLine++;  // move past the '\n' onto the next line
    if (strncasecmp(pLine, szKey, keyLen) == 0)
    {
      const char *pStart = pLine + keyLen;
      while (*pStart == ' ') pStart++;  // skip "Header: <spaces>value"
      const char *pEnd = pStart;
      while ((*pEnd != '\n') && (*pEnd != '\0')) pEnd++;

      uint8_t written = 0;
      while ((pStart < pEnd) && (written + 1 < len))
      {
        *psz++ = *pStart++;
        written++;
      }
      *psz = '\0';
      return(true);
    }
    pLine = strchr(pLine, '\n');
  }
  return(false);
}

void handleWiFi(void)
{
  static enum { S_IDLE, S_WAIT_CONN, S_READ, S_EXTRACT, S_RESPONSE, S_DISCONN } state = S_IDLE;
  static char szBuf[2048];
  static char lastCh = '\0';
  static uint16_t idxBuf = 0;
  static WiFiClient client;
  static uint32_t timeStart;
  static bool wantsNetInfo = false;
  static bool wantsSettings = false;
  static bool wantsApiMessage = false;
  static bool apiMessageOk = false;
  static bool apiSettingsChanged = false;
  static uint16_t apiAlertSecs = 0;   // >0 while handling an ?alert= request
  static bool apiAuthFailed = false;
  static bool wantsApiSettings = false;

  switch (state)
  {
  case S_IDLE:   // initialize
    PRINTS("\nS_IDLE");
    idxBuf = 0;
    lastCh = '\0';
    state = S_WAIT_CONN;
    break;

  case S_WAIT_CONN:   // waiting for connection
    {
      client = server.accept();
      if (!client) break;
      if (!client.connected()) break;

#if DEBUG
      char szTxt[20];
      sprintf(szTxt, "%d:%d:%d:%d", client.remoteIP()[0], client.remoteIP()[1], client.remoteIP()[2], client.remoteIP()[3]);
      PRINT("\nNew client @ ", szTxt);
#endif

      timeStart = millis();
      PRINTS("\nS_READ");
      state = S_READ;
    }
    break;

  case S_READ: // read the request line and headers, up to the blank line
    // that ends them. Headers are kept (as '\n'-separated lines within
    // szBuf) so S_EXTRACT can look up X-API-Key; getParam()/getQueryParam()
    // only ever match against the first line, so this is transparent to them.
    while (client.available() && (state == S_READ))
    {
      char c = client.read();
      if (c == '\r') continue;
      if (c == '\n')
      {
        // Blank line (two consecutive '\n's) marks the end of the headers.
        // Tracked in lastCh rather than szBuf so the end of the request is
        // still detected even when the headers overflow the buffer and the
        // extra characters (including '\n's) are being dropped.
        if (lastCh == '\n')
        {
          state = S_EXTRACT;
        }
        else if (idxBuf < sizeof(szBuf) - 1)
          szBuf[idxBuf++] = '\n';
      }
      else if (idxBuf < sizeof(szBuf) - 1)
        szBuf[idxBuf++] = (char)c;
      // else: request too long for the buffer - silently drop extra characters
      lastCh = c;
    }
    if (state == S_EXTRACT)
    {
      szBuf[idxBuf] = '\0';
      PRINT("\nRecv: ", szBuf);
    }
    if ((state == S_READ) && (millis() - timeStart > 500))
    {
      PRINTS("\nWait timeout");
      state = S_DISCONN;
    }
    break;


  case S_EXTRACT: // extract data
    {
      PRINTS("\nS_EXTRACT");
      char szParam[8];
      bool settingsChanged = false;

      wantsApiMessage = (strncmp(szBuf, "GET /api/display", 16) == 0);
      wantsNetInfo = false;
      wantsSettings = false;
      wantsApiSettings = false;
      apiAuthFailed = false;

      if (wantsApiMessage)
      {
        // If auth is enabled, the caller must send a matching X-API-Key
        // header. Checked before touching anything else so an unauthorized
        // request can never change the message or settings.
        if (apiAuthEnabled)
        {
          char szKeyHeader[API_KEY_LEN + 1];
          apiAuthFailed = !(getHeader(szBuf, "X-API-Key", szKeyHeader, sizeof(szKeyHeader))
                             && (strcmp(szKeyHeader, apiKey) == 0));
        }

        if (apiAuthFailed)
        {
          state = S_RESPONSE;
          break;
        }

        // REST API: standard ?msg=...&spd=...&brt=...&mode=... query string,
        // meant for external callers (Home Assistant, Node-RED, curl, etc).
        apiMessageOk = getQueryParam(szBuf, "msg", newMessage, MESG_SIZE);
        if (apiMessageOk)
        {
          expandIcons(newMessage);
          newMessageAvailable = true;
        }

        // Temporary alert: ?msg=...&alert=<seconds> shows the message for
        // that long, then loop() restores the state snapshotted here -
        // before spd/brt/mode below can touch it. A second alert while one
        // is active keeps the original snapshot and just resets the timer;
        // a plain message cancels any pending revert.
        apiAlertSecs = 0;
        if (apiMessageOk && getQueryParam(szBuf, "alert", szParam, sizeof(szParam)))
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

        if (getQueryParam(szBuf, "spd", szParam, sizeof(szParam)))
        {
          uint8_t v = (uint8_t)constrain(atoi(szParam), MIN_SCROLL_DELAY, MAX_SCROLL_DELAY);
          if (v != scrollDelay) { scrollDelay = v; settingsChanged = true; }
        }
        if (getQueryParam(szBuf, "brt", szParam, sizeof(szParam)))
        {
          uint8_t v = (uint8_t)constrain(atoi(szParam), 0, MAX_INTENSITY);
          if (v != brightness) { brightness = v; mx.control(MD_MAX72XX::INTENSITY, brightness); settingsChanged = true; }
        }
        if (getQueryParam(szBuf, "mode", szParam, sizeof(szParam)))
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
        if (getQueryParam(szBuf, "tz", szParam, sizeof(szParam)))
        {
          int16_t v = (int16_t)constrain(atoi(szParam), MIN_TZ_OFFSET_MIN, MAX_TZ_OFFSET_MIN);
          if (v != tzOffsetMin)
          {
            tzOffsetMin = v;
            if (!apMode) applyTimeConfig();
            settingsChanged = true;
          }
        }
        if (getQueryParam(szBuf, "date", szParam, sizeof(szParam)))
        {
          bool v = (szParam[0] == '1');
          if (v != dateEnabled) { dateEnabled = v; settingsChanged = true; }
        }
        if (getQueryParam(szBuf, "dateint", szParam, sizeof(szParam)))
        {
          uint16_t v = (uint16_t)constrain(atoi(szParam), MIN_DATE_EVERY_S, MAX_DATE_EVERY_S);
          if (v != dateEveryS) { dateEveryS = v; settingsChanged = true; }
        }
        // Top-level mode: ?display=clock|life|gol|message. Sending a
        // message without an explicit display= also switches back to
        // message mode, so a plain ?msg=... always ends up visible.
        {
          uint8_t v = appMode;
          if (getQueryParam(szBuf, "display", szParam, sizeof(szParam)))
          {
            if (strcmp(szParam, "clock") == 0) v = APP_MODE_CLOCK;
            else if (strcmp(szParam, "life") == 0 || strcmp(szParam, "gol") == 0) v = APP_MODE_LIFE;
            else v = APP_MODE_MESSAGE;
            if (apiAlertSecs == 0) alertActive = false;  // explicit mode change wins over a pending revert
          }
          else if (apiMessageOk)
            v = APP_MODE_MESSAGE;
          if (v != appMode)
          {
            setAppMode(v);
            settingsChanged = true;
          }
        }
        // Alert state is temporary by definition - don't persist it
        if (settingsChanged && (apiAlertSecs == 0)) saveSettings();
        apiSettingsChanged = settingsChanged;

        state = S_RESPONSE;
        break;
      }

      // Extract the message text, if there is one
      newMessageAvailable = getParam(szBuf, "MSG", newMessage, MESG_SIZE);
      if (newMessageAvailable)
      {
        expandIcons(newMessage);
        alertActive = false;  // explicit user message cancels a pending alert revert
      }
      PRINT("\nNew Msg: ", newMessage);

      // Optional scroll speed, brightness and display mode
      if (getParam(szBuf, "SPD", szParam, sizeof(szParam)))
      {
        uint8_t v = (uint8_t)constrain(atoi(szParam), MIN_SCROLL_DELAY, MAX_SCROLL_DELAY);
        if (v != scrollDelay) { scrollDelay = v; settingsChanged = true; }
      }
      if (getParam(szBuf, "BRT", szParam, sizeof(szParam)))
      {
        uint8_t v = (uint8_t)constrain(atoi(szParam), 0, MAX_INTENSITY);
        if (v != brightness) { brightness = v; mx.control(MD_MAX72XX::INTENSITY, brightness); settingsChanged = true; }
      }
      if (getParam(szBuf, "MODE", szParam, sizeof(szParam)))
      {
        uint8_t v = (uint8_t)constrain(atoi(szParam), 0, 2);
        if (v != displayMode)
        {
          displayMode = v;
          mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
          resetScrollSource();  // discard any in-progress scroll state from the old mode
          if (displayMode == 0)
          {
            // start scroll mode from the last user message
            newMessageAvailable = true;
          }
          else if (displayMode == 1) showStatic();  // redraw immediately in static mode
          else resetBlinkScroll();                  // blinkScroll() redraws on the next loop()
          settingsChanged = true;
        }
      }
      if (getParam(szBuf, "DMODE", szParam, sizeof(szParam)))
      {
        uint8_t v = (uint8_t)constrain(atoi(szParam), APP_MODE_MESSAGE, APP_MODE_LIFE);
        alertActive = false;  // explicit mode choice from the UI wins over a pending revert
        if (v != appMode)
        {
          setAppMode(v);
          settingsChanged = true;
        }
      }
      if (getParam(szBuf, "TZ", szParam, sizeof(szParam)))
      {
        int16_t v = (int16_t)constrain(atoi(szParam), MIN_TZ_OFFSET_MIN, MAX_TZ_OFFSET_MIN);
        if (v != tzOffsetMin)
        {
          tzOffsetMin = v;
          if (!apMode) applyTimeConfig();
          settingsChanged = true;
        }
      }
      if (getParam(szBuf, "DATEON", szParam, sizeof(szParam)))
      {
        bool v = (szParam[0] == '1');
        if (v != dateEnabled) { dateEnabled = v; settingsChanged = true; }
      }
      if (getParam(szBuf, "DATEIV", szParam, sizeof(szParam)))
      {
        uint16_t v = (uint16_t)constrain(atoi(szParam), MIN_DATE_EVERY_S, MAX_DATE_EVERY_S);
        if (v != dateEveryS) { dateEveryS = v; settingsChanged = true; }
      }
      if (settingsChanged) saveSettings();

      // Optional new network credentials - save and reboot to apply.
      // A blank password keeps whatever password is already saved.
      char newSsid[64], newPass[64];
      if (getParam(szBuf, "NSSID", newSsid, sizeof(newSsid)) && (newSsid[0] != '\0'))
      {
        newPass[0] = '\0';
        getParam(szBuf, "NPASS", newPass, sizeof(newPass));
        if (newPass[0] == '\0') prefs.getString("net_pass", newPass, sizeof(newPass));
        saveNetworkCreds(newSsid, newPass);
        restartPending = true;
        restartTime = millis() + 1000; // let the HTTP response go out first
      }

      // REST API auth config from the web UI: enable/disable, and/or
      // generate a fresh key (checked before the enable flag so turning
      // auth on and generating a key can both happen in the same request).
      char szParam2[8];
      if (getParam(szBuf, "APIREGEN", szParam2, sizeof(szParam2)) && (szParam2[0] == '1'))
        generateApiKey();
      if (getParam(szBuf, "APIAUTH", szParam2, sizeof(szParam2)))
        saveApiAuthEnabled(szParam2[0] == '1');

      // Special requests for the current network name / settings
      wantsNetInfo = (strstr(szBuf, "/&NETINFO=1") != NULL);
      wantsSettings = (strstr(szBuf, "/&SETTINGS=1") != NULL);
      wantsApiSettings = (strstr(szBuf, "/&APISETTINGS=1") != NULL);

      state = S_RESPONSE;
    }
    break;

  case S_RESPONSE: // send the response to the client
    PRINTS("\nS_RESPONSE");
    if (wantsApiMessage && apiAuthFailed)
    {
      const char *szJson = "{\"ok\":false,\"error\":\"unauthorized\"}";
      sendResponseHeader(client, strlen(szJson), false, "application/json", "401 Unauthorized");
      client.print(szJson);
    }
    else if (wantsApiMessage)
    {
      char szJson[MESG_SIZE * 4 + 32];
      if (apiMessageOk)
      {
        // Icon control bytes are not valid inside a JSON string, so echo
        // the message with them collapsed back to their [tag] form, then
        // escape '"' and '\' so the text can't break out of the string
        char szTags[MESG_SIZE * 2];
        char szEsc[MESG_SIZE * 4];
        collapseIcons(newMessage, szTags, sizeof(szTags));
        char *pOut = szEsc;
        for (char *pIn = szTags; *pIn != '\0'; pIn++)
        {
          if ((*pIn == '"') || (*pIn == '\\')) *pOut++ = '\\';
          *pOut++ = *pIn;
        }
        *pOut = '\0';
        if (apiAlertSecs > 0)
          sprintf(szJson, "{\"ok\":true,\"msg\":\"%s\",\"alert\":%u}", szEsc, apiAlertSecs);
        else
          sprintf(szJson, "{\"ok\":true,\"msg\":\"%s\"}", szEsc);
      }
      else if (apiSettingsChanged)
        sprintf(szJson, "{\"ok\":true}");
      else
        sprintf(szJson, "{\"ok\":false,\"error\":\"missing msg parameter\"}");
      sendResponseHeader(client, strlen(szJson), false, "application/json");
      client.print(szJson);
    }
    else if (wantsNetInfo)
    {
      // SSID, RSSI (dBm; 0 in AP setup mode, where there's no upstream link)
      char szNetInfo[80];
      sprintf(szNetInfo, "%s,%d", currentSsid, apMode ? 0 : WiFi.RSSI());
      sendResponseHeader(client, strlen(szNetInfo), false);
      client.print(szNetInfo);
    }
    else if (wantsSettings)
    {
      // scrollDelay, brightness, displayMode, appMode, tzOffsetMin,
      // dateEnabled, dateEveryS, last saved message (icon control bytes
      // collapsed back to [tag] form so it can be edited). The message
      // goes last because it may itself contain commas - the UI rejoins
      // everything after the seventh field.
      char szSettings[MESG_SIZE * 7 + 48]; // worst case: every byte is a 7-char [tag]
      char szLast[MESG_SIZE];

      if (!loadLastMessage(szLast, sizeof(szLast))) szLast[0] = '\0';
      sprintf(szSettings, "%d,%d,%d,%d,%d,%d,%d,", scrollDelay, brightness, displayMode, appMode, tzOffsetMin,
              dateEnabled ? 1 : 0, dateEveryS);
      collapseIcons(szLast, szSettings + strlen(szSettings), sizeof(szSettings) - strlen(szSettings));
      sendResponseHeader(client, strlen(szSettings), false);
      client.print(szSettings);
    }
    else if (wantsApiSettings)
    {
      // apiAuthEnabled (0/1), apiKey
      char szApiSettings[16 + API_KEY_LEN];
      sprintf(szApiSettings, "%d,%s", apiAuthEnabled ? 1 : 0, apiKey);
      sendResponseHeader(client, strlen(szApiSettings), false);
      client.print(szApiSettings);
    }
    else
    {
      // Return the response to the client (web page)
      sendResponseHeader(client, sizeof(WebPage) - 1, true);
      sendChunked(client, WebPage);
    }
    state = S_DISCONN;
    break;

  case S_DISCONN: // disconnect client
    PRINTS("\nS_DISCONN");
    // Drain whatever request bytes (rest of the headers, etc) the browser
    // is still sending before closing - otherwise the socket gets RST
    // instead of closed cleanly, and the browser stalls/retries the next
    // request while it notices the connection was reset.
    while (client.available()) client.read();
    client.flush();
    client.stop();
    // Persist the new message after replying, so the flash write doesn't
    // delay the HTTP response the browser is waiting on. Alert messages
    // are temporary and must not clobber the saved one.
    if (newMessageAvailable && !alertActive) saveLastMessage(newMessage);
    state = S_IDLE;
    break;

  default:  state = S_IDLE;
  }
}

void scrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
// Callback function for data that is being scrolled off the display
{
#if PRINT_CALLBACK
  Serial.print("\n cb ");
  Serial.print(dev);
  Serial.print(' ');
  Serial.print(t);
  Serial.print(' ');
  Serial.println(col);
#endif
}

enum scrollState_t { S_IDLE, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE };
scrollState_t scrollSourceState = S_IDLE;
bool scrollSourceResetPending = false;
char *scrollResumePos = NULL;  // where S_IDLE starts feeding (NULL = start of curMessage)
bool scrollHoldAtEnd = false;  // one-shot pass: feed blanks after the message ends instead of wrapping

void resetScrollSource(void)
// Force scrollDataSource() to restart from the beginning of curMessage
// on its next call, discarding whatever it was in the middle of.
{
  scrollSourceResetPending = true;
  scrollResumePos = NULL;
  scrollHoldAtEnd = false;
}

void resetScrollSourceAt(char *pos)
// One-shot scroll pass starting mid-message: scrollDataSource() feeds
// columns from pos onward and holds blank after the end instead of
// wrapping back to the start of the message.
{
  scrollSourceResetPending = true;
  scrollResumePos = pos;
  scrollHoldAtEnd = true;
}

bool blinkScrollResetPending = false;

void resetBlinkScroll(void)
// Force blinkScroll() to re-measure curMessage and restart from the
// blink phase on its next call (its state persists across mode changes).
{
  blinkScrollResetPending = true;
}

uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
// Callback function for data that is required for scrolling into the display
{
  static char *p;
  static uint16_t curLen, showLen;
  static uint8_t  cBuf[8];
  uint8_t colData = 0;

  if (scrollSourceResetPending)
  {
    scrollSourceState = S_IDLE;
    scrollSourceResetPending = false;
  }

  // finite state machine to control what we do on the callback
  switch (scrollSourceState)
  {
  case S_IDLE: // reset the message pointer and check for new message to load
    PRINTS("\nS_IDLE");
    p = curMessage;      // reset the pointer to start of message
    if (newMessageAvailable)  // there is a new message waiting
    {
      PRINT("\nNew message - ", newMessage);
      strcpy(curMessage, newMessage); // copy it in
      newMessageAvailable = false;
    }
    if (scrollResumePos != NULL)
    {
      p = scrollResumePos;  // one-shot pass resuming mid-message
      scrollResumePos = NULL;
    }
    else if (scrollHoldAtEnd)
      break;  // one-shot pass done: keep feeding blank columns
    scrollSourceState = S_NEXT_CHAR;
    break;

  case S_NEXT_CHAR: // Load the next character from the font table
    PRINT("\nS_NEXT_CHAR ", *p);
    if (*p == '\0')
      scrollSourceState = S_IDLE;
    else
    {
      const icon_t *icon = findIconByCode(*p);
      if (icon != NULL)
      {
        showLen = icon->width;
        memcpy(cBuf, icon->bitmap, showLen);
      }
      else
        showLen = mx.getChar(*p, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
      p++;
      curLen = 0;
      scrollSourceState = S_SHOW_CHAR;
    }
    break;

  case S_SHOW_CHAR: // display the next part of the character
    PRINTS("\nS_SHOW_CHAR");
    colData = cBuf[curLen++];
    if (curLen < showLen)
      break;

    // set up the inter character spacing
    showLen = (*p != '\0' ? CHAR_SPACING : (MAX_DEVICES*COL_SIZE)/2);
    curLen = 0;
    scrollSourceState = S_SHOW_SPACE;
    // fall through

  case S_SHOW_SPACE:  // display inter-character spacing (blank column)
    PRINT("\nS_SHOW_SPACE: ", curLen);
    PRINT("/", showLen);
    curLen++;
    if (curLen == showLen)
      scrollSourceState = S_NEXT_CHAR;
    break;

  default:
    scrollSourceState = S_IDLE;
  }

  return(colData);
}

uint16_t measureStaticText(const char *szMesg)
// Width in columns of a message as renderStaticText() draws it: glyph
// widths plus one spacing column between glyphs (none after the last)
{
  uint16_t w = 0;
  uint8_t cBuf[8];

  for (const char *p = szMesg; *p != '\0'; p++)
  {
    const icon_t *icon = findIconByCode(*p);
    w += ((icon != NULL) ? icon->width : mx.getChar(*p, sizeof(cBuf), cBuf)) + 1;
  }
  return (w > 0) ? w - 1 : 0;
}

void renderStaticText(const char *szMesg, bool center = false, const char **rest = NULL)
// Render a message statically (no scroll), left-aligned by default or
// horizontally centered, truncated to what fits on the display. The
// highest column index is the left edge of the display (TSL shifts
// toward higher columns), so the cursor starts high and walks down
// while each glyph's font data is written forward - the same
// orientation setChar() and the scroll mode produce.
// When rest is given, glyphs are never cut in half: rendering stops
// before the first glyph that doesn't fully fit and *rest points at it
// (or at the terminator when the whole message fit), so a scroll pass
// can resume exactly where the static view left off.
{
  const uint16_t totalCols = MAX_DEVICES * COL_SIZE;
  int16_t col = totalCols - 1;

  if (center)
  {
    uint16_t w = measureStaticText(szMesg);
    if (w < totalCols) col -= (totalCols - w) / 2;
  }
  const char *p = szMesg;
  uint8_t cBuf[8];

  mx.clear();
  while ((*p != '\0') && (col >= 0))
  {
    const icon_t *icon = findIconByCode(*p);
    uint8_t len;

    if (icon != NULL)
    {
      len = icon->width;
      memcpy(cBuf, icon->bitmap, len);
    }
    else
      len = mx.getChar(*p, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);

    if ((rest != NULL) && (col < (int16_t)len - 1))
      break;  // glyph would be cut: stop here so the caller can resume from it

    // cBuf[0] is the glyph's leftmost column and higher display columns
    // are further left, so walk the font data forward while the column
    // cursor moves right (same orientation setChar()/scroll mode use).
    for (uint8_t i = 0; (i < len) && (col >= 0); i++, col--)
      mx.setColumn(col, cBuf[i]);
    if (col >= 0) { mx.setColumn(col, 0); col--; } // spacing

    p++;
  }
  if (rest != NULL) *rest = p;
}

void showStatic(void)
{
  renderStaticText(curMessage);
}

void staticBlink(void)
// Show curMessage statically, blinking the whole display on/off.
{
  static uint32_t prevTime = 0;
  static bool lit = true;

  if (newMessageAvailable)
  {
    strcpy(curMessage, newMessage);
    newMessageAvailable = false;
    showStatic();
  }

  if (millis() - prevTime >= BLINK_INTERVAL)
  {
    lit = !lit;
    mx.control(MD_MAX72XX::SHUTDOWN, lit ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
    prevTime = millis();
  }
}

void blinkScroll(void)
// Blink+Scroll mode: blink the part of the message that fits the
// display for BLINK_PHASE_MS, then scroll once through the part that
// was not visible, repeating. Messages that fit entirely just blink.
{
  static uint32_t prevTime = 0, phaseStart = 0;
  static bool lit = true;
  static bool scrolling = false;
  static int16_t colsLeft = 0;
  static const char *restPos = "";
  static uint16_t restWidth = 0;
  static bool measured = false;
  const uint16_t totalCols = MAX_DEVICES * COL_SIZE;

  if (newMessageAvailable)
  {
    strcpy(curMessage, newMessage);
    newMessageAvailable = false;
    measured = false;
  }
  if (!measured || blinkScrollResetPending)
  {
    measured = true;
    blinkScrollResetPending = false;
    scrolling = false;
    lit = true;
    phaseStart = millis();
    mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
    renderStaticText(curMessage, false, &restPos);
    restWidth = measureStaticText(restPos);
  }

  if (scrolling)
  {
    if (millis() - prevTime >= scrollDelay)
    {
      mx.transform(MD_MAX72XX::TSL);
      prevTime = millis();
      if (--colsLeft <= 0)
      {
        scrolling = false;
        lit = true;
        phaseStart = millis();
        renderStaticText(curMessage, false, &restPos);
      }
    }
    return;
  }

  if (millis() - prevTime >= BLINK_INTERVAL)
  {
    lit = !lit;
    mx.control(MD_MAX72XX::SHUTDOWN, lit ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
    prevTime = millis();
  }
  if ((*restPos != '\0') && (millis() - phaseStart >= BLINK_PHASE_MS))
  {
    scrolling = true;
    mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF); // blink may have left the panel off
    resetScrollSourceAt((char *)restPos);
    colsLeft = restWidth + totalCols; // hidden part enters, then the display drains
    prevTime = millis();
  }
}

// Clock mode ---------------------------------------------------------
// Renders "HH MM ss" across the 32 columns: hours and minutes in a
// 4x7 digit font with a blinking colon between them, seconds in a
// smaller 3x5 font bottom-aligned at the right. When dateEnabled, every
// dateEveryS seconds the date (DD/MM) takes over the display for
// CLOCK_DATE_SHOW_S seconds, using the regular text font.

// 4 columns per digit, LSB = top row, rows 0..6 used
const uint8_t clockDigitBig[10][4] =
{
  { 0x3e, 0x41, 0x41, 0x3e },  // 0
  { 0x00, 0x42, 0x7f, 0x40 },  // 1
  { 0x62, 0x51, 0x49, 0x46 },  // 2
  { 0x22, 0x41, 0x49, 0x36 },  // 3
  { 0x18, 0x14, 0x12, 0x7f },  // 4
  { 0x27, 0x45, 0x45, 0x39 },  // 5
  { 0x3e, 0x49, 0x49, 0x30 },  // 6
  { 0x01, 0x61, 0x19, 0x07 },  // 7
  { 0x36, 0x49, 0x49, 0x36 },  // 8
  { 0x06, 0x49, 0x49, 0x3e },  // 9
};

// 3 columns per digit, LSB = top row, rows 0..4 used (shifted down at
// render time so they sit bottom-aligned with the big digits)
const uint8_t clockDigitSmall[10][3] =
{
  { 0x1f, 0x11, 0x1f },  // 0
  { 0x12, 0x1f, 0x10 },  // 1
  { 0x1d, 0x15, 0x17 },  // 2
  { 0x15, 0x15, 0x1f },  // 3
  { 0x07, 0x04, 0x1f },  // 4
  { 0x17, 0x15, 0x1d },  // 5
  { 0x1f, 0x15, 0x1d },  // 6
  { 0x01, 0x19, 0x07 },  // 7
  { 0x1f, 0x15, 0x1f },  // 8
  { 0x17, 0x15, 0x1f },  // 9
};

bool clockRedrawPending = false;

void clockForceRedraw(void)
// Called when the device switches into clock mode so the next
// clockTick() repaints immediately instead of waiting for the next
// second/colon change.
{
  clockRedrawPending = true;
}

void drawClockGlyph(int16_t &col, const uint8_t *cols, uint8_t width, uint8_t shift)
// Draw one glyph at the cursor and advance it (same column order
// renderStaticText() uses: font data forward, cursor moving right),
// followed by a blank spacing column.
{
  for (uint8_t i = 0; (i < width) && (col >= 0); i++, col--)
    mx.setColumn(col, (uint8_t)(cols[i] << shift));
  if (col >= 0) col--;  // spacing column (display was cleared, already blank)
}

void drawClockFace(const struct tm *t, bool colonOn)
{
  const uint8_t colon = 0x24;  // two 1-pixel dots, rows 2 and 5
  int16_t col = MAX_DEVICES * COL_SIZE - 1;

  mx.clear();
  drawClockGlyph(col, clockDigitBig[t->tm_hour / 10], 4, 0);
  drawClockGlyph(col, clockDigitBig[t->tm_hour % 10], 4, 0);
  if (colonOn) mx.setColumn(col, colon);
  col -= 2;  // colon column + spacing
  drawClockGlyph(col, clockDigitBig[t->tm_min / 10], 4, 0);
  drawClockGlyph(col, clockDigitBig[t->tm_min % 10], 4, 0);
  col--;  // extra gap before the small seconds
  drawClockGlyph(col, clockDigitSmall[t->tm_sec / 10], 3, 2);
  drawClockGlyph(col, clockDigitSmall[t->tm_sec % 10], 3, 2);
}

void clockTick(void)
// Non-blocking clock renderer, called from loop() while in clock mode.
// Repaints only when something visible changed (second rollover, colon
// blink, sync state), checking at most every 100 ms.
{
  static uint32_t prevCheck = 0;
  static int8_t lastSec = -1;
  static bool lastColon = false;
  static bool wasSynced = true;

  if (!clockRedrawPending && (millis() - prevCheck < 100)) return;
  prevCheck = millis();

  struct tm tmNow;
  bool synced = !apMode && getLocalTime(&tmNow, 0);

  if (!synced)
  {
    if (wasSynced || clockRedrawPending)
      renderStaticText("--:--");
    wasSynced = false;
    lastSec = -1;
    clockRedrawPending = false;
    return;
  }
  wasSynced = true;

  // Periodically let the date take over the whole display (epoch-based
  // so intervals longer than a minute work too)
  time_t nowEpoch;
  time(&nowEpoch);
  if (dateEnabled && ((nowEpoch % dateEveryS) < CLOCK_DATE_SHOW_S))
  {
    if ((tmNow.tm_sec != lastSec) || clockRedrawPending)
    {
      char szDate[12];
      sprintf(szDate, "%02d/%02d", tmNow.tm_mday, tmNow.tm_mon + 1);
      renderStaticText(szDate, true);  // centered, it doesn't fill the display
      lastSec = tmNow.tm_sec;
    }
    clockRedrawPending = false;
    return;
  }

  bool colonOn = ((millis() / 500) & 1) == 0;  // blink twice per second
  if ((tmNow.tm_sec != lastSec) || (colonOn != lastColon) || clockRedrawPending)
  {
    drawClockFace(&tmNow, colonOn);
    lastSec = tmNow.tm_sec;
    lastColon = colonOn;
    clockRedrawPending = false;
  }
}

// Game of Life mode --------------------------------------------------
// Direct-draw mode like the clock: a 32x8 pixel board (one byte per
// column, bit0=top row..bit7=bottom row, same convention as iconTable
// and drawClockGlyph) evolved with the classic B3/S23 rule on a
// toroidal (wrap-around) topology and redrawn in full each generation.

uint8_t golGrid[MAX_DEVICES * COL_SIZE];
uint8_t golNextGrid[MAX_DEVICES * COL_SIZE];

bool golRedrawPending = false;
uint8_t golStagnantCount = 0;
uint32_t golPopHistory[4];   // checksums of the last up to 4 generations, for cycle detection
uint8_t golHistoryLen = 0;
bool golPendingReseed = false;
uint32_t golBlankUntil = 0;

void golSeed(void)
// Fill the board with a fresh random pattern and clear stagnation state.
{
  for (uint8_t col = 0; col < MAX_DEVICES * COL_SIZE; col++)
  {
    uint8_t colBits = 0;
    for (uint8_t row = 0; row < 8; row++)
      if ((esp_random() % 100) < GOL_REVIVE_DENSITY_PCT) colBits |= (1 << row);
    golGrid[col] = colBits;
  }
  golStagnantCount = 0;
  golHistoryLen = 0;
}

bool golCellAt(const uint8_t *grid, int8_t col, int8_t row)
// Toroidal (wrap-around) cell lookup.
{
  col = ((col % (MAX_DEVICES * COL_SIZE)) + (MAX_DEVICES * COL_SIZE)) % (MAX_DEVICES * COL_SIZE);
  row = ((row % 8) + 8) % 8;
  return (grid[col] & (1 << row)) != 0;
}

uint8_t golCountNeighbors(const uint8_t *grid, int8_t col, int8_t row)
{
  uint8_t n = 0;
  for (int8_t dc = -1; dc <= 1; dc++)
    for (int8_t dr = -1; dr <= 1; dr++)
    {
      if (dc == 0 && dr == 0) continue;
      if (golCellAt(grid, col + dc, row + dr)) n++;
    }
  return n;
}

bool golStep(void)
// Advance the board by one generation (B3/S23). Returns true if any
// cell changed, false if the new generation is identical to the last
// (still life or extinction).
{
  bool changed = false;

  for (uint8_t col = 0; col < MAX_DEVICES * COL_SIZE; col++)
  {
    uint8_t colBits = 0;
    for (uint8_t row = 0; row < 8; row++)
    {
      uint8_t n = golCountNeighbors(golGrid, col, row);
      bool alive = golCellAt(golGrid, col, row);
      bool next = alive ? (n == 2 || n == 3) : (n == 3);
      if (next) colBits |= (1 << row);
    }
    if (colBits != golGrid[col]) changed = true;
    golNextGrid[col] = colBits;
  }
  memcpy(golGrid, golNextGrid, sizeof(golGrid));
  return changed;
}

uint32_t golChecksum(void)
// Cheap FNV-1a style hash of the board, used for short-cycle detection.
{
  uint32_t hash = 2166136261u;
  for (uint8_t col = 0; col < MAX_DEVICES * COL_SIZE; col++)
  {
    hash ^= golGrid[col];
    hash *= 16777619u;
  }
  return hash;
}

void drawGolFrame(void)
{
  mx.clear();
  for (uint16_t col = 0; col < MAX_DEVICES * COL_SIZE; col++)
    mx.setColumn(col, golGrid[col]);
}

void golForceRedraw(void)
// Called when the device switches into Life mode so the next golTick()
// paints the freshly seeded board immediately instead of waiting for
// the next generation tick.
{
  golRedrawPending = true;
}

void golTick(void)
// Non-blocking Game of Life renderer, called from loop() while in Life
// mode. Advances one generation every GOL_TICK_MS; reseeds (with a
// brief blank pause) once the board has been stagnant - unchanged or
// cycling with period <= 4 - for GOL_STAGNANT_LIMIT generations.
{
  static uint32_t prevTick = 0;

  if (golPendingReseed && (millis() >= golBlankUntil))
  {
    golSeed();
    golPendingReseed = false;
    golRedrawPending = true;
  }

  if (!golRedrawPending && (millis() - prevTick < GOL_TICK_MS)) return;
  prevTick = millis();

  if (golRedrawPending)
  {
    drawGolFrame();
    golRedrawPending = false;
    return;
  }

  bool changed = golStep();
  uint32_t checksum = golChecksum();
  bool cyclic = false;
  for (uint8_t i = 0; i < golHistoryLen; i++)
    if (golPopHistory[i] == checksum) cyclic = true;

  if (!changed || cyclic)
    golStagnantCount++;
  else
    golStagnantCount = 0;

  golPopHistory[golHistoryLen % 4] = checksum;
  if (golHistoryLen < 4) golHistoryLen++;

  drawGolFrame();

  if (golStagnantCount >= GOL_STAGNANT_LIMIT)
  {
    mx.clear();
    golBlankUntil = millis() + GOL_BLANK_PAUSE_MS;
    golPendingReseed = true;
  }
}

void scrollText(void)
{
  static uint32_t	prevTime = 0;

  // Is it time to scroll the text?
  if (millis() - prevTime >= scrollDelay)
  {
    mx.transform(MD_MAX72XX::TSL);  // scroll along - the callback will load all the data
    prevTime = millis();            // starting point for next time
  }
}

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
    resetPrefs.begin("mdmax", false);
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
  server.begin();

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

  handleWiFi();
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

