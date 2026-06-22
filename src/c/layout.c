#include "layout.h"

// Bars below the grid. Sized per display so they fit the shorter legacy canvas.
#if PBL_DISPLAY_WIDTH >= 200
#define BAR_H 10
#define GRID_BAR_GAP 10
#define BAR_GAP 6
#else
#define BAR_H 7
#define GRID_BAR_GAP 7
#define BAR_GAP 5
#endif
#define SUB_GAP TENS_BLOCK_GAP   // gap between week/month/year == hour-block gap
#define N_BARS 2

void tens_layout_init(TensLayout *L, int canvas_w, int canvas_h,
                      bool layout_4x6, bool hours_horizontal) {
  if (layout_4x6) {  // "4x6": 4 cols x 6 rows, cells 3x2
    L->blocks_x = 4;
    L->blocks_y = 6;
    L->cell_x = 3;
    L->cell_y = 2;
  } else {  // "6x4": 6 cols x 4 rows, cells 2x3
    L->blocks_x = 6;
    L->blocks_y = 4;
    L->cell_x = 2;
    L->cell_y = 3;
  }
  L->hours_horizontal = hours_horizontal;
  L->block_w = L->cell_x * TENS_BOX + (L->cell_x - 1) * TENS_CELL_GAP;
  L->block_h = L->cell_y * TENS_BOX + (L->cell_y - 1) * TENS_CELL_GAP;
  L->grid_w = L->blocks_x * L->block_w + (L->blocks_x - 1) * TENS_BLOCK_GAP;
  L->grid_h = L->blocks_y * L->block_h + (L->blocks_y - 1) * TENS_BLOCK_GAP;
  L->ox = (canvas_w - L->grid_w) / 2;
  int bars_total = GRID_BAR_GAP + N_BARS * BAR_H + (N_BARS - 1) * BAR_GAP;
  L->oy = (canvas_h - (L->grid_h + bars_total)) / 2;
}

GRect tens_day_rect(const TensLayout *L) {
  return GRect(L->ox, L->oy, L->grid_w, L->grid_h);
}

GRect tens_hour_block(const TensLayout *L, int hour) {
  int bcol, brow;
  if (L->hours_horizontal) {  // row-major
    bcol = hour % L->blocks_x;
    brow = hour / L->blocks_x;
  } else {  // column-major
    brow = hour % L->blocks_y;
    bcol = hour / L->blocks_y;
  }
  int x = L->ox + bcol * (L->block_w + TENS_BLOCK_GAP);
  int y = L->oy + brow * (L->block_h + TENS_BLOCK_GAP);
  return GRect(x, y, L->block_w, L->block_h);
}

GRect tens_ten_minute_cell(const TensLayout *L, int index) {
  int hour = index / 6;
  int sub = index % 6;  // 0..5 within the hour
  GRect block = tens_hour_block(L, hour);
  int ccol, crow;
  if (L->cell_x == 3) {  // 3-wide: half-hour is a horizontal row of three
    ccol = sub % 3;
    crow = sub / 3;
  } else {  // 2-wide: half-hour is a vertical column of three
    crow = sub % 3;
    ccol = sub / 3;
  }
  int x = block.origin.x + ccol * (TENS_BOX + TENS_CELL_GAP);
  int y = block.origin.y + crow * (TENS_BOX + TENS_CELL_GAP);
  return GRect(x, y, TENS_BOX, TENS_BOX);
}

static GRect bar_slot(const TensLayout *L, int slot) {
  int top = L->oy + L->grid_h + GRID_BAR_GAP + slot * (BAR_H + BAR_GAP);
  return GRect(L->ox, top, L->grid_w, BAR_H);
}

// Top bar (slot 0) split into two halves with SUB_GAP between them.
static GRect half(const TensLayout *L, int i) {
  GRect bar = bar_slot(L, 0);
  int left_w = (bar.size.w - SUB_GAP) / 2;
  if (i == 0) return GRect(bar.origin.x, bar.origin.y, left_w, BAR_H);
  int right_x = bar.origin.x + left_w + SUB_GAP;
  int right = bar.origin.x + bar.size.w;
  return GRect(right_x, bar.origin.y, right - right_x, BAR_H);  // absorbs remainder
}

GRect tens_month_bar(const TensLayout *L) { return half(L, 0); }
GRect tens_year_bar(const TensLayout *L)  { return half(L, 1); }
GRect tens_life_bar(const TensLayout *L)  { return bar_slot(L, 1); }
