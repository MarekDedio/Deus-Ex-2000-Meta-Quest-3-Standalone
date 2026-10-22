#include "quest_dump_location.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{},refusals{};
void Require(bool result,const char* why){++checks;if(!result)throw std::runtime_error(why);}
template<class F>void Refuse(F&& f){bool failed{};try{f();}catch(const std::exception&){failed=true;}Require(failed,"Invalid DumpLocation input did not refuse");++refusals;}
using namespace QuestVr::DumpLocation;
using QuestVr::NativeConfig::OrderedIni;
void Constructor() {
    unsigned calls{};const std::vector<std::string> supplied{"z.DMP","a.dmp","duplicate.DMP","duplicate.DMP"};
    const auto provider=[&](const std::string& pattern,bool files,bool directories){
        ++calls;Require(pattern=="*.DMP" && files && !directories,"Constructor changed original file query");return supplied;};
    for(const auto& source:{std::string{},std::string("[Else]\nCurrentUser=wrong\n"),std::string("[DeusEx.DumpLocation]\nOther=wrong\n")}) {
        const auto data=Construct(OrderedIni::Parse(source),provider);
        Require(data.currentUser.empty() && data.fileIndex==-1 && data.locationIndex==-1 && !data.locationCount,
            "Missing config or constructor indices did not follow audited initialization");
        Require(data.directory==supplied,"Constructor sorted/deduplicated/replaced real provider filenames");
    }
    const auto config=OrderedIni::Parse("[DeusEx.DumpLocation]\nCurrentUser=first\nCurrentUser=Second User\n");
    Require(Construct(config,provider).currentUser=="Second User","Constructor changed existing config duplicate/string behavior");
    for(const auto length:{62u,63u,64u,255u}) {
        const auto data=Construct(OrderedIni::Parse("[DeusEx.DumpLocation]\nCurrentUser="+std::string(length,'x')+"\n"),provider);
        Require(data.currentUser==std::string(std::min(length,63u),'x'),"GetString64 did not truncate at63 UTF16 units");
    }
    Require(calls==8u,"Constructor skipped or repeated a provider query");
    Refuse([&]{Construct(config,{});});
    Refuse([&]{Construct(config,[](const auto&,bool,bool)->std::vector<std::string>{throw std::runtime_error("provider failed");});});
    for(const auto& name:std::vector<std::string>{"",".","..","../bad.DMP","dir/bad.DMP","dir\\bad.DMP",std::string("bad\0.DMP",8u),std::string(MaximumNameBytes+1u,'x')})
        Refuse([&]{Construct(config,[&](const auto&,bool,bool){return std::vector<std::string>{name};});});
    Refuse([&]{Construct(config,[](const auto&,bool,bool){return std::vector<std::string>(MaximumFiles+1u,"x.DMP");});});
    Refuse([&]{Construct(config,[](const auto&,bool,bool){return std::vector<std::string>(MaximumFiles,std::string(1024u,'x'));});});
    Require(Construct(config,[](const auto&,bool,bool){return std::vector<std::string>(MaximumFiles,"x.DMP");}).directory.size()==MaximumFiles,
        "Exact directory count bound refused a valid provider result");
}
void SessionState() {
    Session session;std::array<std::uint32_t,3u> position{1u,2u,3u},rotation{4u,5u,6u};
    const auto originalPosition=position,originalRotation=rotation;
    Require(!session.saved && !session.Load(position,rotation) && position==originalPosition && rotation==originalRotation,
        "Fresh/empty LoadLocation changed destination data");
    for(const auto word:{0u,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0xffffffffu,0x00000001u}) {
        const std::array<std::uint32_t,3u> input{word,word^0x80000000u,word^0x12345678u};
        const std::array<std::uint32_t,3u> view{word^1u,word^0x87654321u,word};
        session.Save(input,view);const auto before=session;
        Construct(OrderedIni::Parse(""),[](const auto&,bool,bool){return std::vector<std::string>{};});
        Require(session==before && session.saved,"Constructing another object reset DLL-session saved-location state");
        position={};rotation={};Require(session.Load(position,rotation) && !session.saved && position==input && rotation==view,
            "Original raw pose word copies/one-shot flag changed");
        Require(session.location==input && session.view==view,"LoadLocation erased globals instead of clearing only saved flag");
        Require(!session.Load(position,rotation) && position==input && rotation==view,"Second LoadLocation did not leave destination alone");
        const auto rootBefore=session;session.Save(view,input);session=rootBefore;
        Require(session==rootBefore,"Session before-image cannot restore all native global words");
    }
    Require(OriginalHelperBytes==0x1220u,"Native constructor helper reservation changed");
}
}
int main(){try{Constructor();SessionState();std::cout<<"DumpLocation constructor/session helper: "<<checks<<" checks / "<<refusals
    <<" refusal controls; provider mocks only, NOT actual UObject/startup/file-service acceptance\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
