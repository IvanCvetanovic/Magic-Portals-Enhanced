#!/usr/bin/env python3
"""Magic Rampage's control buttons, as the art of Magic Portals Enhanced's modern controls (CTL-1).

PRECONDITION. The owner's request of 2026-10-15 ("Reuse the control buttons from Magic Rampage") is
the go-ahead for copying Magic Rampage's sprites into the gitignored reference/magic-rampage/ for
local use. docs/permission.md is still the unfilled placeholder and says nothing about Magic Rampage
(another Asantee game). Nothing this tool reads or writes is committed or shipped, and any release
package containing that folder stays gated on the owner's word with the rights holder (CTL design,
question Q2). This file holds code only: no image, no base64 of one, no byte of Asantee's art.

    python -I tools/art/make_mr_controls.py --assets <folder holding the base APK's assets/>
    python -I tools/art/make_mr_controls.py --xapk   <Magic+Rampage_7.8.7_APKPure.xapk>
    python -I tools/art/make_mr_controls.py --check          (re-derive and compare; writes nothing)
    python -I tools/art/make_mr_controls.py --stage editor   (the adjustment screen's glyphs, CTL-11)
    python -I tools/art/make_mr_controls.py --stage all      (both, each into its own default folder)

Run it with tools/parity/.venv (Pillow and numpy), from inside this repository: the repository root
is `git rev-parse --show-toplevel` of the current directory, never a path computed from this file.
Without --assets or --xapk the package is reference/magic-rampage/assets, which --extract (on by
default) fills with a verbatim copy of the sprites this tool and the editor stage (CTL-11) read, so
the original package, or any other session's scratch copy of it, is needed once and no longer.
That folder is fixed: whatever --out is, the sprites are never written beside it.

WHAT IT WRITES into --out (default reference/magic-rampage/controls), 128 x 128 straight-alpha
RGBA, with manifest.json beside them (and, apart from --out, the --extract copy of the sprites in
reference/magic-rampage/assets):
  walk_left.png      sprites/dpad-left.png as shipped, not resampled
  walk_right.png     sprites/dpad-right.png as shipped, not resampled
  restart.png        sprites/dpad-frame.png + sprites/icon-restart-64.png, restyled as the pad's own
                     icons are (grey 215 fill, black outline, shadow to the lower right) and centred
                     on the body; the icon's own dark shadow is left out of its mask (white_ink)
  pause.png          the frame + two bars this tool draws (a framed square; Magic Rampage's own
                     pause is a wide pill, which does not fit a square box: design Q5)
  clear_portals.png  the frame + a ring with an X in its middle, which this tool draws (Magic
                     Rampage has no such glyph: design Q12)
The "body" is the solid 100 x 100 square (alpha >= 250) inside the 14 px soft shadow every pad
sprite carries: (14, 14, 114, 114). The layout reads its insets from that rect, so every output has
it, and a source that does not is refused.

THE EDITOR STAGE (--stage editor, CTL-11) writes into reference/magic-rampage/editor, 128 x 128
straight-alpha RGBA with a manifest.json of its own, the pictures of the adjustment screen's seven
widgets (and the lock's second state):
  size_minus.png     sprites/smaller-frame-bg.png + sprites/resize-symbol-shrink.png
  size_plus.png      sprites/smaller-frame-bg.png + sprites/resize-symbol-enlarge.png
  opacity_minus.png  sprites/smaller-frame-bg.png + sprites/adjust-symbol-transparent.png
  opacity_plus.png   sprites/smaller-frame-bg.png + sprites/adjust-symbol-opaque.png
  lock_locked.png    sprites/smaller-frame-bg.png + sprites/lock-locked.png
  lock_unlocked.png  sprites/smaller-frame-red-bg.png + sprites/lock-unlocked.png
  restore.png        sprites/smaller-frame-bg.png + sprites/icon-restart-64.png
  back.png           sprites/inventory-back-button.png as shipped
A symbol is laid over its frame 1:1 (never resampled), its own solid body centred on the frame's, to
a whole pixel. Each output records its OWN body rect in the manifest (the frames' bodies differ:
the stone frame's is (19, 19, 110, 110), the red one's (21, 21, 107, 107), Back's (15, 15, 113, 113)),
and the margin the layout reads is the smallest of its four sides; controls.json's
adjust.art.mr.<glyph>.body_inset is that margin over 128, which a test of this tool's checks.

THE OUTPUT FOLDER IS REFUSED unless `git check-ignore` reports every file about to be written as
ignored: the five images and the manifest in --out, and the extracted sprites in
reference/magic-rampage/assets (reference/ is ignored, by the existing unanchored line in
.gitignore: no .gitignore edit is needed). That check runs before the package is even opened, so
a refusal writes nothing.

The helpers (straight_rgba, the restyle, the disc dilation, the shadow) follow the ones in
Penumbra's tools/art/make_mr_touch_art.py: copied and rewritten here, never imported, and nothing
here points at that repository. Exit codes: 0 done (or --check passed), 1 refused / failed /
--check found a difference, 2 usage.
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import io
import json
import math
import pathlib
import subprocess
import sys
import zipfile

import numpy as np
import PIL
from PIL import Image, ImageDraw, ImageFilter

TOOL_VERSION = 1
PILLOW_VERSION = PIL.__version__
MR_VERSION = "7.8.7"
BASE_APK = "com.asanteegames.magicrampage.apk"
XAPK_MANIFEST = "manifest.json"

# Where things go, relative to the repository root. Everything under reference/ is ignored.
DEFAULT_OUT = pathlib.PurePosixPath("reference/magic-rampage/controls")
DEFAULT_ASSETS = pathlib.PurePosixPath("reference/magic-rampage/assets")
MANIFEST_NAME = "manifest.json"
EXTRACT_MANIFEST_NAME = "extract.json"

STAGE_CONTROLS = "controls"
STAGE_EDITOR = "editor"
STAGE_ALL = "all"
STAGES = (STAGE_CONTROLS, STAGE_EDITOR)           # what "all" is
DEFAULT_EDITOR_OUT = pathlib.PurePosixPath("reference/magic-rampage/editor")

EXIT_OK = 0
EXIT_FAILED = 1
EXIT_USAGE = 2

# Every pad sprite is a 128 px square: a solid body inside a soft 14 px shadow.
CANVAS = 128
BODY_RECT = (14, 14, 114, 114)
BODY_ALPHA = 250

# Magic Rampage's pad icons, measured on dpad-left.png: grey fill about 215, a black outline about
# 3.5 px wide, a soft shadow to the lower right.
ICON_FILL = (215, 215, 215)
OUTLINE = 3.5
SHADOW_OFFSET = (3.0, 3.0)
SHADOW_BLUR = 3.5
SHADOW_STRENGTH = 0.6
SUPERSAMPLE = 4
RESTART_BOX = 60          # the restyled icon's largest side, outline included, in output px

# The two drawn glyphs, fill only (the outline grows them by OUTLINE on every side), in output px.
PAUSE_BAR_W = 12
PAUSE_BAR_H = 40
PAUSE_BAR_GAP = 10
PAUSE_BAR_RADIUS = 3
RING_DIAMETER = 53
RING_STROKE = 5
CROSS_REACH = 8           # each arm's end, from the ring's centre along both axes
CROSS_STROKE = 5

# Roles of a source sprite: what the tool uses it for today.
ROLE_CONTROL = "control"
ROLE_EDITOR = "editor"
ROLE_SPARE = "spare"

# Recipes: the id goes in the manifest beside its words.
RECIPE_AS_SHIPPED = "as_shipped"
RECIPE_RESTART = "frame_plus_restyled_icon"
RECIPE_PAUSE = "frame_plus_drawn_bars"
RECIPE_CLEAR = "frame_plus_drawn_ring_and_x"
RECIPE_TILE = "frame_plus_symbol_one_to_one"

RECIPES = {
    RECIPE_AS_SHIPPED: "the sprite as shipped: straight RGBA, colour under alpha 0 made black, "
                       "never resampled",
    RECIPE_RESTART: f"dpad-frame, then icon-restart-64's white ink (grey x alpha; its baked-in "
                    f"dark shadow left out) restyled (fill {ICON_FILL[0]}, black outline "
                    f"{OUTLINE} px, shadow offset {SHADOW_OFFSET} blur {SHADOW_BLUR} strength "
                    f"{SHADOW_STRENGTH}, {SUPERSAMPLE}x supersample, Lanczos) into a "
                    f"{RESTART_BOX} px box centred on the body {BODY_RECT}",
    RECIPE_PAUSE: f"dpad-frame, then two bars drawn here ({PAUSE_BAR_W} x {PAUSE_BAR_H} px, gap "
                  f"{PAUSE_BAR_GAP}, corner radius {PAUSE_BAR_RADIUS}) in the same restyle, "
                  f"centred on the body {BODY_RECT}",
    RECIPE_CLEAR: f"dpad-frame, then a ring ({RING_DIAMETER} px across, {RING_STROKE} px stroke) "
                  f"with an X inside it (arms to +-{CROSS_REACH} px, {CROSS_STROKE} px stroke, "
                  f"round ends) drawn here in the same restyle, concentric, centred on the body "
                  f"{BODY_RECT}",
    RECIPE_TILE: "the frame as shipped, then the symbol as shipped laid over it 1:1 (never "
                 "resampled), its own solid body centred on the frame's solid body to a whole "
                 "pixel (an odd difference rounds down); alpha-composited, straight RGBA",
}


class ToolError(Exception):
    """A refusal or a failure the user is told about; main() turns it into exit 1."""


class UsageError(Exception):
    """Arguments that do not go together; main() turns it into exit 2."""


@dataclasses.dataclass(frozen=True)
class Sprite:
    member: str                           # path under the base APK's assets/
    role: str
    size: tuple[int, int] | None = None   # asserted when the tool decodes it


OUTPUT_NAMES = ("walk_left.png", "walk_right.png", "restart.png", "pause.png", "clear_portals.png")

FRAME = Sprite("sprites/dpad-frame.png", ROLE_CONTROL, (CANVAS, CANVAS))
DPAD_LEFT = Sprite("sprites/dpad-left.png", ROLE_CONTROL, (CANVAS, CANVAS))
DPAD_RIGHT = Sprite("sprites/dpad-right.png", ROLE_CONTROL, (CANVAS, CANVAS))
ICON_RESTART = Sprite("sprites/icon-restart-64.png", ROLE_CONTROL, (64, 64))

# The editor stage's sources (CTL-11). The frames and the back button are 128 px squares, the symbols 64.
FRAME_STONE = Sprite("sprites/smaller-frame-bg.png", ROLE_EDITOR, (CANVAS, CANVAS))
FRAME_RED = Sprite("sprites/smaller-frame-red-bg.png", ROLE_EDITOR, (CANVAS, CANVAS))
SYMBOL_SHRINK = Sprite("sprites/resize-symbol-shrink.png", ROLE_EDITOR, (64, 64))
SYMBOL_ENLARGE = Sprite("sprites/resize-symbol-enlarge.png", ROLE_EDITOR, (64, 64))
SYMBOL_TRANSPARENT = Sprite("sprites/adjust-symbol-transparent.png", ROLE_EDITOR, (64, 64))
SYMBOL_OPAQUE = Sprite("sprites/adjust-symbol-opaque.png", ROLE_EDITOR, (64, 64))
SYMBOL_LOCKED = Sprite("sprites/lock-locked.png", ROLE_EDITOR, (64, 64))
SYMBOL_UNLOCKED = Sprite("sprites/lock-unlocked.png", ROLE_EDITOR, (64, 64))
BACK_BUTTON = Sprite("sprites/inventory-back-button.png", ROLE_EDITOR, (CANVAS, CANVAS))

# The solid body (alpha >= 250) of each frame, MEASURED on the sprites of Magic Rampage 7.8.7 with this
# tool's own body_rect, and recorded in the manifest of the editor stage. The frames differ, and so do
# the margins the layout draws round them (adjust.art.mr in controls.json).
STONE_BODY = (19, 19, 110, 110)
RED_BODY = (21, 21, 107, 107)
BACK_BODY = (15, 15, 113, 113)


@dataclasses.dataclass(frozen=True)
class EditorSpec:
    name: str
    frame: Sprite
    symbol: Sprite | None      # None: the frame IS the output, as shipped (the back button)
    body: tuple[int, int, int, int]


EDITOR_SPECS = (
    EditorSpec("size_minus.png", FRAME_STONE, SYMBOL_SHRINK, STONE_BODY),
    EditorSpec("size_plus.png", FRAME_STONE, SYMBOL_ENLARGE, STONE_BODY),
    EditorSpec("opacity_minus.png", FRAME_STONE, SYMBOL_TRANSPARENT, STONE_BODY),
    EditorSpec("opacity_plus.png", FRAME_STONE, SYMBOL_OPAQUE, STONE_BODY),
    EditorSpec("lock_locked.png", FRAME_STONE, SYMBOL_LOCKED, STONE_BODY),
    EditorSpec("lock_unlocked.png", FRAME_RED, SYMBOL_UNLOCKED, RED_BODY),
    EditorSpec("restore.png", FRAME_STONE, ICON_RESTART, STONE_BODY),
    EditorSpec("back.png", BACK_BUTTON, None, BACK_BODY),
)
EDITOR_OUTPUT_NAMES = tuple(spec.name for spec in EDITOR_SPECS)

# What --extract copies, verbatim: the controls' sources, the editor stage's (CTL-11: the seven
# tiles' frames and symbols, and Back), and a few spares an owner's choice might want later.
SPRITES = (
    FRAME, DPAD_LEFT, DPAD_RIGHT, ICON_RESTART,
    FRAME_STONE, FRAME_RED, SYMBOL_SHRINK, SYMBOL_ENLARGE, SYMBOL_TRANSPARENT, SYMBOL_OPAQUE,
    SYMBOL_LOCKED, SYMBOL_UNLOCKED, BACK_BUTTON,
    Sprite("sprites/pause-pad-button.png", ROLE_SPARE),
    Sprite("sprites/icon-quit-64.png", ROLE_SPARE),
    Sprite("sprites/hd/portal-duel-icon.png", ROLE_SPARE),
    Sprite("sprites/remove-symbol.png", ROLE_SPARE),
    Sprite("sprites/smaller-purple-frame-bg.png", ROLE_SPARE),
    Sprite("sprites/frame-title-bg.png", ROLE_SPARE),
    Sprite("sprites/rounded-edges-frame.png", ROLE_SPARE),
    Sprite("sprites/inventory-next-button.png", ROLE_SPARE),
)


# --- reading the package ---------------------------------------------------------------------

def sha256_of(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class FolderPackage:
    """A folder holding the base APK's assets/ contents (sprites/, SEF/, ...), read only."""

    def __init__(self, root: pathlib.Path):
        self.root = root.resolve()
        if not (self.root / FRAME.member).is_file():
            raise ToolError(f"--assets: {self.root} has no {FRAME.member}; give the folder holding "
                            "the base APK's assets/ contents")

    def has(self, member: str) -> bool:
        return (self.root / member).is_file()

    def read(self, member: str) -> bytes:
        try:
            return (self.root / member).read_bytes()
        except FileNotFoundError:
            raise ToolError(f"Magic Rampage's assets/{member} is missing from {self.root}; "
                            f"this tool reads version {MR_VERSION}") from None

    def describe(self) -> dict:
        return {"kind": "assets-folder", "expected_version": MR_VERSION}


