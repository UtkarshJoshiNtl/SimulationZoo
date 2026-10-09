#!/usr/bin/env python3
"""Render every exhibit and rebuild the committed gallery GIFs.

Presets live here so the repository's showcase is reproducible from one command:

    python3 tools/run_all.py

Run it from the repository root. It expects the compiled runner at build/zoo
(override with --zoo). Raw frames go to out/, finished GIFs to gallery/.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_gif import build_gif  # noqa: E402

# Per-exhibit presentation presets. `every` drops frames to keep GIFs small;
# effective GIF fps is render_fps / every, and `colors` bounds the palette.
PRESETS = {
    "nbody": dict(seed=3, frames=420, width=640, height=640, fps=60, every=6, scale=0.38, colors=64),
    "grayscott": dict(seed=2, frames=360, width=640, height=640, fps=60, every=6, scale=0.38, colors=64),
    "boids": dict(seed=5, frames=360, width=640, height=640, fps=60, every=6, scale=0.38, colors=64),
}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--zoo", default="build/zoo")
    ap.add_argument("--only", default=None, help="render a single exhibit by name")
    args = ap.parse_args()

    os.makedirs("out", exist_ok=True)
    os.makedirs("gallery", exist_ok=True)

    targets = [args.only] if args.only else list(PRESETS)
    for name in targets:
        if name not in PRESETS:
            raise SystemExit(f"no preset for exhibit '{name}'")
        p = PRESETS[name]
        frames_dir = os.path.join("out", name)
        gif_path = os.path.join("gallery", f"{name}.gif")

        cmd = [args.zoo, "run", name,
               "--seed", str(p["seed"]),
               "--frames", str(p["frames"]),
               "--width", str(p["width"]),
               "--height", str(p["height"]),
               "--fps", str(p["fps"]),
               "--out", frames_dir]
        print("$", " ".join(cmd))
        subprocess.run(cmd, check=True)

        build_gif(frames_dir, gif_path, fps=p["fps"] / p["every"],
                  every=p["every"], scale=p["scale"], colors=p["colors"])
        print(f"  -> {gif_path}")


if __name__ == "__main__":
    main()
