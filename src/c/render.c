#include "render.h"
#include "derived.h"
#include "layout.h"
#include "palette.h"

static int rect_right(GRect r) { return r.origin.x + r.size.w; }
static int rect_bottom(GRect r) { return r.origin.y + r.size.h; }

static int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Fill a rect with solid ink.
// TEMP: the grid-wide spectral ramp (rainbow) path has been removed while we
// isolate the boot-loop crash; this always fills solid. The `grid`/`rainbow`
// params are kept so callers and the signature stay stable for an easy revert.
static void draw_ink_rect(GContext *ctx, GRect r, GRect grid, bool rainbow,
                          GColor ink) {
  (void)grid;
  (void)rainbow;
  if (r.size.w <= 0 || r.size.h <= 0) return;
  graphics_context_set_fill_color(ctx, ink);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
}

// The missing (unfilled) part of a bar/box: muted outline border or muted fill.
static void draw_missing(GContext *ctx, GRect bar, bool missing_fill,
                         GColor muted) {
  if (missing_fill) {
    graphics_context_set_fill_color(ctx, muted);
    graphics_fill_rect(ctx, bar, 0, GCornerNone);
  } else {
    graphics_context_set_stroke_color(ctx, muted);
    graphics_draw_rect(ctx, bar);
  }
}

// A solid color bar filled up to progress over a muted track.
static void fill_solid_bar(GContext *ctx, GRect bar, int progress, GColor color,
                           bool missing_fill, GColor muted) {
  draw_missing(ctx, bar, missing_fill, muted);
  progress = clampi(progress, 0, 1000);
  int fill_w = bar.size.w * progress / 1000;
  if (fill_w > 0) {
    graphics_context_set_fill_color(ctx, color);
    graphics_fill_rect(ctx, GRect(bar.origin.x, bar.origin.y, fill_w, bar.size.h),
                       0, GCornerNone);
  }
}

