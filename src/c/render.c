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
      draw_missing(ctx, cell, cfg->missing_fill, muted);
      // Minute lines fill along the cell's long axis (same as the boxes).
      int count = d->minute_of_box;
      GRect fill;
      if (L->cell_x == 3) {
        int cols = clampi(count, 0, cell.size.w);
        int x = cfg->fill_invert ? (rect_right(cell) - cols) : cell.origin.x;
        fill = GRect(x, cell.origin.y, cols, cell.size.h);
      } else {
        int rows = clampi(count, 0, cell.size.h);
        int y = cfg->fill_invert ? (rect_bottom(cell) - rows) : cell.origin.y;
        fill = GRect(cell.origin.x, y, cell.size.w, rows);
      }
      draw_ink_rect(ctx, fill, grid, cfg->rainbow, ink);
    } else {
      // Future box: a centered 4x4 muted dot placeholder.
      int d4 = 4;
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
  // Dark gray on black, light gray on white.
  GColor muted = dm ? GColorDarkGray : GColorLightGray;
  // Contrasty gray: the month/year bars in rainbow mode. Light on black,
  // dark on white.
  GColor gray = dm ? GColorLightGray : GColorDarkGray;

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  TensDerived d;
  tens_derive(now, cfg, &d);

  TensLayout L;
  tens_layout_init(&L, cfg->layout_4x6, cfg->hours_horizontal);

  render_grid(ctx, &L, &d, cfg, ink, muted);

  // Two bars: top half month | year, plus the long life bar. In rainbow mode
  // month/year are the contrasty gray; otherwise their fixed colors. Life
  // mirrors the boxes: spectral when rainbow, solid ink otherwise.
  GColor month_color = cfg->rainbow ? gray : TENS_COLOR_MONTH;
  GColor year_color = cfg->rainbow ? gray : TENS_COLOR_YEAR;
  fill_solid_bar(ctx, tens_month_bar(&L), d.frac_month, month_color,
                 cfg->missing_fill, muted);
  fill_solid_bar(ctx, tens_year_bar(&L), d.frac_year, year_color,
                 cfg->missing_fill, muted);

  // TEMP: rainbow gradient removed — life bar always renders solid.
  GRect life = tens_life_bar(&L);
  fill_solid_bar(ctx, life, d.frac_life, ink, cfg->missing_fill, muted);
}
