# TRUST.md — the reproducibility contract

This file states what the zoo guarantees, why you can rely on it, and how to
check that a claim is true. If an exhibit breaks one of these rules, it is a
bug.

## The promise

**A run is fully determined by `(exhibit, seed, parameters, frame count, image
size, fps)`.** Nothing else influences the output. There is no wall-clock time,
no hidden global state, no randomness outside the seeded generator, no
network, and no dependence on how fast the machine is.

Run the same command twice and you get byte-identical PPM frames. Run it with a
different seed and you get a different, equally valid run.

## Why it holds

The runtime enforces three rules, and every exhibit follows them (see
[`EXHIBIT.md`](EXHIBIT.md)):

1. **One seeded generator.** All randomness comes from a single `Rng`
   (splitmix64 seeding + xoshiro256\*\*) created from the run seed. No exhibit
   calls `rand()`, `random_device`, or reads the clock to make decisions.
2. **Fixed timestep.** The runner advances every exhibit with a constant
   `dt = 1/fps`. An exhibit's `step(dt)` is pure: it only depends on its own
   state and `dt`. Field exhibits that run several iterations per call derive
   the iteration count from `dt`, so they are equally reproducible.
3. **Separated rendering.** `render()` reads state and draws; it never mutates
   simulation state. Rendering cannot feed back into physics.

The frame ordering is also fixed: frame *N* is rendered, *then* the world is
stepped. Frame 0 is always the initial condition.

## How to verify

Determinism is a property you can test in one command:

```sh
./build/zoo run nbody --seed 7 --frames 25 --out /tmp/a
./build/zoo run nbody --seed 7 --frames 25 --out /tmp/b
diff -r /tmp/a /tmp/b && echo "identical"
```

Seed sensitivity (different seeds should diverge):

```sh
./build/zoo run nbody --seed 8 --frames 25 --out /tmp/c
diff -r /tmp/a /tmp/c >/dev/null || echo "seed matters"
```

The gallery is reproducible too: the exact seed, frame count, size, palette and
scale for every committed GIF live in [`tools/run_all.py`](tools/run_all.py).
Re-running it regenerates the same animations.

## Provenance

Every run is self-describing. The runner prints the full configuration it used:

```
render nbody  seed=3 frames=420 fps=60 size=640x640 family=particle
```

`zoo params <exhibit>` prints every tunable with its default. A committed GIF is
therefore tied to a specific `(seed, params)` pair recorded in source control —
not to whatever the author happened to see on screen.

## What is *not* promised

- **Bit-identical output across different CPUs/compilers is not guaranteed.**
  Floating-point results can differ in the last bits under different
  optimization or architecture. Within one build, runs are identical. Across
  builds, visuals are identical to the eye but the frame bytes may differ in
  low bits.
- **No real-time guarantees.** Offline rendering is as-fast-as-possible; the
  fixed timestep exists for determinism, not for pacing.
- **Exhibits are finite and bounded.** Sizes are chosen to fit modest
  hardware; the zoo does not stream or page data.

## Safe by construction

- **Writes only under `--out`** (default `out/<exhibit>`), which defaults to a
  gitignored directory.
- **No network access** anywhere in the codebase.
- **No `system()` / shell execution** from the C++ runner.
- The Python tools only read `frame_*.ppm` and write `.gif`.

## Auditing a single exhibit

To decide whether you trust one animal, read its `.cpp` top to bottom and check:

- [ ] it draws all randomness from the supplied `Rng`.
- [ ] `step()` touches only its own members and `dt` — no I/O, no globals.
- [ ] `render()` is `const` and only reads state.
- [ ] all parameters appear in its `SimInfo::defaults`.
