// Exhibit: N-body galaxy (particle family).
//
// A self-gravitating disk around a fixed central mass, integrated with a
// leapfrog (kick-drift-kick) scheme. Particles are coloured by speed and
// splatted additively, with a light trail fade, so the disk reads as a glowing
// rotating galaxy.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/registry.hpp"
#include "core/render2d.hpp"
#include "core/rng.hpp"
#include "core/sim.hpp"

namespace {

using namespace zoo;

double pget(const Params& p, const char* k, double d) {
    auto it = p.find(k);
    return it == p.end() ? d : it->second;
}
double sq(double x) { return x * x; }

class NBody : public Simulation {
public:
    const SimInfo& info() const override { return info_; }

    void init(uint64_t seed, const Params& p) override {
        Rng rng(seed);
        n_ = static_cast<int>(pget(p, "particles", 1000));
        G_ = pget(p, "G", 1.0);
        core_mass_ = pget(p, "core_mass", 0.27);
        pmass_ = pget(p, "particle_mass", 0.0001);
        eps2_ = sq(pget(p, "softening", 0.008));
        double r_in = pget(p, "r_in", 0.07);
        double r_out = pget(p, "r_out", 0.46);
        substeps_ = std::max(1, static_cast<int>(pget(p, "substeps", 3)));
        trail_ = static_cast<float>(pget(p, "trail", 0.90));
        vmax_ = static_cast<float>(pget(p, "vmax", 2.5));
        radius_ = static_cast<float>(pget(p, "radius", 1.8));
        bright_ = static_cast<float>(pget(p, "bright", 0.55));

        px_.resize(n_);
        py_.resize(n_);
        vx_.resize(n_);
        vy_.resize(n_);
        ax_.assign(n_, 0.0);
        ay_.assign(n_, 0.0);

        const double two_pi = 6.283185307179586;
        for (int i = 0; i < n_; ++i) {
            // Area-uniform sampling of the annulus.
            double r = std::sqrt(rng.range(sq(r_in), sq(r_out)));
            double th = rng.range(0.0, two_pi);
            double x = 0.5 + r * std::cos(th);
            double y = 0.5 + r * std::sin(th);
            // Circular speed from the central mass. Enclosed disk mass makes the
            // true speed a little higher, so the disk gently settles inward.
            double v = std::sqrt(G_ * core_mass_ / std::max(r, 1e-4));
            double vx = -std::sin(th) * v + rng.range(-0.02, 0.02);
            double vy = std::cos(th) * v + rng.range(-0.02, 0.02);
            px_[i] = x;
            py_[i] = y;
            vx_[i] = vx;
            vy_[i] = vy;
        }
        compute_accel();
    }

    void step(double dt) override {
        const double h = dt / static_cast<double>(substeps_);
        for (int s = 0; s < substeps_; ++s) {
            const double half = 0.5 * h;
            for (int i = 0; i < n_; ++i) {
                vx_[i] += ax_[i] * half;
                vy_[i] += ay_[i] * half;
                px_[i] += vx_[i] * h;
                py_[i] += vy_[i] * h;
            }
            compute_accel();
            for (int i = 0; i < n_; ++i) {
                vx_[i] += ax_[i] * half;
                vy_[i] += ay_[i] * half;
            }
        }
    }

    void render(Framebuffer& fb) const override {
        fb.fade(trail_);
        View view{fb.width(), fb.height()};
        const float r = radius_ * static_cast<float>(fb.width()) / 720.0f;
        for (int i = 0; i < n_; ++i) {
            float sp = static_cast<float>(std::sqrt(vx_[i] * vx_[i] + vy_[i] * vy_[i]));
            float t = sp / vmax_;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            Rgb c = particle_palette(t);
            float b = bright_ * (0.35f + 0.65f * t);
            fb.splat(view.x(px_[i]), view.y(py_[i]), r, c.r * b, c.g * b, c.b * b);
        }
        // The central mass.
        fb.splat(view.x(0.5), view.y(0.5), r * 2.5f, 1.0f, 0.85f, 0.55f);
    }

private:
    void compute_accel() {
        std::fill(ax_.begin(), ax_.end(), 0.0);
        std::fill(ay_.begin(), ay_.end(), 0.0);
        const double mu = G_ * core_mass_;
        for (int i = 0; i < n_; ++i) {
            const double dx = 0.5 - px_[i];
            const double dy = 0.5 - py_[i];
            const double r2 = dx * dx + dy * dy + eps2_;
            const double inv = mu / (r2 * std::sqrt(r2));
            ax_[i] += dx * inv;
            ay_[i] += dy * inv;
        }
        const double gpm = G_ * pmass_;
        for (int i = 0; i < n_; ++i) {
            for (int j = i + 1; j < n_; ++j) {
                const double dx = px_[j] - px_[i];
                const double dy = py_[j] - py_[i];
                const double r2 = dx * dx + dy * dy + eps2_;
                const double f = gpm / (r2 * std::sqrt(r2));
                ax_[i] += dx * f;
                ay_[i] += dy * f;
                ax_[j] -= dx * f;
                ay_[j] -= dy * f;
            }
        }
    }

    SimInfo info_{
        "nbody",
        "particle",
        "A self-gravitating disk orbiting a central mass. Speed-coloured trails.",
        {{"particles", 1000}, {"G", 1.0}, {"core_mass", 0.27}, {"particle_mass", 0.0001},
         {"softening", 0.008}, {"r_in", 0.07}, {"r_out", 0.46}, {"substeps", 3},
         {"trail", 0.90}, {"vmax", 2.5}, {"radius", 1.8}, {"bright", 0.55},
         {"exposure", 1.3}},
        RenderStyle::Additive};

    int n_ = 0;
    int substeps_ = 3;
    double G_ = 1.0, core_mass_ = 0.27, pmass_ = 0.0001, eps2_ = 0.000064;
    float trail_ = 0.90f, vmax_ = 2.5f, radius_ = 1.8f, bright_ = 0.55f;
    std::vector<double> px_, py_, vx_, vy_, ax_, ay_;
};

} // namespace

ZOO_REGISTER_EXHIBIT(NBody, "nbody")