// Overlay a tiny label centered on a bar. GOTHIC_14_BOLD is the smallest bold
// system font; we center its line box on the bar so the glyphs sit inside the
// bar rather than floating above it. Gothic carries extra top padding, so a
// small upward nudge (LABEL_VOFFSET) keeps the glyph block visually centered.
// NULL text draws nothing.
#define LABEL_VOFFSET (-3)
static void draw_bar_label(GContext *ctx, GRect bar, const char *text,
                           GColor color) {
  if (!text) return;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  graphics_context_set_text_color(ctx, color);
  const int box_h = 14;
  GRect box = GRect(bar.origin.x,
                    bar.origin.y + (bar.size.h - box_h) / 2 + LABEL_VOFFSET,
                    bar.size.w, box_h);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// TEMP: the rainbow life-bar gradient (fill_gradient_bar) has been removed
// while we isolate the boot-loop crash. The life bar now always renders solid
// via fill_solid_bar(). Restore the per-pixel spectral gradient here once
// rainbow is reimplemented as a fast precomputed bitmap.

static void render_grid(GContext *ctx, const TensLayout *L,
                        const TensDerived *d, const TensSettings *cfg,
                        GColor ink, GColor muted) {
  GRect grid = tens_day_rect(L);
  for (int i = 0; i < 144; i++) {
    GRect cell = tens_ten_minute_cell(L, i);
    if (i < d->ten_minute_index) {
      draw_ink_rect(ctx, cell, grid, cfg->rainbow, ink);
    } else if (i == d->ten_minute_index) {
      // Current box: muted missing part, then the completed-minute lines.
      draw_missing(ctx, cell, cfg->grid_missing_fill, muted);
      // Minute lines fill along the cell's long axis (same as the boxes). A box
      // spans 10 minutes, so the fill is proportional to the cell extent rather
      // than a fixed 1px-per-minute (which only held on emery's 10px boxes).
      int count = d->minute_of_box;
      GRect fill;
      if (L->cell_x == 3) {
        int cols = clampi(count * cell.size.w / 10, 0, cell.size.w);
        int x = cfg->fill_invert ? (rect_right(cell) - cols) : cell.origin.x;
        fill = GRect(x, cell.origin.y, cols, cell.size.h);
      } else {
        int rows = clampi(count * cell.size.h / 10, 0, cell.size.h);
        int y = cfg->fill_invert ? (rect_bottom(cell) - rows) : cell.origin.y;
        fill = GRect(cell.origin.x, y, cell.size.w, rows);
      }
      draw_ink_rect(ctx, fill, grid, cfg->rainbow, ink);
    } else {
      // Future box: a centered muted dot placeholder, sized to the box.
      int d4 = cell.size.w >= 9 ? 4 : 3;
      int ox = cell.origin.x + (cell.size.w - d4) / 2;
      int oy = cell.origin.y + (cell.size.h - d4) / 2;
      graphics_context_set_fill_color(ctx, muted);
      graphics_fill_rect(ctx, GRect(ox, oy, d4, d4), 0, GCornerNone);
    }
  }
}

void tens_render(GContext *ctx, GRect bounds, const struct tm *now,
                 const TensSettings *cfg_in) {
  // TEMP: rainbow disabled on-device while we isolate the boot-loop crash.
  // The per-pixel spectral render (the prime suspect) has been stripped from
  // draw_ink_rect and the life bar; this override also forces the flag off so
  // the month/year bars keep their fixed colors regardless of the saved
  // setting. Remove this override (and use cfg_in directly) once rainbow is
  // reimplemented as a fast precomputed bitmap.
  TensSettings cfg_local = *cfg_in;
  cfg_local.rainbow = false;
  const TensSettings *cfg = &cfg_local;

  bool dm = cfg->dark_mode;
  GColor bg = dm ? GColorBlack : GColorWhite;
  GColor ink = dm ? GColorWhite : GColorBlack;
  // Subtle gray (low-contrast): placeholders and unfilled tracks/outlines.
  // Light gray in light mode (on white), dark gray in dark mode (on black).
  GColor muted = dm ? GColorDarkGray : GColorLightGray;

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  TensDerived d;
  tens_derive(now, cfg, &d);

  TensLayout L;
  tens_layout_init(&L, bounds.size.w, bounds.size.h, cfg->layout_4x6,
                   cfg->hours_horizontal);

  render_grid(ctx, &L, &d, cfg, ink, muted);

  // Three bars in two fixed slots: the top row split into left | right, plus
  // the long bottom bar. The chosen set decides which metric (and color) lands
  // in each slot. Each bar carries a tiny overlaid label: weekday / month /
  // year for the calendar metrics, and the current age (years lived) for life.
  static const char *const WDAY[7] = {"SUN", "MON", "TUE", "WED",
                                      "THU", "FRI", "SAT"};
  static const char *const MON[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  int now_year = now->tm_year + 1900;
  int now_month = now->tm_mon + 1;
  // Whole years lived: drop one if this year's birthday hasn't arrived yet.
  int age = now_year - cfg->birth_year;
  if (now_month < cfg->birth_month ||
      (now_month == cfg->birth_month && now->tm_mday < cfg->birth_day)) {
    age--;
  }
  if (age < 0) age = 0;
  char year_buf[8];
  snprintf(year_buf, sizeof(year_buf), "%d", now_year);
  char age_buf[8];
  snprintf(age_buf, sizeof(age_buf), "%d", age);
  const char *week_label = WDAY[now->tm_wday % 7];
  const char *month_label = MON[now->tm_mon % 12];
  const char *year_label = year_buf;
  const char *age_label = age_buf;

  int top_left_frac, top_right_frac, bottom_frac;
  GColor top_left_color, top_right_color, bottom_color;
  const char *top_left_label, *top_right_label, *bottom_label;
  if (cfg->bar_set == TENS_BARS_WEEK_MONTH_YEAR) {
    top_left_frac = d.frac_week;   top_left_color = TENS_COLOR_WEEK;
    top_left_label = week_label;
    top_right_frac = d.frac_month; top_right_color = TENS_COLOR_MONTH;
    top_right_label = month_label;
    bottom_frac = d.frac_year;     bottom_color = TENS_COLOR_YEAR;
    bottom_label = year_label;
  } else {
    top_left_frac = d.frac_month;  top_left_color = TENS_COLOR_MONTH;
    top_left_label = month_label;
    top_right_frac = d.frac_year;  top_right_color = TENS_COLOR_YEAR;
    top_right_label = year_label;
    bottom_frac = d.frac_life;     bottom_color = TENS_COLOR_LIFE;
    bottom_label = age_label;
  }
  GRect top_left_bar = tens_month_bar(&L);
  GRect top_right_bar = tens_year_bar(&L);
  GRect bottom_bar = tens_life_bar(&L);
  fill_solid_bar(ctx, top_left_bar, top_left_frac, top_left_color,
                 cfg->bars_missing_fill, muted);
  fill_solid_bar(ctx, top_right_bar, top_right_frac, top_right_color,
                 cfg->bars_missing_fill, muted);
  fill_solid_bar(ctx, bottom_bar, bottom_frac, bottom_color,
                 cfg->bars_missing_fill, muted);
  draw_bar_label(ctx, top_left_bar, top_left_label, ink);
  draw_bar_label(ctx, top_right_bar, top_right_label, ink);
  draw_bar_label(ctx, bottom_bar, bottom_label, ink);
}
