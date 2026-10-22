#include "Precomp.h"
#include "GC/GC.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
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
#include <stdexcept>
#include <string>
#include <vector>

// Actual owned declarations and native dispatch, not Session helper mocks.
// The JC birth below is an isolated dormant-world control, NOT original Login,
// possession, begun gameplay, usable loot or accepted Training startup.
// Only Maps/00_Training.dx is loaded. Original EXE/DLL files are never executed.
namespace {
namespace fs=std::filesystem;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Value;
using Action=PortableDumpLocationAction;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};

void Require(bool value,const std::string& why) {
    ++checks;if(!value)throw std::runtime_error(why);
}
std::string Fold(std::string text) {
    for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return text;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto at=child.begin();
    for(auto part=parent.begin();part!=parent.end();++at,++part)
        if(at==child.end()||Fold(at->string())!=Fold(part->string()))return false;
    return true;
}
std::uint64_t Fingerprint(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint original source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t result=14695981039346656037ull;std::uintmax_t total{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Source exceeds fingerprint bound");
        for(std::streamsize i=0;i<count;++i){result^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);result*=1099511628211ull;}
    }
    Require(input.eof(),"Original source fingerprint did not finish");return result;
}
struct SourceGuard {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t fingerprint{};
    explicit SourceGuard(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),fingerprint(Fingerprint(path)){}
    void Unchanged()const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==fingerprint,
            "Owned original bytes/size/timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,source),"Temporary checkpoints overlap original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-native-dump-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated DumpLocation checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files)if(file.parent_path()==directory)fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    fs::path File(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid temporary checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    Bytes Read(const fs::path& path)const {
        Bytes result;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,result),
            "Cannot read bounded runtime checkpoint");return result;
    }
    Bytes Save(const std::string& leaf) {
        const auto path=File(leaf);Require(SavePortableRuntimeState(path.string()),"Cannot capture runtime checkpoint: "+leaf);
        return Read(path);
    }
};

