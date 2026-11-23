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
    int zoomOutPercent = 8;
};
class Animation {
    double from_ = 0, to_ = 0, started_ = 0, duration_ = .4, initialVelocity_ = 0;
    double zoomStart_ = 1, zoomMin_ = 1;
    Easing easing_ = Easing::EaseOut;
public:
    struct Sample { double position, velocity; bool active; double scale; };
    explicit Animation(double position = 0) : from_(position), to_(position) {}
    Sample sample(double now) const {
        const double t = std::clamp((now - started_) / duration_, 0., 1.);
        if (t >= 1) return {to_, 0, false, 1};
        // Two smoothstep halves reach the minimum at the temporal midpoint.
        // Retargeting starts the next half-curve at the current scale.
        const double halfProgress = t < .5 ? 2*t : 2*t-1;
        const double smooth = halfProgress*halfProgress*(3-2*halfProgress);
        const double scale = t < .5 ? std::lerp(zoomStart_, zoomMin_, smooth)
                                    : std::lerp(zoomMin_, 1., smooth);
        if (from_ == to_) return {to_, 0, zoomStart_ < 1, zoomStart_ < 1 ? scale : 1};
        double f = t, derivative = 1;
        if (easing_ == Easing::EaseOut) { f = 1 - std::pow(1-t, 3); derivative = 3*(1-t)*(1-t); }
        if (easing_ == Easing::Spring) {
            // Analytic critically damped response; absolute time, no integration drift.
            const double elapsed = t * duration_, omega = 14 / duration_;
            const double a = from_ - to_, b = initialVelocity_ + omega*a;
            const double decay = std::exp(-omega*elapsed);
            return {to_ + (a+b*elapsed)*decay, (b-omega*(a+b*elapsed))*decay, true, scale};
        }
        return {from_ + (to_-from_)*f, (to_-from_)*derivative/duration_, true, scale};
    }
    void retarget(double destination, double now, const Options& options) {
        const auto current = sample(now);
        zoomMin_ = 1. - std::clamp(options.zoomOutPercent, 0, 25) / 100.;
        zoomStart_ = std::max(current.scale, zoomMin_);
        from_ = current.position; initialVelocity_ = current.velocity; to_ = destination;
        started_ = now; easing_ = options.easing;
        duration_ = std::clamp(options.durationMs * (options.scaleDistance ? std::max(1., std::abs(to_-from_)) : 1.), 1., std::max(1., options.maxDurationMs))/1000.;
    }
};
inline double visibleRadius(double scale) { return .5 + .5 / scale; }
inline bool visible(double coordinate, double position, double scale = 1.) {
    return std::abs(coordinate - position) < visibleRadius(scale);
}
}
