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
const uint8_t DEFAULT_MODE         = 0;   // 0 = scroll, 1 = static blink

const uint8_t MIN_SCROLL_DELAY = 20;
const uint8_t MAX_SCROLL_DELAY = 250;
const uint16_t BLINK_INTERVAL  = 500; // ms, static blink mode on/off period

char curMessage[MESG_SIZE];
char newMessage[MESG_SIZE];
bool newMessageAvailable = false;

// Runtime configuration, persisted in NVS via Preferences
Preferences prefs;
uint8_t scrollDelay = DEFAULT_SCROLL_DELAY;
uint8_t brightness  = DEFAULT_BRIGHTNESS;
uint8_t displayMode = DEFAULT_MODE;

// REST API (/api/message) authentication - off by default so the API
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
  { "[smile]", ICON_SMILE, 8, { 0x3c, 0x42, 0x95, 0x81, 0xa5, 0x99, 0x42, 0x3c } },
  { "[up]",    ICON_UP,    8, { 0x08, 0x0c, 0x0e, 0x7f, 0x7f, 0x0e, 0x0c, 0x08 } },
  { "[down]",  ICON_DOWN,  8, { 0x10, 0x30, 0x70, 0xfe, 0xfe, 0x70, 0x30, 0x10 } },
  { "[left]",  ICON_LEFT,  8, { 0x08, 0x1c, 0x3e, 0x7f, 0x1c, 0x1c, 0x1c, 0x1c } },
  { "[right]", ICON_RIGHT, 8, { 0x1c, 0x1c, 0x1c, 0x1c, 0x7f, 0x3e, 0x1c, 0x08 } },
  { "[star]",  ICON_STAR,  8, { 0x18, 0x1c, 0x7e, 0x3c, 0x3c, 0x7e, 0x1c, 0x18 } },
  { "[music]", ICON_MUSIC, 6, { 0x60, 0x90, 0x90, 0x50, 0x60, 0x3f, 0x00, 0x00 } },
  { "[bell]",  ICON_BELL,  8, { 0x10, 0x38, 0x38, 0x38, 0x7c, 0xfe, 0x10, 0x00 } },
  { "[clock]", ICON_CLOCK, 8, { 0x3c, 0x42, 0x91, 0x95, 0x91, 0x81, 0x42, 0x3c } },
  { "[ok]",    ICON_OK,    8, { 0x01, 0x03, 0x06, 0x8c, 0xd8, 0x70, 0x20, 0x00 } },
  { "[x]",     ICON_X,     8, { 0x42, 0x66, 0x3c, 0x18, 0x18, 0x3c, 0x66, 0x42 } },
  { "[sun]",   ICON_SUN,   8, { 0x24, 0x18, 0x5a, 0x3c, 0x3c, 0x5a, 0x18, 0x24 } },
  { "[rain]",  ICON_RAIN,  8, { 0x60, 0x3c, 0x7e, 0x3c, 0x00, 0x28, 0x50, 0x00 } },
  { "[pin]",   ICON_PIN,   6, { 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00 } },
  { "[plus]",  ICON_PLUS,  6, { 0x18, 0x18, 0x7e, 0x7e, 0x18, 0x18, 0x00, 0x00 } },
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
  apiAuthEnabled = prefs.getBool("apiauth", false);
  prefs.getString("apikey", apiKey, sizeof(apiKey));
}

