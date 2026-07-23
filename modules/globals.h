// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef GLOBALS_H
#define GLOBALS_H

// Shared runtime state used across the display/web modules. Definitions
// live in the .ino's top section; this header only declares them so any
// module that needs to read/write this state can include globals.h alone
// instead of relying on .ino include ordering. Still a single translation
// unit (Arduino concatenates everything), so `extern` here is primarily
// documentation of the cross-module state surface.

extern MD_MAX72XX mx;

extern char currentSsid[64];  // SSID currently in use (saved or factory default)
extern bool apMode;           // true when running the emergency setup AP
extern bool restartPending;
extern uint32_t restartTime;

extern char curMessage[MESG_SIZE];
extern char newMessage[MESG_SIZE];
extern bool newMessageAvailable;

extern Preferences prefs;
extern uint8_t scrollDelay;
extern uint8_t brightness;
extern uint8_t displayMode;
extern uint8_t appMode;
extern char tzName[48];      // IANA zone name, e.g. "America/Sao_Paulo"
extern char language[3];     // 2-letter code + NUL, one of SUPPORTED_LANGUAGES
extern bool dateEnabled;
extern bool dateUsFormat;
extern uint16_t dateEveryS;

// Temporary alert message (?msg=...&alert=<seconds> on the REST API):
// snapshot of the display state to restore when the alert expires,
// checked in loop() like the deferred restart. Never persisted to NVS.
extern bool alertActive;
extern uint32_t alertRevertTime;
extern uint8_t alertPrevAppMode;
extern uint8_t alertPrevDisplayMode;
extern uint8_t alertPrevScrollDelay;
extern uint8_t alertPrevBrightness;
extern char alertPrevMessage[MESG_SIZE];

extern bool apiAuthEnabled;
extern char apiKey[API_KEY_LEN + 1];

#endif // GLOBALS_H
