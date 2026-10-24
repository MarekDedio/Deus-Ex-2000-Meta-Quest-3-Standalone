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
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

// Actual first Training native531 declaration/argument/list prefix. No fake
// GameInfo, begun world, collision hash, successful visibility or hit is made.
// Authentic AddPawn calls intentionally create a dormant list fixture; a final
// real checkpoint Load restores original state. No commercial DLL/EXE executes.
namespace {
namespace fs=std::filesystem;
namespace Vm=QuestVr::Vm;
using Vm::Evaluation;using Vm::Value;using Vm::Kind;
using Bytes=std::vector<std::uint8_t>;
using Point=std::array<float,3u>;
constexpr std::uint32_t InitialSeed=0x13579bdfu;
constexpr const char* Info="00_Training.LevelInfo0";
constexpr const char* Doctor="00_Training.Doctor1";
constexpr const char* Jaime="00_Training.JaimeReyes0";
constexpr const char* Crate="00_Training.CrateBreakableMedGeneral1";
std::size_t checks{},refusals{},calls{},workCuts{};
void Require(bool value,const std::string& reason){++checks;if(!value)throw std::runtime_error(reason);}
std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
bool Within(const fs::path& child,const fs::path& parent){
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t Fingerprint(const fs::path& path){
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned source "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t size{};
    while(file){file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        size+=static_cast<std::uintmax_t>(count);Require(size<=512u*1024u*1024u,"Owned PickTarget source exceeded fingerprint bound");
        for(std::streamsize i=0;i<count;++i){hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    Require(file.eof(),"Owned PickTarget source fingerprint did not finish");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size;fs::file_time_type modified;std::uint64_t fingerprint;
    explicit Source(fs::path value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),fingerprint(Fingerprint(path)){}
    void Unchanged()const{Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==fingerprint,
        "PickTarget changed owned source bytes/size/mtime: "+path.string());}
};
struct Temporary {
    fs::path parent,path;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source){
        parent=fs::canonical(fs::temp_directory_path());Require(parent!=parent.root_path()&&!Within(parent,source),"Unsafe PickTarget checkpoint parent");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto candidate=parent/("quest-pawn-pick-target-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){path=fs::canonical(candidate);break;}}
        Require(!path.empty()&&path.parent_path()==parent,"Cannot create isolated PickTarget checkpoint directory");
    }
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;
        for(const auto& file:files)if(file.parent_path()==path)fs::remove(file,error);
        if(!path.empty()&&path.parent_path()==parent)fs::remove(path,error);}
    fs::path File(const std::string& leaf){
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated PickTarget checkpoint leaf");
        const auto file=path/(leaf+".sav");if(std::find(files.begin(),files.end(),file)==files.end())files.push_back(file);return file;
    }
    Bytes Save(const std::string& leaf="state"){
        const auto file=File(leaf);Require(SavePortableRuntimeState(file.string()),"Cannot save complete PickTarget checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(file.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read complete PickTarget checkpoint");return bytes;
    }
};
Value Declared(const std::string& actor,const char* key,Kind kind){
    auto value=ReadPortableActorDeclaredScriptProperty(actor,key);Require(value.kind==kind,"PickTarget declared field has wrong kind: "+std::string(key));return value;
}
struct Output {
    Value value=Value::Float(-0.0f);float initial{-0.0f};std::size_t reads{},writes{};bool throwRead{};
};
using Outputs=std::array<std::shared_ptr<Output>,2u>;
Outputs NewOutputs(){Outputs result{std::make_shared<Output>(),std::make_shared<Output>()};
    result[1u]->initial=-913.25f;result[1u]->value=Value::Float(result[1u]->initial);return result;}
std::shared_ptr<Vm::Reference> Reference(const std::shared_ptr<Output>& storage){
    auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Float(0.0f);
    reference->read=[storage]{++storage->reads;if(std::exchange(storage->throwRead,false))throw std::runtime_error("Expected PickTarget external Float Step fault");return storage->value;};
    reference->write=[storage](const Value& value){++storage->writes;storage->value=value;};return reference;
}
std::vector<Evaluation> Packet(const Outputs& outputs,Point direction={1,0,0},Point start={}){
    return {{{},Reference(outputs[0u])},{{},Reference(outputs[1u])},{Value::Vector(direction),{}},{Value::Vector(start),{}}};
}
Vm::Result Call(const std::vector<Evaluation>& packet,const Vm::Limits& limits={},const std::string& source=Doctor){
    ++calls;return ExecutePortableActorFunction(source,"Engine.Pawn.PickTarget",packet,limits);
}
void Untouched(const Outputs& outputs){for(const auto& output:outputs)
    Require(output->value.kind==Kind::Float&&std::bit_cast<std::uint32_t>(output->value.floating)==std::bit_cast<std::uint32_t>(output->initial)&&
        !output->writes,"PickTarget changed an external Float OUT despite no accepted visible target");}
void None(const Vm::Result& result,const std::string& label){
    Require(result.passed()&&result.committed&&result.value.kind==Kind::Object&&result.value.text.empty()&&!result.writes&&
        result.nativeWork&&!result.nativeWork->exhausted,"PickTarget no-target prefix failed: "+label+" "+result.error);
}
struct Snapshot {
    Bytes bytes;std::uint64_t revision{};GCStats gc{};PortableObjectLookupState lookup;
    PortableNativeModelSnapshot native;std::vector<PortableActorSnapshot> actors;
    bool dirty{},present{},client{};
    explicit Snapshot(Temporary& temporary,const std::string& leaf="state"):
        bytes(temporary.Save(leaf)),revision(GetPortableRuntimeWorldRevision()),gc(GC::GetStats()),lookup(ReadPortableRuntimeObjectLookup()),
        native(ReadPortableRuntimeNativeModel()),actors(GetPortableRuntimeMapActors(true)),dirty(GetPortableRuntimeUnsavedScriptState()),
        present(GetPortableRuntimeScriptStatePresent()),client(HasPortableRuntimeClient()){}
    void Same(Temporary& temporary,std::optional<std::uint64_t> expectedRevision={})const{
        GC::Collect();const auto currentGc=GC::GetStats();const auto currentLookup=ReadPortableRuntimeObjectLookup();
        const auto collision=ReadPortableRuntimeRetailCollision();const auto currentNative=ReadPortableRuntimeNativeModel();
        const auto currentActors=GetPortableRuntimeMapActors(true);
        Require(temporary.Save()==bytes&&GetPortableRuntimeWorldRevision()==expectedRevision.value_or(revision)&&
            GetPortableRuntimeUnsavedScriptState()==dirty&&GetPortableRuntimeScriptStatePresent()==present&&
            HasPortableRuntimeClient()==client&&currentGc.numObjects==gc.numObjects&&currentGc.memoryUsage==gc.memoryUsage&&
            currentLookup.prepared==lookup.prepared&&currentLookup.generation==lookup.generation&&currentLookup.objects==lookup.objects&&
            currentLookup.retainedBytes==lookup.retainedBytes&&currentNative.mapGeneration==native.mapGeneration&&
            currentNative.levelPath==native.levelPath&&currentNative.modelPath==native.modelPath&&currentActors.size()==actors.size()&&
            !collision.initialized&&!collision.enabled&&!collision.hashIdentity&&collision.actors.empty()&&
            collision.seed==InitialSeed&&!collision.initializationDraws&&!collision.lifecycleOperations&&
            !Declared(Info,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&Declared(Info,"Engine.LevelInfo.Game",Kind::Object).text.empty()&&
            ReadPortableRuntimeLevelTime()==0.0,"PickTarget changed dormant world/save/revision/lookup/GC/hash/RNG/startup state");
        for(std::size_t i=0u;i<actors.size();++i)Require(currentActors[i].objectPath==actors[i].objectPath&&
            currentActors[i].classPath==actors[i].classPath&&currentActors[i].activated==actors[i].activated&&
            currentActors[i].hidden==actors[i].hidden,"PickTarget changed an actual native-slot Actor identity/activity");
    }
};
void PrefixCases(Temporary& temporary,const Snapshot& baseline,const std::string& label){
    {
        auto outputs=NewOutputs();const auto result=Call(Packet(outputs));None(result,label);Untouched(outputs);
        for(const auto& output:outputs)Require(output->reads==1u,"PickTarget failed to copy Float Step exactly once or eagerly re-read live OUT");
        baseline.Same(temporary);
    }
    {
        auto one=std::make_shared<Output>();const Outputs outputs{one,one};None(Call(Packet(outputs)),label+" equal Float aliases");Untouched(outputs);
        Require(one->reads==2u&&!one->writes,"PickTarget aliased initial Float Steps were merged, re-read or written");baseline.Same(temporary);
    }
    {
        auto outputs=NewOutputs();auto packet=Packet(outputs);packet[0u]={};packet[1u]={};
        None(Call(packet),label+" explicit Nothing initialized Float temporaries");Untouched(outputs);
        Require(!outputs[0u]->reads&&!outputs[1u]->reads,"Nothing Float fallback read discarded caller storage");baseline.Same(temporary);
    }
    for(const std::size_t slot:{0u,1u})for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
        for(const unsigned fill:{0u,1u,2u,3u,4u}){
            auto outputs=NewOutputs();auto packet=Packet(outputs);packet[slot].disposition=disposition;packet[slot].zeroFillBytes=static_cast<std::uint8_t>(fill);
            None(Call(packet),label+" Float initialized temporary partial/fullfill");Untouched(outputs);
            Require(!outputs[slot]->reads&&outputs[1u-slot]->reads==1u,"Initialized Float temporary read a NoWrite/zero-filled source alias");
            baseline.Same(temporary);
        }
    for(const std::size_t slot:{2u,3u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs);packet[slot].disposition=Vm::ResultDisposition::ZeroFill;packet[slot].zeroFillBytes=12u;
        None(Call(packet),label+" complete twelve-byte zero-filled Vector");Untouched(outputs);baseline.Same(temporary);
    }
    for(const std::size_t slot:{2u,3u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs);std::size_t reads{};
        auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Vector({});
        reference->resultLayout=Vm::ResultLayout::CoreVector12;
        reference->read=[&]{++reads;if(reads!=1u)throw std::runtime_error("Copied Vector was read more than once");
            return Value::Vector(slot==2u?Point{1,0,0}:Point{73,19,-5});};
        packet[slot]={{},reference};None(Call(packet),label+" copied Vector reference");Untouched(outputs);
        Require(reads==1u,"PickTarget did not copy each complete Vector reference exactly once");baseline.Same(temporary);
    }
    {
        auto outputs=NewOutputs();auto packet=Packet(outputs);packet[0u]={Value::Float(0.9f),{}};packet[1u]={Value::Float(2000.0f),{}};
        None(Call(packet),label+" explicit non-lvalue Float expressions");Untouched(outputs);baseline.Same(temporary);
    }
    {
        auto outputs=NewOutputs();Vm::Limits limits;limits.writes=0u;None(Call(Packet(outputs),limits),label+" zero write allowance");
        Untouched(outputs);baseline.Same(temporary);
    }
}
void RefusalCases(Temporary& temporary,const Snapshot& baseline){
    const auto reject=[&](const std::vector<Evaluation>& packet,const std::string& label,const std::string& source=Doctor){
        const auto result=Call(packet,{},source);Require(!result.passed()&&!result.committed&&!result.writes&&!result.error.empty(),
            "Invalid PickTarget packet/receiver succeeded: "+label);++refusals;baseline.Same(temporary);
    };
    for(const std::size_t size:{0u,1u,2u,3u,5u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs);packet.resize(size);reject(packet,"required four supplied expressions");Untouched(outputs);
    }
    {
        auto outputs=NewOutputs();reject(Packet(outputs),"non-Pawn actual crate borrowed as receiver",Crate);
        Untouched(outputs);Require(!outputs[0u]->reads&&!outputs[1u]->reads,"Non-Pawn receiver inspected Float arguments before authentic source admission");
    }
    for(const std::size_t slot:{0u,1u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs);packet[slot]={Value::Integer(17),{}};reject(packet,"non-Float numeric expression");Untouched(outputs);
        outputs=NewOutputs();outputs[slot]->value=Value::Integer(17);packet=Packet(outputs);reject(packet,"non-Float live OUT Step value");
        Require(outputs[slot]->value.kind==Kind::Int&&outputs[slot]->value.integer==17&&!outputs[slot]->writes,"Invalid Float reference was coerced/written");
        outputs=NewOutputs();outputs[slot]->throwRead=true;packet=Packet(outputs);reject(packet,"external initial Float Step read failure");Untouched(outputs);
        Require(outputs[slot]->reads==1u,"Failed Float Step read was swallowed or retried");
        outputs=NewOutputs();packet=Packet(outputs);packet[slot].disposition=Vm::ResultDisposition::ZeroFill;packet[slot].zeroFillBytes=5u;
        reject(packet,"over-width initialized Float zero fill");Untouched(outputs);
    }
    for(const std::size_t slot:{2u,3u})for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
        for(const unsigned fill:{0u,4u,8u}){
            auto outputs=NewOutputs();auto packet=Packet(outputs);packet[slot].disposition=disposition;packet[slot].zeroFillBytes=static_cast<std::uint8_t>(fill);
            reject(packet,"undefined/partial uninitialized Vector temporary");Untouched(outputs);
        }
    for(const std::size_t slot:{2u,3u}){
        auto outputs=NewOutputs();auto packet=Packet(outputs);packet[slot]={};reject(packet,"Nothing uninitialized Vector");Untouched(outputs);
        outputs=NewOutputs();packet=Packet(outputs);packet[slot]={Value::Float(1.0f),{}};reject(packet,"wrong-kind Vector");Untouched(outputs);
        outputs=NewOutputs();packet=Packet(outputs);packet[slot].disposition=Vm::ResultDisposition::ZeroFill;packet[slot].zeroFillBytes=13u;
        reject(packet,"over-width uninitialized Vector zero fill");Untouched(outputs);
        outputs=NewOutputs();packet=Packet(outputs);std::size_t reads{};
        auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Vector({});reference->resultLayout=Vm::ResultLayout::CoreVector12;
        reference->read=[&]() -> Value {++reads;throw std::runtime_error("Expected copied Vector Step fault");};
        packet[slot]={{},reference};reject(packet,"external copied Vector Step read fault");Untouched(outputs);
        Require(reads==1u,"Failed copied Vector Step was swallowed or retried");
    }
    for(const std::size_t slot:{0u,1u,2u,3u})for(const float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        auto outputs=NewOutputs();auto packet=Packet(outputs);
        if(slot<2u)packet[slot]={Value::Float(invalid),{}};else packet[slot].value.vector[1u]=invalid;
        reject(packet,"nonfinite active Float/Vector Step");Untouched(outputs);
    }
}
void WorkCases(Temporary& temporary,const Snapshot& baseline){
    auto outputs=NewOutputs();const auto warm=Call(Packet(outputs));None(warm,"warm real PawnList work baseline");Untouched(outputs);baseline.Same(temporary);
    const auto work=warm.nativeWork->used;Require(work>0u&&work<1024u,"PickTarget actual nonempty list lacks bounded native-work accounting");
    for(std::size_t cut=0u;cut<work;++cut){
        outputs=NewOutputs();Vm::Limits limits;limits.nativeWork=cut;const auto result=Call(Packet(outputs),limits);
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Budget&&result.nativeWork&&
            result.nativeWork->exhausted&&result.nativeWork->used==cut&&!result.writes,"PickTarget failed exact shared native-work cutoff");
        Untouched(outputs);for(const auto& output:outputs)Require(output->reads==1u,"Native-work cutoff changed initial Step count or eagerly re-read live OUT");
        ++refusals;++workCuts;baseline.Same(temporary);
    }
    outputs=NewOutputs();Vm::Limits limits;limits.nativeWork=work;const auto exact=Call(Packet(outputs),limits);
    None(exact,"exact N real PawnList allowance");Require(exact.nativeWork->used==work,"PickTarget exact N work count changed");Untouched(outputs);baseline.Same(temporary);
}
void CycleCases(Temporary& temporary,const Snapshot& baseline){
    std::optional<Vm::Result> first;
    for(unsigned repeat=0u;repeat<2u;++repeat){
        auto outputs=NewOutputs();const auto result=Call(Packet(outputs));
        Require(!result.passed()&&!result.committed&&result.status==Vm::Status::Unsupported&&!result.writes&&
            result.error=="PickTarget531 PawnList cycles or exceeds its bounded traversal"&&result.nativeWork&&
            !result.nativeWork->exhausted&&result.nativeWork->used>0u,
            "Authentic duplicate-Add cycle was normalized, silently truncated or did not refuse at its reached repeat");
        Untouched(outputs);for(const auto& output:outputs)Require(output->reads==1u,
            "Cycle refusal re-read a live Float OUT after its initial Step");
        if(first)Require(result.error==first->error&&result.function==first->function&&result.offset==first->offset&&
            result.opcode==first->opcode&&result.callStack==first->callStack&&result.nativeWork->used==first->nativeWork->used,
            "Repeated PickTarget cycle refusal changed its exact boundary/native-work prefix");
        else first=result;
        ++refusals;baseline.Same(temporary);
    }
    // Refusing at the repeated link must still share, not reset, the ordinary
    // root allowance. A shorter budget fails BEFORE that reached cycle test.
    auto outputs=NewOutputs();Vm::Limits limits;limits.nativeWork=first->nativeWork->used-1u;
    const auto cut=Call(Packet(outputs),limits);
    Require(!cut.passed()&&!cut.committed&&cut.status==Vm::Status::Budget&&!cut.writes&&cut.nativeWork&&
        cut.nativeWork->exhausted&&cut.nativeWork->used==limits.nativeWork,
        "Cyclic PawnList bypassed its enclosing native-work allowance");
    Untouched(outputs);++refusals;++workCuts;baseline.Same(temporary);
}
void Original(const fs::path& root){
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> scripts;
    for(const auto* name:QuestVr::RuntimeScriptPackages){sources.emplace_back(root/"System"/(std::string(name)+".u"));
        scripts.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"System"/"DeusEx.ini");const auto ini=sources.back().path.string();
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(scripts.size()==38u&&sources.size()==40u&&InitializePortableRuntime(scripts,InitialSeed).passed,
        "Cannot initialize exact shared Quest script packages for PickTarget");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini).prepared,
        "Cannot prepare actual Engine/metadata/config/Packages/lookup/Cylinder services");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load actual first Training for PickTarget");
    const auto native=ReadPortableRuntimeNativeModel();const auto engine=ReadPortableRuntimeEngineCollision();
    Require(native.available&&native.mapGeneration&&engine.boundLevelPath==native.levelPath,"PickTarget lacks native current-Level authority");
    const auto actors=GetPortableRuntimeMapActors(true);
    for(const auto* pawn:{Doctor,Jaime})Require(std::any_of(actors.begin(),actors.end(),[&](const auto& row){return row.objectPath==pawn&&row.pawn;}),
        "PickTarget lacks an actual authored first-Training Pawn");
    Require(Declared(Info,"Engine.LevelInfo.PawnList",Kind::Object).text.empty(),"Untouched Training invented an initialized PawnList");
    const Snapshot original(temporary,"original");original.Same(temporary);
    PrefixCases(temporary,original,"empty genuine PawnList");RefusalCases(temporary,original);
    const auto add=[&](const char* pawn){
        const auto revision=GetPortableRuntimeWorldRevision();const auto result=ExecutePortableActorFunction(pawn,"Engine.Pawn.AddPawn");
        Require(result.passed()&&result.committed&&GetPortableRuntimeWorldRevision()==revision+1u,
            "Original dormant Pawn.AddPawn fixture call failed: "+result.error);
    };
    add(Doctor);Require(Declared(Info,"Engine.LevelInfo.PawnList",Kind::Object).text==Doctor&&
        Declared(Doctor,"Engine.Pawn.nextPawn",Kind::Object).text.empty(),"Authentic Doctor AddPawn failed actual prepend");
    const Snapshot self(temporary);PrefixCases(temporary,self,"actual self-only PawnList");WorkCases(temporary,self);
    Require(Declared(Jaime,"Engine.Pawn.Health",Kind::Int).integer>0&&Declared(Jaime,"Engine.Actor.bProjTarget",Kind::Bool).boolean&&
        Declared(Info,"Engine.LevelInfo.Game",Kind::Object).text.empty(),"Actual nonself candidate must reach original Game=None gate");
    add(Jaime);Require(Declared(Info,"Engine.LevelInfo.PawnList",Kind::Object).text==Jaime&&
        Declared(Jaime,"Engine.Pawn.nextPawn",Kind::Object).text==Doctor,"Authentic Jaime AddPawn failed actual list order");
    const Snapshot nonself(temporary);PrefixCases(temporary,nonself,"actual nonself PawnList skipped at Game=None");
    WorkCases(temporary,nonself);RefusalCases(temporary,nonself);
    // Original AddPawn deliberately does not deduplicate. Re-adding Doctor
    // while Jaime->Doctor is already linked authentically creates a two-Pawn
    // cycle; no save injection, forged object/link or begun-play state is used.
    add(Doctor);Require(Declared(Info,"Engine.LevelInfo.PawnList",Kind::Object).text==Doctor&&
        Declared(Doctor,"Engine.Pawn.nextPawn",Kind::Object).text==Jaime&&
        Declared(Jaime,"Engine.Pawn.nextPawn",Kind::Object).text==Doctor,
        "Authentic repeated AddPawn did not preserve its original two-Pawn cycle consequence");
    const Snapshot cycle(temporary);CycleCases(temporary,cycle);
    const auto fixtureRevision=GetPortableRuntimeWorldRevision();
    Require(ValidatePortableRuntimeState(temporary.File("original").string(),"00_Training")&&
        LoadPortableRuntimeState(temporary.File("original").string()),"Cannot restore original dormant Training checkpoint");
    original.Same(temporary,fixtureRevision+1u);
    Require(Declared(Info,"Engine.LevelInfo.PawnList",Kind::Object).text.empty()&&
        Declared(Doctor,"Engine.Pawn.nextPawn",Kind::Object).text.empty()&&Declared(Jaime,"Engine.Pawn.nextPawn",Kind::Object).text.empty(),
        "Final original checkpoint restore retained nonauthored list-fixture links");
    for(const auto& source:sources)source.Unchanged();
    std::cout<<"PASS owned first Training Pawn.PickTarget531: "<<calls<<" production calls, "<<workCuts
        <<" native-work cutoffs, "<<checks<<" checks / "<<refusals<<" explicit refusals; authentic empty/self/nonself Game=None and duplicate-Add cyclic lists, "
        "copied Float temporaries/live OUT aliases, complete copied Vectors, whole save/revision/GC/hash/RNG/lookup preservation. "
        "Three intentional AddPawn fixture publications and one exact original checkpoint restoration. "
        "NOT successful LOS/selected target, begun startup, weapon damage, crates or headset evidence.\n";
}
} // namespace
int main(int argc,char** argv){
    try{
        fs::path root;if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT");configured&&*configured)root=configured;
        for(int i=1;i<argc;++i){const std::string_view option=argv[i];
            if((option=="--owned-source-root"||option=="--game-root")&&i+1<argc)root=argv[++i];
            else if(argc==2)root=argv[i];else throw std::runtime_error("Usage: portable_pawn_pick_target_test [--owned-source-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");}
        if(root.empty()){std::cout<<"SKIP owned first Training PickTarget531: supply original game root\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Original(fs::canonical(root));
        ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,"PickTarget test leaked rooted UObjects after shutdown");return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL Pawn.PickTarget531: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
