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
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Observation CLI, not an acceptance test or a startup publication path.
// The production source list is shared, but ONLY Maps/00_Training.dx is loaded.
// Owned commercial packages are data; no original EXE/DLL is loaded/executed.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
constexpr std::uint32_t InitialSeed=0x13579bdfu;
constexpr std::uint64_t Client=2801u,Viewport=2802u;

void Require(bool value,const std::string& why) {
    if(!value)throw std::runtime_error(why);
}
std::string Fold(std::string text) {
    for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return text;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();
    for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t Fingerprint(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i){hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(input.eof(),"Source fingerprint did not finish: "+path.string());return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t fingerprint{};
    explicit Source(const fs::path& source):path(fs::canonical(source)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),fingerprint(Fingerprint(path)){}
    void Unchanged()const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==fingerprint,
            "Owned source bytes/size/timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,source),"Temporary checkpoints overlap original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto candidate=parent/("quest-training-startup-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){directory=fs::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated startup checkpoint directory");
    }
    ~Temporary(){
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& path:files)if(path.parent_path()==directory)fs::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    Bytes Save(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot capture full runtime checkpoint");Bytes bytes;
        Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read full runtime checkpoint");
        return bytes;
    }
};
QuestVr::Vm::Value Declared(const char* key,Kind kind) {
    const auto value=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0",key);
    Require(value.kind==kind,std::string("Original LevelInfo field has wrong type: ")+key);return value;
}
bool SameViewport(const QuestVr::NativeViewport& a,const QuestVr::NativeViewport& b) {
    return a.identity==b.identity&&a.width==b.width&&a.height==b.height&&a.swapchainImages==b.swapchainImages&&a.eyeCount==b.eyeCount;
}
bool SameSession(const PortablePlayerSessionSnapshot& a,const PortablePlayerSessionSnapshot& b) {
    return a.clientIdentity==b.clientIdentity&&a.viewportIdentity==b.viewportIdentity&&a.clientPath==b.clientPath&&
        a.viewportPath==b.viewportPath&&a.viewportClassPath==b.viewportClassPath&&a.playerPath==b.playerPath&&
        a.viewportOuterPath==b.viewportOuterPath&&a.clientObjectFlags==b.clientObjectFlags&&a.viewportObjectFlags==b.viewportObjectFlags&&
        SameViewport(a.viewport,b.viewport)&&a.bound==b.bound;
}
struct ActorIdentity {
    std::string path,classPath;std::uint32_t objectIndex{},classIndex{},outerIndex{};
};
std::vector<ActorIdentity> ActorIdentities() {
    std::vector<ActorIdentity> result;
    for(const auto& actor:GetPortableRuntimeMapActors(true)) {
        const auto ownership=ReadPortableRuntimeObjectOwnership(actor.objectPath);
        Require(ownership.objectIndex&&ownership.classPath==actor.classPath,"Training Actor has no genuine UObject identity");
        result.push_back({actor.objectPath,actor.classPath,ownership.objectIndex,ownership.classIndex,ownership.outerIndex});
    }
    return result;
}
bool SameActors(const std::vector<ActorIdentity>& a,const std::vector<ActorIdentity>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0u;i<a.size();++i)if(a[i].path!=b[i].path||a[i].classPath!=b[i].classPath||
        a[i].objectIndex!=b[i].objectIndex||a[i].classIndex!=b[i].classIndex||a[i].outerIndex!=b[i].outerIndex)return false;
    return true;
}
bool SameHash(const PortableRetailCollisionSnapshot& a,const PortableRetailCollisionSnapshot& b) {
    if(a.initialized!=b.initialized||a.enabled!=b.enabled||a.initializedHere!=b.initializedHere||a.levelPath!=b.levelPath||
        a.mapGeneration!=b.mapGeneration||a.hashIdentity!=b.hashIdentity||a.tag!=b.tag||a.seed!=b.seed||a.retainedBytes!=b.retainedBytes||
        a.hashes!=b.hashes||a.allocatedLinks!=b.allocatedLinks||a.activeLinks!=b.activeLinks||
        a.initializationDraws!=b.initializationDraws||a.lifecycleOperations!=b.lifecycleOperations||a.actors.size()!=b.actors.size())return false;
    for(std::size_t i=0u;i<a.actors.size();++i){const auto& x=a.actors[i];const auto& y=b.actors[i];
        if(x.path!=y.path||x.objectIndex!=y.objectIndex||x.tag!=y.tag||x.collideActors!=y.collideActors||
            x.pendingDelete!=y.pendingDelete||x.declaredMirrorsMatch!=y.declaredMirrorsMatch)return false;
        for(unsigned c=0u;c<3u;++c)if(std::bit_cast<std::uint32_t>(x.location[c])!=std::bit_cast<std::uint32_t>(y.location[c])||
            std::bit_cast<std::uint32_t>(x.colLocation[c])!=std::bit_cast<std::uint32_t>(y.colLocation[c]))return false;}
    return true;
}
const char* StatusName(QuestVr::Vm::Status status) {
    using Status=QuestVr::Vm::Status;
    switch(status){case Status::Returned:return "Returned";case Status::Stopped:return "Stopped";
    case Status::Unsupported:return "Unsupported";case Status::Invalid:return "Invalid";case Status::Budget:return "Budget";
    case Status::Waiting:return "Waiting";}return "Unknown";
}
struct Boundary {
    PortableWorldStartupPhase phase;std::size_t slot,offset;std::string actor,function,error;std::uint8_t opcode;
    bool Same(const PortableWorldStartupProbeResult& world)const {
        return phase==world.phase&&slot==world.actorSlot&&offset==world.execution.offset&&actor==world.actorPath&&
            function==world.execution.function&&error==world.execution.error&&opcode==world.execution.opcode;
    }
};
int Run(const fs::path& root,const std::optional<std::string>& expected,unsigned repeats) {
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> scripts;
    sources.reserve(QuestVr::RuntimeScriptPackages.size()+2u);scripts.reserve(QuestVr::RuntimeScriptPackages.size());
    for(const auto* name:QuestVr::RuntimeScriptPackages){sources.emplace_back(root/"System"/(std::string(name)+".u"));
        scripts.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"System"/"DeusEx.ini");const auto ini=sources.back().path.string();
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(scripts.size()==38u,"Startup diagnostic differs from exact Quest script package list");
    Require(InitializePortableRuntime(scripts,InitialSeed).passed,"Cannot initialize original Quest source set");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini).prepared,
        "Cannot prepare genuine Engine/metadata/config/Package/lookup/Cylinder prerequisites");
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,Client);
    RegisterPortableRuntimeViewport(Client,{Viewport,1024u,1152u,3u,2u});
    Require(!PreparePortableRuntimePlayerObjects(Viewport).bound,"Startup diagnostic forced player possession");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load actual first Training map");
    const auto native=ReadPortableRuntimeNativeModel();const auto engine=ReadPortableRuntimeEngineCollision();
    Require(native.available&&native.mapGeneration&&engine.boundLevelPath==native.levelPath,
        "First Training has no actual current native Level/Model/Engine binding");
    Require(!Declared("Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
        Declared("Engine.LevelInfo.Game",Kind::Object).text.empty(),"Fresh Training is not authentically dormant");
    const auto actors=ActorIdentities();Require(!actors.empty(),"First Training has no actual actors");
    const auto hash=ReadPortableRuntimeRetailCollision();Require(!hash.initialized&&!hash.enabled&&hash.seed==InitialSeed,
        "Fresh Training unexpectedly owns a native collision hash or a consumed shared RNG stream");
    const auto client=ReadPortableRuntimeClientView();const auto session=ReadPortableRuntimePlayerSession(Viewport);
    Require(HasPortableRuntimeClient()&&client.size()==1u&&!session.bound,"Actual client/viewport preparation is inconsistent");
    const auto saved=temporary.Save("BeforeStartup");const auto revision=GetPortableRuntimeWorldRevision();
    const bool statePresent=GetPortableRuntimeScriptStatePresent(),unsaved=GetPortableRuntimeUnsavedScriptState();
    const auto time=ReadPortableRuntimeLevelTime();const auto lookup=ReadPortableRuntimeObjectLookup();
    GC::Collect();const auto gc=GC::GetStats();
    const auto rollback=[&] {
        Require(temporary.Save("AfterStartup")==saved,"Startup diagnostic changed the full checkpoint/property/clock/RNG state");
        Require(GetPortableRuntimeWorldRevision()==revision&&GetPortableRuntimeScriptStatePresent()==statePresent&&
            GetPortableRuntimeUnsavedScriptState()==unsaved&&
            std::bit_cast<std::uint64_t>(ReadPortableRuntimeLevelTime())==std::bit_cast<std::uint64_t>(time),
            "Startup diagnostic published revision/dirty/native clock state");
        Require(SameHash(hash,ReadPortableRuntimeRetailCollision()),"Startup diagnostic retained native hash/tags/RNG/initialization work");
        const auto current=ReadPortableRuntimeObjectLookup();
        Require(current.prepared==lookup.prepared&&current.generation==lookup.generation&&current.objects==lookup.objects,
            "Startup diagnostic changed actual UObject lookup root/generation/census");
        Require(SameActors(actors,ActorIdentities()),"Startup diagnostic changed Actor slots/classes/native object indices");
        const auto model=ReadPortableRuntimeNativeModel();
        Require(model.available==native.available&&model.mapGeneration==native.mapGeneration&&model.levelPath==native.levelPath&&
            model.modelPath==native.modelPath&&model.modelExportIndex==native.modelExportIndex,
            "Startup diagnostic replaced the actual native Level/Model identity");
        const auto views=ReadPortableRuntimeClientView();
        Require(lifetime->Alive()&&HasPortableRuntimeClient()&&views.size()==client.size()&&
            std::equal(views.begin(),views.end(),client.begin(),SameViewport)&&
            SameSession(session,ReadPortableRuntimePlayerSession(Viewport)),"Startup diagnostic changed client/viewport/player ownership");
        Require(!Declared("Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
            Declared("Engine.LevelInfo.Game",Kind::Object).text.empty(),"Startup diagnostic retained begun-world/GameInfo publication");
        GC::Collect();const auto actual=GC::GetStats();
        Require(actual.numObjects==gc.numObjects&&actual.memoryUsage==gc.memoryUsage,"Startup diagnostic leaked rooted native UObjects");
    };
    rollback();
    PortableWorldStartupProbeOptions options;options.defaultGameClass="DeusEx.DeusExGameInfo";
    QuestVr::Vm::Limits limits;limits.instructions=4'000'000u;limits.nodes=1'048'576u;
    limits.writes=1'048'576u;limits.retainedBytes=128u<<20u;
    std::optional<Boundary> boundary;bool completed{};
    for(unsigned attempt=1u;attempt<=repeats;++attempt){
        const auto start=std::chrono::steady_clock::now();
        const auto result=ProbePortableRuntimeRetailCollisionStartup(options,limits);const auto& world=result.world;
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"OBSERVED first Training startup attempt="<<attempt<<" seconds="<<seconds
            <<" status="<<StatusName(world.execution.status)<<" phase="<<PortableWorldStartupPhaseName(world.phase)
            <<" slot="<<world.actorSlot<<" actor="<<world.actorPath<<" function="<<world.execution.function
            <<" pc="<<world.execution.offset<<" opcode="<<static_cast<unsigned>(world.execution.opcode)
            <<" instructions="<<world.execution.instructions<<" writes="<<world.execution.writes
            <<" vmCommittedDisposition="<<world.execution.committed<<" error="<<world.execution.error<<'\n';
        std::cout<<"HASH PREFIX initialized="<<result.collision.initialized<<" enabled="<<result.collision.enabled
            <<" actors="<<result.collision.actors.size()<<" links="<<result.collision.activeLinks
            <<" seed="<<result.collision.seed<<" tag="<<result.collision.tag
            <<" initializationDraws="<<result.collision.initializationDraws
            <<" lifecycleOperations="<<result.collision.lifecycleOperations<<'\n';
        for(std::size_t i=0u;i<world.visited.size();++i)if(world.visited[i])
            std::cout<<"VISITED "<<PortableWorldStartupPhaseName(static_cast<PortableWorldStartupPhase>(i))<<'='<<world.visited[i]<<'\n';
        for(const auto& call:world.execution.callStack)std::cout<<"CALL "<<call<<'\n';
        rollback();
        completed=world.execution.status==QuestVr::Vm::Status::Returned&&world.execution.error.empty();
        if(completed)Require(world.phase==PortableWorldStartupPhase::CompactActorSlots&&
            world.visited[static_cast<std::size_t>(PortableWorldStartupPhase::CompactActorSlots)]>0u,
            "Returned startup diagnostic did not reach its authentic final phase");
        if(!completed)Require(!world.execution.passed()&&!world.execution.committed&&!world.execution.error.empty(),
            "Startup observation has neither a genuine completed diagnostic nor an explicit safely rolled-back refusal");
        if(boundary)Require(boundary->Same(world),"Repeated first Training startup observation changed phase/slot/actor/function/PC/opcode/error");
        else boundary=Boundary{world.phase,world.actorSlot,world.execution.offset,world.actorPath,world.execution.function,
            world.execution.error,world.execution.opcode};
        if(expected)Require(!completed&&world.execution.error==*expected,"Actual startup error differs from exact --expected-error boundary");
    }
    for(const auto& source:sources)source.Unchanged();
    std::cout<<"ROLLBACK VERIFIED full save/state/revision/hash/RNG/client/lookup/Actor/GC; "<<sources.size()
        <<" owned sources unchanged. No other map loaded; no renderer, startup bypass or gameplay publication.\n";
    lifetime->Revoke();ClearPortableRuntimeClient();
    if(expected){std::cout<<"EXPECTED-BOUNDARY DIAGNOSTIC ONLY: exact error matched; NOT successful or playable startup.\n";return 0;}
    if(completed){std::cout<<"COMPLETED ROLLBACK-ONLY STARTUP DIAGNOSTIC: NOT published world/gameplay/headset acceptance.\n";return 0;}
    std::cout<<"OBSERVED UNSUPPORTED/REFUSED STARTUP BOUNDARY: rollback/source checks passed, startup did NOT complete.\n";return 2;
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;std::optional<std::string> expected;unsigned repeats=1u;
        if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT");configured&&*configured)root=configured;
        for(int i=1;i<argc;++i){const std::string_view option=argv[i];
            if((option=="--owned-source-root"||option=="--game-root")&&i+1<argc)root=argv[++i];
            else if(option=="--expected-error"&&i+1<argc){expected=argv[++i];Require(!expected->empty()&&expected->size()<=8192u,"Expected error must be nonempty and bounded");}
            else if(option=="--repeat"&&i+1<argc){const std::string_view number=argv[++i];
                Require(number.size()==1u&&number[0]>='1'&&number[0]<='8',"Repeat count must be1..8");repeats=static_cast<unsigned>(number[0]-'0');}
            else throw std::runtime_error("Usage: portable_training_startup_probe --owned-source-root OWNED_GAME_ROOT [--expected-error EXACT_TEXT] [--repeat 1..8]");}
        if(root.empty()){std::cout<<"SKIP owned first Training startup observation: use --owned-source-root or DEUSEX_GAME_ROOT.\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();
        const auto result=Run(fs::canonical(root),expected,repeats);ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(after.numObjects==before.numObjects&&after.memoryUsage==before.memoryUsage,"Startup observation retained native UObjects after shutdown");
        return result;
    }catch(const std::exception& error){std::cerr<<"FAIL startup observation/rollback/source check: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
