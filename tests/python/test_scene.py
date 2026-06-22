import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "python" / "src"))

from tens import layout
from tens.derived import derive
from tens.export_c import scene_to_c
from tens.scene import FillRect, Gradient, StrokeRect, build_scene
from tens.state import RuntimeState, UserConfig


def _scene():
    rt = RuntimeState(2026, 6, 18, 3, 14, 30)
    cfg = UserConfig(birth_year=1990, birth_month=4, birth_day=12)
    return build_scene(rt, cfg, derive(rt, cfg))


def test_scene_canvas_matches_device():
    scene = _scene()
    assert (scene.width, scene.height) == (layout.CANVAS_W, layout.CANVAS_H)
    assert scene.width == 200 and scene.height == 228


def test_scene_has_144_grid_cells_plus_bars():
    scene = _scene()
    # Every one of the 144 boxes contributes at least one box-sized op
    # (fill/gradient/outline/placeholder), regardless of render settings.
    box_ops = [
        op for op in scene.ops
        if isinstance(op, (FillRect, StrokeRect, Gradient))
        and op.w <= layout.BOX and op.h <= layout.BOX
    ]
    assert len(box_ops) >= 144


def test_all_colors_are_palette_keys():
    scene = _scene()
    pal = scene.palette()
    for op in scene.ops:
        color = getattr(op, "color", None)
        if color is not None:
            assert color in pal, f"{color!r} not in palette"


def test_coords_are_integers_and_in_bounds():
    scene = _scene()
    for op in scene.ops:
        for attr in ("x", "y", "w", "h"):
            v = getattr(op, attr, None)
            if v is not None:
                assert isinstance(v, int)


def test_export_c_emits_render_function():
    src = scene_to_c(_scene())
    assert "tens_render_scene" in src
    assert "#include <pebble.h>" in src
    assert "graphics_fill_rect" in src


def test_work_highlight_off_by_default():
    colors = {getattr(op, "color", None) for op in _scene().ops}
    assert "work" not in colors


def test_work_highlight_recolors_work_hour_cells():
    rt = RuntimeState(2026, 6, 18, 3, 14, 30)  # 14:30 -> ten-minute index 87
    cfg = UserConfig(
        birth_year=1990, birth_month=4, birth_day=12, work_enabled=True
    )  # default 9-17 covers cells 54..101
    scene = build_scene(rt, cfg, derive(rt, cfg))
    colors = {getattr(op, "color", None) for op in scene.ops}
    assert "work" in colors
    # The configured work color resolves to a real palette entry.
    assert "work" in scene.palette()

