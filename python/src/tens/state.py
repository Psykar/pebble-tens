"""Raw input state.

Two kinds of raw facts feed the watchface:

- ``RuntimeState`` comes from the watch clock and tick events.
- ``UserConfig`` comes from the phone-side settings page (PebbleKit JS).

Neither holds derived meaning. Season, age, and progress values live in
``derived.py`` and are computed during preprocessing.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class RuntimeState:
    """Facts that come directly from the watch clock / tick events."""

    year: int
    month: int  # 1-12
    day: int  # 1-31
    weekday: int  # 0=Monday .. 6=Sunday
    hour: int  # 0-23
    minute: int  # 0-59

    def __post_init__(self) -> None:
        _check("month", self.month, 1, 12)
        _check("day", self.day, 1, 31)
        _check("weekday", self.weekday, 0, 6)
        _check("hour", self.hour, 0, 23)
        _check("minute", self.minute, 0, 59)

    @classmethod
    def from_datetime(cls, dt) -> "RuntimeState":
        """Build from a ``datetime.datetime`` (handy for previews/tests)."""
        return cls(
            year=dt.year,
            month=dt.month,
            day=dt.day,
            weekday=dt.weekday(),
            hour=dt.hour,
            minute=dt.minute,
        )


@dataclass(frozen=True)
class UserConfig:
    """Facts entered by the user in the phone-side settings page.

    Birth date is kept as structured integer fields rather than a formatted
    string so age and progress calculations are easy in both Python and C.
    """

    birth_year: int = 1988
    birth_month: int = 11  # 1-12
    birth_day: int = 29  # 1-31
    # Appearance / behavior knobs (extend as the settings schema grows).
    palette_name: str = "default"
    dark_mode: bool = False  # False=white bg/black boxes, True=black bg/white boxes
    life_span_years: int = 80  # for life-progress bars / span metrics
    # Hour-block layout. "4x6" = 3x2 cells (half-hour is a horizontal row),
    # "6x4" = 2x3 cells (half-hour is a vertical column). This drives the box
    # and minute-line fill direction.
    layout: str = "6x4"  # "4x6" | "6x4"
    # Order the hour-blocks populate the grid: "vertical" = column-major (hour
    # 1 below hour 0), "horizontal" = row-major (hour 1 right of hour 0).
    hours_direction: str = "horizontal"  # "vertical" | "horizontal"
    fill_invert: bool = False  # vertical fill: False=from top, True=from bottom
    #                            horizontal fill: False=from left, True=from right
    # How the incomplete (missing) part of the current box and the bars renders:
    missing_style: str = "outline"  # "outline" (border) | "fill" (light gray)
    # Rainbow: color the inked boxes/minute-lines by a spectral gradient that
    # spans the whole day grid (the ink acts as a mask over the gradient).
    rainbow: bool = False
    # Work-day highlight: recolor the grid boxes whose 10-minute slot falls in
    # [work_start, work_end) minutes after midnight (no overnight wrap). Off by
    # default; 9-to-5 when on.
    work_enabled: bool = False
    work_start: int = 9 * 60  # work-day start, minutes after midnight (0-1439)
    work_end: int = 17 * 60  # work-day end (exclusive), minutes after midnight
    work_color: int = 0xFFAA00  # RGB hex for highlighted boxes
    # Filled-box ("minute box") colors, chosen per mode so each has a sensible
    # default: black ink on the white light background, white ink on the black.
    grid_color_light: int = 0x000000  # RGB hex for filled boxes in light mode
    grid_color_dark: int = 0xFFFFFF  # RGB hex for filled boxes in dark mode

    def __post_init__(self) -> None:
        _check("birth_month", self.birth_month, 1, 12)
        _check("birth_day", self.birth_day, 1, 31)
        if self.life_span_years <= 0:
            raise ValueError("life_span_years must be positive")
        if self.layout not in ("4x6", "6x4"):
            raise ValueError("layout must be '4x6' or '6x4'")
        if self.hours_direction not in ("vertical", "horizontal"):
            raise ValueError("hours_direction must be 'vertical' or 'horizontal'")
        if self.missing_style not in ("outline", "fill"):
            raise ValueError("missing_style must be 'outline' or 'fill'")
        _check("work_start", self.work_start, 0, 1439)
        _check("work_end", self.work_end, 0, 1440)
        _check("work_color", self.work_color, 0, 0xFFFFFF)
        _check("grid_color_light", self.grid_color_light, 0, 0xFFFFFF)
        _check("grid_color_dark", self.grid_color_dark, 0, 0xFFFFFF)


def _check(name: str, value: int, lo: int, hi: int) -> None:
    if not (lo <= value <= hi):
        raise ValueError(f"{name}={value} out of range [{lo}, {hi}]")
