#include "render.h"
#include "derived.h"
#include "layout.h"
#include "palette.h"

static int rect_right(GRect r) { return r.origin.x + r.size.w; }
static int rect_bottom(GRect r) { return r.origin.y + r.size.h; }

static int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Fill a rect with a solid color.
static void draw_ink_rect(GContext *ctx, GRect r, GColor color) {
  if (r.size.w <= 0 || r.size.h <= 0) return;
  graphics_context_set_fill_color(ctx, color);
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

// The bars always render solid (each slot is a single metric color). The old
// per-pixel spectral life-bar gradient was dropped; only the grid honors the
// rainbow setting now.

static void render_grid(GContext *ctx, const TensLayout *L,
                        const TensDerived *d, const TensSettings *cfg,
                        GColor ink, GColor muted) {
  // Work-day highlight: recolor the inked boxes whose 10-minute slot falls in
  // [work_start, work_end) (minutes after midnight). Disabled when the range is
  // empty/inverted. It takes precedence over rainbow so the work block reads as
  // one distinct color.
  bool work = cfg->work_enabled && cfg->work_end > cfg->work_start;
  GColor work_ink = work ? GColorFromHEX(cfg->work_color) : ink;
  for (int i = 0; i < 144; i++) {
    GRect cell = tens_ten_minute_cell(L, i);
    int cell_min = i * 10;  // minute-of-day at the start of this box
    bool in_work =
        work && cell_min >= cfg->work_start && cell_min < cfg->work_end;
    // In rainbow mode each box takes its color from its position along the
    // spectral ramp (a per-box fill, not a per-pixel gradient); otherwise the
    // inked boxes are solid ink. 144 plain fills, so it stays cheap.
    GColor cell_color;
    if (in_work) {
      cell_color = work_ink;
    } else if (cfg->rainbow) {
      cell_color = tens_spectral((int32_t)i * 1000 / 143);
    } else {
      cell_color = ink;
    }
    if (i < d->ten_minute_index) {
      draw_ink_rect(ctx, cell, cell_color);
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
      draw_ink_rect(ctx, fill, cell_color);
    } else {
      // Future box: a centered dot placeholder, sized to the box. Pending
      // work-hour boxes use the work color so the upcoming work block reads at
      // a glance; the rest stay muted gray.
      int d4 = cell.size.w >= 9 ? 4 : 3;
      int ox = cell.origin.x + (cell.size.w - d4) / 2;
      int oy = cell.origin.y + (cell.size.h - d4) / 2;
      graphics_context_set_fill_color(ctx, in_work ? work_ink : muted);
      graphics_fill_rect(ctx, GRect(ox, oy, d4, d4), 0, GCornerNone);
    }
  }
}

// Resolve a bar slot's metric to its fill fraction (permille), color and label.
// The label may be a static string (weekday/month) or formatted into `buf`
// (year/age/battery). Returns false for OFF so the caller leaves the slot
// blank. Battery has three visual states: bright green while charging (label
// prefixed with '+'), red when low, else a distinct teal.
static bool metric_view(int metric, const TensDerived *d, const struct tm *now,
                        const TensSettings *cfg, BatteryChargeState batt,
                        int *frac, GColor *color, const char **label, char *buf,
                        size_t buflen) {
  static const char *const WDAY[7] = {"SUN", "MON", "TUE", "WED",
                                      "THU", "FRI", "SAT"};
  static const char *const MON[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  switch (metric) {
    case TENS_METRIC_WEEK:
      *frac = d->frac_week;  *color = TENS_COLOR_WEEK;
      *label = WDAY[now->tm_wday % 7];
      return true;
    case TENS_METRIC_MONTH:
      *frac = d->frac_month; *color = TENS_COLOR_MONTH;
      *label = MON[now->tm_mon % 12];
      return true;
    case TENS_METRIC_YEAR:
      *frac = d->frac_year;  *color = TENS_COLOR_YEAR;
      snprintf(buf, buflen, "%d", now->tm_year + 1900);
      *label = buf;
      return true;
    case TENS_METRIC_LIFE: {
      int now_year = now->tm_year + 1900;
      int now_month = now->tm_mon + 1;
      // Whole years lived: drop one if this year's birthday hasn't arrived yet.
      int age = now_year - cfg->birth_year;
      if (now_month < cfg->birth_month ||
          (now_month == cfg->birth_month && now->tm_mday < cfg->birth_day)) {
        age--;
      }
      if (age < 0) age = 0;
      *frac = d->frac_life;  *color = TENS_COLOR_LIFE;
      snprintf(buf, buflen, "%d", age);
      *label = buf;
      return true;
    }
    case TENS_METRIC_BATTERY: {
      int pct = clampi(batt.charge_percent, 0, 100);
      *frac = pct * 10;
      if (batt.is_charging) {
        *color = TENS_COLOR_BATTERY_CHARGING;
        snprintf(buf, buflen, "+%d", pct);
      } else if (pct <= TENS_BATTERY_LOW_PCT) {
        *color = TENS_COLOR_BATTERY_LOW;
        snprintf(buf, buflen, "%d", pct);
      } else {
        *color = TENS_COLOR_BATTERY;
        snprintf(buf, buflen, "%d", pct);
      }
      *label = buf;
      return true;
    }
    default:  // TENS_METRIC_OFF
      return false;
  }
}

void tens_render(GContext *ctx, GRect bounds, const struct tm *now,
                 const TensSettings *cfg) {

  bool dm = cfg->dark_mode;
  GColor bg = dm ? GColorBlack : GColorWhite;
  GColor ink = dm ? GColorWhite : GColorBlack;
  // Configurable color for the filled day-grid boxes (the "minute boxes"),
  // chosen per mode so each has a sensible default (black on white, white on
  // black). Labels and bars keep the plain mode ink.
  GColor grid_ink =
      GColorFromHEX(dm ? cfg->grid_color_dark : cfg->grid_color_light);
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

  render_grid(ctx, &L, &d, cfg, grid_ink, muted);

  // Four configurable bar slots: top-left, top-right, bottom-left, bottom-right.
  // Each slot shows one user-chosen metric (or OFF, drawn blank). Every slot is
  // half-width, so both bar rows split into two. Each carries a tiny overlaid
  // label (weekday / month / year / age / battery percent).
  BatteryChargeState batt = battery_state_service_peek();
  const int slots[4] = {cfg->slot_tl, cfg->slot_tr, cfg->slot_bl, cfg->slot_br};
  for (int s = 0; s < 4; s++) {
    int frac;
    GColor color;
    const char *label = NULL;
    char buf[12];
    if (!metric_view(slots[s], &d, now, cfg, batt, &frac, &color, &label, buf,
                     sizeof(buf))) {
      continue;  // OFF: leave the slot blank
    }
    GRect bar = tens_bar_quad(&L, s);
    fill_solid_bar(ctx, bar, frac, color, cfg->bars_missing_fill, muted);
    draw_bar_label(ctx, bar, label, ink);
  }
}
