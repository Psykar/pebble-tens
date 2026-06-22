"""Scene and operation dataclasses.

A ``Scene`` is a fully resolved render plan: an ordered display list of small,
explicit drawing operations. It is not a UI tree and not a raster. Pebble
drawing is painterly (later ops paint over earlier ones), so order is
significant.

Design rules for every op:
- All coordinates and sizes are absolute integer pixels.
- Colors are semantic palette keys, never raw RGB.
- Draw order is explicit; there is no hidden auto-layout.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from . import layout
from .derived import DerivedState
from .palette import Palette, resolve
from .state import RuntimeState, UserConfig


# --- Operations --------------------------------------------------------------

@dataclass(frozen=True)
class Op:
    """Base class for drawing operations. ``kind`` tags the op for export."""

    kind: str = field(init=False, default="op")


@dataclass(frozen=True)
class FillRect(Op):
    x: int
    y: int
    w: int
    h: int
    color: str  # palette key
    radius: int = 0
    kind: str = field(init=False, default="fill_rect")


@dataclass(frozen=True)
class StrokeRect(Op):
    x: int
    y: int
    w: int
    h: int
    color: str
    radius: int = 0
    kind: str = field(init=False, default="stroke_rect")


@dataclass(frozen=True)
class Line(Op):
    x1: int
    y1: int
    x2: int
    y2: int
    color: str
    width: int = 1
    kind: str = field(init=False, default="line")


@dataclass(frozen=True)
class Text(Op):
    x: int
    y: int
    w: int
    h: int
    text: str
    color: str
    font: str = "GOTHIC_18"  # Pebble system font key
    align: str = "left"  # left | center | right
    kind: str = field(init=False, default="text")


@dataclass(frozen=True)
class Bitmap(Op):
    x: int
    y: int
    resource: str  # resource id declared in package.json
    kind: str = field(init=False, default="bitmap")


@dataclass(frozen=True)
class Pdc(Op):
    """Pebble Draw Command (vector) resource."""

    x: int
    y: int
    resource: str
    kind: str = field(init=False, default="pdc")


@dataclass(frozen=True)
class Gradient(Op):
    """A dithered multi-stop gradient fill (named in palette.GRADIENTS).

    Used for the structured bars. The preview renders it with Floyd-Steinberg
    dithering to the Pebble gamut; on-device this maps to a precomputed bitmap
    or a dithered fill routine.
    """

    x: int
    y: int
    w: int
    h: int
    gradient: str  # key into palette.GRADIENTS
    axis: str = "h"  # "h" (left->right) or "v" (top->bottom)
    span: int = 0  # full extent the ramp maps over; 0 means == w
    offset: int = 0  # starting pixel into the span (window of a larger ramp)
    kind: str = field(init=False, default="gradient")


@dataclass(frozen=True)
class FramebufferPatch(Op):
    """Direct pixel-level control. Use only when truly required."""

    x: int
    y: int
    w: int
    h: int
    data_resource: str
    kind: str = field(init=False, default="framebuffer_patch")


# --- Scene -------------------------------------------------------------------

@dataclass
class Scene:
    width: int
    height: int
    background: str  # palette key
    palette_name: str
    dark_mode: bool = False  # white-on-black when True
    work_color: int = 0xFFAA00  # RGB hex for the work-day highlight
    grid_color_light: int = 0x000000  # RGB hex for filled boxes in light mode
    grid_color_dark: int = 0xFFFFFF  # RGB hex for filled boxes in dark mode
    ops: list[Op] = field(default_factory=list)
    meta: dict = field(default_factory=dict)

    def add(self, op: Op) -> "Scene":
        self.ops.append(op)
        return self

    def palette(self) -> Palette:
        grid_color = self.grid_color_dark if self.dark_mode else self.grid_color_light
        return resolve(self.palette_name, self.dark_mode, self.work_color, grid_color)


# --- Builder -----------------------------------------------------------------

def build_scene(
    rt: RuntimeState,
    cfg: UserConfig,
    derived: DerivedState,
    placeholder: str = "dot",
) -> Scene:
    """Create an ordered scene from resolved state.

    This is the single place that turns meaning into drawing instructions.
    Geometry comes entirely from ``layout``; nothing is computed inline.

    ``placeholder`` controls how not-yet-reached (empty) boxes render:
    "dot" (centered 4x4 muted), "block" (muted 10x10), or "outline".
    """
    scene = Scene(
        width=layout.CANVAS_W,
        height=layout.CANVAS_H,
        background="background",
        palette_name=cfg.palette_name,
        dark_mode=cfg.dark_mode,
        work_color=cfg.work_color,
        grid_color_light=cfg.grid_color_light,
        grid_color_dark=cfg.grid_color_dark,
        meta={
            "version": 1,
            "time": f"{rt.hour:02d}:{rt.minute:02d}",
            "date": f"{rt.year:04d}-{rt.month:02d}-{rt.day:02d}",
        },
    )

    # Ten-minute boxes. Each box is BOX x BOX px and fills one pixel-row per
    # minute, so the box holding "now" shows minute_of_box rows of fill:
    #   - completed boxes  -> solid (all rows)
    #   - the current box  -> minute_of_box lines
    #   - future boxes     -> placeholder (see _placeholder)
    # cfg.layout sets the cell shape (3x2 vs 2x3) and thus the box/minute fill
    # axis; cfg.hours_direction sets how hour-blocks populate the grid. Filled
    # areas are "ink"; with cfg.rainbow they instead reveal a spectral gradient
    # spanning the whole grid (the ink acts as a mask).
    layout_key = cfg.layout
    grid = layout.day_rect(layout_key)
    fill_axis = layout.fill_axis(layout_key)
    # Work-day highlight: recolor inked boxes whose 10-minute slot is in
    # [start, end) minutes. Takes precedence over rainbow so the work block
    # reads as one solid color.
    work = cfg.work_enabled and cfg.work_end > cfg.work_start
    for i in range(144):
        cell = layout.ten_minute_cell(i, layout_key, cfg.hours_direction)
        in_work = work and cfg.work_start <= i * 10 < cfg.work_end
        ink_color = "work" if in_work else "grid"
        if i < derived.ten_minute_index:
            _ink_rect(scene, cell.x, cell.y, cell.w, cell.h, grid, cfg.rainbow,
                      ink_color)
        elif i == derived.ten_minute_index:
            # Show the whole current box (its missing part as outline or fill),
            # then the completed-minute lines on top. The missing part matches
            # the surrounding pending boxes (darker work shade in work hours).
            missing = "work_dark" if in_work else "muted"
            if cfg.missing_style == "fill":
                scene.add(FillRect(cell.x, cell.y, cell.w, cell.h, missing))
            else:
                scene.add(StrokeRect(cell.x, cell.y, cell.w, cell.h, missing))
            _fill_lines(
                scene, cell, derived.minute_of_box,
                fill_axis, cfg.fill_invert, grid, cfg.rainbow, ink_color,
            )
        else:
            # Pending work-hour boxes hint a darker shade of the work color; the
            # rest stay muted.
            _placeholder(scene, cell, placeholder,
                         "work_dark" if in_work else "muted")

    # Two bars under the grid: a top bar split in half (month | year) and the
    # long life bar. Each fills up to its progress over a "muted" track. In
    # rainbow mode month/year are the contrasty gray; otherwise their fixed
    # colors. Life mirrors the boxes: ink, or the spectral gradient in rainbow.
    ms = cfg.missing_style
    month_color = "gray" if cfg.rainbow else "month"
    year_color = "gray" if cfg.rainbow else "year"
    _structured_bar(scene, layout.month_bar(layout_key), [(1.0, month_color)],
                    derived.fraction_of_month, ms)
    _structured_bar(scene, layout.year_bar(layout_key), [(1.0, year_color)],
                    derived.fraction_of_year, ms)
    # Life: same as the boxes -> spectral gradient when rainbow, else solid ink.
    life = layout.life_bar(layout_key)
    if cfg.rainbow:
        _gradient_bar(scene, life, "spectral", derived.fraction_of_life, ms)
    else:
        _structured_bar(scene, life, [(1.0, "ink")], derived.fraction_of_life, ms)

    return scene


# Size of the centered "dot" placeholder, px.
PLACEHOLDER_DOT = 4


def _placeholder(scene: Scene, cell: layout.Rect, style: str,
                 color: str = "muted") -> None:
    """Render an empty (not-yet-reached) box in ``color`` (muted by default).

    "dot"     -> centered PLACEHOLDER_DOT square
    "block"   -> full fill
    "outline" -> outline
    """
    if style == "block":
        scene.add(FillRect(cell.x, cell.y, cell.w, cell.h, color))
    elif style == "outline":
        scene.add(StrokeRect(cell.x, cell.y, cell.w, cell.h, color))
    else:  # "dot"
        d = PLACEHOLDER_DOT
        ox = cell.x + (cell.w - d) // 2
        oy = cell.y + (cell.h - d) // 2
        scene.add(FillRect(ox, oy, d, d, color))


def _ink_rect(
    scene: Scene, x: int, y: int, w: int, h: int,
    grid: layout.Rect, rainbow: bool, color: str = "ink",
) -> None:
    """Draw a filled grid region: solid ``color``, or a slice of the grid-wide
    spectral gradient when ``rainbow`` is set and the region is the plain grid
    box color (the region masks the gradient). A highlight ``color`` (e.g. the
    work-day color) always fills solid and overrides rainbow.
    """
    if rainbow and color == "grid":
        scene.add(Gradient(x, y, w, h, "spectral", "h", span=grid.w, offset=x - grid.x))
    else:
        scene.add(FillRect(x, y, w, h, color))


def _fill_lines(
    scene: Scene,
    cell: layout.Rect,
    count: int,
    axis: str,
    invert: bool,
    grid: layout.Rect,
    rainbow: bool,
    color: str = "ink",
) -> None:
    """Fill ``count`` 1px lines of a box (1 line == 1 completed minute).

    axis "vertical"   -> horizontal rows stacked; invert False=from top,
                         True=from bottom.
    axis "horizontal" -> vertical columns stacked; invert False=from left,
                         True=from right.
    """
    if axis == "horizontal":
        cols = min(cell.w, max(0, count))
        if cols == 0:
            return
        x = cell.right - cols if invert else cell.x
        _ink_rect(scene, x, cell.y, cols, cell.h, grid, rainbow, color)
    else:  # vertical
        rows = min(cell.h, max(0, count))
        if rows == 0:
            return
        y = cell.bottom - rows if invert else cell.y
        _ink_rect(scene, cell.x, y, cell.w, rows, grid, rainbow, color)


def _missing_track(scene: Scene, rect: layout.Rect, missing_style: str) -> None:
    """Render the bar's missing background: muted outline border or muted fill."""
    if missing_style == "fill":
        scene.add(FillRect(rect.x, rect.y, rect.w, rect.h, "muted"))
    else:  # "outline"
        scene.add(StrokeRect(rect.x, rect.y, rect.w, rect.h, "muted"))


