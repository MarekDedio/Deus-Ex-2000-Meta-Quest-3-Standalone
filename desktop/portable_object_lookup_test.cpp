#include "Precomp.h"
#include "GC/GC.h"
#include "Package/NameString.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Owned package bytes are only read/copied, never executed. These tests prove
// actual UObject lookup and its embedding lifetime, not startup or gameplay.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
template<class F>void Refused(F&& operation,const std::string& message) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}
    Require(refused,message);++refusals;
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
std::uint64_t Hash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint fixture: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);
        if(total>512u*1024u*1024u)throw std::runtime_error("Owned lookup fixture exceeds fingerprint bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Cannot finish fixture fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& source):path(fs::canonical(source)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(Hash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Hash(path)==hash,
            "Owned lookup source/copy changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(source)),"Temporary lookup fixture overlaps owned/root directory");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-object-lookup-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated lookup fixture");
        Require(fs::create_directory(directory/"System"),"Cannot create isolated lookup System directory");
        const auto ini=Track(directory/"System"/"DeusEx.ini");
        std::ofstream file(ini,std::ios::binary|std::ios::trunc);
        const std::string text="[Engine.Engine]\nGameEngine=Engine.GameEngine\n";
        Require(bool(file),"Cannot create isolated lookup INI");file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
        Require(bool(file),"Cannot finish isolated lookup INI");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& path:files)
            if(path.parent_path()==directory||path.parent_path()==directory/"System")fs::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent) {
            fs::remove(directory/"System",ignored);fs::remove(directory,ignored);
        }
    }
    fs::path Track(const fs::path& path) {
        Require(path.parent_path()==directory||path.parent_path()==directory/"System","Invalid isolated lookup file path");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    PortablePackageTables Copy(const fs::path& original) {
        const auto target=Track(directory/"System"/original.filename());
        Require(fs::copy_file(original,target),"Cannot copy owned lookup package unchanged");
        return LoadPortablePackageTables(target.string());
    }
    Bytes Snapshot(const std::string& name) {
        Require(!name.empty()&&name.find_first_of("/\\:")==std::string::npos,"Invalid lookup checkpoint leaf");
        const auto path=Track(directory/(name+".sav"));
        Require(SavePortableRuntimeState(path.string()),"Cannot save lookup fixture checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read lookup checkpoint");
        return bytes;
    }
};
void SameGc(const GCStats& expected,const std::string& message) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,message);
}
void SameState(const PortableObjectLookupState& expected,const PortableObjectLookupState& actual) {
    Require(actual.prepared==expected.prepared&&actual.objects==expected.objects&&actual.retainedBytes==expected.retainedBytes&&
        actual.generation==expected.generation,"Lookup membership/accounting/session changed unexpectedly");
}
PortableObjectLookupRequest Request(std::string name,std::uint32_t cls=0u,
    std::uint32_t outer=0u,bool any=true,bool exact=false) {
    return {std::move(name),cls,outer,any,exact};
}
PortableObjectLookupResult Found(const PortableObjectLookupRequest& query,const std::string& expected,
    std::uint64_t generation) {
    const auto result=FindPortableRuntimeObject(query);
    Require(result.has_value(),"Actual UObject lookup missed: "+query.name);
    const auto owner=ReadPortableRuntimeObjectOwnership(expected);
    Require(result->objectPath==expected&&result->objectIndex==owner.objectIndex&&result->classIndex==owner.classIndex&&
        result->outerIndex==owner.outerIndex&&result->generation==generation,"Lookup returned alias/wrong ownership/session: "+query.name);
    const auto dot=expected.find_last_of('.');
    const auto name=NameString::FindCompareIndex(dot==std::string::npos?expected:expected.substr(dot+1u));
    Require(name&&result->nameIndex==*name,"Lookup leaf did not preserve actual FName identity");
    return *result;
}
void Missing(const PortableObjectLookupRequest& query,const std::string& message) {
    Require(!FindPortableRuntimeObject(query),message);
}
void Unprepared() {
    const auto state=ReadPortableRuntimeObjectLookup();
    Require(!state.prepared&&!state.objects&&!state.retainedBytes,"Unprepared lookup exposed partial indexed state");
}
PortablePackageTables SyntheticMap(const fs::path& directory,const std::string& name) {
    PortablePackageTables table;table.sourcePath=(directory/(name+".dx")).string();table.version=68u;
    for(const char* value:{"None","Core","Package","Class","Object","LookupOwnerProbe","Child","Literal.Dotted","Absent","MissingOuter","Orphan"})
        table.names.push_back({NameString(value),0u});
    table.imports={{1,2,0,1},{1,3,-1,4},{1,2,0,8},{1,4,-3,9}};
    table.exports={{-2,0,0,5,ObjectFlags{},0,-1},{-2,0,1,6,ObjectFlags{},0,-1},
        {-2,0,0,7,ObjectFlags{},0,-1},{-2,0,-4,10,ObjectFlags{},0,-1}};
    return table;
}
void MapLifetime(Temporary& temporary,const PortableObjectLookupState& staticState,std::uint32_t objectClass) {
    const auto gc=GC::GetStats();const auto map=SyntheticMap(temporary.directory,"LookupMap");
    const auto loaded=LoadPortableRuntimeMap(map);
    Require(loaded.exports==map.exports.size()&&!loaded.passed&&!loaded.actors,"Tables-only map fabricated playable startup or omitted identities");
    const auto state=ReadPortableRuntimeObjectLookup();
    Require(state.prepared&&state.generation==staticState.generation&&state.objects==staticState.objects+map.exports.size()+1u&&
        state.retainedBytes>staticState.retainedBytes,"Post-prepare map exports/LinkerRoot were not added to weak lookup");
    const auto root=ReadPortableRuntimeObjectOwnership("LookupMap");
    const auto owner=Found(Request("LookupMap.LookupOwnerProbe",objectClass),"LookupMap.LookupOwnerProbe",state.generation);
    Found(Request("Child",objectClass,owner.objectIndex,false,true),"LookupMap.LookupOwnerProbe.Child",state.generation);
    Found(Request("LookupMap.LookupOwnerProbe.Child",objectClass),"LookupMap.LookupOwnerProbe.Child",state.generation);
    Found(Request("LookupOwnerProbe",objectClass,root.objectIndex,false,true),"LookupMap.LookupOwnerProbe",state.generation);
    // Text ResolveName splits dots. A real literal-dot FName must not become a
    // made-up nested Outer tree just because its diagnostic path looks nested.
    Require(NameString::FindCompareIndex("Literal.Dotted").has_value(),"Literal-dot fixture FName is absent");
    Missing(Request("LookupMap.Literal.Dotted",objectClass),"Lookup inferred an Outer from a literal-dot display path");
    Missing(Request("Literal.Dotted",objectClass,root.objectIndex,false),"Explicit Outer bypassed dotted prefix resolution");
    const auto orphanPath="LookupMap."+GetPortableObjectPath(map,4);
    const auto orphan=ReadPortableRuntimeObjectOwnership(orphanPath);
    Require(!orphan.outerIndex&&!orphan.serializedOuterZero,"Fixture lost unresolved nonzero serialized Outer provenance");
    Missing(Request("Orphan",objectClass,root.objectIndex,false),"Unresolved nonzero Outer was promoted to map LinkerRoot");
    const auto actualOrphan=FindPortableRuntimeObject(Request("Orphan",objectClass));
    Require(actualOrphan&&actualOrphan->objectIndex==orphan.objectIndex&&!actualOrphan->outerIndex&&
        actualOrphan->generation==state.generation,"ANY_PACKAGE did not preserve actual null Outer on a valid class object");
    const auto mapRevision=GetPortableRuntimeWorldRevision();const auto mapGc=GC::GetStats();
    auto collision=SyntheticMap(temporary.directory,"Engine");
    Refused([&] {(void)LoadPortableRuntimeMap(collision);},"Map load replaced a static source Package with a same-name different source");
    SameState(state,ReadPortableRuntimeObjectLookup());GC::Collect();SameGc(mapGc,"Source-name collision changed live weak lookup graph");
    Require(GetPortableRuntimeWorldRevision()==mapRevision,"Refused source-name collision advanced world revision");
    Found(Request("LookupMap.LookupOwnerProbe",objectClass),"LookupMap.LookupOwnerProbe",state.generation);
    const auto bytes=temporary.Snapshot("LookupMapWarm");const auto beforeRestore=ReadPortableRuntimeObjectLookup();
    Require(!LoadPortableRuntimeState((temporary.directory/"LookupMapWarm.sav").string()),
        "Tables-only map restore fabricated an unavailable source/native Level");
    SameState(beforeRestore,ReadPortableRuntimeObjectLookup());
    Require(temporary.Snapshot("LookupMapRefusedRestore")==bytes,"Refused tables-only restore changed whole checkpoint");
    Found(Request("LookupMap.LookupOwnerProbe.Child",objectClass),"LookupMap.LookupOwnerProbe.Child",state.generation);
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot unload weak lookup map exports");GC::Collect();
    SameState(staticState,ReadPortableRuntimeObjectLookup());SameGc(gc,"Weak lookup retained unloaded map graph");
    Missing(Request("LookupMap.LookupOwnerProbe",objectClass),"Weak lookup kept retired map prefix/export alive");
    Missing(Request("LookupMap"),"Weak lookup kept retired linker Package alive");
    Refused([&] {(void)FindPortableRuntimeObject(Request("Child",objectClass,owner.objectIndex,false));},
        "Retired map Outer index remained an admitted handle");
    Require(LoadPortableRuntimeMap(map).exports==map.exports.size(),"Cannot reload weak lookup map fixture");
    Found(Request("LookupMap.LookupOwnerProbe.Child",objectClass),"LookupMap.LookupOwnerProbe.Child",state.generation);
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot unload reloaded weak lookup map fixture");GC::Collect();
    SameState(staticState,ReadPortableRuntimeObjectLookup());SameGc(gc,"Reloaded weak lookup leaked map graph");

    const auto extension=Found(Request("Extension"),"Extension",state.generation);
    auto retained=SyntheticMap(temporary.directory,"Extension");
    Require(LoadPortableRuntimeMap(retained).exports==retained.exports.size(),"Cannot load native-retained Package map fixture");
    const auto retainedState=ReadPortableRuntimeObjectLookup();
    Require(retainedState.objects==staticState.objects+retained.exports.size(),"Native-retained map invented a duplicate Package lookup record");
    Found(Request("LookupOwnerProbe",objectClass,extension.objectIndex,false),"Extension.LookupOwnerProbe",state.generation);
    Require(UnloadPortableRuntimeMap()==retained.exports.size(),"Cannot release map linker ownership of native-retained Package");GC::Collect();
    SameState(staticState,ReadPortableRuntimeObjectLookup());SameGc(gc,"Native-retained Package map cleanup leaked lookup objects");
    const auto still=Found(Request("Extension"),"Extension",state.generation);
    Require(still.objectIndex==extension.objectIndex,"Unloading a map replaced its native-retained actual Package");
    Found(Request("Extension.GameEngineExt"),"Extension.GameEngineExt",state.generation);

    auto empty=SyntheticMap(temporary.directory,"EmptyLookupMap");empty.exports.clear();
    Require(LoadPortableRuntimeMap(empty).exports==0u,"Empty source map fabricated export identities");
    const auto emptyState=ReadPortableRuntimeObjectLookup();
    Require(emptyState.objects==staticState.objects+1u&&emptyState.generation==staticState.generation,
        "Empty map did not publish exactly its real linker Package");
    Found(Request("EmptyLookupMap"),"EmptyLookupMap",state.generation);
    Require(UnloadPortableRuntimeMap()==0u,"Empty map unload fabricated removed exports");GC::Collect();
    SameState(staticState,ReadPortableRuntimeObjectLookup());SameGc(gc,"Empty map weak lookup retained its revoked linker root");
    Missing(Request("EmptyLookupMap"),"Empty map Package alias survived GC without an owning graph");
    auto broken=SyntheticMap(temporary.directory,"BrokenLookupMap");
    broken.names.push_back({NameString("Engine"),0u});broken.names.push_back({NameString("Actor"),0u});
    broken.imports.push_back({1,2,0,11});broken.imports.push_back({1,3,-5,12});
    broken.exports.front().ObjClass=-6;broken.exports.front().ObjSize=1;broken.exports.front().ObjOffset=0;
    Refused([&] {(void)LoadPortableRuntimeMap(broken);},"Actor export without source payload unexpectedly loaded");
    GC::Collect();SameState(staticState,ReadPortableRuntimeObjectLookup());SameGc(gc,"Failed map load leaked weak lookup identities");
    Missing(Request("BrokenLookupMap"),"Failed map load leaked a linker Package lookup record");
    Missing(Request("BrokenLookupMap.LookupOwnerProbe"),"Failed map load leaked a staged export lookup record");

    // Canonical-looking Class paths alone do not authenticate their imported
    // ClassPackage. Likewise a null requested-class exact hash hit does NOT
    // perform the separate Class admission needed by filtered/global lookup.
    auto forged=SyntheticMap(temporary.directory,"ForgedLookupMap");
    for(const auto* name:{"ForgedClassPackage","BadClassProbe","BadBaseClass","BadBaseProbe","ClasslessFunctionProbe","Function"})
        forged.names.push_back({NameString(name),0u});
    forged.imports.push_back({11,3,-1,4}); // ForgedClassPackage.Class, canonical path Core.Object.
    forged.imports.push_back({1,3,-1,16}); // Genuine Core.Class import, but Core.Function is not registered here.
    forged.exports={{-5,0,0,12,ObjectFlags{},0,-1},{0,-5,0,13,ObjectFlags{},0,-1},
        {2,0,0,14,ObjectFlags{},0,-1},{-6,0,0,15,ObjectFlags{},0,-1}};
    Require(GetPortableObjectPath(forged,-5)=="Core.Object","Forged import control did not preserve the misleading canonical Class path");
    Require(LoadPortableRuntimeMap(forged).exports==forged.exports.size(),"Cannot load tables-only Class provenance controls");
    const auto forgedRoot=ReadPortableRuntimeObjectOwnership("ForgedLookupMap");
    const auto classClass=ReadPortableRuntimeObjectOwnership("Core.Class").objectIndex;
    for(const auto* name:{"BadClassProbe","BadBaseProbe","ClasslessFunctionProbe"}) {
        const auto path=std::string("ForgedLookupMap.")+name;
        const auto row=ReadPortableRuntimeObjectOwnership(path);
        const auto exact=FindPortableRuntimeObject(Request(name,0u,forgedRoot.objectIndex,false));
        Require(exact&&exact->objectIndex==row.objectIndex&&exact->outerIndex==forgedRoot.objectIndex&&
            exact->generation==state.generation,"Null-class exact hash phase incorrectly required Class/Super admission");
        Refused([&] {(void)FindPortableRuntimeObject(Request(name,objectClass,forgedRoot.objectIndex,false));},
            "Filtered lookup invented Class/Super authority from a forged import path or absent native UClass");
        Refused([&] {(void)FindPortableRuntimeObject(Request(name));},
            "ANY_PACKAGE Object iterator admitted an unproven Class/Super chain");
    }
    const auto absentFunction=ReadPortableRuntimeObjectOwnership("ForgedLookupMap.ClasslessFunctionProbe");
    Require(!absentFunction.classIndex&&absentFunction.classPath.empty(),"Unregistered native Core.Function was invented for classless control");
    const auto badBase=Found(Request("BadBaseClass",classClass,forgedRoot.objectIndex,false,true),
        "ForgedLookupMap.BadBaseClass",state.generation);
    // Original IsA first compares the actual Class pointer. A direct Class
    // equality does not need to read its unproven imported Super. The earlier
    // Core.Object/ANY controls do need that ancestry and correctly refuse it.
    Found(Request("BadBaseProbe",badBase.objectIndex,forgedRoot.objectIndex,false),
        "ForgedLookupMap.BadBaseProbe",state.generation);
    Found(Request("BadBaseProbe",badBase.objectIndex,forgedRoot.objectIndex,false,true),
        "ForgedLookupMap.BadBaseProbe",state.generation);
    Require(UnloadPortableRuntimeMap()==forged.exports.size(),"Cannot retire Class provenance controls");GC::Collect();
    SameState(staticState,ReadPortableRuntimeObjectLookup());SameGc(gc,"Class provenance control map leaked weak lookup objects");
}
std::uint64_t Original(const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    Unprepared();Refused([] {(void)PreparePortableRuntimeObjectLookup();},"Lookup prepared without initialized actual objects");
    Refused([] {(void)FindPortableRuntimeObject(Request("Core.Object"));},"Lookup found string-path alias before preparation");
    const auto summary=InitializePortableRuntime(packages);Require(summary.objects>0u,"Cannot initialize owned lookup fixture");
    Unprepared();
    Refused([] {(void)PreparePortableRuntimeObjectLookup();},"Lookup auto-created absent Package/class dependencies");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare lookup Engine class prerequisite");
    Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare lookup Core metaclass prerequisite");
    Require(PreparePortableRuntimeNativeConfigClasses().prepared,"Cannot prepare lookup native config class prerequisite");
    Unprepared();Refused([] {(void)PreparePortableRuntimeObjectLookup();},"Lookup accepted absent actual Package Outers");
    const auto packagesState=PreparePortableRuntimeNativePackages();Require(packagesState.prepared,"Cannot prepare actual lookup Package ownership");
    Unprepared();Refused([] {(void)FindPortableRuntimeObject(Request("Core.Object"));},"Package phase silently enabled UObject lookup");
    const auto before=temporary.Snapshot("BeforeLookup");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    const auto state=PreparePortableRuntimeObjectLookup();
    std::cout<<"LOOKUP population: source="<<summary.objects<<" indexed="<<state.objects
        <<" packages="<<packagesState.packages.size()<<"; expected22 native UClasses\n";
    Require(state.prepared&&state.generation&&state.retainedBytes>0u&&state.objects==summary.objects+22u+packagesState.packages.size(),
        "Lookup count includes defaults, omits real indexed objects or duplicates actual Packages/native UClasses");
    SameGc(gc,"Weak lookup preparation allocated a GC/root-owned object");
    const auto classClass=ReadPortableRuntimeObjectOwnership("Core.Class").objectIndex;
    const auto objectClass=ReadPortableRuntimeObjectOwnership("Core.Object").objectIndex;
    const auto engine=ReadPortableRuntimeObjectOwnership("Engine").objectIndex;
    const auto core=ReadPortableRuntimeObjectOwnership("Core").objectIndex;
    Require(classClass&&objectClass&&engine&&core&&classClass!=objectClass&&engine!=core,"Owned lookup authority handles alias or are absent");
    Found(Request("Core.Class",classClass,0u,true,true),"Core.Class",state.generation);
    Found(Request("Engine.Console",classClass),"Engine.Console",state.generation);
    Found(Request("Console",classClass,engine,false,true),"Engine.Console",state.generation);
    Found(Request("Core.Language",classClass,0u,true,true),"Core.Language",state.generation);
    Found(Request("Core.Object",objectClass),"Core.Object",state.generation);
    const auto actorKeys=ReadPortableRuntimeEnum("Engine.Actor.EInputKey");
    const auto consoleKeys=ReadPortableRuntimeEnum("Engine.Console.EInputKey");
    // The native UInput ctor uses actual UEnum class + actual AActor Outer,
    // never an ANY_PACKAGE basename or the different Console enum.
    const auto actorKeyFound=Found(Request("EInputKey",actorKeys.classIndex,actorKeys.outerIndex,false,false),
        actorKeys.objectPath,state.generation);
    const auto consoleKeyFound=Found(Request("EInputKey",consoleKeys.classIndex,consoleKeys.outerIndex,false,false),
        consoleKeys.objectPath,state.generation);
    Require(actorKeyFound.objectIndex==actorKeys.objectIndex&&consoleKeyFound.objectIndex==consoleKeys.objectIndex&&
        actorKeyFound.objectIndex!=consoleKeyFound.objectIndex,"Native input Enum lookup substituted a matching basename");
    Missing(Request("EInputKey",actorKeys.classIndex,core,false,true),"UEnum lookup escaped its explicit Struct Outer");
    Missing(Request("EInputKey",classClass,actorKeys.outerIndex,false),"Enum instance was admitted as a UClass");
    Missing(Request("Core.Object",objectClass,0u,true,true),"Exact Core.Object filter admitted actual Core.Class instance");
    Found(Request("Engine",packagesState.defaultClassIndex,0u,false,true),"Engine",state.generation);
    Found(Request("Engine",objectClass,0u,false,false),"Engine",state.generation);
    Missing(Request("Engine",objectClass,0u,false,true),"Exact Object filter admitted Package instance");
    Missing(Request("Console",classClass,core,false),"Explicit Core Outer searched a different Package");
    Found(Request("Engine.Object",classClass),"Core.Object",state.generation);
    Missing(Request("Engine.Object",classClass,0u,false),"Explicit root lookup incorrectly performed ANY_PACKAGE leaf fallback");
    const std::string unknownPrefix="QUEST_LOOKUP_UNINTERNED_PREFIX_20261018";
    const std::string unknownLeaf="QUEST_LOOKUP_UNINTERNED_LEAF_20261018";
    Require(!NameString::FindCompareIndex(unknownPrefix)&&!NameString::FindCompareIndex(unknownLeaf),"Unknown-name observer is contaminated");
    Missing(Request(unknownPrefix+".Object",classClass),"Missing dotted prefix fell back to globally visible Object");
    Missing(Request(unknownLeaf),"Unknown leaf name returned a UObject");
    Missing(Request("Engine."+unknownLeaf,classClass),"Unknown qualified leaf returned a UObject");
    Missing(Request("None"),"None FName matched an indexed UObject");
    Missing(Request("Core.None"),"Qualified None FName matched an indexed UObject");
    Require(!NameString::FindCompareIndex(unknownPrefix)&&!NameString::FindCompareIndex(unknownLeaf),"Read-only lookup interned a query prefix/leaf");
    for(const auto* prefix:{"ini:","INI:","usr:","UsR:"})
        Refused([&] {(void)FindPortableRuntimeObject(Request(std::string(prefix)+"Engine.Engine.GameEngine"));},
            "Unsupported ini:/usr: branch was treated as an ordinary object path");
    Refused([&] {(void)FindPortableRuntimeObject(Request("Object",std::numeric_limits<std::uint32_t>::max()));},
        "Unavailable requested Class handle was accepted");
    Refused([&] {(void)FindPortableRuntimeObject(Request("Console",engine));},"Ordinary Package object was accepted as UClass filter");
    Refused([&] {(void)FindPortableRuntimeObject(Request("Console",classClass,std::numeric_limits<std::uint32_t>::max(),false));},
        "Unavailable Outer handle was accepted");
    Refused([&] {(void)FindPortableRuntimeObject(Request("Console",classClass,engine,true));},"ANY_PACKAGE combined with explicit Outer was accepted");
    for(const auto& malformed:std::array<std::string,5u>{"",".Core","Core.","Core..Object",std::string("Core\0.Object",12u)})
        Refused([&] {(void)FindPortableRuntimeObject(Request(malformed));},"Malformed or embedded-NUL name bypassed bounded lookup profile");
    const auto property=ReadPortableRuntimeObjectOwnership("Core.Object.Class");
    Require(property.classIndex!=0u&&property.outerIndex==objectClass,"Original non-Package prefix fixture lacks actual typed field ownership");
    Found(Request("Core.Object.Class",property.classIndex,0u,true,true),"Core.Object.Class",state.generation);
    Found(Request("Class",property.classIndex,objectClass,false,true),"Core.Object.Class",state.generation);
    Missing(Request("Core.Object.Class",classClass,0u,false),"Property object was admitted as UClass merely because its leaf is Class");
    constexpr std::array<const char*,22u> nativeClasses{
        "Core.Package","Engine.Primitive","Engine.GameEngine","Extension.GameEngineExt","DeusEx.DeusExGameEngine",
        "Core.Field","Core.Struct","Core.State","Core.Class","Core.Property","Core.ObjectProperty","Core.ClassProperty",
        "Core.IntProperty","Core.BoolProperty","Core.StrProperty","Core.ArrayProperty","Core.StructProperty","Core.Enum",
        "Core.Language","Engine.AudioSubsystem","Engine.RenderDevice","Engine.NetDriver"};
    for(const auto* path:nativeClasses)Found(Request(path,classClass,0u,true,true),path,state.generation);
    for(unsigned i=0u;i<3u;++i) {
        SameState(state,PreparePortableRuntimeObjectLookup());SameState(state,ReadPortableRuntimeObjectLookup());
        GC::Collect();SameGc(gc,"Lookup prepare/read/GC changed actual object ownership");
    }
    auto detached=ReadPortableRuntimeObjectLookup();detached.objects=0u;detached.generation=0u;detached.prepared=false;
    Require(!detached.objects&&!detached.generation&&!detached.prepared,"Cannot modify detached lookup snapshot observer");
    SameState(state,ReadPortableRuntimeObjectLookup());
    auto detachedObject=Found(Request("Engine.Console",classClass),"Engine.Console",state.generation);
    detachedObject.objectPath="Fake.Object";detachedObject.objectIndex=0u;detachedObject.classIndex=0u;
    Found(Request("Engine.Console",classClass),"Engine.Console",state.generation);
    Require(temporary.Snapshot("AfterLookup")==before&&GetPortableRuntimeWorldRevision()==revision&&
        GetPortableRuntimeMapActors(true).empty()&&!GetPortableRuntimeUnsavedScriptState(),
        "Lookup mutated dormant saved script/world/revision state");
    // A checkpoint containing native Actor state requires an actual loaded
    // Level. Do not fake successful restore in this no-map metadata fixture;
    // LoadedTrainingRestore below verifies the real positive restore case.
    Require(!LoadPortableRuntimeState((temporary.directory/"AfterLookup.sav").string()),"No-map restore fabricated native Level authority");
    SameState(state,ReadPortableRuntimeObjectLookup());GC::Collect();SameGc(gc,"Refused no-map restore replaced lookup objects");
    Require(temporary.Snapshot("RefusedLookupRestore")==before,"Refused no-map restore changed whole checkpoint");
    MapLifetime(temporary,state,objectClass);
    ShutdownPortableRuntime();Unprepared();GC::Collect();
    Refused([&] {(void)FindPortableRuntimeObject(Request("Console",classClass,engine,false));},
        "Retired session handles retained lookup authority after shutdown");
    return state.generation;
}
void LoadedTrainingRestore(const fs::path& root,Temporary& temporary) {
    std::vector<PortablePackageTables> packages;std::vector<Source> sources;
    // Real source root, not the earlier System-only copied-package fixture:
    // restore must resolve the actual Maps/00_Training.dx backing file.
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
        "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
        sources.emplace_back(root/"System"/(std::string(name)+".u"));
        packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
    }
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(InitializePortableRuntime(packages).passed&&PreparePortableRuntimeNativeEngineClasses().prepared&&
        PreparePortableRuntimeNativeMetadataClasses().prepared&&PreparePortableRuntimeNativeConfigClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared,"Cannot prepare actual first-Training lookup restore fixture");
    Require(PreparePortableRuntimeObjectLookup().prepared&&LoadPortableRuntimeMap(map).passed,
        "Cannot load only actual first Training with live object lookup");
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();
    BindPortableRuntimeClient(lifetime,1401u);RegisterPortableRuntimeViewport(1401u,{1402u,1440u,1440u,3u,2u});
    const auto viewport=PreparePortableRuntimePlayerObjects(1402u);
    const auto lookup=ReadPortableRuntimeObjectLookup();const auto input=ReadPortableRuntimeEnum("Engine.Actor.EInputKey");
    const auto before=temporary.Snapshot("TrainingLookupBefore");const auto gc=GC::GetStats();
    for(unsigned repeat=0;repeat<2u;++repeat) {
        const auto revision=GetPortableRuntimeWorldRevision();
        Require(LoadPortableRuntimeState((temporary.directory/"TrainingLookupBefore.sav").string()),
            "Actual loaded first-Training native checkpoint restore failed");
        Require(GetPortableRuntimeWorldRevision()==revision+1u,"Actual warm restore did not advance world revision once");
        SameState(lookup,ReadPortableRuntimeObjectLookup());
        Require(ReadPortableRuntimePlayerSession(1402u).viewportPath==viewport.viewportPath&&
            !ReadPortableRuntimePlayerSession(1402u).bound,"Actual warm restore replaced or fabricated viewport possession");
        const auto found=Found(Request("EInputKey",input.classIndex,input.outerIndex,false,false),input.objectPath,lookup.generation);
        Require(found.objectIndex==input.objectIndex&&ReadPortableRuntimeEnum(input.objectPath)==input,
            "Warm first-Training restore replaced original indexed input Enum authority");
        Require(temporary.Snapshot("TrainingLookupAfter")==before,"Actual warm restore changed complete dormant checkpoint");
        GC::Collect();SameGc(gc,"Actual warm first-Training restore changed rooted source/map/lookup graph");
    }
    Require(UnloadPortableRuntimeMap()>0u,"Cannot retire first-Training lookup restore fixture");
    Require(ReadPortableRuntimeEnum(input.objectPath)==input,"Map retirement removed static input Enum authority");
    lifetime->Revoke();ClearPortableRuntimeClient();
    ShutdownPortableRuntime();for(const auto& source:sources)source.Unchanged();
}
void LateRegistration(const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    const auto summary=InitializePortableRuntime(packages);Require(summary.objects>0u,"Cannot initialize late-registration lookup session");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared,
        "Cannot prepare actual classes for late registration");
    const auto nativePackages=PreparePortableRuntimeNativePackages();Require(nativePackages.prepared,"Cannot prepare late-registration actual Packages");
    Require(!ReadPortableRuntimeNativeConfigClasses().prepared,"Fresh late-registration session already has config UClasses");
    const auto gc=GC::GetStats();const auto state=PreparePortableRuntimeObjectLookup();
    Require(state.prepared&&state.objects==summary.objects+18u+nativePackages.packages.size(),"Pre-config lookup indexed absent config classes or default aliases");
    SameGc(gc,"Pre-config weak lookup allocated a GC object");
    const auto classClass=ReadPortableRuntimeObjectOwnership("Core.Class").objectIndex;
    const auto engine=ReadPortableRuntimeObjectOwnership("Engine").objectIndex;
    const auto console=Found(Request("Engine.Console",classClass),"Engine.Console",state.generation);
    Missing(Request("Core.Language",classClass,0u,false),"Config-only native UClass existed before its actual registration phase");
    const auto before=temporary.Snapshot("LateConfigBefore");const auto revision=GetPortableRuntimeWorldRevision();
    const auto config=PreparePortableRuntimeNativeConfigClasses();Require(config.prepared,"Late config class registration refused valid actual Packages/lookup");
    const auto after=ReadPortableRuntimeObjectLookup();
    Require(after.prepared&&after.generation==state.generation&&after.objects==state.objects+4u&&after.retainedBytes>state.retainedBytes,
        "Late config registration did not publish exactly four actual class identities into existing lookup");
    Require(GC::GetStats().numObjects==gc.numObjects+5u&&GC::GetStats().memoryUsage>gc.memoryUsage,
        "Late config registration allocated something other than four UClasses and their actual domain");
    const auto afterPackages=ReadPortableRuntimeNativePackages();
    Require(afterPackages.defaultObjectIndex==nativePackages.defaultObjectIndex&&afterPackages.defaultClassIndex==nativePackages.defaultClassIndex&&
        afterPackages.packages.size()==nativePackages.packages.size(),"Late config registration replaced defaults or actual Packages");
    constexpr std::array<const char*,4u> additions{"Core.Language","Engine.AudioSubsystem","Engine.RenderDevice","Engine.NetDriver"};
    for(const auto* path:additions)Found(Request(path,classClass,0u,true,true),path,state.generation);
    const auto sameConsole=Found(Request("Console",classClass,engine,false,true),"Engine.Console",state.generation);
    Require(sameConsole.objectIndex==console.objectIndex&&config.consoleObjectIndex==console.objectIndex,
        "Late config registration replaced original serialized Console UObject");
    const auto configGc=GC::GetStats();
    Require(PreparePortableRuntimeNativeConfigClasses().prepared,"Repeated late config phase lost its actual class domain");
    SameState(after,PreparePortableRuntimeObjectLookup());GC::Collect();SameGc(configGc,"Repeated late config/lookup preparation leaked objects");
    Require(temporary.Snapshot("LateConfigAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
        GetPortableRuntimeMapActors(true).empty()&&!GetPortableRuntimeUnsavedScriptState(),
        "Late config registration changed dormant saved state or world revision");

    const auto objectClass=ReadPortableRuntimeObjectOwnership("Core.Object").objectIndex;
    const auto playerClass=ReadPortableRuntimeObjectOwnership("Engine.Player").objectIndex;
    constexpr std::uint64_t clientIdentity=1201u,firstViewport=1202u,secondViewport=1203u;
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientIdentity);
    RegisterPortableRuntimeViewport(clientIdentity,{firstViewport,1440u,1440u,3u,2u});
    const auto session=PreparePortableRuntimePlayerObjects(firstViewport);
    Require(session.clientIdentity==clientIdentity&&session.viewportIdentity==firstViewport&&
        session.viewportClassPath=="Engine.Viewport"&&session.viewportOuterPath==session.clientPath&&
        !session.bound&&session.playerPath.empty(),"Late native client preparation fabricated a possessed PlayerPawn");
    const auto live=ReadPortableRuntimeObjectLookup();
    Require(live.generation==after.generation&&live.objects==after.objects+4u&&live.retainedBytes>after.retainedBytes,
        "Post-lookup native client did not add exactly Client/Viewport classes and two instances");
    Require(GC::GetStats().numObjects==configGc.numObjects+5u,"Native client allocated other than two classes/two instances/one owning domain");
    const auto clientClass=Found(Request("Client",classClass,engine,false,true),"Engine.Client",state.generation);
    const auto viewportClass=Found(Request("Viewport",classClass,engine,false,true),"Engine.Viewport",state.generation);
    const auto client=Found(Request(session.clientPath,clientClass.objectIndex,0u,false,true),session.clientPath,state.generation);
    Require(client.outerIndex==0u,"Native Client instance acquired an invented Package Outer");
    const auto viewportName=session.viewportPath.substr(session.viewportPath.find_last_of('.')+1u);
    const auto viewport=Found(Request(viewportName,viewportClass.objectIndex,client.objectIndex,false,true),session.viewportPath,state.generation);
    Found(Request(session.viewportPath,playerClass),session.viewportPath,state.generation);
    Found(Request(session.viewportPath,objectClass),session.viewportPath,state.generation);
    Missing(Request(session.viewportPath,playerClass,0u,true,true),"Exact Engine.Player filter admitted native concrete Viewport class");
    Missing(Request(viewportName,clientClass.objectIndex,client.objectIndex,false),"Client filter admitted actual Viewport instance");
    Refused([&] {(void)FindPortableRuntimeObject(Request(viewportName,client.objectIndex));},
        "Native Client instance was accepted as a requested UClass handle");
    const auto liveGc=GC::GetStats();
    Require(PreparePortableRuntimePlayerObjects(firstViewport).viewportPath==session.viewportPath,"Repeated player preparation replaced its actual viewport");
    SameState(live,ReadPortableRuntimeObjectLookup());GC::Collect();SameGc(liveGc,"Idempotent native client preparation republished/duplicated lookup objects");
    RegisterPortableRuntimeViewport(clientIdentity,{secondViewport,1440u,1440u,3u,2u});
    const auto other=PreparePortableRuntimePlayerObjects(secondViewport);
    Require(other.clientPath==session.clientPath&&other.viewportPath!=session.viewportPath&&!other.bound&&other.playerPath.empty(),
        "Second viewport replaced Client or invented possession");
    const auto stereo=ReadPortableRuntimeObjectLookup();
    Require(stereo.objects==live.objects+1u&&stereo.generation==state.generation&&GC::GetStats().numObjects==liveGc.numObjects+1u,
        "Second logical viewport did not publish exactly one actual UObject");
    Found(Request(other.viewportPath,viewportClass.objectIndex,0u,true,true),other.viewportPath,state.generation);
    const auto saved=temporary.Snapshot("LiveLookupSession");
    Require(!LoadPortableRuntimeState((temporary.directory/"LiveLookupSession.sav").string()),
        "No-map native session restore fabricated a native Level");
    SameState(stereo,ReadPortableRuntimeObjectLookup());
    Require(ReadPortableRuntimePlayerSession(firstViewport).viewportPath==session.viewportPath&&
        ReadPortableRuntimePlayerSession(secondViewport).viewportPath==other.viewportPath&&
        temporary.Snapshot("LiveLookupSessionRefusedRestore")==saved,"Refused no-map restore replaced live native viewport identities or saved state");
    Found(Request(session.viewportPath,viewportClass.objectIndex),session.viewportPath,state.generation);
    lifetime->Revoke();
    Refused([&] {(void)ReadPortableRuntimePlayerSession(firstViewport);},"Revoked native client retained usable viewport/session lease");
    ClearPortableRuntimeClient();GC::Collect();
    Require(!HasPortableRuntimeClient(),"Native client clear retained a live renderer registration");
    const auto cleared=ReadPortableRuntimeObjectLookup();
    Require(cleared.objects==after.objects+2u&&cleared.generation==state.generation,
        "Client clear did not retire instances while preserving the two registered engine-owned UClasses");
    Require(GC::GetStats().numObjects==configGc.numObjects+3u,
        "Client clear kept instance graph alive or destroyed genuine engine-owned native UClasses/domain");
    Missing(Request(session.clientPath),"Weak lookup retained cleared native Client instance");
    Missing(Request(session.viewportPath),"Weak lookup retained cleared first Viewport instance");
    Missing(Request(other.viewportPath),"Weak lookup retained cleared second Viewport instance");
    Refused([&] {(void)FindPortableRuntimeObject(Request(viewportName,viewportClass.objectIndex,client.objectIndex,false));},
        "Cleared native Client index was accepted as a live Outer handle");
    Refused([&] {(void)FindPortableRuntimeObject(Request("Class",classClass,viewport.objectIndex,false));},
        "Cleared native Viewport index was accepted as a live Outer handle");
    Require(Found(Request("Engine.Client",classClass),"Engine.Client",state.generation).objectIndex==clientClass.objectIndex&&
        Found(Request("Engine.Viewport",classClass),"Engine.Viewport",state.generation).objectIndex==viewportClass.objectIndex,
        "Client clear replaced retained native UClass identities");

    auto replacement=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(replacement,1301u);
    RegisterPortableRuntimeViewport(1301u,{1302u,1440u,1440u,3u,2u});
    const auto next=PreparePortableRuntimePlayerObjects(1302u);
    Require(!next.bound&&next.playerPath.empty()&&next.clientPath!=session.clientPath,"Fresh native Client generation reused old diagnostic instance identity or fabricated possession");
    const auto replaced=ReadPortableRuntimeObjectLookup();
    Require(replaced.objects==cleared.objects+2u&&replaced.generation==state.generation,
        "Fresh renderer generation duplicated existing UClasses or omitted Client/Viewport instances");
    Found(Request(next.clientPath,clientClass.objectIndex,0u,true,true),next.clientPath,state.generation);
    Found(Request(next.viewportPath,viewportClass.objectIndex,0u,true,true),next.viewportPath,state.generation);
    replacement->Revoke();ClearPortableRuntimeClient();GC::Collect();SameState(cleared,ReadPortableRuntimeObjectLookup());
    Require(temporary.Snapshot("LateRegistrationFinal")==before&&GetPortableRuntimeMapActors(true).empty()&&!GetPortableRuntimeUnsavedScriptState(),
        "Native renderer registration/cleanup contaminated dormant script checkpoint or actor state");
    ShutdownPortableRuntime();Unprepared();GC::Collect();
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* value=std::getenv("DEUSEX_GAME_ROOT");if(value&&*value)root=value;}
        else throw std::runtime_error("Usage: portable_object_lookup_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned actual UObject lookup integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        for(const auto* leaf:{"Core","Engine"}) {
            const auto path=root/"System"/(std::string(leaf)+".u");sources.emplace_back(path);
            packages.push_back(temporary.Copy(path));sources.emplace_back(packages.back().sourcePath);
        }
        const auto oldGeneration=Original(packages,temporary);SameGc(baseline,"Shutdown retained actual weak-lookup object graph");
        Require(InitializePortableRuntime(packages).objects>0u,"Cannot reinitialize owned lookup fixture");Unprepared();
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared,
            "Cannot prepare fresh actual object lookup session");
        const auto fresh=PreparePortableRuntimeObjectLookup();Require(fresh.prepared&&fresh.generation>oldGeneration,
            "Fresh lookup session reused retired generation authority");
        Found(Request("Core.Object"),"Core.Object",fresh.generation);
        ShutdownPortableRuntime();Unprepared();GC::Collect();SameGc(baseline,"Reinitialized weak lookup session survived shutdown");
        LateRegistration(packages,temporary);SameGc(baseline,"Late config/client registration lookup graph survived shutdown");
        LoadedTrainingRestore(root,temporary);SameGc(baseline,"Actual first-Training lookup restore graph survived shutdown");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned actual UObject lookup integration: "<<checks<<" checks, "<<refusals
            <<" refusals; real Class/Super/Outer handles, exact and derived filters, native and serialized objects, "
              "dotted Package/non-Package prefix resolution, qualified ANY_PACKAGE leaf fallback, non-interning names, "
              "weak map publication/unload/reload, native-retained Package survival, late config and native Client/Viewport registration/cleanup, "
              "warm restore and fresh-session generations. "
              "No commercial DLL execution, Engine initialization, crate physics or live VR gameplay is claimed.\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"FAIL owned actual UObject lookup after "<<checks<<" checks: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;
    }
}
