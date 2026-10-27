#include "Precomp.h"
#include "GC/GC.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "quest_engine_config.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Owned-source native Input CLASS registration only. Original package bytes
// are read as data, never executed or modified. This is not native property,
// default/config-copy, Input instance, viewport Init/Reset or gameplay proof.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
using Value=QuestVr::Vm::Value;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
std::string Fold(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();
    for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned Input source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Input source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Cannot finish owned Input source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Owned Input source bytes/size/mtime changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Temporary Input fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-native-input-classes-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated Input fixture");
        Require(fs::create_directory(directory/"System"),"Cannot create isolated Input System directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files)
            if(file.parent_path()==directory||file.parent_path()==directory/"System")fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent) {
            fs::remove(directory/"System",ignored);fs::remove(directory,ignored);
        }
    }
    fs::path Track(const fs::path& path) {
        Require(path.parent_path()==directory||path.parent_path()==directory/"System","Invalid isolated Input file path");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    void Ini(const std::string& inputLines) {
        const auto path=Track(directory/"System"/"DeusEx.ini");std::ofstream file(path,std::ios::binary|std::ios::trunc);
        const auto text=std::string("[Engine.Engine]\nGameEngine=DeusEx.DeusExGameEngine\n")+inputLines;
        Require(bool(file),"Cannot create isolated Input INI");file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
        Require(bool(file),"Cannot finish isolated Input INI");
    }
    void RemoveIni() {
        const auto path=Track(directory/"System"/"DeusEx.ini");
        Require(path.parent_path()==directory/"System"&&fs::remove(path),"Cannot remove only the isolated Input INI");
    }
    Bytes Snapshot(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid Input checkpoint leaf");
        const auto path=Track(directory/(leaf+".sav"));Require(SavePortableRuntimeState(path.string()),"Cannot save whole Input checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read Input checkpoint");
        return bytes;
    }
};