def _structured_bar(
    scene: Scene,
    rect: layout.Rect,
    segments: list[tuple[float, str]],
    progress: float,
    missing_style: str = "outline",
) -> None:
    """Draw a bar split into solid color ``segments`` (no inner margin).

    ``segments`` is a list of (width_fraction, color_key). The missing part is
    drawn first (outline border or light-gray fill); solid segments are then
    emitted left-to-right up to ``progress`` (0..1).
    """
    progress = min(1.0, max(0.0, progress))
    _missing_track(scene, rect, missing_style)
    fill_right = rect.x + round(rect.w * progress)
    x = rect.x
    for i, (frac, color) in enumerate(segments):
        # Last segment absorbs rounding so a full bar reaches the right edge.
        seg_w = (rect.right - x) if i == len(segments) - 1 else round(rect.w * frac)
        draw_w = min(seg_w, fill_right - x)
        if draw_w > 0:
            scene.add(FillRect(x, rect.y, draw_w, rect.h, color))
        x += seg_w
        if x >= fill_right:
            break


def _gradient_bar(
    scene: Scene,
    rect: layout.Rect,
    gradient: str,
    progress: float,
    missing_style: str = "outline",
) -> None:
    """Draw a single continuous gradient bar revealed up to ``progress``.

    The ramp maps across the full bar width (``span``) but only the filled
    portion is drawn, so the colors stay anchored to the whole span as it
    fills. The missing tail is an outline border or light-gray fill.
    """
    progress = min(1.0, max(0.0, progress))
    _missing_track(scene, rect, missing_style)
    fill_w = round(rect.w * progress)
    if fill_w > 0:
        scene.add(Gradient(rect.x, rect.y, fill_w, rect.h, gradient, "h", span=rect.w))
