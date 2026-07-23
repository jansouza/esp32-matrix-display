// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef TEXT_ENCODING_H
#define TEXT_ENCODING_H

inline void decodeUtf8Latin1(char *szMesg)
// Rewrite in place any UTF-8 2-byte sequences in the Latin-1 range
// (0xC2/0xC3 lead byte, U+0080-U+00FF) into the single Latin-1 byte the
// MD_MAX72XX font actually recognizes (e.g. "°" 0xC2 0xB0 -> 0xB0, so
// the web UI and callers can send plain UTF-8 like "25°C"). Bytes that
// aren't part of such a sequence pass through unchanged.
{
  uint8_t *pRead = (uint8_t *)szMesg, *pWrite = (uint8_t *)szMesg;

  while (*pRead != '\0')
  {
    if ((pRead[0] == 0xC2 || pRead[0] == 0xC3) && (pRead[1] >= 0x80) && (pRead[1] <= 0xBF))
    {
      *pWrite++ = ((pRead[0] & 0x03) << 6) | (pRead[1] & 0x3F);
      pRead += 2;
    }
    else
      *pWrite++ = *pRead++;
  }

  *pWrite = '\0';
}

inline void encodeLatin1Utf8(const char *szIn, char *szOut, uint16_t maxLen)
// Inverse of decodeUtf8Latin1(): rewrite high (0x80-0xFF) Latin-1 bytes
// back into their 2-byte UTF-8 form, so text echoed back in an HTTP
// response (JSON, settings list) is valid UTF-8 for the caller to
// decode. Output is truncated (but always null-terminated) if the
// expanded form does not fit in maxLen.
{
  uint16_t idx = 0;

  for (; *szIn != '\0'; szIn++)
  {
    uint8_t c = (uint8_t)*szIn;
    if (c >= 0x80)
    {
      if (idx + 2 >= maxLen) break;
      szOut[idx++] = (char)(0xC0 | (c >> 6));
      szOut[idx++] = (char)(0x80 | (c & 0x3F));
    }
    else
    {
      if (idx + 1 >= maxLen) break;
      szOut[idx++] = (char)c;
    }
  }
  szOut[idx] = '\0';
}

#endif // TEXT_ENCODING_H
