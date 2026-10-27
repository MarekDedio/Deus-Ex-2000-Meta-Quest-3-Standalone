#include "quest_native_text_preferred_size.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace Preferred=QuestVr::NativeTextPreferredSize;
namespace Scalar=QuestVr::NativeScalar;
std::size_t checks{},refusals{};
void Require(const bool condition,const char* message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
std::uint32_t Word(const float value) {return std::bit_cast<std::uint32_t>(value);}
float Float(const std::uint32_t value) {return std::bit_cast<float>(value);}
template<class Function>
void Refuses(Function function,const char* message) {
    bool refused{};try {function();}catch(const std::exception&) {refused=true;}
    Require(refused,message);++refusals;
}
struct Operations {
    int gc{17},background{23},acquired{},depth{},releases{};
    bool text{true},wrap{true},measureAlias{},xIsLive{},yIsLive{};
    bool baseWidthSpecified{},baseHeightSpecified{};
    std::int32_t minimum{-1},maximum{-1};
    float horizontal{2.0f},vertical{3.0f},minimumWidth{},lineHeight{5.0f};
    float measureX{7.0f},measureY{11.0f},baseX{91.0f},baseY{92.0f};
    float* boundWidth{};float* boundHeight{};
    std::map<int,std::pair<std::int32_t,std::int32_t>> backgrounds{{23,{30,20}},{24,{50,40}}};
    std::vector<std::string> events;
    std::vector<std::uint32_t> destinations;
    std::size_t work{},limit{std::numeric_limits<std::size_t>::max()},calls{},faultCall{std::numeric_limits<std::size_t>::max()};
    bool throwAfterMeasureX{},throwAfterBaseX{};
    std::function<void(Operations&,std::string_view)> callback;
    void Step() {if(work==limit)throw std::runtime_error("Fixture preferred-size work exhausted");++work;}
    void Event(const char* name) {
        if(calls++==faultCall)throw std::runtime_error("Fixture authentic preferred-size provider failed");
        events.emplace_back(name);if(callback)callback(*this,name);
    }
    int GetGC() {Event("get");acquired=gc;if(gc)++depth;return gc;}
    void ReleaseGC(const int value) {
        Event("release");if(value!=acquired||!depth)throw std::runtime_error("Wrong acquired context released");--depth;++releases;
    }
    bool HasText() {Event("text");return text;}
    std::int32_t MinimumLines() {Event("minLines");return minimum;}
    std::int32_t MaximumLines() {Event("maxLines");return maximum;}
    bool WordWrap() {Event("wrap");return wrap;}
    float HorizontalMargin() {Event("hMargin");return horizontal;}
    float VerticalMargin() {Event("vMargin");return vertical;}
    float MinimumWidth() {Event("minWidth");return minimumWidth;}
    void Measure(const int value,const float destination,float& x,float& y) {
        Event("measure");if(value!=acquired)throw std::runtime_error("Measurement used an invented context");
        destinations.push_back(Word(destination));measureAlias=&x==&y;xIsLive=&x==boundWidth;yIsLive=&y==boundHeight;
        x=measureX;if(throwAfterMeasureX)throw std::runtime_error("Fixture extent failed after X write");y=measureY;
    }
    float FontHeight(const int value,const bool includeSpacing) {
        Event("font");if(value!=acquired||!includeSpacing)throw std::runtime_error("Wrong original font-height packet");return lineHeight;
    }
    int Background() {Event("background");return background;}
    std::int32_t BackgroundHeight(const int value) {Event("backgroundHeight");return backgrounds.at(value).second;}
    std::int32_t BackgroundWidth(const int value) {Event("backgroundWidth");return backgrounds.at(value).first;}
    void BasePreferredSize(const bool widthSpecified,float& width,const bool heightSpecified,float& height) {
        Event("base");baseWidthSpecified=widthSpecified;baseHeightSpecified=heightSpecified;
        width=baseX;if(throwAfterBaseX)throw std::runtime_error("Fixture script preferred-size failed after width");height=baseY;
    }
};
void Query(Operations& operations,const bool widthSpecified,float& width,const bool heightSpecified,float& height) {
    operations.boundWidth=&width;operations.boundHeight=&height;
    Preferred::Query(operations,widthSpecified,width,heightSpecified,height);
}
std::size_t Count(const Operations& operations,const char* event) {
    return static_cast<std::size_t>(std::count(operations.events.begin(),operations.events.end(),event));
}
Scalar::Real Integer(const std::int32_t value) {return Scalar::Load(static_cast<double>(value));}
float Product(const std::int32_t count,const float height) {
    return Scalar::StoreFloat(Scalar::Multiply(Integer(count),Scalar::Load(height)));
}
float Margins(const float margin,const float value) {
    const auto m=Scalar::Load(margin);return Scalar::StoreFloat(Scalar::Add(Scalar::Add(m,m),Scalar::Load(value)));
}
// Independent fixed-provider outcome oracle. Provider order/mutations/alias
// destination references are checked separately against explicit event fixtures.
void Reference(const Operations& op,const bool ws,float& width,const bool hs,float& height) {
    if(!op.gc)return;
    if(op.text) {
        const bool limits=!hs&&(op.minimum>=0||op.maximum>=0);
        if(!(limits&&ws&&op.wrap))width=op.measureX;
        height=op.measureY;
        if(limits) {
            auto selected=Scalar::Load(height);
            if(op.minimum>=0) {
                const auto product=Scalar::Multiply(Integer(op.minimum),Scalar::Load(op.lineHeight));
                if(Scalar::Compare(177u,product,Scalar::Load(height)))selected=Scalar::Load(Scalar::StoreFloat(product));
            }
            if(op.maximum>=0) {
                const auto high=Product(op.maximum,op.lineHeight);
                if(Scalar::Compare(177u,Scalar::Load(height),Scalar::Load(high)))selected=Scalar::Load(high);
            }
            height=Scalar::StoreFloat(selected);
        }
        width=Margins(op.horizontal,width);height=Margins(op.vertical,height);
    }else if(op.background) {
        const auto dimensions=op.backgrounds.at(op.background);
        height=Scalar::StoreFloat(Integer(dimensions.second));
        if(!hs) {
            if(op.minimum>=0)height=Product(op.minimum,op.lineHeight);
            else if(op.maximum>=0)height=Product(op.maximum,op.lineHeight);
        }
        width=Scalar::StoreFloat(Integer(dimensions.first));
    }else {width=op.baseX;height=op.baseY;}
    if(!ws&&Scalar::Compare(176u,Scalar::Load(width),Scalar::Load(op.minimumWidth)))width=op.minimumWidth;
}
void Branches() {
    for(const bool ws:{false,true})for(const bool hs:{false,true}) {
        Operations op;op.gc=0;op.minimumWidth=1000.0f;float w=Float(0x7f801234u),h=Float(0x80000000u);
        Query(op,ws,w,hs,h);
        Require(Word(w)==0x7f801234u&&Word(h)==0x80000000u&&op.depth==0&&op.releases==0,
            "No-GC branch altered original raw outputs or released an invented context");
        Require(op.events==std::vector<std::string>{"get"},"No-GC branch queried text/layout/fonts/background");
    }
    {
        Operations op;float w=100.0f,h=200.0f;Query(op,true,w,true,h);
        Require(w==11.0f&&h==17.0f&&op.destinations==std::vector<std::uint32_t>{Word(96.0f)},
            "Ordinary wrapped branch did not measure both actual outputs and add margins");
        Require(op.xIsLive&&op.yIsLive&&!op.measureAlias&&op.releases==1&&!op.depth,
            "Ordinary measurement used detached outputs or leaked GC");
        Require(Count(op,"minLines")==0u&&Count(op,"maxLines")==0u&&Count(op,"font")==0u,
            "Height-specified branch evaluated constrained line bounds");
    }
    {
        Operations op;op.minimum=1;float w=100.0f,h=200.0f;Query(op,true,w,false,h);
        Require(w==104.0f&&h==17.0f&&!op.xIsLive&&op.yIsLive,
            "Constrained wrapped branch did not discard measured X/preserve supplied width");
        Require(Count(op,"font")==1u&&Count(op,"minLines")==2u&&Count(op,"maxLines")==1u,
            "Constrained path skipped original current line-count reads");
    }
    {
        Operations op;op.minimum=10;op.maximum=3;op.measureY=1.0f;op.lineHeight=2.0f;
        float w=100.0f,h=200.0f;Query(op,false,w,false,h);
        Require(w==11.0f&&h==26.0f,"Maximum clamp compared against min candidate instead of ORIGINAL measured height");
        Require(op.destinations==std::vector<std::uint32_t>{0u}&&Count(op,"wrap")==0u,
            "Unspecified width queried wrapping or substituted a caller-side width");
    }
    {
        Operations op;op.minimum=0;op.maximum=1;op.measureY=100.0f;float w{},h{};Query(op,false,w,false,h);
        Require(h==11.0f,"Constrained maximum did not replace original measured height");
    }
    for(const bool ws:{false,true})for(const bool hs:{false,true}) {
        Operations op;op.text=false;op.minimum=2;op.maximum=1;float w=100.0f,h=200.0f;Query(op,ws,w,hs,h);
        Require(w==30.0f&&h==(hs?20.0f:10.0f),"Empty-background branch respected specified output or wrong line-bound precedence");
        Require(Count(op,"hMargin")==0u&&Count(op,"vMargin")==0u&&Count(op,"measure")==0u,
            "Empty-background branch added text margins or measured text");
        Require(Count(op,"background")==2u&&Count(op,"backgroundHeight")==1u&&Count(op,"backgroundWidth")==1u,
            "Empty-background branch failed original height-first/background reload protocol");
    }
    {
        Operations op;op.text=false;op.minimum=-1;op.maximum=3;float w{},h{};Query(op,false,w,false,h);
        Require(w==30.0f&&h==15.0f,"Empty-background branch did not use maximum when minimum absent");
    }
    for(const bool ws:{false,true})for(const bool hs:{false,true}) {
        Operations op;op.text=false;op.background=0;float w{},h{};Query(op,ws,w,hs,h);
        Require(w==91.0f&&h==92.0f&&op.baseWidthSpecified==ws&&op.baseHeightSpecified==hs,
            "Empty no-background branch fabricated an empty base script callback");
        Require(Count(op,"background")==1u&&Count(op,"font")==0u&&Count(op,"base")==1u,
            "Base callback branch traversed an unrelated background/font branch");
    }
}
void AliasesAndMutations() {
    {
        Operations op;float both=100.0f;Query(op,true,both,true,both);
        Require(both==21.0f&&op.measureAlias,"Nonempty ordinary aliased outputs lost X/Y then H/V store order");
    }
    {
        Operations op;op.minimum=1;float both=100.0f;Query(op,true,both,false,both);
        Require(both==21.0f&&!op.measureAlias&&!op.xIsLive&&op.yIsLive,
            "Constrained aliased output incorrectly kept pre-measure supplied width");
    }
    {
        Operations op;op.text=false;op.minimum=3;float both=100.0f;Query(op,false,both,false,both);
        Require(both==30.0f,"Empty-background alias did not store USize after height");
    }
    {
        Operations op;op.minimum=1;
        op.callback=[](Operations& current,const std::string_view event) {
            if(event=="font") {current.minimum=3;current.background=24;}
        };
        float w=100.0f,h=200.0f;Query(op,true,w,false,h);
        Require(w==104.0f&&h==21.0f,"Constrained sizing did not reread current line count after FontHeight");
        op.text=false;op.minimum=1;op.background=23;w=100.0f;h=200.0f;Query(op,true,w,false,h);
        Require(w==50.0f&&h==15.0f,"Background path did not reread bound and actual Background after FontHeight");
    }
    {
        Operations op;op.text=false;op.background=0;
        op.callback=[](Operations& current,const std::string_view event) {if(event=="base")current.minimumWidth=130.0f;};
        float w{},h{};Query(op,false,w,false,h);
        Require(w==130.0f&&h==92.0f,"Post-script minimum width used stale pre-callback field");
        Require(op.events==std::vector<std::string>{"get","text","background","base","minWidth","release"},
            "Base script/minimum/release order differs");
    }
    {
        Operations op;op.callback=[](Operations& current,const std::string_view event) {if(event=="measure")current.horizontal=7.0f;};
        float w=100.0f,h=200.0f;Query(op,true,w,true,h);
        Require(w==21.0f&&h==17.0f&&op.destinations[0u]==Word(96.0f),
            "Nonempty margin stores reused stale pre-measure margin");
    }
    {
        Operations op;op.callback=[](Operations& current,const std::string_view event) {if(event=="measure")current.gc=99;};
        float w{},h{};Query(op,false,w,true,h);
        Require(op.acquired==17&&op.releases==1&&!op.depth,"Release reloaded current GC instead of acquired identity");
    }
}
void RawWords() {
    constexpr std::array words{0u,0x80000000u,0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,0x7fc01234u,0x7f801234u};
    for(const auto minimum:words) {
        Operations op;op.text=false;op.background=0;op.baseX=Float(0x7f805678u);op.minimumWidth=Float(minimum);
        float w{},h{};Query(op,false,w,true,h);
        Require(Word(w)==minimum,"C0 unordered minimum-width branch failed raw-word copy");
    }
    {
        Operations op;op.text=false;op.background=0;op.baseX=1.0f;op.minimumWidth=Float(0x7f801234u);
        float w{},h{};Query(op,true,w,true,h);
        Require(w==1.0f&&Count(op,"minWidth")==0u,"Width-specified branch clamped unordered minimum");
    }
    {
        Operations op;op.minimum=16777217;op.maximum=-1;op.lineHeight=1.0f;op.measureY=16777216.0f;
        op.horizontal=op.vertical=0.0f;float w{},h{};Query(op,false,w,false,h);
        Require(h==16777216.0f,"Extended minimum product/FST Float spill differs at integer rounding boundary");
    }
    for(const auto dimension:{std::numeric_limits<std::int32_t>::min(),-1,0,1,std::numeric_limits<std::int32_t>::max()}) {
        Operations op;op.text=false;op.backgrounds[23]={dimension,dimension};float w{},h{};Query(op,true,w,true,h);
        const auto expected=Scalar::StoreFloat(Integer(dimension));
        Require(Word(w)==Word(expected)&&Word(h)==Word(expected),"Background signed dimensions were clamped or unsigned-converted");
    }
    for(const auto margin:words) {
        Operations op;op.horizontal=Float(margin);float w=Float(0x7f800000u),h{};
        const auto m=Scalar::Load(op.horizontal);
        const auto expected=Scalar::StoreFloat(Scalar::Subtract(Scalar::Load(w),Scalar::Add(m,m)));
        Query(op,true,w,true,h);
        Require(op.destinations.size()==1u&&op.destinations[0u]==Word(expected),"Wrapped width discarded PC64/Float nonfinite packet semantics");
    }
}
void Failures() {
    for(const int path:{0,1,2,3}) {
        Operations complete;complete.text=path<2;complete.minimum=path==1?2:-1;complete.background=path==3?0:23;
        complete.minimumWidth=1000.0f;const bool widthSpecified=path%2!=0;
        float w=100.0f,h=200.0f;Query(complete,widthSpecified,w,false,h);
        for(std::size_t cut=0u;cut<complete.work;++cut) {
            Operations op;op.text=path<2;op.minimum=path==1?2:-1;op.background=path==3?0:23;op.limit=cut;
            op.minimumWidth=1000.0f;
            float width=100.0f,height=200.0f;std::size_t observed{};
            Refuses([&] {try {Query(op,widthSpecified,width,false,height);}catch(...) {
                observed=op.work;width=100.0f;height=200.0f;op.depth=0;op.releases=0;throw;
            }},"Preferred-size work failure was swallowed");
            Require(observed==cut&&width==100.0f&&height==200.0f&&!op.depth&&!op.releases,
                "Explicit enclosing-root preferred-size undo/work admission failed");
        }
        for(std::size_t call=0u;call<complete.calls;++call) {
            Operations op;op.text=path<2;op.minimum=path==1?2:-1;op.background=path==3?0:23;op.faultCall=call;
            op.minimumWidth=1000.0f;
            float width=100.0f,height=200.0f;
            Refuses([&] {try {Query(op,widthSpecified,width,false,height);}catch(...) {
                width=100.0f;height=200.0f;op.depth=0;op.releases=0;throw;
            }},"Preferred-size provider failure was converted to empty success");
            Require(width==100.0f&&height==200.0f&&!op.depth&&!op.releases,"Provider-fault outer fixture undo failed");
        }
    }
    for(const bool measure:{false,true}) {
        Operations op;op.text=measure;op.background=0;op.throwAfterMeasureX=measure;op.throwAfterBaseX=!measure;
        float w=100.0f,h=200.0f;
        Refuses([&] {try {Query(op,true,w,false,h);}catch(...) {w=100.0f;h=200.0f;op.depth=0;throw;}},
            "Partially writing actual/script extent provider failure was swallowed");
        Require(w==100.0f&&h==200.0f&&!op.depth,"Partial-provider writes escaped explicit root fixture undo");
    }
}
void Generated() {
    constexpr std::array words{0u,0x80000000u,0x40400000u,0xc0e00000u,0x7f800000u,0xff800000u,0x7fc01234u,0x7f801234u};
    for(const bool text:{false,true})for(const bool ws:{false,true})for(const bool hs:{false,true})
    for(const bool wrap:{false,true})for(const bool background:{false,true})for(const bool alias:{false,true})
    for(const auto minimum:{-2,-1,0,1,3})for(const auto maximum:{-2,-1,0,1,3})for(std::size_t raw=0u;raw<words.size();++raw) {
        Operations op;op.text=text;op.wrap=wrap;op.background=background?23:0;op.minimum=minimum;op.maximum=maximum;
        op.minimumWidth=Float(words[(raw+1u)%words.size()]);op.horizontal=Float(words[(raw+2u)%words.size()]);
        op.vertical=Float(words[(raw+3u)%words.size()]);op.lineHeight=Float(words[(raw+4u)%words.size()]);
        op.measureX=Float(words[(raw+5u)%words.size()]);op.measureY=Float(words[(raw+6u)%words.size()]);
        float actualW=Float(words[raw]),actualH=Float(words[(raw+7u)%words.size()]);
        float expectedW=actualW,expectedH=actualH;
        if(alias) {Reference(op,ws,expectedW,hs,expectedW);Query(op,ws,actualW,hs,actualW);}
        else {Reference(op,ws,expectedW,hs,expectedH);Query(op,ws,actualW,hs,actualH);}
        Require(Word(actualW)==Word(expectedW)&&(alias||Word(actualH)==Word(expectedH)),
            "Generated branch/alias/raw-words outcome differs from independent fixed-provider oracle");
        Require(op.releases==1&&!op.depth,"Generated nonnull-GC branch leaked or double-released context");
    }
}
} // namespace

int main() {
    try {
        Branches();AliasesAndMutations();RawWords();Failures();Generated();
        std::cout<<"PASS original TextWindow preferred sizing: "<<checks<<" checks / "<<refusals
            <<" refusal controls; full text/background/base branches, original line bounds, raw PC64/C0, alias order, "
            "provider reloads and explicit root undo. Mock authenticated-service fixtures only, not GC/font integration or original startup.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL original TextWindow preferred sizing: "<<error.what()<<'\n';return 1;}
}
