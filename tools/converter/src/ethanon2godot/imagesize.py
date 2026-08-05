"""Read image dimensions from file headers, without decoding the image.

Needed for exactly one thing: ``type=2`` (``ET_VERTICAL``) entities anchor at
``(size.x / 2, size.y)`` rather than the centre (``ETHEntity.cpp:270-297``), so placing
one requires knowing how tall its sprite is. Every other entity type anchors at the centre
and cancels out, which is why the converter got this far without reading a single pixel.

Header parsing rather than Pillow, because the converter has **zero runtime dependencies
by design** (`pyproject.toml`) and adding one needs a written reason. Reading 24 bytes is
not a reason.
"""

from __future__ import annotations

import struct
from pathlib import Path

_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def read_size(path: Path) -> tuple[int, int] | None:
    """Return ``(width, height)`` in pixels, or ``None`` if unreadable.

    ``None`` is a normal outcome, not an error: the caller falls back to skipping the
    entity and reporting it, which is better than placing it half a sprite off.
    """
    try:
        with open(path, "rb") as handle:
            head = handle.read(32)
    except OSError:
        return None

    if head.startswith(_PNG_SIGNATURE):
        # IHDR is mandated to be the first chunk, so width/height sit at a fixed offset:
        # 8 signature + 4 length + 4 type = 16.
        if len(head) < 24:
            return None
        width, height = struct.unpack(">II", head[16:24])
        return (width, height)

    if head[:2] == b"BM":
        # BITMAPINFOHEADER. Height is SIGNED — negative means a top-down bitmap, and the
        # magnitude is still the pixel height.
        if len(head) < 26:
            return None
        width, height = struct.unpack("<ii", head[18:26])
        return (abs(width), abs(height))

    # .pkm / .pvr (the compressed Android/iOS variants) are deliberately not handled: the
    # converter reads the .png tier, and every sprite ships one.
    return None
