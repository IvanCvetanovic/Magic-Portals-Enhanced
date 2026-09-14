"""Compare one original frame with one remake frame.

    tools/parity/.venv/Scripts/python tools/parity/compare.py <original.png> <remake.png> <out_prefix>

Writes `<out_prefix>_pair.png` (original above remake, labelled), `<out_prefix>_overlay.png`
(50/50 blend at full resolution) and prints numbers:

  offset_px   global translation of the remake relative to the original, by phase
              correlation over edge maps — a camera or layout shift shows up here first
  edge_iou    overlap of the two edge maps after nothing is shifted, 0..1
  mean_abs    mean absolute grey-level difference, 0..255

The numbers are for ranking and regression, not for proof: lighting, particles and
animation phase all move them. Look at the pair image before believing a score.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

SIZE = (1280, 720)


def load(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGB")
    return image if image.size == SIZE else image.resize(SIZE, Image.Resampling.BILINEAR)


def edges(grey: np.ndarray) -> np.ndarray:
    gy, gx = np.gradient(grey)
    magnitude = np.hypot(gx, gy)
    return magnitude > np.percentile(magnitude, 90)


def phase_offset(a: np.ndarray, b: np.ndarray) -> tuple[int, int]:
    """Translation (dx, dy) that moves `b` onto `a`."""
    fa, fb = np.fft.fft2(a), np.fft.fft2(b)
    cross = fa * np.conj(fb)
    cross /= np.abs(cross) + 1e-9
    corr = np.abs(np.fft.ifft2(cross))
    dy, dx = np.unravel_index(int(np.argmax(corr)), corr.shape)
    h, w = a.shape
    return (int(dx - w) if dx > w // 2 else int(dx), int(dy - h) if dy > h // 2 else int(dy))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("original", type=Path)
    parser.add_argument("remake", type=Path)
    parser.add_argument("out_prefix", type=Path)
    args = parser.parse_args()

    original, remake = load(args.original), load(args.remake)
    grey_o = np.asarray(original.convert("L"), dtype=np.float32)
    grey_r = np.asarray(remake.convert("L"), dtype=np.float32)
    edge_o, edge_r = edges(grey_o), edges(grey_r)

    dx, dy = phase_offset(edge_o.astype(np.float32), edge_r.astype(np.float32))
    union = np.logical_or(edge_o, edge_r).sum()
    metrics = {
        "offset_px": [dx, dy],
        "edge_iou": round(float(np.logical_and(edge_o, edge_r).sum() / max(union, 1)), 4),
        "mean_abs": round(float(np.abs(grey_o - grey_r).mean()), 2),
    }

    args.out_prefix.parent.mkdir(parents=True, exist_ok=True)
    half = (SIZE[0] // 2, SIZE[1] // 2)
    pair = Image.new("RGB", (half[0], half[1] * 2), "black")
    pair.paste(original.resize(half), (0, 0))
    pair.paste(remake.resize(half), (0, half[1]))
    draw = ImageDraw.Draw(pair)
    for label, y in (("ORIGINAL", 0), ("REMAKE", half[1])):
        draw.rectangle((0, y, 80, y + 16), fill="black")
        draw.text((4, y + 2), label, fill="yellow")
    pair.save(args.out_prefix.with_name(args.out_prefix.name + "_pair.png"))
    Image.blend(original, remake, 0.5).save(args.out_prefix.with_name(args.out_prefix.name + "_overlay.png"))
    print(json.dumps(metrics))


if __name__ == "__main__":
    main()