class XapkPackage:
    """Magic Rampage's .xapk: a zip whose base APK is a zip, whose assets/ holds the sprites."""

    def __init__(self, path: pathlib.Path):
        if not path.is_file():
            raise ToolError(f"--xapk: no file at {path}")
        self.name = path.name
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1 << 20), b""):
                digest.update(chunk)
        self.digest = digest.hexdigest()
        self.version: dict = {}
        try:
            with zipfile.ZipFile(path, "r") as outer:
                self.apk = zipfile.ZipFile(io.BytesIO(outer.read(BASE_APK)), "r")
                if XAPK_MANIFEST in outer.namelist():
                    meta = json.loads(outer.read(XAPK_MANIFEST))
                    self.version = {key: meta[key] for key in ("version_name", "version_code")
                                    if key in meta}
        except zipfile.BadZipFile:
            raise ToolError(f"--xapk: {path} is not a zip archive, which an .xapk is") from None
        except KeyError:
            raise ToolError(f"--xapk: {path} holds no {BASE_APK}; is it Magic Rampage's .xapk?") \
                from None
        self.names = set(self.apk.namelist())

    def has(self, member: str) -> bool:
        return f"assets/{member}" in self.names

    def read(self, member: str) -> bytes:
        if not self.has(member):
            raise ToolError(f"Magic Rampage's assets/{member} is missing from {self.name}; "
                            f"this tool reads version {MR_VERSION}")
        return self.apk.read(f"assets/{member}")

    def describe(self) -> dict:
        return {"kind": "xapk", "file": self.name, "sha256": self.digest,
                "expected_version": MR_VERSION, **self.version}