// Independent registration literals: Engine10392FE0..10393047 and
// Extension1002CF10..1002CF64; constructor OR12 at Core1012866F changes each
// supplied0C to baseline1E. Register Super&4EC adds no further bits here.
constexpr std::array<const char*,2u> Paths{"Engine.Input","Extension.InputExt"};
constexpr std::array<const char*,2u> Bases{"Core.Subsystem","Engine.Input"};
constexpr std::array<const char*,2u> Packages{"Engine","Extension"};
std::string CanonicalSelection(const std::optional<std::string>& selected) {
    Require(bool(selected),"Owned Input fixture has no actual selected Input class");
    for(const auto* path:Paths)if(Fold(*selected)==Fold(path))return path;
    throw std::runtime_error("Owned fixture selects an unaudited Input class: "+*selected);
}
void SnapshotIs(const PortableNativeInputClassesSnapshot& value,bool prepared,
    const PortableNativeMetadataClassesSnapshot* metadata=nullptr,const std::string& configured={}) {
    Require(value.prepared==prepared&&value.classes.size()==(prepared?2u:0u),"Input class state/count differs from its closed two-class phase");
    if(!prepared) {
        Require(!value.configuredInputClass&&value.configName.empty(),"Unprepared Input phase exposed a partial config selection");return;
    }
    Require(metadata&&metadata->prepared&&metadata->classes.size()==13u,"Input fixture lacks its actual thirteen-class dependency");
    Require(value.configuredInputClass&&*value.configuredInputClass==configured&&value.configName=="User",
        "Input class phase changed the canonical selected class or original User config name");
    const auto object=metadata->classes[0u].baseClassIndex,meta=metadata->classes[3u].objectIndex;
    const auto subsystem=ReadPortableRuntimeObjectOwnership("Core.Subsystem");
    Require(subsystem.objectIndex&&subsystem.classIndex==meta&&subsystem.objectIndex!=object,
        "Input lacks the actual serialized Core.Subsystem Class dependency");
    std::set<std::uint32_t> indices{object,subsystem.objectIndex};
    for(const auto& row:metadata->classes)Require(row.objectIndex&&indices.insert(row.objectIndex).second,
        "Prerequisite metadata indices are null or alias Input base/Within identities");
    for(std::size_t i=0u;i<Paths.size();++i) {
        const auto& row=value.classes[i];
        Require(row.classPath==Paths[i]&&row.baseClassPath==Bases[i]&&row.withinClassPath=="Core.Object"&&row.metaClassPath=="Core.Class"&&
            row.nativeSize==0xfb0u&&row.registrationClassFlags==0x1eu&&row.registrationObjectFlags==0x04084004u,
            "Input native header literal/relationship differs from original audit");
        Require(row.objectIndex&&indices.insert(row.objectIndex).second&&row.withinClassIndex==object&&row.metaClassIndex==meta&&
            row.baseClassIndex==(i?value.classes[0u].objectIndex:subsystem.objectIndex),
            "Input Super/Within/metaclass does not use distinct actual shared indexed Class identities");
        const auto owner=ReadPortableRuntimeObjectOwnership(row.classPath),package=ReadPortableRuntimeObjectOwnership(Packages[i]);
        Require(owner.objectPath==row.classPath&&owner.objectIndex==row.objectIndex&&owner.classPath=="Core.Class"&&owner.classIndex==meta&&
            owner.outerPath==Packages[i]&&owner.outerIndex==package.objectIndex&&package.objectIndex&&!owner.serializedOuterZero,
            "Native Input UClass has an invented, absent or incorrect actual Package Outer");
    }
}
void SameRow(const PortableNativeMetadataClassSnapshot& a,const PortableNativeMetadataClassSnapshot& b) {
    Require(a.classPath==b.classPath&&a.baseClassPath==b.baseClassPath&&a.withinClassPath==b.withinClassPath&&a.metaClassPath==b.metaClassPath&&
        a.nativeSize==b.nativeSize&&a.registrationClassFlags==b.registrationClassFlags&&a.registrationObjectFlags==b.registrationObjectFlags&&
        a.objectIndex==b.objectIndex&&a.baseClassIndex==b.baseClassIndex&&a.withinClassIndex==b.withinClassIndex&&a.metaClassIndex==b.metaClassIndex,
        "Repeated class operation changed a prior native Class header or actual identity");
}
void SamePrerequisites(const PortableNativeEngineClassesSnapshot& engine,const PortableNativeMetadataClassesSnapshot& metadata) {
    const auto a=ReadPortableRuntimeNativeEngineClasses();const auto b=ReadPortableRuntimeNativeMetadataClasses();
    Require(a.prepared==engine.prepared&&a.classes.size()==engine.classes.size()&&a.configuredGameEngineClass==engine.configuredGameEngineClass,
        "Input phase changed its existing five-class Engine prerequisite");
    for(std::size_t i=0u;i<a.classes.size();++i) {
        const auto& x=a.classes[i];const auto& y=engine.classes[i];
        Require(x.classPath==y.classPath&&x.baseClassPath==y.baseClassPath&&x.withinClassPath==y.withinClassPath&&x.nativeSize==y.nativeSize&&
            x.registrationClassFlags==y.registrationClassFlags&&x.registrationObjectFlags==y.registrationObjectFlags,
            "Input phase replaced an Engine native registration header");
    }
    Require(b.prepared==metadata.prepared&&b.classes.size()==metadata.classes.size()&&b.boundSerializedClasses==metadata.boundSerializedClasses,
        "Input phase changed its existing thirteen-class Core metadata prerequisite");
    for(std::size_t i=0u;i<b.classes.size();++i)SameRow(b.classes[i],metadata.classes[i]);
}
void SameSnapshot(const PortableNativeInputClassesSnapshot& expected,const PortableNativeInputClassesSnapshot& actual,
    const PortableNativeMetadataClassesSnapshot& metadata) {
    SnapshotIs(actual,true,&metadata,*expected.configuredInputClass);
    for(std::size_t i=0u;i<expected.classes.size();++i)SameRow(expected.classes[i],actual.classes[i]);
}
void SameGc(const GCStats& expected,const std::string& why) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,why);
}
bool SameLookup(const PortableObjectLookupState& a,const PortableObjectLookupState& b) {
    return a.prepared==b.prepared&&a.objects==b.objects&&a.retainedBytes==b.retainedBytes&&a.generation==b.generation;
}
bool SameFound(const std::optional<PortableObjectLookupResult>& a,const std::optional<PortableObjectLookupResult>& b) {
    if(bool(a)!=bool(b))return false;
    return !a||(a->objectPath==b->objectPath&&a->objectIndex==b->objectIndex&&a->classIndex==b->classIndex&&
        a->outerIndex==b->outerIndex&&a->nameIndex==b->nameIndex&&a->generation==b->generation);
}
void Lookup(const PortableNativeInputClassesSnapshot& snapshot) {
    const auto lookup=ReadPortableRuntimeObjectLookup();Require(lookup.prepared&&lookup.generation,"Actual weak object lookup is absent");
    for(const auto& row:snapshot.classes) {
        const auto owner=ReadPortableRuntimeObjectOwnership(row.classPath);
        for(const auto& spelling:{row.classPath,Fold(row.classPath)}) {
            const auto found=FindPortableRuntimeObject({spelling,row.metaClassIndex,0u,true,true});
            Require(found&&found->objectPath==row.classPath&&found->objectIndex==row.objectIndex&&found->classIndex==row.metaClassIndex&&
                found->outerIndex==owner.outerIndex&&found->generation==lookup.generation,
                "Native Input class was not published canonically/folded into the actual Class-filtered lookup");
        }
        const auto name=Fold(row.classPath.substr(row.classPath.find('.')+1u));
        const auto direct=FindPortableRuntimeObject({name,row.metaClassIndex,owner.outerIndex,false,true});
        Require(direct&&direct->objectIndex==row.objectIndex&&direct->outerIndex==owner.outerIndex,
            "Input Class direct-Outer lookup does not reuse the genuine Package identity");
        Require(!FindPortableRuntimeObject({"@ClassDefault:"+row.classPath,0u,0u,true,false}),
            "Registration-only Input phase published a fabricated default alias");
    }
}
std::int32_t ExportClass(const PortablePackageTables& table,const std::string& path) {
    const auto stem=fs::path(table.sourcePath).stem().string();
    for(std::size_t i=0u;i<table.exports.size();++i)
        if(table.exports[i].ObjClass==0&&Fold(stem+'.'+GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)))==Fold(path))
            return static_cast<std::int32_t>(i+1u);
    throw std::runtime_error("Actual owned UClass export is absent: "+path);
}
void Headers(const PortablePackageTables& core,const PortablePackageTables& engine) {
    const auto object=ExportClass(core,"Core.Object"),subsystem=ExportClass(core,"Core.Subsystem"),actor=ExportClass(engine,"Engine.Actor");
    for(const auto* path:Paths) {
        const auto cast=[&](const std::string& source,std::int32_t target,bool meta,const std::string& expected) {
            const auto input=Value::Text(Kind::Object,Fold(path));const auto result=CastPortableRuntimeObject(source,target,input,meta);
            Require(result.kind==Kind::Object&&result.text==expected&&result.fields.empty()&&input.text==Fold(path),
                "Registered Input class cast changed identity/kind/input or invented inheritance");
        };
        cast("Core.Object",object,true,path);cast("Core.Object",object,false,path);
        cast("Core.Subsystem",subsystem,true,path);cast("Core.Subsystem",subsystem,false,"");cast("Engine.Actor",actor,true,"");
        Refused([&] {(void)ReadPortableClassDefault(path,"Name");},"Input registration manufactured native default storage");
        Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},"Input registration manufactured serialized authored state metadata");
        Refused([&] {(void)ReadPortableRuntimeDispatchClass(path);},"Input registration manufactured authored dispatch metadata");
        Refused([&] {(void)ReadPortableRuntimeActorToWorld(path);},"Input UClass was accepted as a native Actor");
    }
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> result;
    for(const auto& actor:GetPortableRuntimeMapActors(true))result.push_back(actor.objectPath+'|'+actor.classPath);
    return result;
}
void Dormant() {
    const auto begun=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(begun.kind==Kind::Bool&&!begun.boolean,"Input class registration began the original Training world");
}
void MissingPhases(const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize actual owned Input fixture");
    for(unsigned phase=0u;phase<3u;++phase) {
        if(phase==1u)Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare the five-class Engine phase");
        if(phase==2u)Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare the thirteen-class metadata phase");
        const auto engine=ReadPortableRuntimeNativeEngineClasses();const auto metadata=ReadPortableRuntimeNativeMetadataClasses();
        const auto checkpoint=temporary.Snapshot("MissingPhaseBefore"+std::to_string(phase));
        const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        Refused([&] {(void)PreparePortableRuntimeNativeInputClasses();},"Input phase silently prepared an absent class/Package prerequisite");
        SnapshotIs(ReadPortableRuntimeNativeInputClasses(),false);SamePrerequisites(engine,metadata);
        Require(!ReadPortableRuntimeNativePackages().prepared&&!ReadPortableRuntimeObjectLookup().prepared,
            "Missing-phase Input refusal auto-published Package/default or lookup state");
        for(const auto* path:Paths)Refused([&] {(void)ReadPortableRuntimeObjectOwnership(path);},"Missing-phase refusal published a native Input identity");
        GC::Collect();SameGc(gc,"Missing-phase Input refusal leaked staged classes/domain/index leases");
        Require(temporary.Snapshot("MissingPhaseAfter"+std::to_string(phase))==checkpoint&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
            "Missing-phase Input refusal changed checkpoint/revision/world");
    }
}
void Original(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    MissingPhases(packages,temporary);
    const auto engine=ReadPortableRuntimeNativeEngineClasses();const auto metadata=ReadPortableRuntimeNativeMetadataClasses();
    Require(engine.prepared&&engine.classes.size()==5u&&metadata.prepared&&metadata.classes.size()==13u,"Wrong explicit native prerequisite closure");
    Require(PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeObjectLookup().prepared,
        "Cannot prepare genuine Package roots and actual weak object lookup");
    const auto scriptGc=GC::GetStats();
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant original first Training map");Dormant();
    const auto configured=CanonicalSelection(QuestVr::EngineConfig::ReadInputClass(root));
    const auto checkpoint=temporary.Snapshot("OwnedBefore");const auto world=WorldPaths();const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"),sound=ReadPortableClassDefault("Engine.Engine","UseSound");
    const auto prepared=PreparePortableRuntimeNativeInputClasses();SnapshotIs(prepared,true,&metadata,configured);
    const auto preparedGc=GC::GetStats();const auto preparedLookup=ReadPortableRuntimeObjectLookup();
    Require(preparedGc.numObjects==gc.numObjects+3u&&preparedGc.memoryUsage>gc.memoryUsage,
        "Input phase did not allocate exactly two real native UClasses plus one domain (no Input defaults/instances)");
    Require(preparedLookup.prepared&&preparedLookup.generation==lookup.generation&&preparedLookup.objects==lookup.objects+2u&&
        preparedLookup.retainedBytes>lookup.retainedBytes,"Input phase did not stage exactly two genuine Class records into the existing lookup");
    Headers(packages[0u],packages[1u]);Lookup(prepared);SamePrerequisites(engine,metadata);
    Require(!ReadPortableRuntimeNativeConfigClasses().prepared,"Input registration silently prepared unrelated config constraint classes");
    for(unsigned repeat=0u;repeat<4u;++repeat) {
        SameSnapshot(prepared,PreparePortableRuntimeNativeInputClasses(),metadata);
        SameSnapshot(prepared,ReadPortableRuntimeNativeInputClasses(),metadata);
        Require(SameLookup(preparedLookup,PreparePortableRuntimeObjectLookup()),"Lookup reprepare rebuilt or duplicated native Input registrations");
        Lookup(prepared);GC::Collect();SameGc(preparedGc,"Repeated Input prepare/read/lookup/GC changed the native graph");
    }
    auto detached=ReadPortableRuntimeNativeInputClasses();detached.classes[0u].classPath="Changed.Detached";
    detached.classes[1u].baseClassIndex=0u;detached.configName="Changed";detached.configuredInputClass="Changed.Input";detached.prepared=false;
    SameSnapshot(prepared,ReadPortableRuntimeNativeInputClasses(),metadata);
    ClearPortableRuntimeClient();SameSnapshot(prepared,ReadPortableRuntimeNativeInputClasses(),metadata);
    Require(!HasPortableRuntimeClient(),"Class-only Input preparation fabricated a native Client");
    Require(temporary.Snapshot("OwnedAfter")==checkpoint&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths()==world&&
        ReadPortableRuntimeLevelTime()==0.0&&!GetPortableRuntimeUnsavedScriptState()&&
        QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"))&&
        QuestVr::Vm::Equal(sound,ReadPortableClassDefault("Engine.Engine","UseSound")),
        "Input class registration changed dormant checkpoint/revision/actors/clock/dirty state or original Engine defaults");Dormant();
    const auto warmRevision=GetPortableRuntimeWorldRevision();
    Require(LoadPortableRuntimeState((temporary.directory/"OwnedBefore.sav").string()),"Cannot warm-restore the dormant Training checkpoint");
    Require(GetPortableRuntimeWorldRevision()==warmRevision+1u,"Warm checkpoint replacement did not advance revision exactly once");
    SameSnapshot(prepared,ReadPortableRuntimeNativeInputClasses(),metadata);SamePrerequisites(engine,metadata);Lookup(prepared);
    GC::Collect();SameGc(preparedGc,"Warm checkpoint replacement lost or duplicated engine-session Input class roots");
    Require(temporary.Snapshot("OwnedWarm")==checkpoint&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,
        "Warm checkpoint replacement contaminated the dormant Training world");Dormant();
    Require(UnloadPortableRuntimeMap()>0u&&WorldPaths().empty(),"Cannot unload first Training map");GC::Collect();
    SameSnapshot(prepared,PreparePortableRuntimeNativeInputClasses(),metadata);SamePrerequisites(engine,metadata);Lookup(prepared);
    Require(GC::GetStats().numObjects==scriptGc.numObjects+3u,"Map unload failed to retain exactly two Input UClasses and their domain");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload first Training with native Input classes");
    const auto reloadedGc=GC::GetStats();SameSnapshot(prepared,ReadPortableRuntimeNativeInputClasses(),metadata);Lookup(prepared);
    SameSnapshot(prepared,PreparePortableRuntimeNativeInputClasses(),metadata);GC::Collect();SameGc(reloadedGc,"Map reload lost or duplicated static Input class roots");
    Require(temporary.Snapshot("OwnedReload")==checkpoint&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,
        "Training reload changed saved actor/default state after Input registration");Dormant();
    ShutdownPortableRuntime();SnapshotIs(ReadPortableRuntimeNativeInputClasses(),false);
    for(const auto* path:Paths)Refused([&] {(void)ReadPortableRuntimeObjectOwnership(path);},"Shutdown retained a published native Input identity");
}
void ConfigFixtures(const fs::path& root,const std::vector<PortablePackageTables>& original,Temporary& temporary) {
    const auto copyPath=temporary.Track(temporary.directory/"System"/"Core.u");
    Require(fs::copy_file(root/"System"/"Core.u",copyPath),"Cannot create isolated unchanged Core source copy");
    const Source copy(copyPath);auto packages=original;packages[0u].sourcePath=copyPath.string();
    temporary.Ini("Input=Extension.InputExt\n");
    Require(InitializePortableRuntime(packages).passed&&PreparePortableRuntimeNativeEngineClasses().prepared&&
        PreparePortableRuntimeNativeMetadataClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared,"Cannot initialize genuine config-redirection prerequisites");
    const auto engine=ReadPortableRuntimeNativeEngineClasses();const auto metadata=ReadPortableRuntimeNativeMetadataClasses();
    // Only the copied Core's adjacent INI changes. Real package payload bytes,
    // Class identities and the original installation's INI remain untouched.
    const std::array<std::string,5u> invalid{
        "", "Input=InputExt\n", "Input=Extension.NotAudited\n",
        "Input=Extension.InputExt\nInput=Engine.Input\n", "Input=Extension..InputExt\n"};
    for(std::size_t i=0u;i<=invalid.size();++i) {
        if(i==invalid.size())temporary.RemoveIni();else temporary.Ini(invalid[i]);
        const auto before=temporary.Snapshot("ConfigBefore"+std::to_string(i));const auto gc=GC::GetStats();
        const auto lookup=ReadPortableRuntimeObjectLookup();const auto revision=GetPortableRuntimeWorldRevision();
        std::array<std::optional<PortableObjectLookupResult>,2u> priorAny;
        for(std::size_t p=0u;p<Paths.size();++p)priorAny[p]=FindPortableRuntimeObject({Paths[p],0u,0u,true,false});
        Refused([&] {(void)PreparePortableRuntimeNativeInputClasses();},"Missing/malformed/conflicting/unaudited Input config was silently accepted");
        SnapshotIs(ReadPortableRuntimeNativeInputClasses(),false);SamePrerequisites(engine,metadata);
        Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup()),"Config refusal changed actual lookup objects/capacity/generation");
        for(std::size_t p=0u;p<Paths.size();++p) {
            const auto* path=Paths[p];
            Refused([&] {(void)ReadPortableRuntimeObjectOwnership(path);},"Config refusal published a partial native Input Class");
            // ANY_PACKAGE deliberately falls back to the final leaf anywhere,
            // so absent Engine.Input can resolve an unrelated authored Input
            // property. Preserve that real result, and test absence using the
            // actual Class filter and resolved Package Outer without fallback.
            Require(SameFound(priorAny[p],FindPortableRuntimeObject({path,0u,0u,true,false})),
                "Config refusal changed original ANY_PACKAGE lookup semantics/results");
            const auto meta=metadata.classes[3u].objectIndex;
            for(const auto& spelling:{std::string(path),Fold(path)})
                Require(!FindPortableRuntimeObject({spelling,meta,0u,false,true}),
                    "Config refusal published a canonical/folded native Input Class into lookup");
            const auto package=ReadPortableRuntimeObjectOwnership(Packages[p]);
            const auto leaf=Fold(std::string(path).substr(std::string(path).find('.')+1u));
            Require(!FindPortableRuntimeObject({leaf,meta,package.objectIndex,false,true}),
                "Config refusal published a native Input Class into direct Package-Outer lookup");
        }
        GC::Collect();SameGc(gc,"Invalid Input config leaked staged UClasses/domain/index leases");
        Require(temporary.Snapshot("ConfigAfter"+std::to_string(i))==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
            "Invalid Input config changed dormant checkpoint/revision/world");
    }
    temporary.Ini("Input=Engine.Input\n");const auto gc=GC::GetStats();
    const auto selected=PreparePortableRuntimeNativeInputClasses();SnapshotIs(selected,true,&metadata,"Engine.Input");Lookup(selected);
    Require(GC::GetStats().numObjects==gc.numObjects+3u,"Base Input selection changed the closed two-Class registration allocation graph");
    temporary.Ini("Input=malformed\n");
    SameSnapshot(selected,ReadPortableRuntimeNativeInputClasses(),metadata);
    SameSnapshot(selected,PreparePortableRuntimeNativeInputClasses(),metadata);
    temporary.RemoveIni();SameSnapshot(selected,ReadPortableRuntimeNativeInputClasses(),metadata);
    SameSnapshot(selected,PreparePortableRuntimeNativeInputClasses(),metadata);
    SamePrerequisites(engine,metadata);copy.Unchanged();ShutdownPortableRuntime();

    temporary.Ini("iNpUt=eXtEnSiOn.iNpUtExT\n");
    Require(InitializePortableRuntime(packages).passed&&PreparePortableRuntimeNativeEngineClasses().prepared&&
        PreparePortableRuntimeNativeMetadataClasses().prepared&&PreparePortableRuntimeNativePackages().prepared,
        "Cannot initialize folded InputExt config fixture");
    const auto foldedMetadata=ReadPortableRuntimeNativeMetadataClasses();
    const auto folded=PreparePortableRuntimeNativeInputClasses();SnapshotIs(folded,true,&foldedMetadata,"Extension.InputExt");
    Require(!ReadPortableRuntimeObjectLookup().prepared,"Input class registration implicitly prepared unrelated object lookup service");
    Require(PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare lookup after native Input registration");Lookup(folded);
    SameSnapshot(folded,PreparePortableRuntimeNativeInputClasses(),foldedMetadata);copy.Unchanged();ShutdownPortableRuntime();
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        else throw std::runtime_error("Usage: portable_native_input_classes_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native Input class integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        SnapshotIs(ReadPortableRuntimeNativeInputClasses(),false);
        Refused([&] {(void)PreparePortableRuntimeNativeInputClasses();},"Input class phase succeeded without initialized runtime");
        GC::Collect();SameGc(baseline,"No-runtime Input class preparation leaked native objects");
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");
        for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
            "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Original(root,packages,map,temporary);ConfigFixtures(root,packages,temporary);
        ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Native Input class roots/index leases survived explicit runtime shutdown");
        SnapshotIs(ReadPortableRuntimeNativeInputClasses(),false);
        Require(!ReadPortableRuntimeObjectLookup().prepared,"Shutdown retained the Input-bearing object lookup service");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned native Input class integration: "<<checks<<" checks, "<<refusals
            <<" refusals; exact two native UClasses/+3 GC graph, actual Super/Within/metaclass/Package indices, canonical/folded lookup and casts, "
              "unchanged five/thirteen prerequisites, atomic missing-phase/config refusal, copied-Core adjacent config selection, idempotent committed "
              "reads/preparation, first Training dormant save/warm restore/map/GC/shutdown lifetime. Not native properties/defaults, User.ini binding "
              "imports, Input instances, viewport Init/Reset, native ReadInput, startup, physics or VR rendering.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
