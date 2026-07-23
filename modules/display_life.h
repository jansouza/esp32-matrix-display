// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef DISPLAY_LIFE_H
#define DISPLAY_LIFE_H

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

inline void golSeed(void)
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

inline bool golCellAt(const uint8_t *grid, int8_t col, int8_t row)
// Toroidal (wrap-around) cell lookup.
{
  col = ((col % (MAX_DEVICES * COL_SIZE)) + (MAX_DEVICES * COL_SIZE)) % (MAX_DEVICES * COL_SIZE);
  row = ((row % 8) + 8) % 8;
  return (grid[col] & (1 << row)) != 0;
}

inline uint8_t golCountNeighbors(const uint8_t *grid, int8_t col, int8_t row)
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

inline bool golStep(void)
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

inline uint32_t golChecksum(void)
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

inline void drawGolFrame(void)
{
  mx.clear();
  for (uint16_t col = 0; col < MAX_DEVICES * COL_SIZE; col++)
    mx.setColumn(col, golGrid[col]);
}

inline void golForceRedraw(void)
// Called when the device switches into Life mode so the next golTick()
// paints the freshly seeded board immediately instead of waiting for
// the next generation tick.
{
  golRedrawPending = true;
}

inline void golTick(void)
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

#endif // DISPLAY_LIFE_H
