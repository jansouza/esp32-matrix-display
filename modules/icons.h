// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef ICONS_H
#define ICONS_H

// Custom icons -----------------------------------------------------
// Icons are encoded in the message as control bytes 0x01-0x14 (values
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

inline const icon_t *findIconByCode(char c)
// Return pointer to the icon_t matching a control byte, or NULL
{
  for (uint8_t i = 0; i < ICON_TABLE_SIZE; i++)
    if (iconTable[i].code == c) return(&iconTable[i]);
  return(NULL);
}

inline void expandIcons(char *szMesg)
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

inline void collapseIcons(const char *szIn, char *szOut, uint16_t maxLen)
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

#endif // ICONS_H
