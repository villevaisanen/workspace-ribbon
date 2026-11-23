#include <ribbon/Animation.hpp>
#include <cstdlib>
#include <iostream>
void check(bool b) { if (!b) { std::cerr << "animation assertion failed\n"; std::exit(1); } }
bool close(double a,double b) { return std::abs(a-b)<1e-8; }
int main() {
    using namespace ribbon;
    for (double destination : {0.,2.,3.,4.}) {
        Animation a(1); a.retarget(destination,10,{});
        check(close(a.sample(10).position,1));
        double previous=1;
        for (int i=1;i<=100;++i) { auto s=a.sample(10+i*.004); check((destination-1)*(s.position-previous)>=-1e-9); previous=s.position; }
        check(close(a.sample(11).position,destination)); check(!a.sample(11).active);
    }
    Animation a(0); a.retarget(4,0,{}); auto before=a.sample(.1);
    a.retarget(1,.1,{}); check(close(a.sample(.1).position,before.position));
    for(int i=0;i<100;++i) { double now=.11+i*.002; double p=a.sample(now).position; a.retarget(i%5,now,{}); check(close(a.sample(now).position,p)); }
    Options o; o.easing=Easing::Spring;
    Animation spring; spring.retarget(4,0,o); auto s=spring.sample(.1); spring.retarget(2,.1,o);
    check(close(spring.sample(.1).position,s.position)); check(close(spring.sample(.1).velocity,s.velocity));
    o.easing=Easing::Linear; o.scaleDistance=true; o.maxDurationMs=800;
    Animation scaled; scaled.retarget(10,0,o); check(scaled.sample(.799).active); check(!scaled.sample(.8).active);
    Animation other(8); check(close(other.sample(.2).position,8));
    check(visible(2,1.5)&&visible(1,1.5)&&!visible(3,1.5));
    for (const auto ease : {Easing::Linear, Easing::EaseOut, Easing::Spring}) {
        Options zoom;
        zoom.easing = ease;
        Animation single(1);
        single.retarget(2, 0, zoom);
        check(close(single.sample(0).scale, 1));
        check(close(single.sample(.1).scale, .96));
        check(close(single.sample(.2).scale, .92));
        check(close(single.sample(.3).scale, .96));
        check(close(single.sample(.4).scale, 1));

        Animation series(0);
        series.retarget(4, 0, zoom);
        double previousScale = series.sample(0).scale;
        for (const double time : {.05, .10, .15, .20}) {
            const auto before = series.sample(time);
            check(before.scale <= previousScale && before.scale >= .92);
            series.retarget(time == .20 ? 1 : (time == .10 ? 3 : 4), time, zoom);
            check(close(series.sample(time).position, before.position));
            check(close(series.sample(time).scale, before.scale));
            previousScale = before.scale;
        }
        check(close(series.sample(.4).scale, .92));
        check(close(series.sample(.601).scale, 1));
        check(!series.sample(.601).active);
    }
    Options noZoom;
    noZoom.zoomOutPercent = 0;
    Animation flat(1);
    flat.retarget(2, 0, noZoom);
    check(flat.sample(.2).active && close(flat.sample(.2).scale, 1));
    check(close(flat.sample(.2).position, 1.875));

    const double edge = visibleRadius(.92);
    check(!visible(1, 0));
    check(visible(1, 0, .92));
    check(visible(edge - .000001, 0, .92));
    check(!visible(edge + .000001, 0, .92));
    check(visible(-edge + .000001, 0, .92));
    std::cout << "animation model checks passed\n";
}