Package = FolderPackage | XapkPackage


# --- pixels ----------------------------------------------------------------------------------

def straight_rgba(image: Image.Image) -> Image.Image:
    """Palette + tRNS, grey + alpha, or anything else, as straight RGBA with colour under alpha 0
    made black (a filtered, magnified sprite would bleed it in)."""
    rgba = np.array(image.convert("RGBA"))
    rgba[rgba[:, :, 3] == 0, :3] = 0
    return Image.fromarray(rgba)


def decode(data: bytes) -> Image.Image:
    return straight_rgba(Image.open(io.BytesIO(data)))


def load(package: Package, sprite: Sprite) -> Image.Image:
    image = decode(package.read(sprite.member))
    if sprite.size is not None and image.size != sprite.size:
        raise ToolError(f"assets/{sprite.member} is {image.width} x {image.height}, not "
                        f"{sprite.size[0]} x {sprite.size[1]}: not Magic Rampage {MR_VERSION}'s "
                        "art. An as-shipped sprite is never resampled to fit.")
    return image


def has_magenta(array: np.ndarray) -> bool:
    return bool(((array[:, :, 0] == 255) & (array[:, :, 1] == 0) & (array[:, :, 2] == 255)).any())


def body_rect(array: np.ndarray) -> tuple[int, int, int, int]:
    """The solid body: the box of every pixel with alpha >= 250, right and lower edges exclusive.
    (A plain alpha bounding box would include the soft shadow.)"""
    ys, xs = np.nonzero(array[:, :, 3] >= BODY_ALPHA)
    if xs.size == 0:
        return (0, 0, 0, 0)
    return (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)


