#include "../core/fixed_view_hold.hpp"
#include <cassert>
#include <iostream>
#ifdef NDEBUG
#undef NDEBUG
#endif
// Keep checks active in release builds.
#undef assert
#define assert(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
int main(){
    using namespace d4vr;
    auto make=[](uint32_t seq,double x=0.){auto view=identity();view[3]=x;return CameraPair({make_eye_state(0,seq,view,identity()),make_eye_state(1,seq,view,identity())});};
    auto a=make(57),next=make(58);FixedViewHold off;off.queued(a);assert(off.select(2)==2 && off.select(1)==1 && !off.preceding(a));
    FixedViewHold hold(true);assert(hold.select(2)==2);assert(hold.accepts(a));hold.queued(a);assert(hold.select(2)==0 && hold.select(1)==1 && hold.select(0,true)==2);
    auto observe=[&](uint64_t p,uint64_t ms,const CameraPair* pair,uint32_t mask=1){CameraInterval i;i.submitted(mask,3,p,pair);hold.observe(p,ms,mask,3,i);};
    observe(100,10,&a);observe(101,5010,&a);auto proof=hold.preceding(next);assert(proof && proof->sameRequestedMapping && proof->longestEndMs-proof->longestStartMs==5000 && proof->camera.packet.header[2]==57 && proof->longestFirst==100 && proof->longestLast==101);
    auto old=*proof;observe(102,5020,nullptr);observe(103,5030,&a);proof=hold.preceding(next);assert(proof->rejected==1 && proof->longestFirst==100 && proof->runFirst==103);
    observe(105,10040,&a);proof=hold.preceding(next);assert(proof->runFirst==105 && proof->longestFirst==100); // Missing present breaks elapsed continuity.
    observe(106,10000,&a);assert(hold.preceding(next)->runFirst==106); // Host clock reversal cannot add elapsed time.
    observe(107,16000,&next);assert(hold.preceding(next)->rejected==2); // A newer packet never repairs the original observation.
    assert(!hold.preceding(make(59,.1))->sameRequestedMapping);hold.queued(next);assert(hold.preceding(next)->observed==0 && old.camera.packet.header[2]==57);
    auto left=next.eye(0),right=next.eye(1);right.eyeFromGameView[3]=.01f;CameraPair stereo({left,right});assert(!hold.accepts(stereo));bool refused=false;try{hold.queued(stereo);}catch(const std::invalid_argument&){refused=true;}assert(refused);hold.clear();assert(hold.select(2)==2);
    std::cout<<"{\"fixed_view_hold_tests_passed\":true,\"host_interval_not_gpu_time\":true,\"old_receipt_immutable\":true}\n";
}