void saveSettings(void)
{
  prefs.putUChar("spd", scrollDelay);
  prefs.putUChar("brt", brightness);
  prefs.putUChar("mode", displayMode);
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
"<title>MD_MAX72xx Control Panel</title>\n" \
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
"  input[type=text]{\n" \
"    width:100%; padding:11px 13px; border-radius:9px; border:1px solid var(--border);\n" \
"    background:var(--inset); color:var(--text); font-size:1rem; font-family:var(--font-body);\n" \
"  }\n" \
"  input[type=text]:focus{outline:2px solid var(--accent); outline-offset:1px; border-color:transparent;}\n" \
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
"        <button type=\"button\" onclick=\"InsertIcon('[up]')\">&uarr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[down]')\">&darr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[left]')\">&larr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[right]')\">&rarr;</button>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <div class=\"card\">\n" \
"      <h2>Display</h2>\n" \
"      <div class=\"row\">\n" \
"        <label>Speed</label>\n" \
"        <input type=\"range\" id=\"spd\" min=\"20\" max=\"250\" value=\"75\" oninput=\"onSpdInput()\">\n" \
"        <span class=\"val\" id=\"spdVal\">75</span>\n" \
"      </div>\n" \
"      <div class=\"row\">\n" \
"        <label>Brightness</label>\n" \
"        <input type=\"range\" id=\"brt\" min=\"0\" max=\"15\" value=\"8\" oninput=\"onBrtInput()\">\n" \
"        <span class=\"val\" id=\"brtVal\">8</span>\n" \
"      </div>\n" \
"      <div class=\"modes\">\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"0\" checked onchange=\"SendText()\"><span>Scroll</span></label>\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"1\" onchange=\"SendText()\"><span>Blink</span></label>\n" \
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
"      <div class=\"preview\" id=\"apiExample\" style=\"word-break:break-all; font-size:.85rem;\">GET /api/message?msg=Hello</div>\n" \
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
"  var qs = \"&MSG=\" + encodeURIComponent(msg) +\n" \
"           \"/&SPD=\" + spd +\n" \
"           \"/&BRT=\" + brt +\n" \
"           \"/&MODE=\" + mode +\n" \
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
"    \"GET /api/message?msg=Hello\" + (enabled ? \"  (header X-API-Key required)\" : \"\");\n" \
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
"    // scrollDelay, brightness, displayMode, last message\n" \
"    var parts = request.responseText.split(\",\");\n" \
"    if (parts.length < 3) return;\n" \
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
"    // Prefill the message box with the last message sent (the message\n" \
"    // may contain commas, so rejoin everything after the 3rd field),\n" \
"    // unless the user already started typing.\n" \
"    var lastMsg = parts.slice(3).join(\",\");\n" \
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

      wantsApiMessage = (strncmp(szBuf, "GET /api/message", 16) == 0);
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
          uint8_t v = (uint8_t)constrain(atoi(szParam), 0, 1);
          if (v != displayMode)
          {
            displayMode = v;
            mx.control(MD_MAX72XX::SHUTDOWN, MD_MAX72XX::OFF);
            resetScrollSource();
            if (displayMode == 0) newMessageAvailable = true;
            else showStatic();
            settingsChanged = true;
          }
        }
        if (settingsChanged) saveSettings();

        state = S_RESPONSE;
        break;
      }

      // Extract the message text, if there is one
      newMessageAvailable = getParam(szBuf, "MSG", newMessage, MESG_SIZE);
      if (newMessageAvailable)
        expandIcons(newMessage);
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
        uint8_t v = (uint8_t)constrain(atoi(szParam), 0, 1);
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
          else showStatic();  // redraw immediately in static mode
          settingsChanged = true;
        }
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
      char szJson[MESG_SIZE * 2 + 32];
      if (apiMessageOk)
      {
        // Escape '"' and '\' so the message can't break out of the JSON string
        char szEsc[MESG_SIZE * 2];
        char *pOut = szEsc;
        for (char *pIn = newMessage; *pIn != '\0'; pIn++)
        {
          if ((*pIn == '"') || (*pIn == '\\')) *pOut++ = '\\';
          *pOut++ = *pIn;
        }
        *pOut = '\0';
        sprintf(szJson, "{\"ok\":true,\"msg\":\"%s\"}", szEsc);
      }
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
      // scrollDelay, brightness, displayMode, last saved message (icon
      // control bytes collapsed back to [tag] form so it can be edited).
      // The message goes last because it may itself contain commas - the
      // UI rejoins everything after the third field.
      char szSettings[MESG_SIZE * 7 + 16]; // worst case: every byte is a 7-char [tag]
      char szLast[MESG_SIZE];

      if (!loadLastMessage(szLast, sizeof(szLast))) szLast[0] = '\0';
      sprintf(szSettings, "%d,%d,%d,", scrollDelay, brightness, displayMode);
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
    // delay the HTTP response the browser is waiting on.
    if (newMessageAvailable) saveLastMessage(newMessage);
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

void resetScrollSource(void)
// Force scrollDataSource() to restart from the beginning of curMessage
// on its next call, discarding whatever it was in the middle of.
{
  scrollSourceResetPending = true;
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

void showStatic(void)
// Render curMessage statically (no scroll), left-aligned, truncated to
// what fits on the display. Columns are filled from the rightmost
// device backwards, matching how the hardware chain is wired (the
// same orientation the scrolling mode ends up on screen).
{
  const uint16_t totalCols = MAX_DEVICES * COL_SIZE;
  int16_t col = totalCols - 1;
  char *p = curMessage;
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

    for (int8_t i = len - 1; (i >= 0) && (col >= 0); i--, col--)
      mx.setColumn(col, cBuf[i]);
    if (col >= 0) { mx.setColumn(col, 0); col--; } // spacing

    p++;
  }
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

  if (displayMode == 1) showStatic();  // static mode: render immediately
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

  handleWiFi();
  if (displayMode == 0)
    scrollText();
  else
    staticBlink();
}

