# EXHIBIT.md — how to add an animal to the zoo

An exhibit ("animal") is **one `.cpp` file** that defines a simulation and its
signature visual. If you follow this recipe, you never edit the runner, the
registry, or any other exhibit.

Read [`DESIGN.md`](DESIGN.md) for how the pieces fit together and
[`TRUST.md`](TRUST.md) for the reproducibility rules you must uphold.

## The three rules

An exhibit is trustworthy when:

1. **Randomness lives only in `init`**, drawn from the supplied `Rng`.
2. **`step(dt)` is pure physics** — its own state plus `dt`, nothing else. No
   I/O, no clock, no globals.
3. **`render` only draws** — it is `const` and never mutates state.

Break a rule and the run stops being reproducible. There is no exception.

## The recipe

### 1. Make a folder

```
exhibits/myanimal/myanimal.cpp
```

The build globs `exhibits/**/*.cpp`, so a new folder is picked up after you
re-run CMake. (With `CONFIGURE_DEPENDS`, a plain `cmake --build` usually
re-triggers it for you.)

### 2. Implement the interface

Here is the smallest complete exhibit — a drifting particle cloud:

```cpp
// exhibits/myanimal/myanimal.cpp
#include <cmath>
#include <vector>

#include "core/registry.hpp"
#include "core/render2d.hpp"
#include "core/rng.hpp"
#include "core/sim.hpp"

namespace {

using namespace zoo;

// Local convenience: read a param with a fallback.
double pget(const Params& p, const char* k, double d) {
    auto it = p.find(k);
    return it == p.end() ? d : it->second;
}

class MyAnimal : public Simulation {
public:
    const SimInfo& info() const override { return info_; }

    void init(uint64_t seed, const Params& p) override {
        Rng rng(seed);
        n_ = static_cast<int>(pget(p, "particles", 500));
        speed_ = static_cast<float>(pget(p, "speed", 0.05));

        x_.resize(n_);
        y_.resize(n_);
        vx_.resize(n_);
        vy_.resize(n_);
        for (int i = 0; i < n_; ++i) {
            x_[i] = rng.unit();               // [0,1)
            y_[i] = rng.unit();
            vx_[i] = speed_ * (2.0f * rng.unit() - 1.0f);
            vy_[i] = speed_ * (2.0f * rng.unit() - 1.0f);
        }
    }

    void step(double dt) override {
        const float f = static_cast<float>(dt) * 60.0f; // frames are ~1/60 s
        for (int i = 0; i < n_; ++i) {
            x_[i] += vx_[i] * f;
            y_[i] += vy_[i] * f;
            if (x_[i] < 0) x_[i] += 1.0f; else if (x_[i] >= 1) x_[i] -= 1.0f;
            if (y_[i] < 0) y_[i] += 1.0f; else if (y_[i] >= 1) y_[i] -= 1.0f;
        }
    }

    void render(Framebuffer& fb) const override {
        fb.fade(0.85f);                       // trails
        View view{fb.width(), fb.height()};
        for (int i = 0; i < n_; ++i) {
            Rgb c = particle_palette(0.7f);
            fb.splat(view.x(x_[i]), view.y(y_[i]), 1.6f, c.r, c.g, c.b);
        }
    }

private:
    SimInfo info_{
        "myanimal", "particle",
        "A drifting cloud.",
        {{"particles", 500}, {"speed", 0.05}, {"exposure", 1.2}},
        RenderStyle::Additive};

    int n_ = 0;
    float speed_ = 0.05f;
    std::vector<float> x_, y_, vx_, vy_;
};

} // namespace

ZOO_REGISTER_EXHIBIT(MyAnimal, "myanimal")
```

### 3. Register it

The `ZOO_REGISTER_EXHIBIT(TYPE, NAME)` macro at the bottom of the file does the
rest. `TYPE` must be the class name (it becomes an identifier); `NAME` is the
string users type. Give the class an `info_.name` that matches `NAME`.

### 4. Build and run

```sh
cmake --build build -j
./build/zoo list
./build/zoo params myanimal
./build/zoo run myanimal --frames 30 --out /tmp/myanimal
```

### 5. Verify determinism

```sh
./build/zoo run myanimal --seed 4 --frames 20 --out /tmp/a
./build/zoo run myanimal --seed 4 --frames 20 --out /tmp/b
diff -r /tmp/a /tmp/b && echo ok
```

### 6. (Optional) add it to the gallery

Add a preset to [`tools/run_all.py`](tools/run_all.py) and run it:

```python
PRESETS = {
    # ...
    "myanimal": dict(seed=1, frames=300, width=640, height=640, fps=60, every=6,
                     scale=0.38, colors=64),
}
```

Tune `every`/`scale`/`colors` until the GIF is both pretty and small.

## Choosing a family and a render style

| Family | Use for | Typical render |
|---|---|---|
| `particle` | points integrated through space (N-body) | `Additive` |
| `field` | values living on a grid (reaction-diffusion) | `Direct` |
| `agent` | behaviors that steer (boids) | `Additive` |

- **`Additive`** accumulate light with `splat`/`add`; `fade` gives trails;
  `resolve` applies exposure + tone map + gamma. Expose an `exposure` default so
  users can dial the brightness.
- **`Direct`** write finished colors with `set`; `resolve` only clamps. Map
  scalars through [`colormap.hpp`](core/colormap.hpp) (`viridis`, `magma`).

## Parameters

- Parameters are a flat `std::map<std::string, double>`.
- **List every tunable in `info_.defaults`.** `zoo params` is generated from it,
  so an undocumented knob effectively doesn't exist.
- **Reserved names.** The runner reads `exposure` from the params for additive
  exhibits. Field exhibits conventionally use `width`/`height` for their *grid*
  resolution (distinct from the image size passed with `--width`/`--height`).

## Testing checklist

- [ ] `cmake --build build -j` succeeds with no warnings you introduced.
- [ ] `zoo list` shows the exhibit with the right family and blurb.
- [ ] `zoo params <name>` lists every knob.
- [ ] A short run produces frames and no `nan`/`inf` values.
- [ ] Two runs with the same seed are byte-identical (`diff -r`).
- [ ] Two runs with different seeds differ.
- [ ] `render` is `const`; you did not store mutable state in it.

## Performance notes

Cheap things that pay off as particle counts grow:

- **Structure of arrays.** N-body stores `px_`, `py_`, `vx_`, … as separate
  vectors. It is fast and it keeps the physics loop branch-free.
- **Subdivide `dt`, don't shrink it.** N-body takes a `substeps` parameter and
  integrates `dt/substeps` multiple times rather than changing the global
  timestep. This keeps stability without breaking determinism.
- **Fields: double-buffer.** Gray-Scott keeps `u_/v_` and `u2_/v2_` and swaps
  them, so no allocation happens inside the iteration loop.
- **Avoid `sqrt` when a squared comparison will do**, and hoist invariants out
  of inner loops.

Don't reach for SIMD, threads or the GPU until a single exhibit is genuinely too
slow for an offline render — [`PLAN.md`](PLAN.md) covers where that work would
go.
