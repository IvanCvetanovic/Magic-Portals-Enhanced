"""Find where (and at what scale) a sprite is drawn in a captured frame.

    tools/parity/.venv/Scripts/python tools/parity/locate.py <frame.png> <sprite.png> [--scales 1.40625,2.8125] [--top 3] [--region x0,y0,x1,y1]

Prints the best matches as JSON: centre in screen pixels, top-left, drawn size, scale and
normalised correlation score. Alpha is honoured: transparent sprite pixels are excluded
from the match (TM_CCORR_NORMED with a mask), so a round button over any background works.

Why: positions and sizes in the original must be MEASURED (MILESTONE_1 §3). Reading them off
a screenshot by eye is how 180 px becomes "about 170".

Scale convention: 1x art drawn at world scale in a 720 px frame is 2.8125 (720/256); `hd/` art
is twice the resolution, so the same on-screen size is scale 1.40625.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np


def load_sprite(path: Path) -> tuple[np.ndarray, np.ndarray | None]:
    raw = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if raw is None:
        raise SystemExit(f"cannot read {path}")
    if raw.ndim == 2:
        return cv2.cvtColor(raw, cv2.COLOR_GRAY2BGR), None
    if raw.shape[2] == 4:
        return raw[:, :, :3], raw[:, :, 3]
    return raw, None


def match(frame: np.ndarray, sprite: np.ndarray, mask: np.ndarray | None, scale: float, top: int) -> list[dict]:
    w = max(2, round(sprite.shape[1] * scale))
    h = max(2, round(sprite.shape[0] * scale))
    if w >= frame.shape[1] or h >= frame.shape[0]:
        return []
    tpl = cv2.resize(sprite, (w, h), interpolation=cv2.INTER_LINEAR)
    m = None
    if mask is not None:
        m = cv2.resize(mask, (w, h), interpolation=cv2.INTER_LINEAR)
        m = cv2.merge([m, m, m])
        if m.max() == 0:
            return []
    result = cv2.matchTemplate(frame, tpl, cv2.TM_CCOEFF_NORMED if m is None else cv2.TM_CCORR_NORMED, mask=m)
    result = np.nan_to_num(result, nan=-1.0, posinf=-1.0, neginf=-1.0)
    found: list[dict] = []
    work = result.copy()
    for _ in range(top):
        _, score, _, (x, y) = cv2.minMaxLoc(work)
        found.append({
            "center": [round(x + w / 2, 1), round(y + h / 2, 1)],
            "top_left": [int(x), int(y)],
            "size": [w, h],
            "scale": scale,
            "score": round(float(score), 4),
        })
        # Suppress this peak so the next iteration finds a different placement.
        cv2.rectangle(work, (max(0, x - w // 2), max(0, y - h // 2)), (x + w // 2, y + h // 2), -1.0, -1)
    return found


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("frame", type=Path)
    parser.add_argument("sprite", type=Path)
    parser.add_argument("--scales", default="1.40625,2.8125")
    parser.add_argument("--sweep", default="", help="lo,hi,step — search a range of scales instead")
    parser.add_argument("--top", type=int, default=3)
    parser.add_argument("--region", default="", help="x0,y0,x1,y1 — only search inside this box")
    args = parser.parse_args()

    frame = cv2.imread(str(args.frame), cv2.IMREAD_COLOR)
    ox = oy = 0
    if args.region:
        x0, y0, x1, y1 = (int(v) for v in args.region.split(","))
        frame, ox, oy = frame[y0:y1, x0:x1], x0, y0
    sprite, mask = load_sprite(args.sprite)

    if args.sweep:
        lo, hi, step = (float(v) for v in args.sweep.split(","))
        scales = list(np.arange(lo, hi + step / 2, step))
    else:
        scales = [float(s) for s in args.scales.split(",")]

    results: list[dict] = []
    for scale in scales:
        results.extend(match(frame, sprite, mask, round(float(scale), 5), args.top))
    results.sort(key=lambda r: -r["score"])
    for r in results:
        r["center"] = [r["center"][0] + ox, r["center"][1] + oy]
        r["top_left"] = [r["top_left"][0] + ox, r["top_left"][1] + oy]
    print(json.dumps(results[: args.top], indent=1))


if __name__ == "__main__":
    main()
