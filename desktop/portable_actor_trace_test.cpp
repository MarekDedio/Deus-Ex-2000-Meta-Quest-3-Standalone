#include "Precomp.h"
#include "GC/GC.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

// Production native277 on the actual first Training map only. This deliberately
// leaves the world dormant and its actor hash absent: world BSP hits/misses are
// proven here, NOT actor-hash hits, weapon damage, startup or headset gameplay.
// The independent existing SingleLine integration owns actor-hash selection
// coverage. No original commercial executable or DLL is loaded/executed.
namespace {
namespace fs=std::filesystem;
namespace Vm=QuestVr::Vm;
using Vm::Evaluation;using Vm::Value;using Vm::Kind;
using Point=std::array<float,3u>;
using Bytes=std::vector<std::uint8_t>;
using Command=PortableRetailCollisionCommand;
using Operation=PortableRetailCollisionOperation;
constexpr std::uint32_t InitialSeed=0x13579bdfu;
constexpr const char* Doctor="00_Training.Doctor1";
constexpr const char* Crate="00_Training.CrateBreakableMedGeneral1";
std::size_t checks{},refusals{},queries{},workCuts{};

void Require(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
bool Within(const fs::path& child,const fs::path& parent){
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
bool SamePoint(const Point& a,const Point& b){
    for(std::size_t i=0u;i<3u;++i)if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;
    return true;
}
void VectorIs(const Value& value,const Point& expected,const std::string& message){
    Require(value.kind==Kind::Vector&&SamePoint(value.vector,expected),message);
}
std::uint64_t Fingerprint(const fs::path& path){
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned source "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t size{};
    while(file){file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        size+=static_cast<std::uintmax_t>(count);Require(size<=512u*1024u*1024u,"Owned source fingerprint exceeded bound");
        for(std::streamsize i=0;i<count;++i){hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    Require(file.eof(),"Owned source fingerprint did not finish");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size;fs::file_time_type modified;std::uint64_t fingerprint;
    explicit Source(fs::path value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),fingerprint(Fingerprint(path)){}
    void Unchanged()const{Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==fingerprint,
        "Trace changed owned source bytes/size/mtime: "+path.string());}
};
struct Temporary {
    fs::path parent,path;
    explicit Temporary(const fs::path& source){
        parent=fs::canonical(fs::temp_directory_path());Require(parent!=parent.root_path()&&!Within(parent,source),"Unsafe Trace checkpoint parent");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto candidate=parent/("quest-actor-trace-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){path=fs::canonical(candidate);break;}}
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create isolated Trace checkpoint directory");
    }
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;
        if(!path.empty()&&path.parent_path()==parent){fs::remove(path/"state.sav",error);fs::remove(path,error);}}
    Bytes Save(){
        const auto file=path/"state.sav";Require(SavePortableRuntimeState(file.string()),"Cannot save complete Trace checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(file.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read complete Trace checkpoint");return bytes;
    }
};
Value Declared(const std::string& actor,const char* key,Kind kind){
    auto value=ReadPortableActorDeclaredScriptProperty(actor,key);Require(value.kind==kind,"Trace declared field has wrong kind");return value;
}
struct Output {
    Point initial{-0.0f,-37.0f,91.0f};Value value=Value::Vector(initial);
    std::size_t reads{},writes{};bool throwRead{},throwWrite{};
};
using Outputs=std::array<std::shared_ptr<Output>,2u>;
Outputs NewOutputs(){return {std::make_shared<Output>(),std::make_shared<Output>()};}
std::shared_ptr<Vm::Reference> Reference(const std::shared_ptr<Output>& storage){
    auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Vector({});
    reference->resultLayout=Vm::ResultLayout::CoreVector12;
    reference->read=[storage]{++storage->reads;if(std::exchange(storage->throwRead,false))throw std::runtime_error("Trace external OUT read fault");return storage->value;};
    reference->write=[storage](const Value& value){++storage->writes;storage->value=value;
        if(std::exchange(storage->throwWrite,false))throw std::runtime_error("Trace external OUT store fault");};
    return reference;
}
std::vector<Evaluation> Packet(const Outputs& outputs,Point end,Point start,bool actors=false,Point extent={}){
    return {{{},Reference(outputs[0u])},{{},Reference(outputs[1u])},{Value::Vector(end),{}},
        {Value::Vector(start),{}},{Value::Bool(actors),{}},{Value::Vector(extent),{}}};
}
Vm::Result Call(const std::string& actor,const std::vector<Evaluation>& arguments,const Vm::Limits& limits={}){
    ++queries;return ExecutePortableActorFunction(actor,"Engine.Actor.Trace",arguments,limits);
}
PortableRetailSingleLineSnapshot Expected(const std::string& source,Point end,Point start,bool actors,Point extent={}){
    Command command{Operation::SingleLineCheck};command.actorPath=source;command.end=end;command.start=start;
    command.extent=extent;command.flags=static_cast<std::uint8_t>((actors?0x31u:0u)+6u);
    const auto result=ProbePortableRuntimeRetailCollision({command});
    Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.singles.size()==1u&&
        !result.singles.front().marksAfter&&!result.singles.front().arenasAfter,
        "Reference actual-Level SingleLine query failed: "+result.execution.error);
    return result.singles.front();
}
void CheckResult(const Vm::Result& result,const Outputs& outputs,const PortableRetailSingleLineSnapshot& expected,
    const std::string& label){
    Require(result.passed()&&result.committed&&result.value.kind==Kind::Object&&result.writes==2u&&
        result.value.text==(expected.clear?std::string{}:expected.hit.actorPath)&&result.nativeWork&&!result.nativeWork->exhausted,
        "Production Trace failed or returned a wrong actual hit: "+label+" "+result.error);
    VectorIs(outputs[0u]->value,expected.clear?Point{}:expected.hit.location,"Trace copied wrong hit Location or miss zero: "+label);
    VectorIs(outputs[1u]->value,expected.clear?Point{}:expected.hit.normal,"Trace copied wrong hit Normal or miss zero: "+label);
    for(const auto& output:outputs)Require(output->reads==1u&&output->writes==1u,
        "Trace read old OUT before its sole journal/store or published more than once: "+label);
}
void Original(const fs::path& root){
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> scripts;
    for(const auto* name:QuestVr::RuntimeScriptPackages){sources.emplace_back(root/"System"/(std::string(name)+".u"));
        scripts.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"System"/"DeusEx.ini");const auto ini=sources.back().path.string();
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(scripts.size()==38u&&InitializePortableRuntime(scripts,InitialSeed).passed,"Cannot initialize exact production Quest script packages");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini).prepared,
        "Cannot prepare actual Engine/metadata/config/Packages/lookup/Cylinder services");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load actual first Training map for Trace");
    const auto native=ReadPortableRuntimeNativeModel();const auto engine=ReadPortableRuntimeEngineCollision();
    const auto world=GetPortableRuntimeMapActors(true);
    Require(native.available&&native.mapGeneration&&engine.boundLevelPath==native.levelPath&&!world.empty(),
        "Trace lacks the real current native Level/Model/Engine binding");
    const auto sourceIdentity=ReadPortableRuntimeObjectOwnership(Doctor),crateIdentity=ReadPortableRuntimeObjectOwnership(Crate);
    Require(sourceIdentity.objectIndex&&crateIdentity.objectIndex&&sourceIdentity.objectIndex!=crateIdentity.objectIndex,
        "Trace source/target are not actual distinct Training UObjects");
    const auto baseline=temporary.Save();const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto dirty=GetPortableRuntimeUnsavedScriptState(),present=GetPortableRuntimeScriptStatePresent();
    const auto unchanged=[&]{
        GC::Collect();const auto currentGc=GC::GetStats();const auto currentLookup=ReadPortableRuntimeObjectLookup();
        const auto collision=ReadPortableRuntimeRetailCollision();const auto currentNative=ReadPortableRuntimeNativeModel();
        Require(temporary.Save()==baseline&&GetPortableRuntimeWorldRevision()==revision&&
            GetPortableRuntimeUnsavedScriptState()==dirty&&GetPortableRuntimeScriptStatePresent()==present&&
            currentGc.numObjects==gc.numObjects&&currentGc.memoryUsage==gc.memoryUsage&&
            currentLookup.prepared==lookup.prepared&&currentLookup.generation==lookup.generation&&
            currentLookup.objects==lookup.objects&&currentLookup.retainedBytes==lookup.retainedBytes&&
            currentNative.mapGeneration==native.mapGeneration&&currentNative.levelPath==native.levelPath&&
            currentNative.modelPath==native.modelPath&&GetPortableRuntimeMapActors(true).size()==world.size()&&
            !collision.initialized&&!collision.enabled&&!collision.hashIdentity&&collision.actors.empty()&&
            collision.seed==InitialSeed&&!collision.initializationDraws&&!collision.lifecycleOperations&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
            Declared("00_Training.LevelInfo0","Engine.LevelInfo.Game",Kind::Object).text.empty()&&
            ReadPortableRuntimeLevelTime()==0.0,
            "Trace published dormant world/save/revision/lookup/GC/hash/RNG or startup state");
    };
    unchanged();
    auto start=Declared(Crate,"Engine.Actor.Location",Kind::Vector).vector;start[2u]+=128.0f;
    auto end=start;end[2u]-=512.0f;
    const auto floor=Expected(Doctor,end,start,false);
    Require(!floor.clear&&floor.hit.actorPath=="00_Training.LevelInfo0"&&floor.hit.primitivePath==native.modelPath,
        "Known first Training downward ray lacks its genuine loader-Model floor hit");unchanged();
    std::size_t completeWork{};
    for(const auto* source:{Doctor,Crate})for(const bool actors:{false,true})for(const bool box:{false,true}){
        const Point extent=box?Point{4,4,4}:Point{};const auto expected=Expected(source,end,start,actors,extent);unchanged();
        auto outputs=NewOutputs();const auto result=Call(source,Packet(outputs,end,start,actors,extent));
        CheckResult(result,outputs,expected,"actual dormant source/world "+std::string(source));
        Require(!expected.clear&&expected.hit.actorPath=="00_Training.LevelInfo0","Trace world-hit cohort lost its positive BSP witness");
        unchanged();
        if(source==Doctor&&!actors&&!box)completeWork=result.nativeWork->used;
    }
    auto clearEnd=start;clearEnd[2u]+=8.0f;const auto clear=Expected(Doctor,clearEnd,start,false);
    Require(clear.clear,"Known first Training upward ray did not provide a genuine clear Model query");unchanged();
    {auto outputs=NewOutputs();const auto result=Call(Doctor,Packet(outputs,clearEnd,start));CheckResult(result,outputs,clear,"genuine clear world");unchanged();}
    const auto defaultStart=Declared(Doctor,"Engine.Actor.Location",Kind::Vector).vector;
    const bool defaultActors=Declared(Doctor,"Engine.Actor.bCollideActors",Kind::Bool).boolean;
    auto defaultEnd=defaultStart;defaultEnd[2u]-=512.0f;
    const auto defaults=Expected(Doctor,defaultEnd,defaultStart,defaultActors);unchanged();
    // The actual receiver supplies Location/bCollideActors at sequential native
    // Step boundaries; no player, startup, external pose or hash is invented.
    for(std::size_t count=3u;count<=6u;++count){
        auto outputs=NewOutputs();auto packet=Packet(outputs,defaultEnd,start);packet.resize(count);
        for(std::size_t slot=3u;slot<count;++slot)packet[slot]={};
        const auto result=Call(Doctor,packet);CheckResult(result,outputs,defaults,"missing/trailing explicit Nothing defaults");unchanged();
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill}){
        auto outputs=NewOutputs();auto packet=Packet(outputs,end,start);
        packet[0u].disposition=disposition;packet[0u].zeroFillBytes=12u;
        const auto result=Call(Doctor,packet);
        Require(result.passed()&&result.committed&&result.writes==1u&&result.value.kind==Kind::Object&&result.value.text==floor.hit.actorPath&&
            !outputs[0u]->reads&&!outputs[0u]->writes,"Unwritten OUT destination was not discarded without old-value reads");
        VectorIs(outputs[0u]->value,outputs[0u]->initial,"Disposable OUT changed caller storage");
        VectorIs(outputs[1u]->value,floor.hit.normal,"Trace disposable Location changed Normal output");unchanged();
    }
    {
        auto one=std::make_shared<Output>();const Outputs outputs{one,one};const auto result=Call(Doctor,Packet(outputs,end,start));
        Require(result.passed()&&result.writes==2u&&one->reads==2u&&one->writes==2u,"Trace equal OUT aliases did not retain both ordered stores");
        VectorIs(one->value,floor.hit.normal,"Trace equal OUT alias did not retain later Normal");unchanged();
    }
    {
        auto outputs=NewOutputs();auto packet=Packet(outputs,end,start);packet[0u]={Value::Integer(123),{}};packet[1u]={};
        const auto result=Call(Doctor,packet);Require(result.passed()&&result.committed&&!result.writes&&
            result.value.kind==Kind::Object&&result.value.text==floor.hit.actorPath,"Non-lvalue Trace OUTs were not disposable native locals");unchanged();
    }
    for(std::size_t cut=0u;cut<2u;++cut){
        auto outputs=NewOutputs();Vm::Limits limits;limits.writes=cut;const auto result=Call(Doctor,Packet(outputs,end,start),limits);
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Budget&&result.writes==cut,
            "Trace ignored an exact shared external-OUT write cutoff");
        for(const auto& output:outputs)VectorIs(output->value,output->initial,"Trace write cutoff did not restore exact external OUT bits");
        Require(outputs[0u]->reads==cut&&outputs[1u]->reads==0u,"Trace write cutoff read an unreached OUT journal value");
        ++refusals;unchanged();
    }
    {
        auto one=std::make_shared<Output>();const Outputs outputs{one,one};Vm::Limits limits;limits.writes=1u;
        const auto result=Call(Doctor,Packet(outputs,end,start),limits);
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Budget&&result.writes==1u&&
            one->reads==1u&&one->writes==2u,"Trace alias cutoff did not journal/store then undo its exact first publication");
        VectorIs(one->value,one->initial,"Trace aliased output cutoff retained a hit Location prefix");++refusals;unchanged();
    }
    // Warm and repeat the exact same query before deriving its shared-work
    // allowance: immutable decoding cache warmth must not masquerade as a cut.
    {auto outputs=NewOutputs();const auto result=Call(Doctor,Packet(outputs,end,start));CheckResult(result,outputs,floor,"warm work baseline");
        completeWork=result.nativeWork->used;unchanged();}
    Require(completeWork>0u&&completeWork<=8192u,"Trace world query native-work ledger is absent or unexpectedly unbounded");
    for(std::size_t cut=0u;cut<completeWork;++cut){
        auto outputs=NewOutputs();Vm::Limits limits;limits.nativeWork=cut;const auto result=Call(Doctor,Packet(outputs,end,start),limits);
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Budget&&result.nativeWork&&
            result.nativeWork->exhausted&&result.nativeWork->used==cut&&!result.writes,
            "Trace did not exhaust the same native-work prefix before publishing OUTs");
        for(const auto& output:outputs){Require(!output->reads&&!output->writes,"Trace native-work cutoff eagerly read/wrote old OUT");
            VectorIs(output->value,output->initial,"Trace native-work cutoff changed external OUT bits");}
        ++refusals;++workCuts;unchanged();
    }
    {auto outputs=NewOutputs();Vm::Limits limits;limits.nativeWork=completeWork;const auto result=Call(Doctor,Packet(outputs,end,start),limits);
        CheckResult(result,outputs,floor,"exact N shared-work success");Require(result.nativeWork->used==completeWork,"Trace exact N query changed work ledger");unchanged();}
    for(const bool storeFault:{false,true}){
        auto outputs=NewOutputs();if(storeFault)outputs[1u]->throwWrite=true;else outputs[1u]->throwRead=true;
        const auto result=Call(Doctor,Packet(outputs,end,start));
        Require(!result.passed()&&!result.committed&&result.writes==2u&&result.nativeWork&&result.nativeWork->used==completeWork,
            "Late external OUT fault did not fail after detached query and first publication");
        for(const auto& output:outputs)VectorIs(output->value,output->initial,"Late Trace external fault failed exact journal rollback");
        ++refusals;unchanged();
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})for(const unsigned fill:{0u,4u,8u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs,end,start);packet[2u].disposition=disposition;
        packet[2u].zeroFillBytes=static_cast<std::uint8_t>(fill);const auto result=Call(Doctor,packet);
        Require(!result.passed()&&!result.committed&&!result.writes,"Trace fabricated undefined/partial uninitialized TraceEnd");
        for(const auto& output:outputs)Require(!output->reads&&!output->writes,"Undefined TraceEnd eagerly read old OUT");
        ++refusals;unchanged();
    }
    {
        auto outputs=NewOutputs();auto packet=Packet(outputs,end,start);packet[2u].disposition=Vm::ResultDisposition::ZeroFill;packet[2u].zeroFillBytes=12u;
        const auto expected=Expected(Doctor,{},start,false);unchanged();
        const auto result=Call(Doctor,packet);CheckResult(result,outputs,expected,"fully defined zero-fill TraceEnd");unchanged();
    }
    {
        auto outputs=NewOutputs();auto packet=Packet(outputs,end,start);packet[2u].value.vector[1u]=std::numeric_limits<float>::quiet_NaN();
        for(auto& output:outputs)output->value.vector[0u]=std::numeric_limits<float>::quiet_NaN();
        const auto result=Call(Doctor,packet);Require(!result.passed()&&!result.committed&&!result.writes,
            "Trace accepted a nonfinite active endpoint");
        for(const auto& output:outputs)Require(!output->reads&&!output->writes,"Invalid query read/validated inactive old OUT contents");
        ++refusals;unchanged();
    }
    for(const std::size_t slot:{3u,5u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs,end,start);packet[slot].value.vector[2u]=std::numeric_limits<float>::infinity();
        const auto result=Call(Doctor,packet);Require(!result.passed()&&!result.committed&&!result.writes,
            "Trace accepted a nonfinite active Start/Extent input");
        for(const auto& output:outputs)Require(!output->reads&&!output->writes,"Invalid Start/Extent query eagerly read old OUT");
        ++refusals;unchanged();
    }
    for(const auto& source:sources)source.Unchanged();
    std::cout<<"PASS owned first Training Actor.Trace277: "<<queries<<" production calls, "<<workCuts
        <<" exact shared native-work cutoffs, "<<checks<<" checks / "<<refusals<<" refusals; genuine dormant current-Level world hits/misses, "
        "authored receiver defaults, OUT aliases/discards/publication/rollback and whole save/revision/lookup/GC/hash/RNG preservation. "
        "NOT actor-hash hit, weapon damage, startup, crate gameplay or headset evidence.\n";
}
} // namespace
int main(int argc,char** argv){
    try{
        fs::path root;if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT");configured&&*configured)root=configured;
        for(int i=1;i<argc;++i){const std::string_view option=argv[i];
            if((option=="--owned-source-root"||option=="--game-root")&&i+1<argc)root=argv[++i];
            else if(argc==2)root=argv[i];else throw std::runtime_error("Usage: portable_actor_trace_test [--owned-source-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");}
        if(root.empty()){std::cout<<"SKIP owned first Training Actor.Trace277: supply original game root\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Original(fs::canonical(root));
        ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,"Actor.Trace test leaked rooted native UObjects after shutdown");
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL Actor.Trace277: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
