// Exhibit: Gray-Scott reaction-diffusion (field family).
//
// Two chemicals U and V on a periodic grid. A fixed number of explicit
// finite-difference iterations are run per call, and the V concentration is
// mapped through a colormap. Cheap, deterministic, and endlessly varied.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/colormap.hpp"
#include "core/registry.hpp"
#include "core/rng.hpp"
#include "core/sim.hpp"

namespace {

using namespace zoo;

double pget(const Params& p, const char* k, double d) {
    auto it = p.find(k);
    return it == p.end() ? d : it->second;
}

class GrayScott : public Simulation {
public:
    const SimInfo& info() const override { return info_; }

    void init(uint64_t seed, const Params& p) override {
        W_ = static_cast<int>(pget(p, "width", 256));
        H_ = static_cast<int>(pget(p, "height", 256));
        Du_ = pget(p, "Du", 0.16);
        Dv_ = pget(p, "Dv", 0.08);
        F_ = pget(p, "F", 0.035);
        k_ = pget(p, "k", 0.060);
        iters_per_second_ = pget(p, "iters_per_second", 900.0);
        gain_ = static_cast<float>(pget(p, "gain", 2.8));

        u_.assign(static_cast<size_t>(W_) * H_, 1.0f);
        v_.assign(static_cast<size_t>(W_) * H_, 0.0f);
        u2_.resize(u_.size());
        v2_.resize(v_.size());

        Rng rng(seed);
        const int seeds = static_cast<int>(pget(p, "seeds", 14));
        const int rad = static_cast<int>(pget(p, "seed_radius", 5));
        for (int s = 0; s < seeds; ++s) {
            const int cx = static_cast<int>(rng.below(static_cast<uint32_t>(W_)));
            const int cy = static_cast<int>(rng.below(static_cast<uint32_t>(H_)));
            for (int dy = -rad; dy <= rad; ++dy) {
                for (int dx = -rad; dx <= rad; ++dx) {
                    if (dx * dx + dy * dy > rad * rad) continue;
                    const int x = (cx + dx + W_) % W_;
                    const int y = (cy + dy + H_) % H_;
                    const size_t i = static_cast<size_t>(y) * W_ + x;
                    u_[i] = 0.5f;
                    v_[i] = 1.0f;
                }
            }
        }
    }

    void step(double dt) override {
        int n = static_cast<int>(std::lround(dt * iters_per_second_));
        if (n < 1) n = 1;
        for (int i = 0; i < n; ++i) iterate();
    }

    void render(Framebuffer& fb) const override {
        fb.clear();
        const int fw = fb.width();
        const int fh = fb.height();
        for (int py = 0; py < fh; ++py) {
            const int gy = py * H_ / fh;
            for (int px = 0; px < fw; ++px) {
                const int gx = px * W_ / fw;
                const float v = v_[static_cast<size_t>(gy) * W_ + gx];
                Rgb c = magma(v * gain_);
                fb.set(px, py, c.r, c.g, c.b);
            }
        }
    }

private:
    void iterate() {
        for (int y = 0; y < H_; ++y) {
            const int ym = (y + H_ - 1) % H_;
            const int yp = (y + 1) % H_;
            for (int x = 0; x < W_; ++x) {
                const int xm = (x + W_ - 1) % W_;
                const int xp = (x + 1) % W_;
                const size_t i = static_cast<size_t>(y) * W_ + x;
                const float uc = u_[i];
                const float vc = v_[i];
                const float lu = u_[static_cast<size_t>(y) * W_ + xm] +
                                 u_[static_cast<size_t>(y) * W_ + xp] +
                                 u_[static_cast<size_t>(ym) * W_ + x] +
                                 u_[static_cast<size_t>(yp) * W_ + x] - 4.0f * uc;
                const float lv = v_[static_cast<size_t>(y) * W_ + xm] +
                                 v_[static_cast<size_t>(y) * W_ + xp] +
                                 v_[static_cast<size_t>(ym) * W_ + x] +
                                 v_[static_cast<size_t>(yp) * W_ + x] - 4.0f * vc;
                const float uvv = uc * vc * vc;
                u2_[i] = uc + (static_cast<float>(Du_) * lu - uvv +
                               static_cast<float>(F_) * (1.0f - uc));
                v2_[i] = vc + (static_cast<float>(Dv_) * lv + uvv -
                               static_cast<float>(F_ + k_) * vc);
            }
        }
        u_.swap(u2_);
        v_.swap(v2_);
    }

    SimInfo info_{
        "grayscott",
        "field",
        "Gray-Scott reaction-diffusion: organic coral from two chemicals.",
        {{"width", 256}, {"height", 256}, {"Du", 0.16}, {"Dv", 0.08}, {"F", 0.035},
         {"k", 0.060}, {"seeds", 14}, {"seed_radius", 5}, {"iters_per_second", 900},
         {"gain", 2.8}},
        RenderStyle::Direct};

    int W_ = 256, H_ = 256;
    double Du_ = 0.16, Dv_ = 0.08, F_ = 0.035, k_ = 0.060, iters_per_second_ = 900.0;
    float gain_ = 2.8f;
    std::vector<float> u_, v_, u2_, v2_;
};

} // namespace

ZOO_REGISTER_EXHIBIT(GrayScott, "grayscott")
