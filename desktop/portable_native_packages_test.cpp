#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Actual package/default/Outer identity and embedding lifetime only. The owned
// inputs are read as bytes; no commercial executable or library is executed.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}
std::string Fold(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin(),b=parent.begin();
    for(;b!=parent.end();++a,++b)if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
template<class F> void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read package fixture source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Package source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Cannot finish package fixture source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Owned/source copy bytes, size or mtime changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Temporary package fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-native-packages-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated package fixture");
        Require(fs::create_directory(directory/"System"),"Cannot create package fixture System directory");
        const auto ini=Track(directory/"System"/"DeusEx.ini");std::ofstream file(ini,std::ios::binary|std::ios::trunc);
        const std::string text="[Engine.Engine]\nGameEngine=Engine.GameEngine\n";
        Require(bool(file),"Cannot create isolated package INI");file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
        Require(bool(file),"Cannot finish isolated package INI");
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
        Require(path.parent_path()==directory||path.parent_path()==directory/"System","Invalid isolated package file path");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    PortablePackageTables Copy(const fs::path& path) {
        const auto target=Track(directory/"System"/path.filename());
        Require(fs::copy_file(path,target),"Cannot copy unchanged owned package into isolated fixture");
        return LoadPortablePackageTables(target.string());
    }
    Bytes Snapshot(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid package checkpoint leaf");
        const auto path=Track(directory/(leaf+".sav"));Require(SavePortableRuntimeState(path.string()),"Cannot save whole package fixture checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read package fixture checkpoint");
        return bytes;
    }
};
void SameGc(const GCStats& expected,const std::string& why) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,why);
}
const PortableNativePackageSnapshot& Package(const PortableNativePackagesSnapshot& snapshot,const std::string& path) {
    const auto found=std::find_if(snapshot.packages.begin(),snapshot.packages.end(),[&](const auto& row) {return row.objectPath==path;});
    Require(found!=snapshot.packages.end(),"Actual package is absent: "+path);return *found;
}
void SnapshotIs(const PortableNativePackagesSnapshot& snapshot,bool prepared,std::size_t count=4u) {
    Require(snapshot.prepared==prepared&&snapshot.packages.size()==(prepared?count:0u),"Package preparation state/count differs");
    if(!prepared) {
        Require(!snapshot.defaultObjectIndex&&!snapshot.defaultClassIndex,"Unprepared package phase exposed partial default identity");return;
    }
    Require(snapshot.defaultObjectIndex&&snapshot.defaultClassIndex&&snapshot.defaultObjectIndex!=snapshot.defaultClassIndex,
        "Prepared Package default lacks distinct actual default/Class identities");
    std::set<std::uint32_t> indices;
    for(const auto& row:snapshot.packages) {
        Require(!row.objectPath.empty()&&row.objectIndex&&indices.insert(row.objectIndex).second&&
            row.objectIndex!=snapshot.defaultObjectIndex&&row.objectIndex!=snapshot.defaultClassIndex,
            "Package object has an empty, duplicate or default/Class-alias identity");
        Require(row.classIndex==snapshot.defaultClassIndex&&!row.outerIndex&&row.objectFlags==4u&&row.packageFlags==0u&&
            row.legacyHandleAbsent&&row.bindingAttempted,"Top-level package did not preserve audited missing-module constructor state");
    }
    const auto cls=ReadPortableRuntimeObjectOwnership("Core.Package");
    Require(cls.objectIndex==snapshot.defaultClassIndex&&cls.classPath=="Core.Class"&&
        cls.outerIndex==Package(snapshot,"Core").objectIndex&&cls.outerPath=="Core"&&!cls.serializedOuterZero,
        "Core.Package UClass/default and actual Core owning Package are not distinct linked identities");
}
void SamePackages(const PortableNativePackagesSnapshot& expected,const PortableNativePackagesSnapshot& actual,std::size_t count=4u) {
    SnapshotIs(actual,true,count);
    Require(actual.defaultObjectIndex==expected.defaultObjectIndex&&actual.defaultClassIndex==expected.defaultClassIndex,
        "Repeated package phase replaced prepared actual default/Class identities");
    for(const auto& row:expected.packages) {
        const auto& other=Package(actual,row.objectPath);
        Require(other.objectIndex==row.objectIndex&&other.classIndex==row.classIndex&&other.outerIndex==row.outerIndex&&
            other.objectFlags==row.objectFlags&&other.packageFlags==row.packageFlags&&
            other.legacyHandleAbsent==row.legacyHandleAbsent&&other.bindingAttempted==row.bindingAttempted,
            "Repeated package phase replaced identity or retried/changed constructor binding state");
    }
}
void Ownership(const std::string& path,const std::string& cls,const std::string& outer,
    std::uint32_t outerIndex,bool serializedZero) {
    const auto value=ReadPortableRuntimeObjectOwnership(path);
    Require(value.objectPath==path&&value.objectIndex&&value.classPath==cls&&value.classIndex&&
        value.outerPath==outer&&value.outerIndex==outerIndex&&value.serializedOuterZero==serializedZero,
        "Actual UObject ownership/provenance differs: "+path);
}
void NativeOwners(const PortableNativePackagesSnapshot& snapshot) {
    // Independent original registration package literals; not derived from a
    // production table or inferred by a generic display-path prefix service.
    constexpr std::array<const char*,16u> native{
        "Core.Package","Engine.Primitive","Engine.GameEngine","Extension.GameEngineExt","DeusEx.DeusExGameEngine",
        "Core.Field","Core.Struct","Core.State","Core.Class","Core.Property","Core.ObjectProperty",
        "Core.ClassProperty","Core.IntProperty","Core.BoolProperty","Core.StrProperty","Core.ArrayProperty"};
    constexpr std::array<const char*,16u> owners{
        "Core","Engine","Engine","Extension","DeusEx","Core","Core","Core","Core","Core","Core","Core","Core","Core","Core","Core"};
    for(std::size_t i=0u;i<native.size();++i)
        Ownership(native[i],"Core.Class",owners[i],Package(snapshot,owners[i]).objectIndex,false);
    Ownership("Core.Object","Core.Class","Core",Package(snapshot,"Core").objectIndex,true);
    Ownership("Engine.Console","Core.Class","Engine",Package(snapshot,"Engine").objectIndex,true);
    const auto engine=ReadPortableRuntimeObjectOwnership("Engine.Engine");
    Ownership("Engine.Engine.Cylinder","Core.ObjectProperty","Engine.Engine",engine.objectIndex,false);
}
void Prerequisites(const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    Require(InitializePortableRuntime(packages).objects>0u,"Cannot initialize original package fixture");
    SnapshotIs(ReadPortableRuntimeNativePackages(),false);
    for(unsigned phase=0u;phase<2u;++phase) {
        if(phase)Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare actual native Engine dependency");
        const auto gc=GC::GetStats();const auto before=temporary.Snapshot(phase?"OnlyEngineBefore":"NoPhaseBefore");
        const auto revision=GetPortableRuntimeWorldRevision();
        Refused([&] {(void)PreparePortableRuntimeNativePackages();},"Package phase silently prepared missing Core metadata");
        SnapshotIs(ReadPortableRuntimeNativePackages(),false);
        Require(!ReadPortableRuntimeNativeMetadataClasses().prepared&&ReadPortableRuntimeNativeEngineClasses().prepared==bool(phase),
            "Package refusal auto-published or removed its prerequisite class phase");
        GC::Collect();SameGc(gc,"Missing-phase Package refusal leaked defaults, packages or index leases");
        Require(temporary.Snapshot(phase?"OnlyEngineAfter":"NoPhaseAfter")==before&&GetPortableRuntimeWorldRevision()==revision,
            "Missing-phase package refusal changed checkpoint or world revision");
    }
    const auto unbound=ReadPortableRuntimeObjectOwnership("Engine.Engine.Cylinder");
    Require(!unbound.classIndex&&unbound.classPath.empty(),"Fixture unexpectedly has an unregistered Core.ObjectProperty class");
    const auto metadata=PreparePortableRuntimeNativeMetadataClasses();
    Require(metadata.prepared,"Cannot prepare actual Core metadata dependency");
    for(const auto& table:packages) for(std::size_t i=0u;i<table.exports.size();++i) {
        const auto& entry=table.exports[i];if(entry.ObjClass>=0)continue;
        const auto path=GetPortableObjectPath(table,entry.ObjClass);
        const auto row=std::find_if(metadata.classes.begin(),metadata.classes.end(),[&](const auto& cls) {return cls.classPath==path;});
        if(row==metadata.classes.end())continue;
        const auto owner=ReadPortableRuntimeObjectOwnership(fs::path(table.sourcePath).stem().string()+'.'+
            GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)));
        Require(owner.classPath==row->classPath&&owner.classIndex==row->objectIndex,
            "Original metadata Class import was not bound to its actual registered allocation");
    }
}
PortablePackageTables SyntheticMap(const fs::path& directory,const std::string& name) {
    PortablePackageTables result;result.sourcePath=(directory/(name+".dx")).string();result.version=68u;
    for(const char* text:{"None","Core","Package","Class","Object","PackageOwnerProbe","Child","Literal.Dotted","Absent","MissingOuter","Orphan"})
        result.names.push_back({NameString(text),0u});
    result.imports={{1,2,0,1},{1,3,-1,4},{1,2,0,8},{1,4,-3,9}};
    result.exports={{-2,0,0,5,ObjectFlags{},0,-1},{-2,0,1,6,ObjectFlags{},0,-1},
        {-2,0,0,7,ObjectFlags{},0,-1},{-2,0,-4,10,ObjectFlags{},0,-1}};
    return result;
}
void SyntheticOwnership(const PortableNativePackagesSnapshot& persistent,Temporary& temporary) {
    const auto map=SyntheticMap(temporary.directory,"OwnershipMap");
    const auto loaded=LoadPortableRuntimeMap(map);
    Require(loaded.exports==map.exports.size()&&!loaded.passed&&loaded.actors==0u,
        "Tables-only ownership fixture did not publish its identities or invented playable script/actor success");
    const auto withMap=ReadPortableRuntimeNativePackages();SamePackages(persistent,withMap,5u);
    const auto root=Package(withMap,"OwnershipMap").objectIndex;
    Ownership("OwnershipMap.PackageOwnerProbe","Core.Object","OwnershipMap",root,true);
    const auto parent=ReadPortableRuntimeObjectOwnership("OwnershipMap.PackageOwnerProbe");
    Ownership("OwnershipMap.PackageOwnerProbe.Child","Core.Object",parent.objectPath,parent.objectIndex,false);
    Ownership("OwnershipMap.Literal.Dotted","Core.Object","OwnershipMap",root,true);
    const auto orphanPath="OwnershipMap."+GetPortableObjectPath(map,4);
    const auto orphan=ReadPortableRuntimeObjectOwnership(orphanPath);
    Require(!orphan.outerIndex&&orphan.outerPath.empty()&&!orphan.serializedOuterZero,
        "Unresolved nonzero serialized Outer was incorrectly promoted to the source LinkerRoot");
    const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    auto collision=SyntheticMap(temporary.directory,"Engine");
    Refused([&] {(void)LoadPortableRuntimeMap(collision);},"Different source map overwrote an existing static Engine linker root");
    SamePackages(withMap,ReadPortableRuntimeNativePackages(),5u);GC::Collect();SameGc(gc,"Map source-name refusal retired/replaced current identities");
    Require(GetPortableRuntimeWorldRevision()==revision&&ReadPortableRuntimeObjectOwnership(parent.objectPath).objectIndex==parent.objectIndex,
        "Map source-name refusal replaced current map or advanced its revision");
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot unload synthetic ownership exports");GC::Collect();
    SamePackages(persistent,ReadPortableRuntimeNativePackages());
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership("OwnershipMap");},"Retired map-only package still has an indexed lookup alias");
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership(parent.objectPath);},"Retired map export still has an indexed lookup alias");
    Require(LoadPortableRuntimeMap(map).exports==map.exports.size(),"Cannot reload synthetic package ownership fixture");
    const auto reloaded=ReadPortableRuntimeNativePackages();SamePackages(persistent,reloaded,5u);
    // An allocation-slot index is reusable AFTER actual GC destruction. The
    // unavailable old alias/GC-baseline assertions prove retirement; requiring
    // a monotonically different index would contradict the real slot pool.
    Require(Package(reloaded,"OwnershipMap").objectIndex!=0u,"Reloaded Package lacks its own live index lease");
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot retire reloaded synthetic package fixture");
    // Extension has a native class but no serialized source linker in this
    // minimal Core/Engine fixture. Its real class->Outer edge must survive
    // releasing a later map loader that temporarily uses this same Package.
    const auto extension=Package(persistent,"Extension").objectIndex;
    auto retained=SyntheticMap(temporary.directory,"Extension");
    Require(LoadPortableRuntimeMap(retained).exports==retained.exports.size(),"Cannot load native-owned package map fixture");
    SamePackages(persistent,ReadPortableRuntimeNativePackages());
    Ownership("Extension.PackageOwnerProbe","Core.Object","Extension",extension,true);
    Require(UnloadPortableRuntimeMap()==retained.exports.size(),"Cannot revoke map ownership of native-retained Package");GC::Collect();
    SamePackages(persistent,ReadPortableRuntimeNativePackages());NativeOwners(persistent);
    auto empty=SyntheticMap(temporary.directory,"EmptyOwnershipMap");empty.exports.clear();
    Require(LoadPortableRuntimeMap(empty).exports==0u,"Empty source published fabricated exports");
    SamePackages(persistent,ReadPortableRuntimeNativePackages(),5u);
    Require(UnloadPortableRuntimeMap()==0u,"Empty source unload fabricated removed exports");GC::Collect();
    SamePackages(persistent,ReadPortableRuntimeNativePackages());
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership("EmptyOwnershipMap");},"Empty source retained its revoked linker root");
    auto broken=SyntheticMap(temporary.directory,"BrokenOwnershipMap");
    broken.names.push_back({NameString("Engine"),0u});broken.names.push_back({NameString("Actor"),0u});
    broken.imports.push_back({1,2,0,11});broken.imports.push_back({1,3,-5,12});
    broken.exports[0].ObjClass=-6;broken.exports[0].ObjSize=1;broken.exports[0].ObjOffset=0;
    Refused([&] {(void)LoadPortableRuntimeMap(broken);},"Invalid Actor payload unexpectedly loaded without its source bytes");
    GC::Collect();SamePackages(persistent,ReadPortableRuntimeNativePackages());NativeOwners(persistent);
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership("BrokenOwnershipMap");},"Failed map load leaked Package alias");
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership("BrokenOwnershipMap.PackageOwnerProbe");},"Failed map load leaked export alias");
}
void Original(const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    Prerequisites(packages,temporary);
    const auto checkpoint=temporary.Snapshot("BeforePackages");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs");
    const auto prepared=PreparePortableRuntimeNativePackages();SnapshotIs(prepared,true);NativeOwners(prepared);
    Require(GC::GetStats().numObjects==gc.numObjects+5u&&GC::GetStats().memoryUsage>gc.memoryUsage,
        "Package preparation did not create exactly four real Packages and one actual class-owned default");
    const auto preparedGc=GC::GetStats();
    for(unsigned i=0u;i<4u;++i) {
        SamePackages(prepared,PreparePortableRuntimeNativePackages());SamePackages(prepared,ReadPortableRuntimeNativePackages());
        NativeOwners(prepared);GC::Collect();SameGc(preparedGc,"Repeated package prepare/read/GC changed actual owning graph");
    }
    auto detached=ReadPortableRuntimeNativePackages();detached.packages.front().objectPath="Changed.Package";
    detached.defaultObjectIndex=0u;detached.prepared=false;SamePackages(prepared,ReadPortableRuntimeNativePackages());
    Require(temporary.Snapshot("AfterPackages")==checkpoint&&GetPortableRuntimeWorldRevision()==revision&&
        QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"))&&
        GetPortableRuntimeMapActors(true).empty()&&!GetPortableRuntimeUnsavedScriptState(),
        "Package/default preparation changed dormant checkpoint, world, clock or original Engine defaults");
    Refused([&] {(void)ReadPortableRuntimeLevelTime();},"Package preparation invented a slot-zero LevelInfo/clock without a map");
    const auto loaded=LoadPortableRuntimeMap(map);
    Require(loaded.exports==map.exports.size(),"Cannot publish original Training export identities for ownership inspection");
    const auto withMap=ReadPortableRuntimeNativePackages();SamePackages(prepared,withMap,5u);
    const auto mapIndex=Package(withMap,"00_Training").objectIndex;
    std::size_t top{},nested{};
    for(std::size_t i=0u;i<map.exports.size();++i) {
        const auto& entry=map.exports[i];const auto path="00_Training."+GetPortableObjectPath(map,static_cast<std::int32_t>(i+1u));
        const auto actual=ReadPortableRuntimeObjectOwnership(path);
        Require(actual.objectPath==path&&actual.objectIndex&&actual.serializedOuterZero==(entry.ObjOuter==0),
            "Original Training raw serialized Outer provenance was lost");
        if(entry.ObjOuter==0) {
            ++top;Require(actual.outerIndex==mapIndex&&actual.outerPath=="00_Training",
                "Original Training top-level export lacks its actual source LinkerRoot");
        } else if(entry.ObjOuter>0) {
            ++nested;const auto parent=ReadPortableRuntimeObjectOwnership("00_Training."+GetPortableObjectPath(map,entry.ObjOuter));
            Require(actual.outerIndex==parent.objectIndex&&actual.outerPath==parent.objectPath,
                "Original Training nested export replaced its actual serialized parent with a package prefix");
        }
    }
    Require(top>0u,"Original map fixture did not exercise source-root export ownership");
    std::cout<<"Original Training Outer census: "<<top<<" source-root exports, "<<nested
        <<" nested exports (nested ownership also checked in Engine metadata and synthetic map).\n";
    const auto mapGc=GC::GetStats();const auto mapCheckpoint=temporary.Snapshot("MapWithPackages");
    SamePackages(withMap,PreparePortableRuntimeNativePackages(),5u);GC::Collect();SameGc(mapGc,"Repeated package preparation changed original map ownership");
    Require(LoadPortableRuntimeState((temporary.directory/"MapWithPackages.sav").string()),"Cannot warm-restore original map ownership fixture");
    SamePackages(withMap,ReadPortableRuntimeNativePackages(),5u);GC::Collect();SameGc(mapGc,"Warm restore replaced Package/default objects");
    Require(temporary.Snapshot("MapWarm")==mapCheckpoint,"Warm restore changed whole original map checkpoint");
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot unload original map export ownership");GC::Collect();
    SamePackages(prepared,ReadPortableRuntimeNativePackages());SameGc(preparedGc,"Map unload retained map-only Package/default/export graph");
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership("00_Training");},"Original map-only Package remained indexed after retirement/GC");
    NativeOwners(prepared);SyntheticOwnership(prepared,temporary);
    SamePackages(prepared,ReadPortableRuntimeNativePackages());GC::Collect();SameGc(preparedGc,"Synthetic map lifecycle leaked Package identities");
    ShutdownPortableRuntime();SnapshotIs(ReadPortableRuntimeNativePackages(),false);
}
void WrongMetadataImport(std::vector<PortablePackageTables> packages) {
    auto& engine=packages.at(1u);std::size_t changed{};
    const auto badName=static_cast<std::int32_t>(engine.names.size());
    engine.names.push_back({NameString("NotCore"),0u});
    for(std::size_t i=0u;i<engine.imports.size();++i)
        if(GetPortableObjectPath(engine,-static_cast<std::int32_t>(i+1u))=="Core.ObjectProperty") {
            engine.imports[i].ClassPackage=badName;++changed;
        }
    Require(changed>0u,"Original import authority control did not alter an actual ObjectProperty Class import");
    Require(InitializePortableRuntime(packages).objects>0u,"Cannot initialize malformed import authority control");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare independent Engine classes for import authority control");
    Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare genuine native metadata registrations for import control");
    const auto field=ReadPortableRuntimeObjectOwnership("Engine.Engine.Cylinder");
    Require(!field.classIndex&&field.classPath.empty(),
        "An import typed NotCore.Class granted native Core.ObjectProperty Class authority from its display path");
    ShutdownPortableRuntime();
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        else throw std::runtime_error("Usage: portable_native_packages_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native Package/Outer integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        for(const char* name:{"Core","Engine"}) {
            const auto path=root/"System"/(std::string(name)+".u");sources.emplace_back(path);
            packages.push_back(temporary.Copy(path));sources.emplace_back(packages.back().sourcePath);
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Original(packages,map,temporary);WrongMetadataImport(packages);
        ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Package/default ownership graph survived explicit shutdown");
        SnapshotIs(ReadPortableRuntimeNativePackages(),false);
        Refused([&] {(void)PreparePortableRuntimeNativePackages();},"Package phase succeeded without initialized runtime");
        Refused([&] {(void)ReadPortableRuntimeObjectOwnership("Core");},"Shutdown Package retained public indexed ownership authority");
        GC::Collect();SameGc(baseline,"No-runtime Package operations leaked actual graph");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned native Package/Outer integration: "<<checks<<" checks, "<<refusals
            <<" refusals; actual Package/default/Class indices, explicit absent legacy-module binding state, audited native ownership, "
              "serialized source-root/nested/unresolved Outer provenance, dormant checkpoint preservation, map reload/warm restore/GC/shutdown, "
              "and native-retained Package survival. Minimal original Core/Engine plus Training identity inspection; not full gameplay, "
              "legacy DLL loading, Engine/Cylinder instances, begun world, crate physics or VR rendering.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
