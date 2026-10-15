#include "quest_native_window_configure.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
unsigned checks{};
void Require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
std::uint32_t Raw(float value) {std::uint32_t out;std::memcpy(&out,&value,4u);return out;}
struct Fixture {
    std::map<std::string,float> floats{{"X",3},{"Y",4},{"Width",10},{"Height",20}};
    std::map<std::string,bool> flags{{"bIsVisible",true},{"bConfigured",false},{"bNeedsReconfigure",false},{"bNeedsQuery",true}};
    std::vector<std::string> events;
    unsigned depth{1u};std::uint8_t type{};
    std::function<void(Fixture&)> callback;
    float ReadFloat(const char* name) {events.push_back(std::string("read.")+name);return floats.at(name);}
    bool ReadBool(const char* name) {events.push_back(std::string("read.")+name);return flags.at(name);}
    void WriteFloat(const char* name,float value) {events.push_back(std::string("write.")+name);floats.at(name)=value;}
    void WriteBool(const char* name,bool value) {events.push_back(std::string("write.")+name);flags.at(name)=value;}
    void ChangeConfiguration() {events.push_back("change");if(callback) callback(*this);}
    bool ShouldClip() {events.push_back("clip?");return depth==1u || type>=1u;}
    void ClipTree() {events.push_back("clip");}
    void ResortSelection() {events.push_back("resort");}
};
void Run(Fixture& fixture,float x=1,float y=2,float width=10,float height=20) {
    QuestVr::ConfigureNativeWindowGeometry(fixture,x,y,width,height);
}
bool Has(const Fixture& fixture,const char* event) {
    for(const auto& value:fixture.events) if(value==event) return true;
    return false;
}
}
int main() {
    try {
        Fixture fixture;Run(fixture);
        Require(fixture.events==std::vector<std::string>{"write.bConfigured","read.bIsVisible","read.Width","read.Height",
            "read.bNeedsReconfigure","write.bNeedsReconfigure","write.X","write.Y","write.Width","write.Height","clip?","clip","resort"},
            "Unchanged visible ConfigureChild statement order changed");
        Require(fixture.floats.at("X")==1 && fixture.floats.at("Y")==2 && fixture.flags.at("bConfigured") &&
            fixture.flags.at("bNeedsQuery") && !Has(fixture,"change"),"Position-only configure invented size/query effects");
        fixture=Fixture{};fixture.flags.at("bIsVisible")=false;Run(fixture,-9,-8,-7,-6);
        Require(fixture.events==std::vector<std::string>{"write.bConfigured","read.bIsVisible"} &&
            fixture.floats.at("Width")==10 && fixture.floats.at("X")==3 && fixture.flags.at("bConfigured"),
            "Locally hidden configure touched geometry/callback/clip");
        for(bool sizeChanges:{false,true}) {
            fixture=Fixture{};fixture.flags.at("bNeedsReconfigure")=!sizeChanges;
            fixture.callback=[](Fixture& self) {
                Require(self.flags.at("bConfigured") && !self.flags.at("bNeedsReconfigure") && self.floats.at("X")==1,
                    "ConfigurationChanged ran before geometry/flag writes");
                self.flags.at("bNeedsReconfigure")=true;self.flags.at("bNeedsQuery")=false;
                self.flags.at("bIsVisible")=false;
            };
            Run(fixture,1,2,sizeChanges?11:10,20);
            Require(Has(fixture,"change") && Has(fixture,"clip") && fixture.flags.at("bNeedsReconfigure") &&
                !fixture.flags.at("bNeedsQuery"),"Callback dirtiness/visibility was overwritten or rechecked");
            Require(fixture.events.back()=="resort","Clipping/resort did not follow callback");
        }
        for(unsigned type=0u;type<256u;++type) for(unsigned depth:{1u,2u,128u}) {
            fixture=Fixture{};fixture.type=static_cast<std::uint8_t>(type);fixture.depth=depth;Run(fixture);
            Require(Has(fixture,"clip")== (depth==1u || type>=1u) && Has(fixture,"resort")==Has(fixture,"clip"),
                "Native configure nesting/type clip gate changed");
        }
        fixture=Fixture{};Run(fixture,-4,-3,-2,-1);
        Require(fixture.floats.at("Width")==-2 && fixture.floats.at("Height")==-1 && Has(fixture,"change"),
            "Negative ConfigureChild geometry was clamped or rejected");
        fixture=Fixture{};fixture.floats.at("Width")=0;fixture.floats.at("Height")=0;Run(fixture,-0.0f,-0.0f,-0.0f,-0.0f);
        Require(!Has(fixture,"change") && Raw(fixture.floats.at("Width"))==0x80000000u &&
            Raw(fixture.floats.at("Height"))==0x80000000u && Raw(fixture.floats.at("X"))==0x80000000u,
            "Numeric zero equality suppressed unconditional current geometry writes");
        for(float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) for(unsigned slot=0u;slot<4u;++slot) {
            fixture=Fixture{};const auto beforeFloats=fixture.floats;const auto beforeFlags=fixture.flags;
            std::array<float,4u> values{1,2,10,20};values[slot]=bad;bool refused{};
            try {Run(fixture,values[0u],values[1u],values[2u],values[3u]);} catch(const std::runtime_error&) {refused=true;}
            Require(refused && fixture.events.empty() && fixture.floats==beforeFloats && fixture.flags==beforeFlags,
                "Nonfinite copied geometry mutated state before refusal");
        }
        fixture=Fixture{};fixture.callback=[](Fixture&) {throw std::runtime_error("unsupported callback");};
        bool refused{};try {Run(fixture,1,2,11,20);} catch(const std::runtime_error&) {refused=true;}
        Require(refused && Has(fixture,"change") && !Has(fixture,"clip?") && !Has(fixture,"resort"),
            "Callback failure continued clipping or was swallowed");
        // Rollback belongs to runtime integration, not this pure dispatch helper.
        std::cout<<"Native configure geometry tests passed: "<<checks<<" checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
