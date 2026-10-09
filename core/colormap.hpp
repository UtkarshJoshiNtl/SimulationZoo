#pragma once
// Colormaps for turning a scalar in [0,1] into RGB.
//
// The tables are the published viridis/magma colormaps sampled at 16 points.
// Linear interpolation between them is exact enough for a visual and keeps the
// core dependency-free.

#include <algorithm>

namespace zoo {

struct Rgb {
    float r, g, b;
};

namespace detail {

inline Rgb lerp_rgb(const float* table, int count, float t) {
    t = std::min(1.0f, std::max(0.0f, t));
    const float x = t * static_cast<float>(count - 1);
    const int i = static_cast<int>(x);
    const int j = std::min(count - 1, i + 1);
    const float f = x - static_cast<float>(i);
    return Rgb{
        (table[i * 3 + 0] * (1.0f - f) + table[j * 3 + 0] * f) / 255.0f,
        (table[i * 3 + 1] * (1.0f - f) + table[j * 3 + 1] * f) / 255.0f,
        (table[i * 3 + 2] * (1.0f - f) + table[j * 3 + 2] * f) / 255.0f,
    };
}

// Viridis, 16 samples (matplotlib, sampled at i/15).
inline const float* viridis_table() {
    static const float t[] = {
        68, 1, 84, 72, 26, 108, 71, 47, 125, 65, 68, 135, 57, 86, 140, 49, 104, 142,
        42, 120, 142, 35, 136, 142, 31, 152, 139, 34, 168, 132, 53, 183, 121,
        84, 197, 104, 122, 209, 81, 165, 219, 54, 210, 226, 27, 253, 231, 37};
    return t;
}

// Magma, 16 samples (matplotlib, sampled at i/15).
inline const float* magma_table() {
    static const float t[] = {
        0, 0, 4, 11, 9, 36, 32, 17, 75, 59, 15, 112, 87, 21, 126, 114, 31, 129,
        140, 41, 129, 168, 50, 125, 196, 60, 117, 222, 73, 104, 241, 96, 93,
        250, 127, 94, 254, 159, 109, 254, 191, 132, 253, 222, 160, 252, 253, 191};
    return t;
}

} // namespace detail

inline Rgb viridis(float t) { return detail::lerp_rgb(detail::viridis_table(), 16, t); }
inline Rgb magma(float t) { return detail::lerp_rgb(detail::magma_table(), 16, t); }

} // namespace zoo