QuestVr::Vm::Limits Ample() {
    QuestVr::Vm::Limits result;result.instructions=1'000'000u;result.nodes=1'048'576u;
    result.writes=65536u;result.retainedBytes=128u<<20u;result.nativeWork=1'048'576u;return result;
}
void Passed(const QuestVr::Vm::Result& result,const std::string& why,bool committed) {
    Require(result.passed()&&result.committed==committed,why+": "+result.error+" at "+result.function+':'+std::to_string(result.offset));
    Require(result.nativeWork&&result.nativeWork->used&&!result.nativeWork->exhausted,
        "Actual native DumpLocation root omitted its work ledger");
}
bool Same(const Value& a,const Value& b) {
    if(a.kind!=b.kind||a.fields.size()!=b.fields.size())return false;
    switch(a.kind) {
    case Kind::Float:if(std::bit_cast<std::uint32_t>(a.floating)!=std::bit_cast<std::uint32_t>(b.floating))return false;break;
    case Kind::Vector:
        for(std::size_t i=0u;i<3u;++i)
            if(std::bit_cast<std::uint32_t>(a.vector[i])!=std::bit_cast<std::uint32_t>(b.vector[i]))return false;
        break;
    case Kind::Rotator:if(a.rotation!=b.rotation)return false;break;
    case Kind::Byte:case Kind::Int:if(a.integer!=b.integer)return false;break;
    case Kind::Bool:if(a.boolean!=b.boolean)return false;break;
    case Kind::Name:case Kind::Object:case Kind::String:if(a.text!=b.text)return false;break;
    case Kind::IntArray:if(a.intArray!=b.intArray)return false;break;
    case Kind::Struct:case Kind::Nothing:break;
    }
    auto other=b.fields.begin();
    for(const auto& [key,value]:a.fields) {
        if(other==b.fields.end()||other->first!=key||!Same(value,other->second))return false;
        ++other;
    }
    return true;
}
Value Pose(const Value& initial,const bool second) {
    Require(initial.kind==Kind::Struct&&initial.fields.size()==8u,"Original DumpLocationStruct does not have its actual eight fields");
    auto value=initial;
    for(const auto* key:{"mapname","title","desc","gameversion"}) {
        Require(value.fields.at(key).kind==Kind::String,"Original DumpLocationStruct FString type differs");
        value.fields.at(key)=Value::Text(Kind::String,std::string(second?"load-":"save-")+key);
    }
    Require(value.fields.at("location").kind==Kind::Vector&&value.fields.at("viewrotation").kind==Kind::Rotator&&
        value.fields.at("locationid").kind==Kind::Int&&value.fields.at("bdeleted").kind==Kind::Bool,
        "Original DumpLocationStruct pose/id/deletion declaration differs");
    // Finite raw words, including signed zero/subnormals. Runtime admission
    // intentionally rejects persistent nonfinite values; helper tests cover
    // the original raw Session NaN/Inf copy separately.
    const std::array<std::uint32_t,3u> position=second?
        std::array<std::uint32_t,3u>{0x80000000u,0x00800000u,0xc2480000u}:
        std::array<std::uint32_t,3u>{0x00000001u,0x80000000u,0x42f68000u};
    std::array<float,3u> vector{};for(std::size_t i=0u;i<3u;++i)vector[i]=std::bit_cast<float>(position[i]);
    value.fields.at("location")=Value::Vector(vector);
    value.fields.at("viewrotation")=Value::Rotator(second?
        std::array<std::int32_t,3u>{7,-8,9}:
        std::array<std::int32_t,3u>{std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max(),
            std::bit_cast<std::int32_t>(0xa5a5a5a5u)});
    value.fields.at("locationid")=Value::Integer(second?987654321:-123456789);
    value.fields.at("bdeleted")=Value::Bool(second);return value;
}
Value LoadedPose(const Value& saved,const Value& destination) {
    auto result=destination;result.fields.at("location")=saved.fields.at("location");
    result.fields.at("viewrotation")=saved.fields.at("viewrotation");return result;
}
bool SameLookup(const PortableObjectLookupState& a,const PortableObjectLookupState& b) {
    return a.prepared==b.prepared&&a.objects==b.objects&&a.retainedBytes==b.retainedBytes&&a.generation==b.generation;
}
void Gone(const PortableDumpLocationObservation& object) {
    if(object.path.empty())return;
    const auto cls=ReadPortableRuntimeObjectOwnership("DeusEx.DumpLocation");
    const auto outer=ReadPortableRuntimeObjectOwnership(object.outerPath);
    const auto name=object.path.substr(object.path.find_last_of('.')+1u);
    Require(!FindPortableRuntimeObject({name,cls.objectIndex,outer.objectIndex,false,true}),
        "Ephemeral DumpLocation survived its native weak lookup root");
    bool refused{};
    try{static_cast<void>(ReadPortableRuntimeObjectOwnership(object.path));}catch(const std::exception&){refused=true;}
    Require(refused,"Ephemeral DumpLocation canonical path remains readable after its caller root");++refusals;
}
void Constructor(const PortableDumpLocationProbeResult& result) {
    const auto& object=result.object;
    Require(result.available&&!object.path.empty()&&object.classPath=="DeusEx.DumpLocation"&&object.outerPath=="00_Training"&&
        object.objectIndex&&object.flags==0u&&object.fileIndex==-1&&object.locationIndex==-1&&object.locationCount==0&&
        object.nonActor&&object.noExport&&object.noActorSlot,
        "Actual native3015 lost original class/Outer/flags/indices or became an Actor/export");
    Require(result.before.kind==Kind::Struct&&result.before.fields.size()==8u,"Constructor snapshot is not original typed DumpLocationStruct");
    for(const auto* key:{"mapname","title","desc","gameversion"})
        Require(result.before.fields.at(key).kind==Kind::String&&result.before.fields.at(key).text.empty(),
            "Native FString member constructor did not replace copied defaults with an empty string");
}
PortableDumpLocationProbeResult Probe(const std::string& player,Action action=Action::Inspect,
    std::optional<Value> input={},bool rollback=true,const QuestVr::Vm::Limits& limits=Ample()) {
    return ProbePortableDumpLocationSession({player,action,std::move(input),rollback},limits);
}
std::string Initialize(const std::vector<PortablePackageTables>& scripts,const PortablePackageTables& map,const fs::path& root) {
    Require(InitializePortableRuntime(scripts,1u,QuestVr::OriginalLinkerLoadProfile{false,true,true}).passed,
        "Cannot initialize original standalone source profile");
    PreparePortableRuntimeDumpLocationServices((root/"System"/"User.ini").string(),(root/"System").string());
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare actual native class/Package/lookup prerequisites");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load only actual first Training map");
    const auto begun=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(begun.kind==Kind::Bool&&!begun.boolean,"Isolated DumpLocation control unexpectedly started the world");
    const auto actors=GetPortableRuntimeMapActors(true);
    const auto start=std::find_if(actors.begin(),actors.end(),[](const auto& actor){return actor.classPath=="Engine.PlayerStart"&&actor.hasLocation;});
    Require(start!=actors.end(),"Actual first Training PlayerStart missing");
    const auto birth=ExecutePortableActorFunction("00_Training.LevelInfo0","Engine.Actor.Spawn",{
        {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},{Value::Text(Kind::Object,{}),{}},
        {Value::Text(Kind::Name,{}),{}},{Value::Vector({start->x,start->y,start->z}),{}},
        {Value::Rotator({start->pitch,start->yaw,start->roll}),{}}},Ample());
    Passed(birth,"Genuine dormant original player native birth failed",true);
    Require(birth.value.kind==Kind::Object&&!birth.value.text.empty(),"Original native player birth returned None");
    Require(ReadPortableRuntimeObjectOwnership(birth.value.text).classPath=="DeusEx.JCDentonMale",
        "Native player birth used a fabricated replacement class");return birth.value.text;
}

