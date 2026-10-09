#!/usr/bin/env python3
"""Assemble a GIF (or MP4) from a directory of PPM frames.

The runner writes lossless PPM frames; this turns them into the committed
gallery animation. A single shared palette keeps GIFs small for the repository.
"""
from __future__ import annotations

import argparse
import glob
import os
import sys

from PIL import Image


def build_gif(input_dir: str, output: str, fps: float = 20.0, every: int = 1,
              scale: float = 1.0, loop: int = 0, colors: int = 256) -> str:
    files = sorted(glob.glob(os.path.join(input_dir, "frame_*.ppm")))
    if not files:
        raise SystemExit(f"no frames found in {input_dir}")

    duration = max(20, int(round(1000.0 / fps)))
    frames = []
    for i, path in enumerate(files):
        if i % every:
            continue
        im = Image.open(path).convert("RGB")
        if scale != 1.0:
            w, h = im.size
            im = im.resize((max(1, int(w * scale)), max(1, int(h * scale))),
                           Image.LANCZOS)
        frames.append(im)
    if not frames:
        raise SystemExit("frame stride removed every frame")

    # Quantize to one shared adaptive palette so the GIF stays compact. Dithering
    # is deliberately off: it looks fine here and keeps file size down.
    palette = frames[len(frames) // 2].quantize(colors=colors, method=Image.MEDIANCUT)
    quantized = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]

    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    quantized[0].save(output, save_all=True, append_images=quantized[1:],
                      duration=duration, loop=loop, optimize=True, disposal=2)
    return output


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", help="directory containing frame_*.ppm")
    ap.add_argument("output", help="output .gif path")
    ap.add_argument("--fps", type=float, default=20.0)
    ap.add_argument("--every", type=int, default=1, help="use every Nth frame")
    ap.add_argument("--scale", type=float, default=1.0)
    ap.add_argument("--loop", type=int, default=0)
    ap.add_argument("--colors", type=int, default=256)
    args = ap.parse_args()
    out = build_gif(args.input, args.output, args.fps, args.every, args.scale, args.loop,
                    args.colors)
    size = os.path.getsize(out) // 1024
    print(f"wrote {out} ({size} KiB)")


if __name__ == "__main__":
    sys.exit(main())
