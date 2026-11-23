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
    double zoomInDuration_ = 0, slideDelay_ = 0;

    static double smoothstep(double progress) {
        const double t = std::clamp(progress, 0., 1.);
        return t*t*(3-2*t);
    }

    Easing easing_ = Easing::EaseOut;
public:
    struct Sample { double position, velocity; bool active; double scale; };
    explicit Animation(double position = 0) : from_(position), to_(position) {}
    Sample sample(double now) const {
        const double elapsed = std::max(0., now - started_);
        if (elapsed >= duration_ || (from_ == to_ && zoomStart_ == 1.))
            return {to_, 0, false, 1};

        const double slideDuration = duration_ - slideDelay_;
        const double t = std::max(0., elapsed - slideDelay_) / slideDuration;
        const double scale = elapsed < zoomInDuration_
            ? std::lerp(zoomStart_, zoomMin_, smoothstep(elapsed / zoomInDuration_))
            : std::lerp(zoomMin_, 1., smoothstep((t - .45) / .55));
        if (elapsed < slideDelay_ || from_ == to_)
            return {from_, 0, true, scale};
        double f = t, derivative = 1;
        if (easing_ == Easing::EaseOut) { f = 1 - std::pow(1-t, 3); derivative = 3*(1-t)*(1-t); }
        if (easing_ == Easing::Spring) {
            // Analytic critically damped response; absolute time, no integration drift.
            const double slideElapsed = t * slideDuration, omega = 14 / slideDuration;
            const double a = from_ - to_, b = initialVelocity_ + omega*a;
            const double decay = std::exp(-omega*slideElapsed);
            return {to_ + (a+b*slideElapsed)*decay, (b-omega*(a+b*slideElapsed))*decay, true, scale};
        }
        return {from_ + (to_-from_)*f, (to_-from_)*derivative/slideDuration, true, scale};
    }
    void retarget(double destination, double now, const Options& options) {
        const auto current = sample(now);
        zoomMin_ = 1. - std::clamp(options.zoomOutPercent, 0, 25) / 100.;
        zoomStart_ = std::max(current.scale, zoomMin_);
        from_ = current.position; initialVelocity_ = current.velocity; to_ = destination;
        started_ = now; easing_ = options.easing;
        duration_ = std::clamp(options.durationMs * (options.scaleDistance ? std::max(1., std::abs(to_-from_)) : 1.), 1., std::max(1., options.maxDurationMs))/1000.;
        // Start a fresh slide once the camera has pulled back. Retargets
        // keep moving while any remaining zoom-in travel finishes.
        zoomInDuration_ = zoomMin_ < 1. ? std::min(.070, duration_ * .15) : 0.;
        slideDelay_ = current.active ? 0. : zoomInDuration_;
    }
};
inline double visibleRadius(double scale) { return .5 + .5 / scale; }
inline bool visible(double coordinate, double position, double scale = 1.) {
    return std::abs(coordinate - position) < visibleRadius(scale);
}
}
