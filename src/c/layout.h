// Geometry mirroring python/src/tens/layout.py. The Python design targets the
// 200x228 emery (Pebble Time 2) display, but the runtime scales to the actual
// canvas so the face also fits the 144x168 legacy displays (basalt / Pebble
// Time + Pebble Time Steel). The unit sizes below are selected per display at
// compile time; centering uses the real canvas bounds passed in at render time.
// The layout is chosen at runtime from the hour-fill direction, so a
// TensLayout is built once per render.
#pragma once
#include <pebble.h>

#if PBL_DISPLAY_WIDTH >= 200
// emery (200x228): the original Python design metrics.
#define TENS_BOX 10        // edge of one ten-minute box, px
#define TENS_CELL_GAP 3    // gap between boxes inside an hour-block
#define TENS_BLOCK_GAP 8   // gap between hour-blocks
#else
// 144x168 legacy displays (basalt etc.): a proportionally smaller field so the
// 12x12 box grid plus the bars fit within the shorter canvas.
#define TENS_BOX 7
#define TENS_CELL_GAP 2
#define TENS_BLOCK_GAP 6
#endif

typedef struct {
  int blocks_x, blocks_y;   // hour-blocks across / down
  int cell_x, cell_y;       // boxes inside one block
  int block_w, block_h;
  int grid_w, grid_h;
  int ox, oy;               // grid origin (centered)
  bool hours_horizontal;    // hour-block order: row-major vs column-major
} TensLayout;

// layout_4x6=true  -> "4x6": 4 cols x 6 rows, cells 3x2, horizontal half-hours.
// layout_4x6=false -> "6x4": 6 cols x 4 rows, cells 2x3, vertical half-hours.
// hours_horizontal sets the order hour-blocks populate the grid.
// canvas_w/canvas_h are the live display bounds, used to center the content.
void tens_layout_init(TensLayout *layout, int canvas_w, int canvas_h,
                      bool layout_4x6, bool hours_horizontal);

GRect tens_day_rect(const TensLayout *layout);
GRect tens_hour_block(const TensLayout *layout, int hour);
GRect tens_ten_minute_cell(const TensLayout *layout, int index);
// Bar slot rect by index: 0=top-left, 1=top-right, 2=bottom-left,
// 3=bottom-right. Both bar rows are split into two half-width slots.
GRect tens_bar_quad(const TensLayout *layout, int slot);
