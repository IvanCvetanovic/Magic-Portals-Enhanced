"""Turn a screen recording into frames and timestamped contact sheets.

    tools/parity/.venv/Scripts/python tools/parity/frames.py <video.mp4> [--fps 4] [--cols 4]

Writes `<video>_frames/NNNN_<ms>ms.png` and `<video>_sheetK.png`. The sheets exist so a whole
sequence can be inspected in one image; the individual frames are for measurement.

Frame timestamps come from ffmpeg's `fps` filter over the container clock, so "t" is time
since the recording started — not since the tap that triggered whatever is being filmed.
"""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

import imageio_ffmpeg
from PIL import Image, ImageDraw


def extract(video: Path, fps: float) -> list[tuple[float, Path]]:
    out_dir = video.with_name(video.stem + "_frames")
    out_dir.mkdir(parents=True, exist_ok=True)
    for old in out_dir.glob("*.png"):
        old.unlink()
    pattern = out_dir / "%04d.png"
    subprocess.run(
        [imageio_ffmpeg.get_ffmpeg_exe(), "-v", "error", "-i", str(video),
         "-vf", f"fps={fps}", str(pattern)],
        check=True,
    )
    frames: list[tuple[float, Path]] = []
    for i, path in enumerate(sorted(out_dir.glob("[0-9][0-9][0-9][0-9].png"))):
        t = i / fps
        named = path.with_name(f"{i:04d}_{int(t * 1000):06d}ms.png")
        path.rename(named)
        frames.append((t, named))
    return frames


def sheets(video: Path, frames: list[tuple[float, Path]], cols: int, rows: int) -> list[Path]:
    thumb_w, thumb_h = 480, 270
    per_sheet = cols * rows
    written: list[Path] = []
    for s in range(0, len(frames), per_sheet):
        batch = frames[s : s + per_sheet]
        sheet = Image.new("RGB", (cols * thumb_w, rows * thumb_h), "black")
        draw = ImageDraw.Draw(sheet)
        for i, (t, path) in enumerate(batch):
            x, y = (i % cols) * thumb_w, (i // cols) * thumb_h
            sheet.paste(Image.open(path).convert("RGB").resize((thumb_w, thumb_h)), (x, y))
            draw.rectangle((x, y, x + 86, y + 18), fill="black")
            draw.text((x + 4, y + 3), f"t={t:5.2f}s", fill="yellow")
        target = video.with_name(f"{video.stem}_sheet{s // per_sheet}.png")
        sheet.save(target)
        written.append(target)
    return written


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("video", type=Path)
    parser.add_argument("--fps", type=float, default=4.0)
    parser.add_argument("--cols", type=int, default=4)
    parser.add_argument("--rows", type=int, default=4)
    args = parser.parse_args()
    frames = extract(args.video, args.fps)
    for path in sheets(args.video, frames, args.cols, args.rows):
        print(path)
    print(f"{len(frames)} frames")


if __name__ == "__main__":
    main()
