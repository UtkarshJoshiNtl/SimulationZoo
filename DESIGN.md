# DESIGN.md — how the zoo is built

This document describes the pieces of the system and the reasoning behind them.
It is the reference for anyone changing the core. For the recipe to add an
exhibit, see [`EXHIBIT.md`](EXHIBIT.md); for the reproducibility contract, see
[`TRUST.md`](TRUST.md); for why the stack is what it is, see [`PLAN.md`](PLAN.md).

## Goals

- **One exhibit, one file.** Adding an animal is a bounded task that never asks
  you to touch the runner.
- **Deterministic and headless.** Rendering works on a machine with no display
  and no GPU.
- **Cool to watch.** Visuals are the product; the frame pipeline exists to make
  them.
- **Small and readable.** No framework, no dependencies in the C++ core.

## Non-goals

- A general-purpose engine, an ECS, or a scene graph.
- Real-time interaction (that is [`PLAN.md`](PLAN.md), future).
- Networking, a GUI, or an editor.

## Layout

```
core/            the runtime; knows nothing about any specific exhibit
  sim.hpp        the exhibit contract (Simulation, SimInfo, Params)
  registry.hpp   name -> factory registration
  registry.cpp   the registry implementation
  rng.hpp        seeded xoshiro256** generator
  framebuffer.hpp  float RGB accumulate / fade / splat / resolve
  colormap.hpp   viridis + magma sample tables
  render2d.hpp   View mapping + particle palette
  image.hpp      binary PPM writer
  clock.hpp      fixed-timestep accumulator (for the future live viewer)
exhibits/
  nbody/         particle family
  grayscott/     field family
  boids/         agent family
apps/
  zoo.cpp        the CLI: list / params / run
tools/
  make_gif.py    PPM frames -> GIF
  run_all.py     renders every exhibit and rebuilds the gallery
gallery/         the committed GIFs the README shows
CMakeLists.txt
```

The dependency direction is strictly one-way: `exhibits` and `apps` depend on
`core`, and `core` depends on nothing but the standard library.

## The exhibit contract

Defined in [`core/sim.hpp`](core/sim.hpp):

```cpp
class Simulation {
    virtual const SimInfo& info() const = 0;
    virtual void init(uint64_t seed, const Params& params) = 0;
    virtual void step(double dt) = 0;
    virtual void render(Framebuffer& fb) const = 0;
};
```

`SimInfo` carries the exhibit's identity (`name`, `category`, `blurb`), its
tunable `defaults`, and one rendering choice (`RenderStyle`). `Params` is a
`std::map<std::string, double>` — deliberately unglamorous, so the CLI, the
docs and the code all agree on the same flat set of knobs.

The lifecycle the runner performs:

```
make_sim(name)  ->  init(seed, params)  ->  [ render(); step(dt); ] * frames
```

## The registry

Exhibits register themselves at static-initialisation time:

```cpp
ZOO_REGISTER_EXHIBIT(NBody, "nbody")
```

This is why [`CMakeLists.txt`](CMakeLists.txt) builds exhibits as an **OBJECT
library** rather than a static one: a static library would let the linker drop
object files whose only exported symbol is a registration side effect, silently
removing exhibits. OBJECT libraries guarantee every exhibit object is linked
into the executable.

Consequence: the runner ([`apps/zoo.cpp`](apps/zoo.cpp)) only ever calls
`sim_names()` and `make_sim()`. It has no `#include` of any exhibit and no
`switch` over names. Adding an exhibit cannot break it.

## The frame pipeline

```
exhibit.render(fb)                       fill the float framebuffer
      |
fb.resolve(rgb, exposure, tone_map)      -> 8-bit RGB
      |
write_ppm(out/frame_NNNN.ppm)            lossless intermediate
      |
tools/make_gif.py                        -> gallery/<exhibit>.gif
```

PPM is used deliberately: it needs no image library, so the C++ core stays
dependency-free, and Pillow reads it directly when the Python tools assemble the
GIF.

## Rendering model

There are two render styles, chosen per exhibit:

- **Additive** (particles, agents). The framebuffer accumulates light.
  [`Framebuffer::fade`](core/framebuffer.hpp) multiplies the whole buffer by a
  factor each frame to produce motion trails for free. `resolve` then applies
  exposure, a Reinhard tone map and gamma, so bright cores roll off to white
  instead of clipping.
- **Direct** (scalar fields). The exhibit writes finished colors with
  `Framebuffer::set`, usually through a colormap. `resolve` only clamps and
  quantizes — no tone map, no gamma — because the colors are already display
  values.

`Framebuffer::splat` draws a soft cosine bump rather than a hard pixel; a field
of overlapping bumps reads as a glow after tone mapping. All drawing is clipped,
never wrapped, so a stray coordinate cannot corrupt unrelated pixels.

Colormaps ([`core/colormap.hpp`](core/colormap.hpp)) are 16-sample viridis and
magma tables, chosen so the core needs no plotting library and the Python tooling
needs no shared colormap state.

## Determinism and time

The runner computes a single `dt = 1/fps` and calls `step(dt)` for every frame
after rendering. There is no wall clock anywhere in the offline path.
Exhibits that need finer integration subdivide internally (N-body has a
`substeps` parameter) or convert `dt` into a whole number of internal iterations
(Gray-Scott derives its iteration count from `dt`).

[`core/clock.hpp`](core/clock.hpp) provides a `FixedStep` accumulator with a
spiral-of-death guard. It is not used offline — where the frame count is exact —
but is the intended timing source for the live viewer described in
[`PLAN.md`](PLAN.md).

## Build and portability

CMake ≥ 3.16, C++17, no external dependencies. Warnings are on (`-Wall -Wextra`
or `/W4`). Exhibit sources are globbed with `CONFIGURE_DEPENDS`, so dropping a
new folder under `exhibits/` and re-running CMake is enough to include it.

The code uses only `<cstdint>`, `<cmath>`, `<vector>`, `<string>`, `<map>`,
`<filesystem>` and friends — all available on GCC, Clang and MSVC. There is no
POSIX-only API, no `fork`, no socket, no `system()`.

## Why this shape

The design is a direct response to the two failure modes of "cool demo" repos:
they are either a pile of unrelated one-off scripts, or an over-built engine
nobody can extend. The interface is intentionally tiny and the runner is
intentionally ignorant, so interesting work stays in the exhibits and plumbing
stays out of the way. The next section of the roadmap — interaction, effects,
GPU — is designed to slot into this same contract; see [`PLAN.md`](PLAN.md).
