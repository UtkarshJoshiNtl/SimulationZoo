#pragma once
// Small 2D rendering helpers shared by particle and agent exhibits.

#include <cmath>

#include "colormap.hpp"
#include "framebuffer.hpp"

namespace zoo {

// Maps a normalized world position in [0,1]^2 to pixel coordinates. The
// vertical axis is flipped so that +y points up, matching the way these
// simulations are usually written.
struct View {
    int w;
    int h;

    float x(double u) const { return static_cast<float>(u) * static_cast<float>(w); }
    float y(double v) const { return static_cast<float>(1.0 - v) * static_cast<float>(h); }
};

// A tasteful default particle palette, hot core fading to cool. Input is a
// normalized speed/energy in [0,1].
inline Rgb particle_palette(float t) { return magma(t); }

} // namespace zoo
