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
    std::cout << "animation model checks passed\n";
}
