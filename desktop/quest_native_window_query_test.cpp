#include "quest_native_window_query.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
std::size_t checks{};
void Require(bool condition,const char* message) {++checks;if(!condition) throw std::runtime_error(message);}
struct Operations {
    std::map<std::string,float> floats{{"hardcodedWidth",-1.0f},{"hardcodedHeight",-1.0f},
        {"lastSpecifiedWidth",0.0f},{"lastSpecifiedHeight",0.0f},{"lastQueryWidth",0.0f},{"lastQueryHeight",0.0f}};
    std::map<std::string,bool> flags{{"bNeedsQuery",true},{"bLastWidthSpecified",true},{"bLastHeightSpecified",true}};
    std::vector<std::string> trace;
    std::function<void(bool,float&,bool,float&)> callback;
    std::function<float(bool)> fallback;
    float ReadFloat(const char* name) {trace.push_back("read:"+std::string(name));return floats.at(name);}
    bool ReadBool(const char* name) {trace.push_back("read:"+std::string(name));return flags.at(name);}
    void WriteFloat(const char* name,float value) {trace.push_back("write:"+std::string(name));floats.at(name)=value;}
    void WriteBool(const char* name,bool value) {trace.push_back("write:"+std::string(name));flags.at(name)=value;}
    void ParentRequestedPreferredSize(bool ws,float& w,bool hs,float& h) {
        trace.push_back("callback");if(callback) callback(ws,w,hs,h);
    }
    float FallbackSize(bool width) {trace.push_back(width?"fallback:w":"fallback:h");return fallback?fallback(width):(width?10.0f:11.0f);}
};
std::vector<std::string> Writes(const Operations& operations) {
    std::vector<std::string> writes;
    for(const auto& item:operations.trace) if(item.starts_with("write:")) writes.push_back(item);
    return writes;
}
bool RawEqual(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
}
int main() {
    try {
        using QuestVr::QueryNativeWindowPreferredSize;
        for(float requestedWidth:{-20.0f,-1.0f,-0.0f,0.0f,44.0f})
            for(float requestedHeight:{-30.0f,-1.0f,-0.0f,0.0f,54.0f}) {
            Operations operations;operations.floats["hardcodedWidth"]=requestedWidth;
            operations.floats["hardcodedHeight"]=requestedHeight;
            const bool widthSpecified=requestedWidth>=0.0f,heightSpecified=requestedHeight>=0.0f;
            operations.callback=[&](bool ws,float& w,bool hs,float& h) {
                Require(ws==widthSpecified && hs==heightSpecified,"Callback specified flags changed");
                Require(RawEqual(w,ws?requestedWidth:-1.0f) && RawEqual(h,hs?requestedHeight:-1.0f),
                    "Callback packet lost requested or original -1 unspecified values");
                Require(!operations.flags.at("bNeedsQuery") && operations.flags.at("bLastWidthSpecified")==ws &&
                    operations.flags.at("bLastHeightSpecified")==hs,"Cache flags were not stored before callback");
                w=111.0f;h=222.0f; // Specified dimensions must ignore modified callback packet values.
            };
            auto result=QueryNativeWindowPreferredSize(operations);
            Require(RawEqual(result.width,widthSpecified?requestedWidth:111.0f) &&
                RawEqual(result.height,heightSpecified?requestedHeight:222.0f),"Preferred-size promotion or callback outputs differ");
            const auto writes=Writes(operations);
            if(widthSpecified && heightSpecified) {
                Require(writes.empty() && operations.trace==std::vector<std::string>{"read:hardcodedWidth","read:hardcodedHeight"},
                    "Both specified dimensions touched cache/callback/fallback");
            } else {
                Require(writes==std::vector<std::string>{"write:lastSpecifiedHeight","write:bLastWidthSpecified",
                    "write:bLastHeightSpecified","write:bNeedsQuery","write:lastSpecifiedWidth","write:lastQueryHeight","write:lastQueryWidth"},
                    "Native cache store order changed");
                operations.trace.clear();const auto repeated=QueryNativeWindowPreferredSize(operations);
                Require(RawEqual(result.width,repeated.width) && RawEqual(result.height,repeated.height) && Writes(operations).empty() &&
                    std::find(operations.trace.begin(),operations.trace.end(),"callback")==operations.trace.end(),
                    "Clean unchanged request missed native cached result");
            }
        }
        {
            Operations operations;float textureWidth=32.0f;
            operations.fallback=[&](bool width) {return width?textureWidth:16.0f;};
            auto result=QueryNativeWindowPreferredSize(operations);
            Require(result.width==32.0f && result.height==16.0f && operations.floats.at("lastQueryWidth")==-1.0f &&
                operations.floats.at("lastQueryHeight")==-1.0f,"Fallback polluted pre-fallback query cache");
            textureWidth=64.0f;operations.trace.clear();result=QueryNativeWindowPreferredSize(operations);
            Require(result.width==64.0f && result.height==16.0f && Writes(operations).empty() &&
                operations.trace[operations.trace.size()-2u]=="fallback:w" && operations.trace.back()=="fallback:h",
                "Cached negative result failed to read actual changing background each query");
        }
        {
            Operations operations;operations.callback=[&](bool,float& w,bool,float& h) {
                operations.flags["bNeedsQuery"]=true;w=0.0f;h=-0.0f;
            };
            const auto result=QueryNativeWindowPreferredSize(operations);
            Require(RawEqual(result.width,0.0f) && RawEqual(result.height,-0.0f) && operations.flags.at("bNeedsQuery"),
                "Callback dirty flag or signed zero outputs were overwritten");
            operations.trace.clear();QueryNativeWindowPreferredSize(operations);
            Require(std::count(operations.trace.begin(),operations.trace.end(),"callback")==1,
                "Callback re-dirtied query failed to request another callback");
        }
        {
            Operations operations;operations.floats["hardcodedWidth"]=0.0f;
            operations.callback=[](bool,float&,bool,float& h) {h=3.0f;};
            QueryNativeWindowPreferredSize(operations);operations.trace.clear();
            operations.floats["hardcodedWidth"]=-0.0f;
            const auto result=QueryNativeWindowPreferredSize(operations);
            Require(RawEqual(result.width,0.0f) && result.height==3.0f && Writes(operations).empty(),
                "Signed-zero request equality should reuse prior cache bits without writes");
        }
        {
            Operations operations;unsigned callbacks{};
            operations.callback=[&](bool,float& w,bool,float& h) {
                ++callbacks;
                const auto recursive=QueryNativeWindowPreferredSize(operations);
                Require(recursive.width==0.0f && recursive.height==0.0f && callbacks==1u,
                    "Reentrant same-size query failed to observe cache flags before callback");
                w=4.0f;h=5.0f;
            };
            const auto result=QueryNativeWindowPreferredSize(operations);
            Require(result.width==4.0f && result.height==5.0f && callbacks==1u,
                "Reentrant query changed the outer callback outputs");
        }
        {
            Operations operations;operations.callback=[&](bool,float& w,bool,float& h) {
                operations.floats["hardcodedWidth"]=541.0f;operations.floats["hardcodedHeight"]=69.0f;
                w=12.0f;h=13.0f;
            };
            const auto result=QueryNativeWindowPreferredSize(operations);
            Require(result.width==12.0f && result.height==13.0f,
                "Query reloaded request fields after callback changed them");
        }
        for(unsigned changed=0u;changed<5u;++changed) {
            Operations operations;operations.callback=[](bool,float& w,bool,float& h) {w=2.0f;h=3.0f;};
            QueryNativeWindowPreferredSize(operations);operations.trace.clear();
            switch(changed) {
            case 0:operations.flags["bNeedsQuery"]=true;break;
            case 1:operations.flags["bLastWidthSpecified"]=true;break;
            case 2:operations.flags["bLastHeightSpecified"]=true;break;
            case 3:operations.floats["lastSpecifiedWidth"]=99.0f;break;
            case 4:operations.floats["lastSpecifiedHeight"]=99.0f;break;
            }
            const auto result=QueryNativeWindowPreferredSize(operations);
            Require(result.width==2.0f && result.height==3.0f &&
                std::count(operations.trace.begin(),operations.trace.end(),"callback")==1,"One changed cache predicate failed to recompute");
        }
        {
            Operations operations;operations.floats["hardcodedHeight"]=12.0f;
            operations.callback=[](bool,float& w,bool,float& h) {w=99.0f;h=88.0f;};
            auto result=QueryNativeWindowPreferredSize(operations,true,-5.0f,false,0.0f);
            Require(result.width==-5.0f && result.height==12.0f && Writes(operations).empty(),
                "Both explicit/promoted dimensions should bypass fallback even when explicitly negative");
        }
        for(const auto bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
            for(unsigned source=0u;source<6u;++source) {
            Operations operations;bool refused{};
            if(source<2u) operations.floats[source==0u?"hardcodedWidth":"hardcodedHeight"]=bad;
            else if(source<4u) operations.callback=[=](bool,float& w,bool,float& h) {if(source==2u) w=bad;else h=bad;};
            else operations.fallback=[=](bool width) {return width==(source==4u)?bad:10.0f;};
            try {QueryNativeWindowPreferredSize(operations);}catch(const std::exception&) {refused=true;}
            Require(refused,"Nonfinite preferred size was accepted");
        }
        {
            Operations operations;operations.fallback=[](bool width) {return width?-4.0f:-5.0f;};
            const auto result=QueryNativeWindowPreferredSize(operations);
            Require(result.width==-4.0f && result.height==-5.0f,"Native fallback was incorrectly clamped to zero");
        }
        std::cout<<"Native window preferred-size query: "<<checks<<" checks passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
