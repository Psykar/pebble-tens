// User settings, mirroring python/src/tens/state.py UserConfig. Loaded from
// persistent storage and updated from the PebbleKit JS config page.
#pragma once
#include <pebble.h>

// What a single bar slot displays. The four slots (top-left, top-right,
// bottom-left, bottom-right) are each assignable to one of these metrics, so
// the bar area is fully user-configurable rather than a fixed trio.
enum {
  TENS_METRIC_OFF = 0,     // blank slot
  TENS_METRIC_WEEK = 1,    // progress through the week + weekday label
  TENS_METRIC_MONTH = 2,   // progress through the month + month label
  TENS_METRIC_YEAR = 3,    // progress through the year + year label
  TENS_METRIC_LIFE = 4,    // progress through the lifespan + age label
  TENS_METRIC_BATTERY = 5, // battery charge + percent label (charge/low cues)
  TENS_METRIC_MIN = TENS_METRIC_OFF,
  TENS_METRIC_MAX = TENS_METRIC_BATTERY,
};

// Default work-day highlight color (RGB hex, fed to GColorFromHEX): warm amber
// (GColorChromeYellow), distinct from the bar colors and readable in both modes.
#define TENS_WORK_COLOR_DEFAULT 0xFFAA00

typedef struct {
  bool rainbow;          // spectral gradient mask over the inked grid/life bar
  bool dark_mode;        // true=black background/white ink, false=white/black
  bool layout_4x6;       // true="4x6" (3x2 cells), false="6x4" (2x3 cells).
                         // Drives the box + minute-line fill axis.
  bool hours_horizontal; // hour-block order: true=row-major, false=column-major
  bool fill_invert;      // horizontal fill: false=from left, true=from right
                         // vertical fill:   false=from top,  true=from bottom
  bool bars_missing_fill; // month/year/life bars' missing part:
                          //   false=outline, true=muted fill
  bool grid_missing_fill; // current 10-min block's missing part:
                          //   false=outline, true=muted fill
  bool work_enabled;     // recolor the grid boxes that fall in the work day
  int slot_tl;           // TENS_METRIC_* shown in the top-left bar slot
  int slot_tr;           // TENS_METRIC_* shown in the top-right bar slot
  int slot_bl;           // TENS_METRIC_* shown in the bottom-left bar slot
  int slot_br;           // TENS_METRIC_* shown in the bottom-right bar slot
  int birth_year;
  int birth_month;       // 1..12
  int birth_day;         // 1..31
  int life_span_years;
  int work_start;        // work-day start, minutes after midnight (0..1439)
  int work_end;          // work-day end (exclusive), minutes after midnight
  int work_color;        // RGB hex for work-day boxes (GColorFromHEX)
} TensSettings;

// Access the current settings (valid after tens_settings_init).
const TensSettings *tens_settings(void);

// Load from persistent storage, falling back to defaults.
void tens_settings_init(void);

// Apply an incoming config dictionary (from pkjs), then persist it.
// Returns true if anything changed (caller should redraw).
bool tens_settings_apply(DictionaryIterator *iter);