def disc_dilate(mask: np.ndarray, radius: float) -> np.ndarray:
    """The mask grown by a disc of `radius` px (max over the disc's offsets)."""
    out = mask.copy()
    reach = math.ceil(radius)
    height, width = mask.shape
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            if dx * dx + dy * dy > radius * radius:
                continue
            into = (slice(max(0, dy), height + min(0, dy)), slice(max(0, dx), width + min(0, dx)))
            source = (slice(max(0, -dy), height - max(0, dy)),
                      slice(max(0, -dx), width - max(0, dx)))
            np.maximum(out[into], mask[source], out=out[into])
    return out


def shadow_layer(alpha: np.ndarray) -> np.ndarray:
    """Magic Rampage's soft shadow to the lower right, from a supersampled alpha mask (0..1)."""
    s = SUPERSAMPLE
    shadow = Image.fromarray((alpha * 255).astype(np.uint8))
    shadow = shadow.transform(shadow.size, Image.Transform.AFFINE,
                              (1, 0, -SHADOW_OFFSET[0] * s, 0, 1, -SHADOW_OFFSET[1] * s))
    shadow = shadow.filter(ImageFilter.GaussianBlur(SHADOW_BLUR * s))
    return np.asarray(shadow, np.float32) / 255.0 * SHADOW_STRENGTH


def white_ink(glyph: Image.Image) -> Image.Image:
    """The glyph's own light strokes, as an 8-bit mask: grey x alpha, scaled so the strongest
    pixel is 255. icon-restart-64 is a white arrow with a soft dark shadow baked into its alpha
    (alpha 1..25 over 467 px, grey 0): the plain alpha is 40 x 41 px with the shadow, the arrow
    itself 31 x 33, so scaling and centring on the alpha would size and place the shadow, and
    its pale fringe would muddy the restyle's outline. A glyph with no shadow is unchanged."""
    array = np.asarray(glyph).astype(np.float32)
    ink = array[:, :, :3].mean(axis=2) * array[:, :, 3] / 255.0
    return Image.fromarray(np.rint(ink * 255.0 / max(float(ink.max()), 1.0)).astype(np.uint8))


def mask_from_glyph(glyph: Image.Image, box: float) -> np.ndarray:
    """A supersampled 0..1 mask of a glyph's white ink, cropped to what is visible and scaled so
    that its largest side plus the outline on both sides is `box` output px."""
    ink = white_ink(glyph)
    ink = ink.crop(ink.getbbox())
    scale = (box - 2 * OUTLINE) / max(ink.width, ink.height)
    size = (max(1, round(ink.width * scale * SUPERSAMPLE)),
            max(1, round(ink.height * scale * SUPERSAMPLE)))
    big = ink.resize(size, Image.Resampling.LANCZOS)
    return np.clip(np.asarray(big, np.float32) / 255.0, 0.0, 1.0)


