# PLAN.md — where the zoo goes next

This is the roadmap. Nothing here is implemented yet; the shipped program is
exactly what [`README.md`](README.md), [`DESIGN.md`](DESIGN.md) and
[`EXHIBIT.md`](EXHIBIT.md) describe. This file is the place for ambition, kept
separate so the documentation never overpromises.

## Principles for anything new

- It must not break the [reproducibility contract](TRUST.md). Every feature
  keeps seeds and fixed timesteps sacred.
- It must not force every exhibit to change. New capability is opt-in and lives
  behind the same tiny [`Simulation`](core/sim.hpp) interface.
- Offline, deterministic rendering stays the default. Interactivity is an
  addition, never a replacement.

## 1. A live interactive viewer (next)

**What.** A window that runs an exhibit in real time: pan/zoom, pause/step,
scrub seeds, and tweak parameters while it runs. Same exhibits, same physics —
just a different front end from `zoo run`.

**Why it is deferred.** Offline rendering already produces the gallery and
proves the physics. Interactivity is the single biggest usability win for
*exploring* the zoo, so it is the first thing on the list — but it needs a
window, an event loop and a GPU/GL context, which is real platform surface the
current headless core deliberately avoids.

**How it fits.** [`core/clock.hpp`](core/clock.hpp) already provides the
`FixedStep` accumulator with a spiral-of-death guard; the viewer would drive
`step(dt)` through it and render at display rate with interpolation between the
last two states. The exhibits do not change.

**Options.** GLFW + OpenGL (portable, GPU-accelerated, matches the existing
`Framebuffer`); SDL (also gives input/audio); or a terminal renderer for
zero-dependency use. Likely a `zoo view <exhibit>` subcommand plus an optional
build target, so the headless binary stays dependency-free.

## 2. A sandbox mode

**What.** A control panel over the live viewer: every parameter in
`info.defaults` becomes a slider, plus seed hotkeys and a "record this to GIF"
button.

**Why it fits.** The registry already knows every exhibit's parameters at
runtime; the CLI already parses `--param k=v`. A sandbox is mostly presentation
over that existing metadata.

**Nice-to-haves.** Hot-reload of exhibit code for rapid iteration; named preset
files (reintroducing a `params/` directory with a small loader) so a good seed
can be saved and shared.

## 3. Effects and richer rendering

**What.** Post-processing over the resolved image: bloom, chromatic aberration,
vignette, film grain; optional depth-of-field and a 2.5D projection for
particles; direct recording to MP4 (via ffmpeg, which the tooling already
assumes).

**Why it fits.** The `Framebuffer` is the natural seam. Post effects are passes
over the float buffer before `resolve`. `RenderStyle` can grow new variants
(e.g. `Additive + bloom`) without touching exhibits, which keep splatting light
as they do today.

**Constraint.** Any randomness in post (grain, dithering) must draw from the run
seed so recordings stay reproducible.

## 4. More exhibits

The three families exist to prove the interface. Candidate animals, roughly by
effort:

- **Easy / field & particle:** Game of Life (with a colormap), Ising
  magnetization, 1D/2D wave equation, n-body binaries, a Julia/Mandelbrot
  explorer, Lorenz attractor point cloud.
- **Medium / agent & grid:** slime mold (Physarum) trails, traffic (Nagel–
  Schreckenberg), ant colony, sandpile (Abelian), Lenia (continuous life).
- **Ambitious:** SPH fluid, cloth/soft bodies, SPH/particle galaxy with
  collisions, a small stable-fluids solver.

Each is one file plus a gallery preset, per [`EXHIBIT.md`](EXHIBIT.md).

## 5. Performance layer (only if needed)

Offline rendering is fast enough for every current exhibit; this section exists
so the work has a home, not because it is scheduled.

- **SIMD:** the host is AVX2+FMA. Hottest kernels (N-body accelerations,
  Gray-Scott stencils) vectorize by hand or trust `-O3` auto-vectorization
  first.
- **OpenMP:** the N-body O(N²) loop and field iterations parallelize cleanly.
  Must not introduce nondeterminism — reduce in a fixed order, or accept that
  pairwise forces make results order-independent.
- **GPU (CUDA):** an RTX-class GPU is present. A CUDA backend behind the same
  `Simulation` contract could run large fields and N-body counts. Determinism
  across CPU/GPU is *not* promised (see [`TRUST.md`](TRUST.md)); a GPU run would
  be its own reproducible-within-itself path.
- **Rules:** an optimization lands only if it is off by default or preserves the
  exact output of the reference implementation.

## 6. Web / WASM (possible, later)

**What.** A browser build of the viewer (Emscripten) so exhibits run in a page
with no install.

**Why it is last, and why the stack is C++ anyway.** The heavy, interesting code
— the simulations — is portable C++ with no platform dependencies, which is
exactly what compiles to WASM cleanly. A web viewer is therefore a *front end
question*, not a rewrite, and it can come after the native viewer exists. The
zoo does not start on the web because the goal is a serious, reproducible,
locally-runnable artifact, not a demo that depends on a toolchain and a hosting
story.

## Why this stack at all

- **C++ core.** The exhibits are numerical and performance-sensitive at scale;
  C++ gives direct control of memory layout and the option to SIMD/parallelize
  later without rewriting. It compiles to a dependency-free headless binary on
  Linux, macOS and Windows.
- **Python tooling.** GIF assembly, gallery presets and future plotting are
  glue work where Python's ecosystem (Pillow, numpy, imageio, ffmpeg) is the
  right tool. Tools never touch simulation state; they only read rendered PPM
  frames.
- **Offline, deterministic rendering.** Committing reproducible GIFs makes the
  gallery verifiable and the physics auditable, which is the whole point of the
  [trust model](TRUST.md). A live viewer is desirable — and planned — but it
  cannot replace reproducibility as the foundation.
- **One idea, done well.** The zoo deliberately contains a single cohesive
  concept (small deterministic simulations with great visuals) rather than a
  bundle of unrelated tricks.

## Non-goals, permanently

- A general game engine, an ECS, or a scene graph.
- Any feature that requires the network to render.
- Any behavior that makes a run depend on the wall clock or the machine.
