#pragma once
// The exhibit contract.
//
// An exhibit is one simulation plus its signature visual. This interface is the
// only thing the runner knows about. Keep it small and keep the rules:
//
//   * init() is the only place randomness is drawn, and it draws it from the
//     supplied Rng. The same (seed, params) must reproduce the same state.
//   * step() is pure physics. No file I/O, no wall-clock time, no global state.
//     It advances the world by exactly `dt` seconds.
//   * render() only reads state and draws into the framebuffer. It must not
//     mutate simulation state (it is marked const).
//
// If an exhibit obeys these three rules it is deterministic, replayable and
// safe to render headlessly. See EXHIBIT.md for the full recipe.

#include <cstdint>
#include <map>
#include <string>

#include "framebuffer.hpp"

namespace zoo {

// Parameters are plain doubles keyed by name. They are surfaced by `zoo params`
// and overridable with `--param key=value` on the command line.
using Params = std::map<std::string, double>;

// How the framebuffer is turned into pixels.
//   Additive: accumulate light (particles/agents); resolves with tone mapping.
//   Direct:   write finished colors (fields/scalar maps); resolves by clamping.
enum class RenderStyle { Additive, Direct };

struct SimInfo {
    std::string name;     // stable identifier, e.g. "nbody"
    std::string category; // "particle" | "field" | "agent"
    std::string blurb;    // one line, shown by `zoo list`
    Params defaults;      // every tunable with its default value
    RenderStyle render = RenderStyle::Additive;
};

class Simulation {
public:
    virtual ~Simulation() = default;

    virtual const SimInfo& info() const = 0;

    virtual void init(uint64_t seed, const Params& params) = 0;

    virtual void step(double dt) = 0;

    virtual void render(Framebuffer& fb) const = 0;
};

} // namespace zoo
