// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef DISPLAY_MESSAGE_H
#define DISPLAY_MESSAGE_H

inline void scrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
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

enum scrollState_t { S_SCROLL_IDLE, S_NEXT_CHAR, S_SHOW_CHAR, S_SHOW_SPACE };
scrollState_t scrollSourceState = S_SCROLL_IDLE;
bool scrollSourceResetPending = false;
char *scrollResumePos = NULL;  // where S_SCROLL_IDLE starts feeding (NULL = start of curMessage)
bool scrollHoldAtEnd = false;  // one-shot pass: feed blanks after the message ends instead of wrapping

inline void resetScrollSource(void)
// Force scrollDataSource() to restart from the beginning of curMessage
// on its next call, discarding whatever it was in the middle of.
{
  scrollSourceResetPending = true;
  scrollResumePos = NULL;
  scrollHoldAtEnd = false;
}

inline void resetScrollSourceAt(char *pos)
// One-shot scroll pass starting mid-message: scrollDataSource() feeds
// columns from pos onward and holds blank after the end instead of
// wrapping back to the start of the message.
{
  scrollSourceResetPending = true;
  scrollResumePos = pos;
  scrollHoldAtEnd = true;
}

bool blinkScrollResetPending = false;

inline void resetBlinkScroll(void)
// Force blinkScroll() to re-measure curMessage and restart from the
// blink phase on its next call (its state persists across mode changes).
{
  blinkScrollResetPending = true;
}

inline uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
// Callback function for data that is required for scrolling into the display
{
  static char *p;
  static uint16_t curLen, showLen;
  static uint8_t  cBuf[8];
  uint8_t colData = 0;

  if (scrollSourceResetPending)
  {
    scrollSourceState = S_SCROLL_IDLE;
    scrollSourceResetPending = false;
  }

  // finite state machine to control what we do on the callback
  switch (scrollSourceState)
  {
  case S_SCROLL_IDLE: // reset the message pointer and check for new message to load
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
      scrollSourceState = S_SCROLL_IDLE;
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
    scrollSourceState = S_SCROLL_IDLE;
  }

  return(colData);
}

inline uint16_t measureStaticText(const char *szMesg)
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

inline void renderStaticText(const char *szMesg, bool center = false, const char **rest = NULL)
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

inline void showStatic(void)
{
  renderStaticText(curMessage);
}

inline void staticBlink(void)
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

inline void blinkScroll(void)
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

inline void scrollText(void)
{
  static uint32_t	prevTime = 0;

  // Is it time to scroll the text?
  if (millis() - prevTime >= scrollDelay)
  {
    mx.transform(MD_MAX72XX::TSL);  // scroll along - the callback will load all the data
    prevTime = millis();            // starting point for next time
  }
}

#endif // DISPLAY_MESSAGE_H
