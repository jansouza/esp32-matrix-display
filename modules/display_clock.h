// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef DISPLAY_CLOCK_H
#define DISPLAY_CLOCK_H

// Clock mode ---------------------------------------------------------
// Renders "HH MM ss" across the 32 columns: hours and minutes in a
// 4x7 digit font with a blinking colon between them, seconds in a
// smaller 3x5 font bottom-aligned at the right. When dateEnabled, every
// dateEveryS seconds the date takes over the display for
// CLOCK_DATE_SHOW_S seconds: weekday abbreviation (small 3x5 font,
// dot row above it) on the left, day/month (big 4x7 font) on the right.

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

// 3x5 small letters used by weekday abbreviations (from days_lookup.h,
// for the languages in SUPPORTED_LANGUAGES), same bit layout as
// clockDigitSmall (LSB = top row, rows 0..4 used)
inline uint8_t smallLetterGlyph(char c, uint8_t out[3])
{
  switch (c)
  {
    case 'A': { const uint8_t g[3] = { 0x1e, 0x05, 0x1e }; memcpy(out, g, 3); return 3; }
    case 'B': { const uint8_t g[3] = { 0x1f, 0x15, 0x0a }; memcpy(out, g, 3); return 3; }
    case 'C': { const uint8_t g[3] = { 0x0e, 0x11, 0x11 }; memcpy(out, g, 3); return 3; }
    case 'D': { const uint8_t g[3] = { 0x1f, 0x11, 0x0e }; memcpy(out, g, 3); return 3; }
    case 'E': { const uint8_t g[3] = { 0x1f, 0x15, 0x15 }; memcpy(out, g, 3); return 3; }
    case 'F': { const uint8_t g[3] = { 0x1f, 0x05, 0x01 }; memcpy(out, g, 3); return 3; }
    case 'G': { const uint8_t g[3] = { 0x1f, 0x11, 0x1d }; memcpy(out, g, 3); return 3; }
    case 'H': { const uint8_t g[3] = { 0x1f, 0x04, 0x1f }; memcpy(out, g, 3); return 3; }
    case 'I': { const uint8_t g[3] = { 0x11, 0x1f, 0x11 }; memcpy(out, g, 3); return 3; }
    case 'J': { const uint8_t g[3] = { 0x10, 0x10, 0x1f }; memcpy(out, g, 3); return 3; }
    case 'L': { const uint8_t g[3] = { 0x1f, 0x10, 0x10 }; memcpy(out, g, 3); return 3; }
    case 'M': { const uint8_t g[3] = { 0x1f, 0x06, 0x1f }; memcpy(out, g, 3); return 3; }
    case 'N': { const uint8_t g[3] = { 0x17, 0x0a, 0x1d }; memcpy(out, g, 3); return 3; }
    case 'O': { const uint8_t g[3] = { 0x0e, 0x11, 0x0e }; memcpy(out, g, 3); return 3; }
    case 'Q': { const uint8_t g[3] = { 0x0e, 0x15, 0x1e }; memcpy(out, g, 3); return 3; }
    case 'R': { const uint8_t g[3] = { 0x1f, 0x05, 0x1a }; memcpy(out, g, 3); return 3; }
    case 'S': { const uint8_t g[3] = { 0x17, 0x15, 0x1d }; memcpy(out, g, 3); return 3; }
    case 'T': { const uint8_t g[3] = { 0x01, 0x1f, 0x01 }; memcpy(out, g, 3); return 3; }
    case 'U': { const uint8_t g[3] = { 0x0f, 0x10, 0x0f }; memcpy(out, g, 3); return 3; }
    case 'V': { const uint8_t g[3] = { 0x07, 0x18, 0x07 }; memcpy(out, g, 3); return 3; }
    case 'W': { const uint8_t g[3] = { 0x1f, 0x08, 0x1f }; memcpy(out, g, 3); return 3; }
    case 'X': { const uint8_t g[3] = { 0x1b, 0x04, 0x1b }; memcpy(out, g, 3); return 3; }
    default:  { const uint8_t g[3] = { 0x00, 0x00, 0x00 }; memcpy(out, g, 3); return 3; }
  }
}

bool clockRedrawPending = false;

inline void clockForceRedraw(void)
// Called when the device switches into clock mode so the next
// clockTick() repaints immediately instead of waiting for the next
// second/colon change.
{
  clockRedrawPending = true;
}

inline void drawClockGlyph(int16_t &col, const uint8_t *cols, uint8_t width, uint8_t shift)
// Draw one glyph at the cursor and advance it (same column order
// renderStaticText() uses: font data forward, cursor moving right),
// followed by a blank spacing column.
{
  for (uint8_t i = 0; (i < width) && (col >= 0); i++, col--)
    mx.setColumn(col, (uint8_t)(cols[i] << shift));
  if (col >= 0) col--;  // spacing column (display was cleared, already blank)
}

inline void drawClockFace(const struct tm *t, bool colonOn)
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

inline void drawDateFace(const struct tm *t)
// Draws the periodic date screen: weekday abbreviation (3x5 small font)
// under a solid row of decorative dots spanning every column of the
// weekday block, on the left edge of the display; day/month (4x7 big
// font, same as the clock digits) on the right.
{
  const char *const *days = getDaysOfWeek(language);   // days_lookup.h, lowercase abbreviations
  const char *wd = days[t->tm_wday];

  mx.clear();

  // Left side: dot row + weekday abbreviation. The highest column index
  // is the display's left edge, so this is drawn first.
  int16_t col = MAX_DEVICES * COL_SIZE - 1;
  int16_t dotStart = col;

  for (const char *p = wd; *p != '\0'; p++)
  {
    uint8_t g[3];
    smallLetterGlyph(toupper((unsigned char)*p), g);
    for (uint8_t i = 0; (i < 3) && (col >= 0); i++, col--)
      mx.setColumn(col, (uint8_t)(g[i] << 2));  // rows 2..6, bottom-aligned like clockDigitSmall
    if (col >= 0) col--;  // spacing column
  }

  for (int16_t c = dotStart; c > col; c--)
    if (c >= 0) mx.setColumn(c, (uint8_t)(mx.getColumn(c) | 0x01));  // dot on row 0

  col--;  // extra gap before the date

  // Right side: day/month (or month/day) in the big digit font, filling
  // the rest.
  uint8_t mon = t->tm_mon + 1;
  uint8_t first  = dateUsFormat ? mon : t->tm_mday;
  uint8_t second = dateUsFormat ? t->tm_mday : mon;
  drawClockGlyph(col, clockDigitBig[first / 10], 4, 0);
  drawClockGlyph(col, clockDigitBig[first % 10], 4, 0);
  drawClockGlyph(col, clockDigitBig[second / 10], 4, 0);
  drawClockGlyph(col, clockDigitBig[second % 10], 4, 0);
}

inline void clockTick(void)
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
      drawDateFace(&tmNow);
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

#endif // DISPLAY_CLOCK_H