def styled(mask: np.ndarray) -> Image.Image:
    """A glyph mask (supersampled, 0..1) painted as Magic Rampage's pad icons are: shadow, black
    outline, grey fill; straight RGBA. The canvas is a whole, even number of output px with the
    mask in its middle, so centring it on the body (even too) is exact."""
    s = SUPERSAMPLE
    mask_h, mask_w = mask.shape
    pad = math.ceil((OUTLINE + SHADOW_OFFSET[0] + 3 * SHADOW_BLUR) * s)
    unit = 2 * s
    height = -(-(mask_h + 2 * pad) // unit) * unit
    width = -(-(mask_w + 2 * pad) // unit) * unit
    top, left = (height - mask_h) // 2, (width - mask_w) // 2
    canvas = np.zeros((height, width), np.float32)
    canvas[top:top + mask_h, left:left + mask_w] = mask
    outline = disc_dilate(canvas, OUTLINE * s)

    # Painted from the bottom, premultiplied: the shadow, the black outline, the fill.
    rgb = np.zeros((height, width, 3), np.float32)
    alpha = np.zeros((height, width), np.float32)
    for colour, cover in (((0, 0, 0), shadow_layer(outline)), ((0, 0, 0), outline),
                          (ICON_FILL, canvas)):
        cover3 = cover[:, :, None]
        rgb = np.array(colour, np.float32) / 255.0 * cover3 + rgb * (1.0 - cover3)
        alpha = cover + alpha * (1.0 - cover)
    premultiplied = np.clip(np.dstack([rgb, alpha]), 0.0, 1.0)
    layer = Image.frombytes("RGBa", (width, height),
                            (premultiplied * 255 + 0.5).astype(np.uint8).tobytes())
    small = layer.resize((width // s, height // s), Image.Resampling.LANCZOS)
    return small.convert("RGBA")


def paint_mask(width: int, height: int, paint) -> np.ndarray:
    """A supersampled 0..1 mask of width x height output px, painted by paint(draw, s)."""
    s = SUPERSAMPLE
    image = Image.new("L", (width * s, height * s), 0)
    paint(ImageDraw.Draw(image), s)
    return np.asarray(image, np.float32) / 255.0


def pause_mask() -> np.ndarray:
    width = 2 * PAUSE_BAR_W + PAUSE_BAR_GAP

    def paint(draw: ImageDraw.ImageDraw, s: int) -> None:
        for left in (0, PAUSE_BAR_W + PAUSE_BAR_GAP):
            draw.rounded_rectangle([left * s, 0, (left + PAUSE_BAR_W) * s - 1, PAUSE_BAR_H * s - 1],
                                   radius=PAUSE_BAR_RADIUS * s, fill=255)

    return paint_mask(width, PAUSE_BAR_H, paint)


def ring_and_cross_mask() -> np.ndarray:
    """A ring with an X in its middle, the two concentric (not the X at the ring's corner, as the
    original's own clear button has it: this is centred on the body)."""
    size = RING_DIAMETER

    def paint(draw: ImageDraw.ImageDraw, s: int) -> None:
        draw.ellipse([0, 0, size * s - 1, size * s - 1], outline=255, width=RING_STROKE * s)
        centre = (size * s - 1) / 2
        reach = CROSS_REACH * s
        cap = CROSS_STROKE * s / 2
        for sign in (1, -1):
            start = (centre - reach, centre - sign * reach)
            end = (centre + reach, centre + sign * reach)
            draw.line([start, end], fill=255, width=CROSS_STROKE * s)
            for x, y in (start, end):
                draw.ellipse([x - cap, y - cap, x + cap, y + cap], fill=255)

    return paint_mask(size, size, paint)


def place_on_body(frame: Image.Image, icon: Image.Image,
                  body: tuple[int, int, int, int] = BODY_RECT) -> Image.Image:
    """The icon's canvas centred on the body rect (not on the 128 px image: the same today, but
    the body is what the layout measures)."""
    left, top, right, bottom = body
    if (left + right - icon.width) % 2 or (top + bottom - icon.height) % 2:
        raise ToolError(f"a {icon.width} x {icon.height} glyph cannot be centred on the body "
                        f"{body} to a whole pixel")
    x, y = (left + right - icon.width) // 2, (top + bottom - icon.height) // 2
    if x < 0 or y < 0 or x + icon.width > frame.width or y + icon.height > frame.height:
        raise ToolError(f"a {icon.width} x {icon.height} glyph does not fit the frame")
    out = frame.copy()
    out.alpha_composite(icon, (x, y))
    return straight_rgba(out)


# --- the outputs -----------------------------------------------------------------------------

@dataclasses.dataclass
class Output:
    name: str
    recipe: str
    sources: tuple[str, ...]
    image: Image.Image
    body: tuple[int, int, int, int] = BODY_RECT      # the solid body this output must have


def as_shipped(sprite: Image.Image) -> Image.Image:
    """The decoded sprite and nothing else: it is the output, byte for byte in pixels."""
    return sprite


def derive(package: Package) -> list[Output]:
    """All five controls, in memory, each validated; nothing touches disk."""
    frame = load(package, FRAME)
    outputs = [
        Output("walk_left.png", RECIPE_AS_SHIPPED, (DPAD_LEFT.member,),
               as_shipped(load(package, DPAD_LEFT))),
        Output("walk_right.png", RECIPE_AS_SHIPPED, (DPAD_RIGHT.member,),
               as_shipped(load(package, DPAD_RIGHT))),
        Output("restart.png", RECIPE_RESTART, (FRAME.member, ICON_RESTART.member),
               place_on_body(frame, styled(mask_from_glyph(load(package, ICON_RESTART),
                                                          RESTART_BOX)))),
        Output("pause.png", RECIPE_PAUSE, (FRAME.member,),
               place_on_body(frame, styled(pause_mask()))),
        Output("clear_portals.png", RECIPE_CLEAR, (FRAME.member,),
               place_on_body(frame, styled(ring_and_cross_mask()))),
    ]
    for output in outputs:
        validate(output)
    return outputs


def place_centred_on_bodies(frame: Image.Image, frame_body: tuple[int, int, int, int],
                            symbol: Image.Image) -> Image.Image:
    """The symbol laid over the frame 1:1, the middle of its own solid body on the middle of the
    frame's, to a whole pixel (an odd difference rounds down, so the answer is the same on every
    machine). The symbol's canvas must lie wholly inside the frame."""
    symbol_body = body_rect(np.asarray(symbol))
    if symbol_body == (0, 0, 0, 0):
        raise ToolError("a symbol has no solid body (no pixel of alpha >= 250) to centre")
    x = (frame_body[0] + frame_body[2]) // 2 - (symbol_body[0] + symbol_body[2]) // 2
    y = (frame_body[1] + frame_body[3]) // 2 - (symbol_body[1] + symbol_body[3]) // 2
    if x < 0 or y < 0 or x + symbol.width > frame.width or y + symbol.height > frame.height:
        raise ToolError(f"a {symbol.width} x {symbol.height} symbol does not fit the frame at ({x}, {y})")
    out = frame.copy()
    out.alpha_composite(symbol, (x, y))
    return straight_rgba(out)


def derive_editor(package: Package) -> list[Output]:
    """The eight editor pictures, in memory, each validated against its own body; nothing touches disk."""
    outputs = []
    for spec in EDITOR_SPECS:
        frame = load(package, spec.frame)
        if spec.symbol is None:
            outputs.append(Output(spec.name, RECIPE_AS_SHIPPED, (spec.frame.member,),
                                  as_shipped(frame), spec.body))
            continue
        image = place_centred_on_bodies(frame, spec.body, load(package, spec.symbol))
        outputs.append(Output(spec.name, RECIPE_TILE, (spec.frame.member, spec.symbol.member),
                              image, spec.body))
    for output in outputs:
        validate(output)
    return outputs


def margin_px(body: tuple[int, int, int, int]) -> int:
    """The transparent margin the layout reads: the smallest of the four sides of the body."""
    return min(body[0], body[1], CANVAS - body[2], CANVAS - body[3])


def derive_stage(stage: str, package: Package) -> list[Output]:
    return derive(package) if stage == STAGE_CONTROLS else derive_editor(package)


def output_names(stage: str) -> tuple[str, ...]:
    return OUTPUT_NAMES if stage == STAGE_CONTROLS else EDITOR_OUTPUT_NAMES


def default_out(stage: str) -> pathlib.PurePosixPath:
    return DEFAULT_OUT if stage == STAGE_CONTROLS else DEFAULT_EDITOR_OUT


def validate(output: Output) -> None:
    image = output.image
    if image.mode != "RGBA" or image.size != (CANVAS, CANVAS):
        raise ToolError(f"{output.name}: {image.mode} {image.width} x {image.height}, "
                        f"not RGBA {CANVAS} x {CANVAS}")
    array = np.asarray(image)
    if has_magenta(array):
        raise ToolError(f"{output.name} has a pixel of exact magenta (255, 0, 255)")
    if (array[:, :, :3][array[:, :, 3] == 0] != 0).any():
        raise ToolError(f"{output.name} has colour under alpha 0")
    if body_rect(array) != output.body:
        raise ToolError(f"{output.name}: its solid body is {body_rect(array)}, not {output.body}: "
                        f"not Magic Rampage {MR_VERSION}'s art")


def encode_png(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def pixels_sha256(image: Image.Image) -> str:
    return sha256_of(np.asarray(image).tobytes())


def build_manifest(package: Package, outputs: list[Output], encoded: dict[str, bytes],
                   stage: str = STAGE_CONTROLS) -> dict:
    used = sorted({member for output in outputs for member in output.sources})
    manifest = {
        "tool": "tools/art/make_mr_controls.py",
        "tool_version": TOOL_VERSION,
        "pillow": PILLOW_VERSION,
        "stage": stage,
        "package": package.describe(),
        "canvas": [CANVAS, CANVAS],
        "sources": {member: {"sha256": sha256_of(package.read(member)),
                             "bytes": len(package.read(member))} for member in used},
        "outputs": {
            output.name: {
                "sources": list(output.sources),
                "recipe_id": output.recipe,
                "recipe": RECIPES[output.recipe],
                "sha256": sha256_of(encoded[output.name]),
                "pixels_sha256": pixels_sha256(output.image),
                "bytes": len(encoded[output.name]),
                "size": [output.image.width, output.image.height],
                "mode": output.image.mode,
                "body": list(body_rect(np.asarray(output.image))),
            } for output in outputs
        },
    }
    if stage == STAGE_CONTROLS:
        manifest["body_rect"] = list(BODY_RECT)
    else:
        # Every editor picture has a body of its own; the layout's margin is the smallest side of it.
        for output in outputs:
            manifest["outputs"][output.name]["body_margin_px"] = margin_px(output.body)
    return manifest


def dump_json(data: dict) -> bytes:
    return (json.dumps(data, indent=2, sort_keys=True) + "\n").encode("utf-8")


# --- the repository and the hard rule ---------------------------------------------------------

def repo_root(cwd: pathlib.Path) -> pathlib.Path:
    """`git rev-parse --show-toplevel` of the current directory: this file may live anywhere."""
    try:
        done = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=cwd,
                              capture_output=True, text=True, check=False)
    except FileNotFoundError:
        raise ToolError("git is not on PATH, and this tool will not write without asking it "
                        "whether the destination is ignored") from None
    if done.returncode != 0 or not done.stdout.strip():
        raise ToolError(f"{cwd} is not inside a git repository: run this from the repository "
                        "whose reference/ folder is to be filled")
    return pathlib.Path(done.stdout.strip()).resolve()


def refuse_unignored(root: pathlib.Path, targets: list[pathlib.Path]) -> None:
    """THE HARD RULE as a gate: every file about to be written must be reported ignored by git
    (a path that is tracked, or outside the repository, is not). Nothing is written before this."""
    relative = []
    for target in targets:
        try:
            relative.append(target.resolve().relative_to(root).as_posix())
        except ValueError:
            raise ToolError(f"refusing to write {target}: it is outside the repository at {root}, "
                            "so git cannot say it is ignored") from None
    done = subprocess.run(["git", "check-ignore", "--stdin", "-z"], cwd=root, check=False,
                          input="\0".join(relative) + "\0", capture_output=True, text=True)
    if done.returncode not in (0, 1):
        raise ToolError(f"git check-ignore failed ({done.returncode}): {done.stderr.strip()}")
    ignored = {path for path in done.stdout.split("\0") if path}
    missing = [path for path in relative if path not in ignored]
    if missing:
        more = f", and {len(missing) - 3} more" if len(missing) > 3 else ""
        raise ToolError(f"refusing to write: git does not report {', '.join(missing[:3])}{more} as "
                        "ignored. Magic Rampage's art must never reach a tracked path (CLAUDE.md, "
                        f"THE HARD RULE); the default, {DEFAULT_OUT}, is ignored")


# --- extraction ------------------------------------------------------------------------------

def extract(package: Package, dest: pathlib.Path) -> dict:
    """Copy the sprites this tool and the editor stage read, byte for byte, under dest/<member>
    (so dest is itself a --assets folder), and say what was copied in dest/extract.json."""
    files: dict[str, dict] = {}
    skipped: list[str] = []
    for sprite in SPRITES:
        if not package.has(sprite.member):
            if sprite.role == ROLE_SPARE:
                skipped.append(sprite.member)
                continue
            raise ToolError(f"Magic Rampage's assets/{sprite.member} is missing; "
                            f"this tool reads version {MR_VERSION}")
        data = package.read(sprite.member)
        path = dest / sprite.member
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        files[sprite.member] = {"sha256": sha256_of(data), "bytes": len(data), "role": sprite.role}
    record = {"tool_version": TOOL_VERSION, "package": package.describe(), "files": files,
              "missing_spares": skipped}
    (dest / EXTRACT_MANIFEST_NAME).write_bytes(dump_json(record))
    return record


def extract_targets(dest: pathlib.Path) -> list[pathlib.Path]:
    return [dest / sprite.member for sprite in SPRITES] + [dest / EXTRACT_MANIFEST_NAME]


# --- --check ---------------------------------------------------------------------------------

def check(out: pathlib.Path, package: Package, stage: str = STAGE_CONTROLS) -> int:
    """Re-derive in memory and compare with the files and the manifest. Writes nothing."""
    manifest_path = out / MANIFEST_NAME
    if not manifest_path.is_file():
        print(f"FAIL no {manifest_path}: run the tool first")
        return EXIT_FAILED
    manifest = json.loads(manifest_path.read_text("utf-8"))
    outputs = derive_stage(stage, package)
    problems: list[str] = []
    if manifest.get("tool_version") != TOOL_VERSION:
        problems.append(f"manifest tool_version is {manifest.get('tool_version')}, "
                        f"the tool's is {TOOL_VERSION}")
    if manifest.get("stage", STAGE_CONTROLS) != stage:
        problems.append(f"the manifest is the {manifest.get('stage')} stage's, not the {stage} stage's")
    for output in outputs:
        recorded_body = manifest.get("outputs", {}).get(output.name, {}).get("body")
        if recorded_body is not None and list(output.body) != list(recorded_body):
            problems.append(f"{output.name}: the manifest says its body is {recorded_body}, the tool "
                            f"expects {list(output.body)}")
    recorded = manifest.get("outputs", {})
    if set(recorded) != {output.name for output in outputs}:
        problems.append(f"the manifest lists {sorted(recorded)}, the tool makes "
                        f"{sorted(output.name for output in outputs)}")
    for member, entry in manifest.get("sources", {}).items():
        now = sha256_of(package.read(member))
        if now != entry.get("sha256"):
            problems.append(f"source {member} is not the one the outputs were made from")
    hint = ""
    if manifest.get("pillow") != PILLOW_VERSION:
        hint = f" (made with Pillow {manifest.get('pillow')}, this is {PILLOW_VERSION})"
    for output in outputs:
        entry = recorded.get(output.name, {})
        path = out / output.name
        mine = []
        if not path.is_file():
            mine.append("file missing")
        else:
            on_disk = path.read_bytes()
            if sha256_of(on_disk) != entry.get("sha256"):
                mine.append("file bytes differ from the manifest's sha256")
            if not np.array_equal(np.asarray(Image.open(io.BytesIO(on_disk)).convert("RGBA")),
                                  np.asarray(output.image)):
                mine.append("pixels differ from a fresh derivation" + hint)
        if pixels_sha256(output.image) != entry.get("pixels_sha256"):
            mine.append("a fresh derivation differs from the manifest's pixels_sha256" + hint)
        print(f"  {output.name:18s} {'ok' if not mine else 'FAIL ' + '; '.join(mine)}")
        problems.extend(f"{output.name}: {line}" for line in mine)
    print("check: every output reproduces" if not problems
          else f"check: {len(problems)} problem(s)")
    for line in problems:
        print(f"  - {line}", file=sys.stderr)
    return EXIT_OK if not problems else EXIT_FAILED


# --- the run ---------------------------------------------------------------------------------

def open_package(args: argparse.Namespace, root: pathlib.Path) -> Package:
    if args.xapk:
        return XapkPackage(pathlib.Path(args.xapk).resolve())
    if args.assets:
        return FolderPackage(pathlib.Path(args.assets))
    default = root / DEFAULT_ASSETS
    if (default / FRAME.member).is_file():
        return FolderPackage(default)
    raise ToolError("no package: give --assets <folder holding the base APK's assets/> or --xapk "
                    f"<Magic+Rampage_{MR_VERSION}_APKPure.xapk>; the default, "
                    f"{DEFAULT_ASSETS}, does not exist yet (the first run with a package fills it)")


def write_stage(stage: str, out: pathlib.Path, package: Package, outputs: list[Output]) -> None:
    """One stage's images and its manifest, from outputs already derived and validated."""
    encoded = {output.name: encode_png(output.image) for output in outputs}
    manifest = build_manifest(package, outputs, encoded, stage)
    out.mkdir(parents=True, exist_ok=True)
    for output in outputs:
        (out / output.name).write_bytes(encoded[output.name])
        entry = manifest["outputs"][output.name]
        print(f"  {output.name:18s} {output.image.width}x{output.image.height} RGBA  "
              f"{entry['bytes']:5d} bytes  sha256 {entry['sha256'][:16]}  "
              f"body {tuple(entry['body'])}")
    (out / MANIFEST_NAME).write_bytes(dump_json(manifest))   # last: its presence says it finished
    print(f"wrote {len(outputs)} images and {MANIFEST_NAME} into {out}")


def run(args: argparse.Namespace) -> int:
    stages = STAGES if args.stage == STAGE_ALL else (args.stage,)
    if args.out and len(stages) > 1:
        raise UsageError("--out names one folder, and --stage all writes two: give --stage controls or "
                         f"--stage editor with it (the defaults are {DEFAULT_OUT} and {DEFAULT_EDITOR_OUT})")
    root = repo_root(pathlib.Path.cwd())
    outs = {stage: (pathlib.Path(args.out).resolve() if args.out else root / default_out(stage))
            for stage in stages}
    if args.check:
        package = open_package(args, root)
        return max(check(outs[stage], package, stage) for stage in stages)

    # The sprites always go to the one fixed folder, whatever --out is: a stray --out must not
    # scatter Magic Rampage's art into out/ (the converter's output) or reference/extracted/
    # (the port's _ORIGINAL), and the extract has a single place to look for.
    assets_dest = root / DEFAULT_ASSETS
    targets = [outs[stage] / name for stage in stages for name in (*output_names(stage), MANIFEST_NAME)]
    # The destination is checked before the package is opened, and before anything is built.
    refuse_unignored(root, targets + (extract_targets(assets_dest) if args.extract else []))
    package = open_package(args, root)
    # Every stage is derived and validated before anything is written: a stage that fails leaves
    # nothing of the other, and no sprite, written.
    derived = {stage: derive_stage(stage, package) for stage in stages}

    copy_sprites = args.extract and not (isinstance(package, FolderPackage)
                                         and package.root == assets_dest.resolve())
    if copy_sprites:
        record = extract(package, assets_dest)
        print(f"extracted {len(record['files'])} sprites, verbatim, into {assets_dest}"
              + (f" ({len(record['missing_spares'])} spare(s) not in the package)"
                 if record["missing_spares"] else ""))
    elif args.extract:
        print(f"the package is {assets_dest} itself: nothing to extract")
    for stage in stages:
        write_stage(stage, outs[stage], package, derived[stage])
    return EXIT_OK


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Magic Rampage's control buttons as derived art for the modern controls "
                    "(gitignored output only).",
        epilog="Run from inside the repository; the module's docstring has the rest.")
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--assets",
                       help="a folder holding the base APK's assets/ contents (read only)")
    where.add_argument("--xapk", help=f"Magic Rampage {MR_VERSION}'s .xapk (read only)")
    parser.add_argument("--stage", choices=(STAGE_CONTROLS, STAGE_EDITOR, STAGE_ALL),
                        default=STAGE_CONTROLS,
                        help="controls (the five buttons, default), editor (the adjustment screen's "
                             "eight pictures, CTL-11) or all (both, each into its own default folder)")
    parser.add_argument("--out", help=f"where to write the images and the manifest of ONE stage; "
                                      f"default {DEFAULT_OUT} (controls) or {DEFAULT_EDITOR_OUT} "
                                      "(editor) under the repository root. Refused unless "
                                      "git reports it ignored. Whatever it is, the sprites are "
                                      f"extracted into {DEFAULT_ASSETS} (also checked), never "
                                      "beside it.")
    parser.add_argument("--extract", action=argparse.BooleanOptionalAction, default=True,
                        help="copy the sprites the tool and the editor stage read, verbatim, "
                             "(default on; --no-extract to skip)")
    parser.add_argument("--check", action="store_true",
                        help="re-derive in memory and compare with the files and the manifest; "
                             "writes nothing")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return run(args)
    except UsageError as note:
        print(f"make_mr_controls: {note}", file=sys.stderr)
        return EXIT_USAGE
    except ToolError as error:
        print(f"make_mr_controls: {error}", file=sys.stderr)
        return EXIT_FAILED


if __name__ == "__main__":
    sys.exit(main())
