#pragma once
// Fixed-timestep clock.
//
// Offline rendering always advances the world in whole steps of a fixed dt;
// this class is used by the future live viewer (see PLAN.md) to turn variable
// wall-clock time into a whole number of steps while capping catch-up so a
// stall cannot spiral into an unbounded number of steps.

#include <algorithm>
#include <cmath>

namespace zoo {

class FixedStep {
public:
    FixedStep(double dt, int max_steps_per_frame = 5)
        : dt_(dt), max_steps_(max_steps_per_frame) {}

    // Feed elapsed wall-clock seconds; get the number of whole steps to run.
    // Extra time stays in the accumulator for the next frame.
    int advance(double real_seconds) {
        acc_ += std::min(real_seconds, 0.25); // clamp a long stall
        int steps = 0;
        while (acc_ >= dt_ && steps < max_steps_) {
            acc_ -= dt_;
            ++steps;
        }
        if (steps == max_steps_ && acc_ >= dt_) acc_ = dt_; // drop the backlog
        return steps;
    }

    // Fractional position between the previous and current step, for smooth
    // rendering. In [0,1).
    double alpha() const { return acc_ / dt_; }

    double dt() const { return dt_; }

private:
    double dt_;
    int max_steps_;
    double acc_ = 0.0;
};

} // namespace zoo
