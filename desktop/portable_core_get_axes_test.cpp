#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "quest_original_get_axes.h"
#include "quest_original_sine_table.h"
#include "quest_runtime_script_packages.h"

#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

namespace {
namespace fs=std::filesystem;
namespace Vm=QuestVr::Vm;
using Vm::Value;using Vm::Evaluation;using Vm::Kind;
std::size_t checks{},refusals{};
void Require(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
struct Temporary {
    fs::path parent,path;
    Temporary(){
        parent=fs::canonical(fs::temp_directory_path());Require(parent!=parent.root_path(),"Unsafe checkpoint parent");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto candidate=parent/("quest-core-get-axes-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){path=fs::canonical(candidate);break;}}
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create isolated GetAxes checkpoint directory");
    }
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;
        if(!path.empty()&&path.parent_path()==parent){fs::remove(path/"state.sav",error);fs::remove(path,error);}}
    std::vector<std::uint8_t> Save(){
        const auto file=path/"state.sav";Require(SavePortableRuntimeState(file.string()),"Cannot save GetAxes checkpoint");
        const auto size=fs::file_size(file);Require(size<64u*1024u*1024u,"GetAxes checkpoint exceeded bound");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));std::ifstream in(file,std::ios::binary);
        in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));Require(bool(in),"Cannot read GetAxes checkpoint");return bytes;
    }
};
struct Output {Value value=Value::Vector({-2,-3,-4});std::size_t reads{},writes{};};
std::shared_ptr<Vm::Reference> Reference(const std::shared_ptr<Output>& storage){
    auto r=std::make_shared<Vm::Reference>();r->zero=Value::Vector({});r->resultLayout=Vm::ResultLayout::CoreVector12;
    r->read=[storage]{++storage->reads;return storage->value;};
    r->write=[storage](const Value& v){++storage->writes;storage->value=v;};return r;
}
Vm::Result Call(const std::vector<Evaluation>& args,const Vm::Limits& limits={}){
    return ExecutePortableActorFunction("00_Training.Doctor1","Core.Object.GetAxes",args,limits);
}
std::vector<Evaluation> Packet(std::array<std::int32_t,3u> a,const std::array<std::shared_ptr<Output>,3u>& outputs){
    std::vector<Evaluation> args{{Value::Rotator(a),{}}};for(const auto& p:outputs)args.push_back({{},Reference(p)});return args;
}
std::array<std::shared_ptr<Output>,3u> Outputs(){return {std::make_shared<Output>(),std::make_shared<Output>(),std::make_shared<Output>()};}
bool Bits(const Value& a,PortableModelVec3 b){
    return a.kind==Kind::Vector&&std::bit_cast<std::uint32_t>(a.vector[0])==std::bit_cast<std::uint32_t>(b.x)&&
        std::bit_cast<std::uint32_t>(a.vector[1])==std::bit_cast<std::uint32_t>(b.y)&&
        std::bit_cast<std::uint32_t>(a.vector[2])==std::bit_cast<std::uint32_t>(b.z);
}
void Original(const fs::path& root){
    Temporary temporary;std::vector<PortablePackageTables> packages;
    for(const auto* name:QuestVr::RuntimeScriptPackages)packages.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned GetAxes packages");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare actual native metadata/lookup");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed,"Cannot load first Training for Core229");
    const auto level=ReadPortableActorDeclaredScriptProperty("00_Training.Doctor1","Engine.Actor.Level");
    Require(level.kind==Kind::Object&&!level.text.empty()&&!ReadPortableActorDeclaredScriptProperty(level.text,"Engine.LevelInfo.bBegunPlay").boolean,
        "Static GetAxes controls must not manufacture begun play/player possession");
    const auto baseline=temporary.Save();const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto unchanged=[&]{const auto current=GC::GetStats();Require(temporary.Save()==baseline&&GetPortableRuntimeWorldRevision()==revision&&
        current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage,"Core229 changed world/save/revision/native ownership");};
    const QuestVr::OriginalModelCoords unit{{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
    std::size_t work{};
    for(const auto a:{std::array<std::int32_t,3u>{},std::array<std::int32_t,3u>{1234,5678,9012},
        std::array<std::int32_t,3u>{std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max(),-65537}}){
        auto output=Outputs();const auto result=Call(Packet(a,output));Require(result.passed()&&result.committed&&result.writes==3u&&
            result.value.kind==Kind::Nothing&&result.nativeWork&&!result.nativeWork->exhausted,"Actual Core229 call failed: "+result.error);
        work=result.nativeWork->used;Require(work>=15u&&work<128u,"Core229 omitted bounded shared native work");
        const auto expected=QuestVr::ComputeOriginalGetAxes({a[0],a[1],a[2]},unit,QuestVr::OriginalSineProfile::Table());
        const std::array<PortableModelVec3,3u> axes{expected.xAxis,expected.yAxis,expected.zAxis};
        for(std::size_t i=0u;i<3u;++i) {
            Require(Bits(output[i]->value,axes[i])&&output[i]->reads==1u&&output[i]->writes==1u,
                "Production Core229 output/store order or no-eager-read contract differs");
        }
        unchanged();
    }
    {
        auto one=std::make_shared<Output>();const std::array<std::shared_ptr<Output>,3u> outputs{one,one,one};
        const auto result=Call(Packet({1234,5678,9012},outputs));Require(result.passed()&&result.writes==3u&&one->reads==3u&&one->writes==3u,
            "Production aliased OUT packet was not written exactly three times");
        const auto expected=QuestVr::ComputeOriginalGetAxes({1234,5678,9012},unit,QuestVr::OriginalSineProfile::Table());
        Require(Bits(one->value,expected.zAxis),"Production Core229 all-equal OUT did not retain Z");unchanged();
    }
    {
        const auto result=Call({{Value::Rotator({}),{}},{Value::Integer(123),{}},{},{}});
        Require(result.passed()&&result.writes==0u,"Production Core229 non-lvalue outputs were not discarded");unchanged();
    }
    for(std::size_t cut=0u;cut<3u;++cut){
        auto output=Outputs();Vm::Limits limits;limits.writes=cut;const auto result=Call(Packet({1234,5678,9012},output),limits);
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Budget&&result.writes==cut,"Core229 ignored shared write prefix");
        for(const auto& o:output)Require(o->value.vector==std::array<float,3u>{-2,-3,-4},"Core229 write refusal retained external output");
        ++refusals;unchanged();
    }
    for(std::size_t cut=0u;cut<work;++cut){
        auto output=Outputs();Vm::Limits limits;limits.nativeWork=cut;const auto result=Call(Packet({},output),limits);
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Budget&&result.nativeWork&&result.nativeWork->exhausted&&result.writes==0u,
            "Core229 native-work refusal wrote output or lost root exhaustion");
        for(const auto& o:output)Require(!o->reads&&!o->writes,"Core229 read OUT before completing detached math");
        ++refusals;unchanged();
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})for(const auto fill:{0u,4u,8u,12u}){
        auto output=Outputs();auto args=Packet({},output);args[0].disposition=disposition;args[0].zeroFillBytes=static_cast<std::uint8_t>(fill);
        const auto result=Call(args);if(disposition==Vm::ResultDisposition::ZeroFill&&fill==12u)Require(result.passed(),"Core229 rejected full defined zero-fill rotation");
        else {Require(!result.passed()&&!result.committed&&result.writes==0u,"Core229 manufactured an undefined rotation");++refusals;}unchanged();
    }
    std::cout<<"PASS owned first-Training Core229 checks="<<checks<<" refusals="<<refusals<<" nativeWork="<<work
        <<"; actual Core declaration/typed aliases/ordered stores and rollback, NOT trace/hit/crate or headset acceptance\n";
}
}
int main(int argc,char** argv){try{if(argc==1){std::cout<<"SKIP owned Core229 controls: supply original game root\n";return 77;}
    Require(argc==2,"Usage: portable_core_get_axes_test OWNED_GAME_ROOT");Original(argv[1]);return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Core229: "<<e.what()<<'\n';return 1;}}
