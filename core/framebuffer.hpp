#pragma once
// Framebuffer: a floating-point RGB accumulation buffer.
//
// Rendering is additive. Exhibits splat light into the buffer, and the buffer
// is resolved to 8-bit RGB once per frame. Fading the buffer slightly between
// frames is how trails are produced cheaply (see fade()).
//
// All coordinates are in pixels with the origin at the top-left. Splats that
// fall outside the buffer are clipped, never wrapped, so a mis-placed point
// cannot corrupt unrelated pixels.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace zoo {

class Framebuffer {
public:
    Framebuffer(int width, int height)
        : w_(width), h_(height), buf_(static_cast<size_t>(width) * height * 3, 0.0f) {}

    int width() const { return w_; }
    int height() const { return h_; }

    void clear(float r = 0.0f, float g = 0.0f, float b = 0.0f) {
        for (size_t i = 0; i < buf_.size(); i += 3) {
            buf_[i] = r;
            buf_[i + 1] = g;
            buf_[i + 2] = b;
        }
    }

    // Multiply the whole buffer by `factor`. factor=1 keeps it, factor<1 fades
    // toward black producing motion trails.
    void fade(float factor) {
        for (auto& v : buf_) v *= factor;
    }

    // Add light to a single pixel.
    void add(int x, int y, float r, float g, float b) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        const size_t i = (static_cast<size_t>(y) * w_ + x) * 3;
        buf_[i] += r;
        buf_[i + 1] += g;
        buf_[i + 2] += b;
    }

    // Overwrite a single pixel. Used by direct/field rendering where the value
    // is a finished color rather than accumulated light.
    void set(int x, int y, float r, float g, float b) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        const size_t i = (static_cast<size_t>(y) * w_ + x) * 3;
        buf_[i] = r;
        buf_[i + 1] = g;
        buf_[i + 2] = b;
    }

    // Add a soft round blob centred at (cx, cy). The falloff is a smooth
    // cosine bump, which reads as a glow after tone mapping.
    void splat(float cx, float cy, float radius, float r, float g, float b) {
        if (radius <= 0.0f) {
            add(static_cast<int>(std::lround(cx)), static_cast<int>(std::lround(cy)), r, g, b);
            return;
        }
        const int x0 = std::max(0, static_cast<int>(std::floor(cx - radius)));
        const int x1 = std::min(w_ - 1, static_cast<int>(std::ceil(cx + radius)));
        const int y0 = std::max(0, static_cast<int>(std::floor(cy - radius)));
        const int y1 = std::min(h_ - 1, static_cast<int>(std::ceil(cy + radius)));
        const float inv = 1.0f / radius;
        for (int y = y0; y <= y1; ++y) {
            const float dy = (static_cast<float>(y) + 0.5f) - cy;
            for (int x = x0; x <= x1; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f) - cx;
                const float d = std::sqrt(dx * dx + dy * dy) * inv;
                if (d >= 1.0f) continue;
                // Smooth bump: 0.5 + 0.5*cos(pi*d), clamped to [0,1].
                const float w = 0.5f + 0.5f * std::cos(3.14159265358979f * d);
                const size_t i = (static_cast<size_t>(y) * w_ + x) * 3;
                buf_[i] += r * w;
                buf_[i + 1] += g * w;
                buf_[i + 2] += b * w;
            }
        }
    }

    // Convert the float buffer to 8-bit RGB.
    //
    //   tone_map = true  -> additive/HDR path: apply exposure, a Reinhard tone
    //                       map and gamma, so bright regions roll off.
    //   tone_map = false -> direct path: values are already display colors in
    //                       [0,1]; just clamp and quantize.
    void resolve(std::vector<uint8_t>& out, float exposure = 1.0f,
                 bool tone_map = true) const {
        out.resize(static_cast<size_t>(w_) * h_ * 3);
        for (size_t i = 0; i < buf_.size(); ++i) {
            float v = buf_[i];
            if (tone_map) {
                v *= exposure;
                v = v / (1.0f + v);                       // tone map to [0,1)
                v = std::pow(std::max(0.0f, v), 1.0f / 2.2f); // gamma
            }
            out[i] = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, v)) * 255.0f + 0.5f);
        }
    }

private:
    int w_;
    int h_;
    std::vector<float> buf_;
};

} // namespace zoo
