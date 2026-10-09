// Exhibit: Boids (agent family).
//
// Reynolds' flocking with separation, alignment and cohesion on a torus.
// Neighbour search is O(N^2) here, kept small and honest; PLAN.md notes the
// spatial-hash upgrade. Boids are coloured by heading, with trails, so flocks
// read as flowing coloured streams.

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

class Boids : public Simulation {
public:
    const SimInfo& info() const override { return info_; }

    void init(uint64_t seed, const Params& p) override {
        Rng rng(seed);
        n_ = static_cast<int>(pget(p, "count", 400));
        max_speed_ = pget(p, "max_speed", 0.18);
        max_force_ = pget(p, "max_force", 0.5);
        perception_ = pget(p, "perception", 0.06);
        sep_dist_ = pget(p, "sep_dist", 0.02);
        w_sep_ = pget(p, "w_sep", 1.5);
        w_align_ = pget(p, "w_align", 1.0);
        w_coh_ = pget(p, "w_coh", 1.0);
        trail_ = static_cast<float>(pget(p, "trail", 0.92));
        radius_ = static_cast<float>(pget(p, "radius", 1.6));
        bright_ = static_cast<float>(pget(p, "bright", 0.5));

        px_.resize(n_);
        py_.resize(n_);
        vx_.resize(n_);
        vy_.resize(n_);
        for (int i = 0; i < n_; ++i) {
            px_[i] = rng.next_double();
            py_[i] = rng.next_double();
            double a = rng.range(0.0, 6.283185307179586);
            double s = max_speed_ * (0.5 + 0.5 * rng.next_double());
            vx_[i] = std::cos(a) * s;
            vy_[i] = std::sin(a) * s;
        }
    }

    void step(double dt) override {
        const double perc2 = perception_ * perception_;
        const double sep2 = sep_dist_ * sep_dist_;
        for (int i = 0; i < n_; ++i) {
            double sep_x = 0, sep_y = 0;
            double ali_x = 0, ali_y = 0;
            double coh_x = 0, coh_y = 0;
            int ali_n = 0, coh_n = 0;
            for (int j = 0; j < n_; ++j) {
                if (j == i) continue;
                double dx = px_[j] - px_[i];
                double dy = py_[j] - py_[i];
                if (dx > 0.5) dx -= 1.0;
                if (dx < -0.5) dx += 1.0;
                if (dy > 0.5) dy -= 1.0;
                if (dy < -0.5) dy += 1.0;
                const double d2 = dx * dx + dy * dy;
                if (d2 >= perc2 || d2 == 0.0) continue;
                ali_x += vx_[j];
                ali_y += vy_[j];
                coh_x += px_[i] + dx;
                coh_y += py_[i] + dy;
                ++ali_n;
                ++coh_n;
                if (d2 < sep2) {
                    sep_x -= dx / d2;
                    sep_y -= dy / d2;
                }
            }
            double ax = 0, ay = 0;
            if (ali_n > 0) {
                // Alignment: steer toward the local average velocity.
                double sx = 0, sy = 0;
                steer(ali_x / ali_n, ali_y / ali_n, sx, sy);
                ax += sx * w_align_;
                ay += sy * w_align_;
                // Cohesion: steer toward the local centre of mass.
                double tx = coh_x / coh_n;
                double ty = coh_y / coh_n;
                double ux = tx - px_[i];
                double uy = ty - py_[i];
                double len = std::sqrt(ux * ux + uy * uy);
                if (len > 1e-9) {
                    double cxs = 0, cys = 0;
                    steer(ux / len * max_speed_, uy / len * max_speed_, cxs, cys);
                    ax += cxs * w_coh_;
                    ay += cys * w_coh_;
                }
            }
            // Separation: steer along the accumulated repulsion.
            double slen = std::sqrt(sep_x * sep_x + sep_y * sep_y);
            if (slen > 1e-9) {
                double sxs = 0, sys = 0;
                steer(sep_x / slen * max_speed_, sep_y / slen * max_speed_, sxs, sys);
                ax += sxs * w_sep_;
                ay += sys * w_sep_;
            }
            // Limit the steering force.
            double alen = std::sqrt(ax * ax + ay * ay);
            if (alen > max_force_) {
                ax = ax / alen * max_force_;
                ay = ay / alen * max_force_;
            }
            double nvx = vx_[i] + ax * dt;
            double nvy = vy_[i] + ay * dt;
            double vlen = std::sqrt(nvx * nvx + nvy * nvy);
            if (vlen > max_speed_) {
                nvx = nvx / vlen * max_speed_;
                nvy = nvy / vlen * max_speed_;
            }
            vx_[i] = nvx;
            vy_[i] = nvy;
            px_[i] += nvx * dt;
            py_[i] += nvy * dt;
            px_[i] -= std::floor(px_[i]);
            py_[i] -= std::floor(py_[i]);
        }
    }

    void render(Framebuffer& fb) const override {
        fb.fade(trail_);
        View view{fb.width(), fb.height()};
        const float r = radius_ * static_cast<float>(fb.width()) / 720.0f;
        for (int i = 0; i < n_; ++i) {
            float rn = static_cast<float>(vx_[i] / max_speed_);
            float gn = static_cast<float>(vy_[i] / max_speed_);
            float red = 0.5f + 0.5f * std::max(-1.0f, std::min(1.0f, rn));
            float grn = 0.5f + 0.5f * std::max(-1.0f, std::min(1.0f, gn));
            fb.splat(view.x(px_[i]), view.y(py_[i]), r,
                     red * bright_, grn * bright_, 0.55f * bright_);
        }
    }

private:
    // Reynolds steering: desired velocity minus current, limited to max_force.
    void steer(double des_x, double des_y, double& out_x, double& out_y) const {
        double len = std::sqrt(des_x * des_x + des_y * des_y);
        if (len > 1e-9) {
            des_x = des_x / len * max_speed_;
            des_y = des_y / len * max_speed_;
        }
        out_x = des_x;
        out_y = des_y;
    }

    SimInfo info_{
        "boids",
        "agent",
        "Reynolds flocking: separation, alignment, cohesion on a torus.",
        {{"count", 400}, {"max_speed", 0.18}, {"max_force", 0.5}, {"perception", 0.06},
         {"sep_dist", 0.02}, {"w_sep", 1.5}, {"w_align", 1.0}, {"w_coh", 1.0},
         {"trail", 0.92}, {"radius", 1.6}, {"bright", 0.5}, {"exposure", 1.2}},
        RenderStyle::Additive};

    int n_ = 400;
    double max_speed_ = 0.18, max_force_ = 0.5, perception_ = 0.06, sep_dist_ = 0.02;
    double w_sep_ = 1.5, w_align_ = 1.0, w_coh_ = 1.0;
    float trail_ = 0.92f, radius_ = 1.6f, bright_ = 0.5f;
    std::vector<double> px_, py_, vx_, vy_;
};

} // namespace

ZOO_REGISTER_EXHIBIT(Boids, "boids")