void Run(const fs::path& root) {
    Temporary temporary(root);std::vector<SourceGuard> guards;std::vector<PortablePackageTables> scripts;
    guards.reserve(QuestVr::RuntimeScriptPackages.size()+3u);scripts.reserve(QuestVr::RuntimeScriptPackages.size());
    for(const auto* name:QuestVr::RuntimeScriptPackages) {
        guards.emplace_back(root/"System"/(std::string(name)+".u"));scripts.push_back(LoadPortablePackageTables(guards.back().path.string()));
    }
    guards.emplace_back(root/"System"/"User.ini");guards.emplace_back(root/"System"/"DeusEx.ini");
    guards.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(guards.back().path.string());
    try {
        const auto player=Initialize(scripts,map,root);
        const auto baseline=temporary.Save("Baseline");const auto checkpoint=temporary.File("Baseline");
        const auto actorCount=GetPortableRuntimeMapActors(true).size();const auto revision=GetPortableRuntimeWorldRevision();
        const auto lookup=ReadPortableRuntimeObjectLookup();const auto order=ReadPortableWorldActorOrder();
        GC::Collect();const auto gc=GC::GetStats();
        const auto unchanged=[&](bool save) {
            Require(GetPortableRuntimeMapActors(true).size()==actorCount&&GetPortableRuntimeWorldRevision()==revision&&
                ReadPortableWorldActorOrder()==order,"Non-Actor DumpLocation changed actual Actor slots or world revision");
            Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup()),"DumpLocation root retained actual native lookup objects/capacity");
            GC::Collect();const auto now=GC::GetStats();
            Require(now.numObjects==gc.numObjects&&now.memoryUsage==gc.memoryUsage,"Ephemeral DumpLocation root leaked native GC objects/helper");
            if(save)Require(temporary.Save("AfterRollback")==baseline,"Rolled-back DumpLocation changed full state/clock/RNG checkpoint");
        };
        const auto fresh=Probe(player);Passed(fresh.execution,"Fresh actual native3015/3039 inspection failed",false);Constructor(fresh);
        Require(!fresh.savedBefore&&!fresh.savedAfter&&!fresh.object.saved,"Fresh actual session getter is not false");Gone(fresh.object);unchanged(true);
        const auto saved=Pose(fresh.before,false),destination=Pose(fresh.before,true),loaded=LoadedPose(saved,destination);
        const auto zero=Probe(player,Action::Load,destination);
        Passed(zero.execution,"Actual zero-flag LoadLocation failed",false);Constructor(zero);
        Require(!zero.savedBefore&&!zero.savedAfter&&Same(zero.after,destination),"Zero-flag LoadLocation wrote destination fields");
        Require(zero.object==fresh.object,"Rolled-back constructor changed actual unique name/native ObjectIndex or constructor state");
        Gone(zero.object);unchanged(true);
        const auto saveRollback=Probe(player,Action::Save,saved);
        Passed(saveRollback.execution,"Actual SaveLocation rollback root failed",false);Constructor(saveRollback);
        Require(!saveRollback.savedBefore&&saveRollback.savedAfter&&Same(saveRollback.after,saved),
            "Actual SaveLocation did not set session flag or unexpectedly changed its struct");
        Gone(saveRollback.object);unchanged(true);
        const auto stillFresh=Probe(player);Passed(stillFresh.execution,"Post-Save rollback getter failed",false);
        Require(!stillFresh.savedBefore&&!stillFresh.savedAfter,"Rolled-back native3037 leaked DLL-session state");Gone(stillFresh.object);unchanged(true);

        const auto committed=Probe(player,Action::Save,saved,false);
        Passed(committed.execution,"Actual SaveLocation commit failed",true);Constructor(committed);
        Require(!committed.savedBefore&&committed.savedAfter&&Same(committed.after,saved),"Committed native3037 did not publish exact session pose");
        Gone(committed.object);unchanged(false);
        Require(GetPortableRuntimeUnsavedScriptState(),"Committed native session is absent from native archive/discard guard");
        Require(!SavePortableRuntimeState(checkpoint.string())&&temporary.Read(checkpoint)==baseline,
            "Unsupported DumpLocation session archive silently succeeded or overwrote existing checkpoint");++refusals;
        const auto another=Probe(player,Action::Inspect,{},false);
        Passed(another.execution,"Second actual constructor/getter commit failed",true);Constructor(another);
        Require(another.savedBefore&&another.savedAfter&&another.object.saved,"Constructing another DumpLocation reset DLL-session flag");
        Require(another.object.path!=committed.object.path,"Committed constructor did not consume its concrete class unique-name counter");
        Gone(another.object);unchanged(false);
        const auto loadRollback=Probe(player,Action::Load,destination);
        Passed(loadRollback.execution,"Actual LoadLocation rollback root failed",false);Constructor(loadRollback);
        Require(loadRollback.savedBefore&&!loadRollback.savedAfter&&Same(loadRollback.after,loaded),
            "Actual native3038 did not copy six raw pose words while preserving all destination nonpose fields");
        Gone(loadRollback.object);unchanged(false);
        const auto remains=Probe(player);Passed(remains.execution,"Post-Load rollback getter failed",false);
        Require(remains.savedBefore&&remains.savedAfter,"Rolled-back native3038 lost saved session flag");Gone(remains.object);unchanged(false);
        auto zeroWrites=Ample();zeroWrites.writes=0u;
        const auto failedLoad=Probe(player,Action::Load,{},false,zeroWrites);
        Require(failedLoad.execution.status==QuestVr::Vm::Status::Budget&&!failedLoad.execution.committed&&
            failedLoad.execution.error=="VM write limit","Actual native3038 did not fail its shared zero-write budget after clearing the session flag");
        ++refusals;Gone(failedLoad.object);unchanged(false);
        const auto afterFailure=Probe(player);Passed(afterFailure.execution,"Getter after native3038 write failure failed",false);
        Require(afterFailure.savedBefore&&afterFailure.savedAfter,"Native3038 write-budget refusal failed to restore global session before-image");
        Gone(afterFailure.object);unchanged(false);
        const auto consumed=Probe(player,Action::Load,destination,false);
        Passed(consumed.execution,"Actual native3038 commit failed",true);Constructor(consumed);
        Require(consumed.savedBefore&&!consumed.savedAfter&&Same(consumed.after,loaded),
            "Committed native3038 copied stale/wrong words, altered nonpose fields or failed to consume the flag");
        Gone(consumed.object);unchanged(false);
        const auto oneShot=Probe(player,Action::Load,destination);
        Passed(oneShot.execution,"Second actual native3038 load failed",false);
        Require(!oneShot.savedBefore&&!oneShot.savedAfter&&Same(oneShot.after,destination),
            "Consumed native session was loaded twice instead of leaving destination unchanged");Gone(oneShot.object);unchanged(false);
        Require(temporary.Save("ConsumedSession")==baseline,"Consumed ephemeral session retained unsupported archive payload or modified map state");

        const auto resetSeed=Probe(player,Action::Save,saved,false);Passed(resetSeed.execution,"Session reset seed commit failed",true);
        Require(resetSeed.savedAfter&&GetPortableRuntimeUnsavedScriptState(),"Fresh-engine reset lacks positive saved-session witness");Gone(resetSeed.object);
        const auto core=std::find_if(scripts.begin(),scripts.end(),[](const auto& package){return Fold(fs::path(package.sourcePath).stem().string())=="core";});
        Require(core!=scripts.end(),"Actual Core package unavailable for single-package reset");
        Require(InitializePortableRuntime(*core,1u,QuestVr::OriginalLinkerLoadProfile{false,true,true}).passed,
            "Single-package engine reinitialization failed");
        Require(!GetPortableRuntimeUnsavedScriptState(),"Single-package initialization retained old DumpLocation session/weak registry");
        // A different immutable binding proves shutdown retired services too.
        PreparePortableRuntimeDumpLocationServices((root/"System"/"DeusEx.ini").string(),temporary.directory.string());
        const auto freshPlayer=Initialize(scripts,map,root);
        const auto reset=Probe(freshPlayer);Passed(reset.execution,"Fresh-engine actual native3039 inspection failed",false);Constructor(reset);
        Require(!reset.savedBefore&&!reset.savedAfter&&!reset.object.saved,"Fresh engine inherited previous saved location flag");Gone(reset.object);
        Require(!ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay").boolean,
            "DumpLocation integration tests forced begun gameplay");
        std::cout<<"PASS first Training actual native3015/3037/3038/3039: genuine dormant JC; typed constructor, exact six-word pose copy, "
            "nonpose preservation, zero-load/one-shot semantics, commit/rollback/write-budget/session reset, weak lookup/GC cleanup; checks="
            <<checks<<" refusals="<<refusals<<'\n';
        std::cout<<"NOT original Login completion, persistent DumpLocation reference/field rollback, DMP file/archive support, crate loot/gravity or playable Training.\n";
    }catch(...) {
        ShutdownPortableRuntime();for(const auto& guard:guards)guard.Unchanged();throw;
    }
    ShutdownPortableRuntime();for(const auto& guard:guards)guard.Unchanged();
    std::cout<<"UNCHANGED owned sources="<<guards.size()<<"; only first Training map loaded\n";
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        Require(argc<=2,"Usage: portable_dump_location_test [OWNED_GAME_ROOT] (ONLY Maps/00_Training.dx)");
        const auto* environment=std::getenv("DEUSEX_GAME_ROOT");
        const fs::path root=argc==2?fs::path(argv[1]):environment?fs::path(environment):fs::path{};
        if(root.empty()){std::cout<<"SKIP: supply owned Deus Ex root for actual DumpLocation native integration\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Run(fs::canonical(root));
        ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,"Final shutdown leaked native DumpLocation/player objects");
        return 0;
    }catch(const std::exception& error){ShutdownPortableRuntime();std::cerr<<"FAIL actual DumpLocation integration: "<<error.what()<<'\n';return 1;}
}
