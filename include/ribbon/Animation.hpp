// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
namespace ribbon {
enum class Easing { Linear, EaseOut, Spring };
struct Options {
    double durationMs = 400;
    double maxDurationMs = 1200;
    bool scaleDistance = false;
    Easing easing = Easing::EaseOut;
};
class Animation {
    double from_ = 0, to_ = 0, started_ = 0, duration_ = .4, initialVelocity_ = 0;
    Easing easing_ = Easing::EaseOut;
public:
    struct Sample { double position, velocity; bool active; };
    explicit Animation(double position = 0) : from_(position), to_(position) {}
    Sample sample(double now) const {
        const double t = std::clamp((now - started_) / duration_, 0., 1.);
        if (t >= 1 || from_ == to_) return {to_, 0, false};
        double f = t, derivative = 1;
        if (easing_ == Easing::EaseOut) { f = 1 - std::pow(1-t, 3); derivative = 3*(1-t)*(1-t); }
        if (easing_ == Easing::Spring) {
            // Analytic critically damped response; absolute time, no integration drift.
            const double elapsed = t * duration_, omega = 14 / duration_;
            const double a = from_ - to_, b = initialVelocity_ + omega*a;
            const double decay = std::exp(-omega*elapsed);
            return {to_ + (a+b*elapsed)*decay, (b-omega*(a+b*elapsed))*decay, true};
        }
        return {from_ + (to_-from_)*f, (to_-from_)*derivative/duration_, true};
    }
    void retarget(double destination, double now, const Options& options) {
        const auto current = sample(now);
        from_ = current.position; initialVelocity_ = current.velocity; to_ = destination;
        started_ = now; easing_ = options.easing;
        duration_ = std::clamp(options.durationMs * (options.scaleDistance ? std::max(1., std::abs(to_-from_)) : 1.), 1., std::max(1., options.maxDurationMs))/1000.;
    }
};
inline bool visible(double coordinate, double position) { return std::abs(coordinate-position) < 1.; }
}
