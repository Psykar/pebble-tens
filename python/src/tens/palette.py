"""Semantic palette and Pebble-compatible color mapping.

Scene operations reference *semantic* palette entries (e.g. ``"ink"``,
``"accent"``) rather than arbitrary RGB. That keeps scenes easy to diff and
lets the C exporter emit the matching ``GColor`` constants.

Pebble Time 2 renders 64 colors (2 bits per channel). Each ``PaletteColor``
records the desktop-preview RGB plus the Pebble ``GColor*`` name used in C.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class PaletteColor:
    """A single semantic color.

    ``rgb`` is used by the desktop preview. ``gcolor`` is the Pebble C
    constant (e.g. ``GColorWhite``) emitted by the exporter.
    """

    rgb: tuple[int, int, int]
    gcolor: str


class Palette:
    """Named collection of semantic colors."""

    def __init__(self, name: str, colors: dict[str, PaletteColor]) -> None:
        if "background" not in colors:
            raise ValueError("palette must define a 'background' color")
        self.name = name
        self._colors = dict(colors)

    def __contains__(self, key: str) -> bool:
        return key in self._colors

    def __getitem__(self, key: str) -> PaletteColor:
        try:
            return self._colors[key]
        except KeyError as exc:
            raise KeyError(f"unknown palette color {key!r}") from exc

    def rgb(self, key: str) -> tuple[int, int, int]:
        return self[key].rgb

    def gcolor(self, key: str) -> str:
        return self[key].gcolor

    def names(self) -> list[str]:
        return list(self._colors)


# Fixed colors (independent of dark_mode).
_BLACK = PaletteColor((0, 0, 0), "GColorBlack")
_WHITE = PaletteColor((255, 255, 255), "GColorWhite")
_DARK_GRAY = PaletteColor((85, 85, 85), "GColorDarkGray")
_LIGHT_GRAY = PaletteColor((170, 170, 170), "GColorLightGray")
_MONTH = PaletteColor((255, 170, 85), "GColorRajah")
_YEAR = PaletteColor((85, 170, 255), "GColorPictonBlue")


# --- Gradients ---------------------------------------------------------------
# Only the life bar uses a gradient: a continuous "spectral" ramp with no
# divisions. Stops are raw RGB; the preview dithers them down to the Pebble
# 64-color gamut so intermediate colors still read as a smooth ramp.
GRADIENTS = {
    "spectral": [
        (255, 0, 0),      # red
        (255, 85, 0),    # orange
        (255, 170, 0),    # yellow
        (85, 170, 85),      # green
        (85, 170, 170),  # light blue
        (0, 85, 170),      # blue
    ],
}


def gradient_stops(name: str) -> list[tuple[int, int, int]]:
    try:
        return GRADIENTS[name]
    except KeyError as exc:
        raise KeyError(f"unknown gradient {name!r}") from exc


def _from_hex(value: int) -> PaletteColor:
    """A PaletteColor from a 0xRRGGBB int (preview RGB + a C GColorFromHEX)."""
    r, g, b = (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF
    return PaletteColor((r, g, b), f"GColorFromHEX(0x{value:06X})")


def _quant85(c: int) -> int:
    """Snap a 0..255 channel to the nearest 2-bit Pebble level (0/85/170/255)."""
    return min(3, (c + 42) // 85) * 85


def _darken(value: int) -> int | None:
    """A darker companion for ``value`` (0xRRGGBB) for the pending work boxes.

    Tries ~two then ~three gamut shades darker; returns ``None`` when darkening
    would collapse to black, letting the caller drop to a greyscale tone.
    """
    r, g, b = (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF
    for pct in (40, 20):
        dr, dg, db = _quant85(r * pct // 100), _quant85(g * pct // 100), _quant85(b * pct // 100)
        if (dr or dg or db) and (dr, dg, db) != (r, g, b):
            return (dr << 16) | (dg << 8) | db
    return None


def resolve(name: str = "default", dark_mode: bool = False,
            work_color: int = 0xFFAA00, grid_color: int | None = None) -> Palette:
    """Build the palette for the chosen background.

    dark_mode=False -> white background, black ink (boxes).
    dark_mode=True  -> black background, white ink.

    "muted" is one contrasty gray used for placeholders, unfilled tracks /
    outlines, and the month/year bars in rainbow mode: dark gray on a white
    background, light gray on a black one (so it stays visible in both).
    "month"/"year" are the fixed non-rainbow bar colors. "work" is the
    user-configurable work-day highlight color (RGB hex ``work_color``); its
    pending (future) boxes use "work_dark", a shade or two darker, or the muted
    gray when the pick is too dark to darken. "grid" is the configurable
    filled-box color (RGB hex ``grid_color``); it defaults to the mode ink when
    unset.
    """
    ink = _WHITE if dark_mode else _BLACK
    muted = _DARK_GRAY if dark_mode else _LIGHT_GRAY
    grid = ink if grid_color is None else _from_hex(grid_color)
    work_dark = _darken(work_color)
    return Palette(
        name,
        {
            "background": _BLACK if dark_mode else _WHITE,
            "ink": ink,
            "grid": grid,
            "muted": muted,
            "gray": _LIGHT_GRAY if dark_mode else _DARK_GRAY,
            "month": _MONTH,
            "year": _YEAR,
            "work": _from_hex(work_color),
            "work_dark": muted if work_dark is None else _from_hex(work_dark),
        },
    )
