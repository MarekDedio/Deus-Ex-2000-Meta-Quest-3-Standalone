#include "Precomp.h"
#include "GC/GC.h"
#include "Package/NameString.h"
#include "portable_input_schema.h"
#include "portable_object_lookup.h"
#include "portable_system_services.h"
#include "portable_unreal_runtime.h"
#include "quest_native_config.h"
#include "quest_runtime_script_packages.h"
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

// Actual native USystem registration/default/GSys service only. Original owned
// files are data, never executable modules. This is not full FExec, engine or
// player startup, live Quest rendering, or a proof of crate/loot/gravity play.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using QuestVr::Vm::Kind;
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
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
Bytes Read(const fs::path& path,std::size_t maximum=1u<<20u) {
    Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),maximum,bytes),"Cannot read bounded System fixture: "+path.string());return bytes;
}
std::uint64_t Hash(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint owned System source: "+path.string());
    std::array<char,16384u> buffer{};std::uintmax_t count{};std::uint64_t hash=14695981039346656037ull;
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto n=input.gcount();
        count+=static_cast<std::uintmax_t>(n);Require(count<=512u*1024u*1024u,"System source fingerprint exceeds bound");
        for(std::streamsize i=0;i<n;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(input.eof()&&!input.bad(),"Cannot finish owned System source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Hash(path)==hash,
            "System service changed owned source bytes/size/mtime: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"System temporary fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned suffix=0u;suffix<20u;++suffix) {
            const auto candidate=parent/("quest-system-services-"+std::to_string(stamp)+'-'+std::to_string(suffix));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated System fixture directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code error;
        for(const auto& file:files)if(file.parent_path()==directory) {
            const auto status=fs::symlink_status(file,error);
            if(!error&&fs::is_regular_file(status)&&!fs::is_symlink(status))fs::remove(file,error);
        }
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,error);
    }
    fs::path Track(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid System fixture leaf");
        const auto path=directory/leaf;Require(path.parent_path()==directory,"System fixture escaped isolated directory");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    fs::path CopyIni(const fs::path& original) {
        const auto bytes=Read(original);const auto path=Track("DeusEx.ini");
        std::ofstream output(path,std::ios::binary|std::ios::trunc);Require(bool(output),"Cannot write isolated System INI copy");
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));output.close();
        Require(bool(output),"Cannot finish isolated System INI copy");return path;
    }
    Bytes Save(const std::string& leaf) {
        const auto path=Track(leaf);Require(SavePortableRuntimeState(path.string()),"Cannot checkpoint dormant System fixture");
        return Read(path,QuestVr::kMaximumSaveRuntimeBytes);
    }
    void Remove(const fs::path& path) {
        Require(path.parent_path()==directory&&std::find(files.begin(),files.end(),path)!=files.end()&&
            fs::is_regular_file(fs::symlink_status(path))&&!fs::is_symlink(fs::symlink_status(path)),"Unsafe isolated System INI removal");
        Require(fs::remove(path),"Cannot remove tracked isolated System INI copy");
    }
};
void SameGc(const GCStats& before,const std::string& why) {
    const auto after=GC::GetStats();Require(after.numObjects==before.numObjects&&after.memoryUsage==before.memoryUsage,why);
}
bool SameLookup(const PortableObjectLookupState& a,const PortableObjectLookupState& b) {
    return a.prepared==b.prepared&&a.objects==b.objects&&a.retainedBytes==b.retainedBytes&&a.generation==b.generation;
}
void Empty(const PortableSystemServicesSnapshot& value) {
    Require(!value.prepared&&!value.rooted&&!value.execInstalled&&!value.constructorLinksVerified&&value.classPath.empty()&&value.objectPath.empty()&&
        value.outerPath.empty()&&value.configPath.empty()&&!value.classIndex&&!value.objectIndex&&!value.outerIndex&&!value.objectFlags&&
        !value.classFlags&&!value.nativeSize&&!value.superIndex&&!value.withinIndex&&!value.purgeCacheDays&&value.savePath.empty()&&
        value.cachePath.empty()&&value.cacheExt.empty()&&value.paths.empty()&&value.suppress.empty()&&
        value.defaultPaths.empty()&&value.defaultSuppress.empty()&&value.fields.empty(),
        "Unprepared System service exposed partial identity/config/field authority");
}
void Same(const PortableSystemServicesSnapshot& a,const PortableSystemServicesSnapshot& b) {
    Require(a.prepared==b.prepared&&a.rooted==b.rooted&&a.execInstalled==b.execInstalled&&
        a.constructorLinksVerified==b.constructorLinksVerified&&a.classPath==b.classPath&&
        a.objectPath==b.objectPath&&a.outerPath==b.outerPath&&a.configPath==b.configPath&&a.classIndex==b.classIndex&&
        a.objectIndex==b.objectIndex&&a.outerIndex==b.outerIndex&&a.objectFlags==b.objectFlags&&a.classFlags==b.classFlags&&
        a.nativeSize==b.nativeSize&&a.superIndex==b.superIndex&&a.withinIndex==b.withinIndex&&a.purgeCacheDays==b.purgeCacheDays&&
        a.savePath==b.savePath&&a.cachePath==b.cachePath&&a.cacheExt==b.cacheExt&&a.paths==b.paths&&a.suppress==b.suppress&&
        a.defaultPaths==b.defaultPaths&&a.defaultSuppress==b.defaultSuppress&&
        a.fields.size()==b.fields.size(),"System service snapshot identity/config/count changed");
    for(std::size_t i=0u;i<a.fields.size();++i) {
        const auto& x=a.fields[i];const auto& y=b.fields[i];
        Require(x.path==y.path&&x.classPath==y.classPath&&x.outerPath==y.outerPath&&x.nextPath==y.nextPath&&x.innerPath==y.innerPath&&
            x.category==y.category&&x.index==y.index&&x.classIndex==y.classIndex&&x.outerIndex==y.outerIndex&&x.objectFlags==y.objectFlags&&
            x.flags==y.flags&&x.dimension==y.dimension&&x.elementSize==y.elementSize&&x.offset==y.offset,
            "System service field header/layout/link changed: "+x.path);
    }
}
const PortableSystemField& Field(const PortableSystemServicesSnapshot& value,const std::string& path) {
    const auto found=std::find_if(value.fields.begin(),value.fields.end(),[&](const auto& field){return field.path==path;});
    Require(found!=value.fields.end(),"System service lacks actual field: "+path);return *found;
}
void Header(const std::string& path,std::uint32_t index,const std::string& cls,std::uint32_t classIndex,
    const std::string& outer,std::uint32_t outerIndex,std::uint32_t flags) {
    const auto owner=ReadPortableRuntimeObjectOwnership(path);
    Require(index&&classIndex&&owner.objectPath==path&&owner.classPath==cls&&owner.outerPath==outer&&owner.objectIndex==index&&
        owner.classIndex==classIndex&&owner.outerIndex==outerIndex&&!owner.serializedOuterZero,"System native object lacks actual Class/Outer/index: "+path);
    const auto lookup=FindPortableRuntimeObject({Fold(path),classIndex,0u,true,true});
    Require(lookup&&lookup->objectPath==path&&lookup->objectIndex==index&&lookup->classIndex==classIndex&&lookup->outerIndex==outerIndex,
        "System native object missing from genuine class-filtered object lookup: "+path);
    const auto leaf=path.substr(path.rfind('.')+1u);
    const auto direct=FindPortableRuntimeObject({Fold(leaf),classIndex,outerIndex,false,true});
    Require(direct&&direct->objectIndex==index&&direct->outerIndex==outerIndex,"System direct-Outer lookup changed identity: "+path);
    const auto read=[&](const char* property,Kind kind) {
        const auto result=ReadPortableRuntimeDeclaredScriptProperty(path,std::string("Core.Object.")+property);
        Require(result.kind==kind,"System generic header has wrong value kind: "+path+' '+property);return result;
    };
    Require(read("Class",Kind::Object).text==cls&&read("Name",Kind::Name).text==leaf&&read("Outer",Kind::Object).text==outer&&
        static_cast<std::uint32_t>(read("ObjectFlags",Kind::Int).integer)==flags,"System generic header differs from actual native storage: "+path);
}
void Layout(const PortableSystemServicesSnapshot& value) {
    Require(value.prepared&&value.rooted&&value.execInstalled&&value.constructorLinksVerified&&
        value.classPath=="Core.System"&&value.outerPath=="Transient"&&
        value.objectFlags==0u&&value.classFlags==0x16u&&value.nativeSize==0x6cu&&value.fields.size()==8u&&value.classIndex&&
        value.objectIndex&&value.outerIndex&&value.classIndex!=value.objectIndex,"System is not the actual rooted native USystem/GSys graph");
    const auto cls=ReadPortableRuntimeObjectOwnership(value.classPath),meta=ReadPortableRuntimeObjectOwnership("Core.Class");
    const auto core=ReadPortableRuntimeObjectOwnership("Core"),subsystem=ReadPortableRuntimeObjectOwnership("Core.Subsystem");
    const auto object=ReadPortableRuntimeObjectOwnership("Core.Object"),transient=ReadPortableRuntimeObjectOwnership("Transient");
    Require(value.classIndex==cls.objectIndex&&value.superIndex==subsystem.objectIndex&&value.withinIndex==object.objectIndex&&
        value.outerIndex==transient.objectIndex,"System native registration lost its actual Super/Within/Transient identities");
    Header(value.classPath,value.classIndex,"Core.Class",meta.objectIndex,"Core",core.objectIndex,0x04084004u);
    Header(value.objectPath,value.objectIndex,value.classPath,value.classIndex,value.outerPath,value.outerIndex,0u);
    std::set<std::uint32_t> indices{value.classIndex,value.objectIndex};
    const std::array<const char*,6u> names{{"PurgeCacheDays","SavePath","CachePath","CacheExt","Paths","Suppress"}};
    const std::array<const char*,6u> classes{{"Core.IntProperty","Core.StrProperty","Core.StrProperty","Core.StrProperty","Core.ArrayProperty","Core.ArrayProperty"}};
    const std::array<std::uint32_t,6u> offsets{{0x2cu,0x30u,0x3cu,0x48u,0x54u,0x60u}};
    for(std::size_t i=0u;i<names.size();++i) {
        const auto fieldPath=value.classPath+'.'+names[i];
        const auto& field=Field(value,fieldPath);const auto type=ReadPortableRuntimeObjectOwnership(classes[i]);
        const auto next=i?value.classPath+'.'+names[i-1u]:std::string{};
        Require(field.classPath==classes[i]&&field.outerPath==value.classPath&&field.outerIndex==value.classIndex&&
            field.classIndex==type.objectIndex&&field.objectFlags==4u&&field.flags==(i?0x404000u:0x4000u)&&field.dimension==1u&&
            field.elementSize==(i?12u:4u)&&field.offset==offsets[i]&&field.category=="Options"&&field.nextPath==next&&
            indices.insert(field.index).second,"System own field has wrong native type/layout/config/Next identity: "+field.path);
        Header(field.path,field.index,field.classPath,field.classIndex,field.outerPath,field.outerIndex,4u);
        if(i<4u)Require(field.innerPath.empty(),"System scalar manufactured an array inner");
        else {
            Require(!field.innerPath.empty(),"System native array omitted its actual indexed inner field");
            const auto& inner=Field(value,field.innerPath);const auto* innerClass=i==4u?"Core.StrProperty":"Core.NameProperty";
            const auto innerType=ReadPortableRuntimeObjectOwnership(innerClass);
            Require(inner.outerPath==field.path&&inner.outerIndex==field.index&&inner.classPath==innerClass&&inner.classIndex==innerType.objectIndex&&
                inner.objectFlags==4u&&inner.flags==(i==4u?0x400000u:0u)&&inner.dimension==1u&&inner.elementSize==(i==4u?12u:4u)&&
                !inner.offset&&inner.category=="None"&&inner.nextPath.empty()&&inner.innerPath.empty()&&indices.insert(inner.index).second,
                "System array inner lost actual type/Outer/native flags/layout: "+inner.path);
            Header(inner.path,inner.index,inner.classPath,inner.classIndex,inner.outerPath,inner.outerIndex,4u);
        }
    }
    const auto cdo=ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.System");
    Require(cdo.objectPath=="@ClassDefault:Core.System"&&cdo.objectIndex&&indices.insert(cdo.objectIndex).second&&
        cdo.classIndex==value.classIndex&&cdo.classPath==value.classPath&&!cdo.outerIndex&&cdo.outerPath.empty()&&!cdo.serializedOuterZero,
        "System native Class default lacks an independent genuine allocation identity");
    Require(!FindPortableRuntimeObject({"@ClassDefault:Core.System",0u,0u,true,false}),"System Class default leaked into named lookup");
    const auto integerClass=ReadPortableRuntimeObjectOwnership("Core.IntProperty");
    const auto integerDefault=ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.IntProperty");
    Require(integerDefault.objectPath=="@ClassDefault:Core.IntProperty"&&integerDefault.objectIndex&&
        integerDefault.objectIndex!=integerClass.objectIndex&&indices.insert(integerDefault.objectIndex).second&&
        integerDefault.classIndex==integerClass.objectIndex&&integerDefault.classPath=="Core.IntProperty"&&
        !integerDefault.outerIndex&&integerDefault.outerPath.empty()&&!integerDefault.serializedOuterZero,
        "System-owned IntProperty default lacks a distinct genuine allocation/Class/null-Outer identity");
    Require(!FindPortableRuntimeObject({"@ClassDefault:Core.IntProperty",0u,0u,true,false}),"System-owned IntProperty default leaked into named lookup");
    Require(indices.size()==12u,"System Class/GSys/eight fields/two CDOs do not have twelve distinct actual UObject allocations");
    for(const auto* type:{"Core.System","Core.IntProperty"}) {
        const auto defaultName=ReadPortableClassDefault(type,"Name"),defaultClass=ReadPortableClassDefault(type,"Class");
        const auto defaultOuter=ReadPortableClassDefault(type,"Outer"),defaultFlags=ReadPortableClassDefault(type,"ObjectFlags");
        Require(defaultName.kind==Kind::Name&&defaultName.text=="None"&&defaultClass.kind==Kind::Object&&defaultClass.text==type&&
            defaultOuter.kind==Kind::Object&&defaultOuter.text.empty()&&defaultFlags.kind==Kind::Int&&!defaultFlags.integer,
            "System/property default did not preserve the actual None/Class/null/zero header");
    }
}
void Config(const PortableSystemServicesSnapshot& value,const QuestVr::NativeConfig::OrderedIni& ini) {
    const auto scalar=[&](const char* name) {
        const auto raw=ini.PropertyScalar("Core.System",name);Require(raw.has_value(),"Owned System INI lacks oracle scalar: "+std::string(name));return *raw;
    };
    // The oracle deliberately does not call the System LoadConfig helper.
    std::size_t consumed{};const auto purgeText=scalar("PurgeCacheDays");const auto purge=std::stol(purgeText,&consumed);
    Require(consumed==purgeText.size()&&value.purgeCacheDays==purge&&value.savePath==scalar("SavePath")&&
        value.cachePath==scalar("CachePath")&&value.cacheExt==scalar("CacheExt"),"System instance config differs from independent owned INI oracle");
    const auto paths=ini.StringArray("Core.System","Paths"),suppress=ini.StringArray("Core.System","Suppress");
    Require(paths.has_value()&&suppress.has_value()&&value.paths==*paths&&value.suppress==*suppress&&
        value.defaultPaths==*paths&&value.defaultSuppress==*suppress,
        "System instance/default typed Paths/Suppress changed independent owned INI duplicate-key source order or text");
    const auto check=[&](const char* name,Kind kind,const std::string& text,std::int32_t integer=0) {
        const auto current=ReadPortableRuntimeDeclaredScriptProperty(value.objectPath,value.classPath+'.'+name);
        const auto defaults=ReadPortableClassDefault(value.classPath,name);
        Require(current.kind==kind&&defaults.kind==kind&&current.text==text&&defaults.text==text&&
            current.integer==integer&&defaults.integer==integer,"System instance/default scalar read differs from copied config: "+std::string(name));
        Refused([&]{(void)ReadPortableRuntimeDeclaredScriptProperty(value.objectPath,value.classPath+'.'+name,1u);},"System scalar ignored array bounds");
    };
    check("PurgeCacheDays",Kind::Int,"",value.purgeCacheDays);check("SavePath",Kind::String,value.savePath);
    check("CachePath",Kind::String,value.cachePath);check("CacheExt",Kind::String,value.cacheExt);
    for(const auto* name:{"Paths","Suppress"}) {
        Refused([&]{(void)ReadPortableRuntimeDeclaredScriptProperty(value.objectPath,value.classPath+'.'+name);},"System dynamic non-INT array acquired a fabricated generic VM value");
        Refused([&]{(void)ReadPortableClassDefault(value.classPath,name);},"System dynamic non-INT array default acquired a fabricated generic VM value");
    }
    for(const auto* raw:{"Core.Object.ObjectInternal","Core.Subsystem.ExecVtbl"})
        Refused([&]{(void)ReadPortableRuntimeDeclaredScriptProperty(value.objectPath,raw);},"System exposed unauthenticated native pointer storage as a script value");
}
void Dormant() {
    const auto begun=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(begun.kind==Kind::Bool&&!begun.boolean&&ReadPortableRuntimeLevelTime()==0.0&&!HasPortableRuntimeClient()&&
        !GetPortableRuntimeUnsavedScriptState(),"System preparation began Training/client/player/clock or dirtied actor state");
}
struct Baseline {
    Temporary& temporary;Bytes save;PortableSystemServicesSnapshot system;PortableObjectLookupState lookup;GCStats gc;std::uint64_t revision{};
    explicit Baseline(Temporary& work):temporary(work),save(work.Save("Before.sav")),system(ReadPortableRuntimeSystemServices()),
        lookup(ReadPortableRuntimeObjectLookup()),gc(GC::GetStats()),revision(GetPortableRuntimeWorldRevision()) {}
    void Unchanged() const {
        Same(system,ReadPortableRuntimeSystemServices());GC::Collect();SameGc(gc,"System refusal/query changed GC allocations or retained native graph");
        Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
            temporary.Save("After.sav")==save,"System refusal/query changed lookup/world revision/whole actor save");Dormant();
    }
};
void Missing(const std::vector<PortablePackageTables>& packages,const std::string& ini) {
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned System dependency source graph");
    const auto refuse=[&] {
        Empty(ReadPortableRuntimeSystemServices());const auto before=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
        Refused([&]{(void)PreparePortableRuntimeSystemServices(ini);},"System preparation silently supplied an absent explicit prerequisite");
        Empty(ReadPortableRuntimeSystemServices());GC::Collect();SameGc(before,"Missing-prerequisite System refusal leaked native graph");
        Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup()),"Missing-prerequisite System refusal published lookup nodes");
    };
    refuse();Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare native Engine classes");refuse();
    Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare native metadata classes");refuse();
    Require(PreparePortableRuntimeNativeConfigClasses().prepared,"Cannot prepare native config classes");refuse();
    Require(PreparePortableRuntimeNativePackages().prepared,"Cannot prepare native Package objects");refuse();
    Require(PreparePortableRuntimeNativeInputClasses().prepared,"Cannot prepare configured native Input class closure");refuse();
    Require(PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare native object lookup");refuse();
    Require(PreparePortableRuntimeEngineCollision(ini).prepared,"Cannot prepare native Engine collision/default prerequisites");refuse();
    Require(PreparePortableRuntimeInputSchema().prepared,"Cannot prepare shared native Input/NameProperty defaults");
}
void FailedNameImport(const fs::path& root,const std::vector<PortablePackageTables>& packages,Temporary& temporary,const GCStats& initial) {
    const auto stamp=static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto unique="UniqueSystemRefusalName_X_"+std::to_string(stamp);
    Require(!NameString::FindCompareIndex(unique),"System refusal fixture name already exists in the live FName arena");
    const auto iniPath=temporary.CopyIni(root/"System"/"DeusEx.ini");
    Require(iniPath.parent_path()==temporary.directory&&fs::is_regular_file(fs::symlink_status(iniPath))&&
        !fs::is_symlink(fs::symlink_status(iniPath)),"System refusal append escaped the tracked regular temporary INI copy");
    {
        std::ofstream output(iniPath,std::ios::binary|std::ios::app);Require(bool(output),"Cannot append isolated System refusal config");
        const auto suffix="\r\n[Core.System]\r\nSuppress="+unique+"\r\nPurgeCacheDays=2147483648\r\n";
        output.write(suffix.data(),static_cast<std::streamsize>(suffix.size()));output.close();
        Require(bool(output),"Cannot finish isolated System refusal config");
    }
    const Source copied(iniPath);
    const auto oracle=QuestVr::NativeConfig::ReadOrderedIni(iniPath);Require(oracle.has_value(),"Cannot read isolated System refusal oracle");
    const auto suppress=oracle->StringArray("Core.System","Suppress");
    Require(suppress.has_value()&&!suppress->empty()&&suppress->back()==unique&&
        oracle->PropertyScalar("Core.System","PurgeCacheDays")==std::optional<std::string>{"2147483648"},
        "System refusal fixture lost the Name-import prefix or later signed Integer overflow");
    Missing(packages,iniPath.string());Empty(ReadPortableRuntimeSystemServices());
    Require(!NameString::FindCompareIndex(unique),"System prerequisites prematurely interned the isolated config name");
    Refused([&]{(void)ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.IntProperty");},
        "System refusal baseline already has an IntProperty default");
    const auto before=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto revision=GetPortableRuntimeWorldRevision();const auto nameDefault=ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.NameProperty");
    std::string refusal;
    try {(void)PreparePortableRuntimeSystemServices(iniPath.string());}
    catch(const std::exception& error) {refusal=error.what();}
    // ConfigLink imports Suppress before PurgeCacheDays. Pin the late overflow
    // refusal, not an earlier unavailable prerequisite/source/budget error.
    Require(refusal=="Native config import: _wtoi signed overflow profile is not established",
        "System staged-name regression did not reach the signed Integer refusal after its Name-import prefix");++refusals;
    Empty(ReadPortableRuntimeSystemServices());
    Require(!NameString::FindCompareIndex(unique),"Failed native System preparation leaked a live config FName identity");
    Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
        ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.NameProperty").objectIndex==nameDefault.objectIndex,
        "Failed native System preparation changed lookup/revision/shared NameProperty default authority");
    for(const auto* path:{"Core.System","@ClassDefault:Core.System","@ClassDefault:Core.IntProperty"})
        Refused([&]{(void)ReadPortableRuntimeObjectOwnership(path);},"Failed native System preparation leaked Class/default namespace authority");
    Require(!FindPortableRuntimeObject({"Core.System",0u,0u,true,false}),"Failed native System preparation published its Class in weak lookup");
    GC::Collect();SameGc(before,"Late native System config refusal leaked staged GC objects or allocation-index leases");
    copied.Unchanged();ShutdownPortableRuntime();GC::Collect();SameGc(initial,"System refusal fixture retained roots after shutdown");
    Empty(ReadPortableRuntimeSystemServices());Require(!ReadPortableRuntimeObjectLookup().prepared&&!NameString::FindCompareIndex(unique),
        "System refusal fixture teardown retained lookup or a supposedly detached FName identity");
    temporary.Remove(iniPath);
}
void Commands(Temporary& temporary) {
    const Baseline baseline(temporary);
    for(const auto* command:{"SET InputExt T Talk","NOT_A_SYSTEM_COMMAND","MEMSTATX",""}) {
        const auto result=ExecutePortableRuntimeSystemCommand(command);
        Require(result.execution.passed()&&result.execution.committed&&!result.execution.writes&&!result.handled&&result.output.empty(),
            "System stole unknown/SET command or manufactured native side effects");
    }
    for(const auto* token:{"MEMSTAT","CONFIGHASH","EXIT","QUIT","RELAUNCH","DEBUG"}) {
        const auto result=ExecutePortableRuntimeSystemCommand(Fold(token)+" optional-tail");
        Require(result.handled&&!result.execution.passed()&&!result.execution.committed&&!result.execution.writes&&result.output.empty()&&
            result.execution.status==QuestVr::Vm::Status::Unsupported&&
            result.execution.error==std::string("Original System native command service is not implemented: ")+token,
            "System recognized native command was silently ignored or executed an unauthenticated host service");++refusals;
    }
    for(const auto work:{0u,1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=work;const auto result=ExecutePortableRuntimeSystemCommand("SET InputExt T Talk",limits);
        Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.writes,
            "System command ignored shared native-work budget");++refusals;
    }
    for(const auto bytes:{0u,1u}) {
        QuestVr::Vm::Limits limits;limits.retainedBytes=bytes;const auto result=ExecutePortableRuntimeSystemCommand("SET InputExt T Talk",limits);
        Require(!result.execution.passed()&&!result.execution.committed&&!result.execution.writes,
            "System command ignored shared retained admission");++refusals;
    }
    baseline.Unchanged();
}
void Original(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    const auto iniPath=temporary.CopyIni(root/"System"/"DeusEx.ini");const Source copied(iniPath);
    const auto ini=QuestVr::NativeConfig::ReadOrderedIni(iniPath);Require(ini.has_value(),"Cannot independently read owned System config oracle");
    Missing(packages,iniPath.string());Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant first Training for System integration");Dormant();
    const auto before=temporary.Save("Before.sav");const auto revision=GetPortableRuntimeWorldRevision();const auto beforeGc=GC::GetStats();
    const auto beforeLookup=ReadPortableRuntimeObjectLookup();const auto engine=ReadPortableRuntimeNativeEngineClasses();
    const auto metadata=ReadPortableRuntimeNativeMetadataClasses();const auto config=ReadPortableRuntimeNativeConfigClasses();
    const auto input=ReadPortableRuntimeInputSchema();const auto collision=ReadPortableRuntimeEngineCollision();
    const auto existingNameDefault=ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.NameProperty");
    const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs");
    const auto refused=[&](const std::string& path) {
        Refused([&]{(void)PreparePortableRuntimeSystemServices(path);},"System accepted an invalid first config source");
        Empty(ReadPortableRuntimeSystemServices());GC::Collect();SameGc(beforeGc,"Invalid System source leaked native allocation/default graph");
        Require(SameLookup(beforeLookup,ReadPortableRuntimeObjectLookup())&&temporary.Save("After.sav")==before&&
            GetPortableRuntimeWorldRevision()==revision,"Invalid System source changed lookup/checkpoint/world revision");
    };
    refused("");refused((temporary.directory/"Absent.ini").string());
    const auto ready=PreparePortableRuntimeSystemServices(iniPath.string());Layout(ready);Config(ready,*ini);
    Require(fs::path(ready.configPath).lexically_normal()==iniPath.lexically_normal(),"System cached config source authority differs from explicit INI path");
    Require(ReadPortableRuntimeNativeEngineClasses().classes.size()==engine.classes.size()&&
        ReadPortableRuntimeNativeMetadataClasses().classes.size()==metadata.classes.size()&&
        ReadPortableRuntimeNativeConfigClasses().classes.size()==config.classes.size()&&
        ReadPortableRuntimeInputSchema().fields.size()==input.fields.size()&&ReadPortableRuntimeInputSchema().defaults.size()==input.defaults.size()&&
        ReadPortableRuntimeEngineCollision().fields.size()==collision.fields.size()&&ReadPortableRuntimeEngineCollision().defaults.size()==collision.defaults.size()&&
        ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.NameProperty").objectIndex==existingNameDefault.objectIndex&&
        QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs")),
        "System polluted fixed prerequisite closures, replaced shared NameProperty CDO or changed Engine defaults");
    const auto afterGc=GC::GetStats();const auto afterLookup=ReadPortableRuntimeObjectLookup();
    Require(afterGc.numObjects==beforeGc.numObjects+13u&&afterGc.memoryUsage>beforeGc.memoryUsage&&
        afterLookup.objects==beforeLookup.objects+10u&&afterLookup.generation==beforeLookup.generation&&afterLookup.retainedBytes>beforeLookup.retainedBytes,
        "System failed exact thirteen-GC-object ownership or ten named Class/GSys/eight-field weak lookup publication");
    for(unsigned repeat=0u;repeat<3u;++repeat) {
        Same(ready,PreparePortableRuntimeSystemServices(iniPath.string()));Same(ready,ReadPortableRuntimeSystemServices());
        Require(SameLookup(afterLookup,PreparePortableRuntimeObjectLookup()),"System reprepare duplicated/replaced weak lookup nodes");
        GC::Collect();SameGc(afterGc,"System reprepare/read/GC lost or duplicated native roots");
    }
    auto detached=ReadPortableRuntimeSystemServices();detached.prepared=false;detached.objectIndex=0u;detached.savePath="Detached";
    detached.paths.push_back("Detached");detached.suppress.clear();detached.fields.front().outerPath="Detached";
    detached.defaultPaths.push_back("DetachedDefaultPath");detached.defaultSuppress.push_back("Detached");detached.constructorLinksVerified=false;
    // Snapshot value detachment is observable. This does not prove mutation
    // independence between the live native instance and CDO: no supported
    // native System mutation service exists to exercise that stronger claim.
    Require(detached.paths!=ready.paths&&detached.defaultPaths!=ready.defaultPaths&&
        detached.defaultSuppress!=ready.defaultSuppress,"System snapshot detachment fixture did not change the observed typed arrays");
    Same(ready,ReadPortableRuntimeSystemServices());copied.Unchanged();
    Require(temporary.Save("After.sav")==before&&GetPortableRuntimeWorldRevision()==revision,"System native session state leaked into actor saves/revision");Dormant();
    Commands(temporary);
    temporary.Remove(iniPath);const Baseline cached(temporary);
    Same(ready,ReadPortableRuntimeSystemServices());Same(ready,PreparePortableRuntimeSystemServices(iniPath.string()));
    Same(ready,PreparePortableRuntimeSystemServices((temporary.directory/"."/"DeusEx.ini").string()));
    Refused([&]{(void)PreparePortableRuntimeSystemServices((temporary.directory/"OtherAbsent.ini").string());},"System rebound a committed config source");cached.Unchanged();
    const auto warmRevision=GetPortableRuntimeWorldRevision();
    Require(LoadPortableRuntimeState((temporary.directory/"Before.sav").string()),"Cannot warm-restore dormant Training with System roots");
    Require(GetPortableRuntimeWorldRevision()==warmRevision+1u,"Warm System fixture replacement did not advance revision once");
    Same(ready,ReadPortableRuntimeSystemServices());Layout(ready);Config(ready,*ini);GC::Collect();SameGc(afterGc,"Warm restore lost or duplicated session System roots");
    Require(temporary.Save("After.sav")==before,"Warm restore leaked System native graph into original Training actor checkpoint");Dormant();
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload first Training with rooted System service");
    Same(ready,ReadPortableRuntimeSystemServices());GC::Collect();SameGc(afterGc,"Training reload lost/duplicated rooted System graph");
    Require(temporary.Save("After.sav")==before,"Training reload contaminated dormant actor checkpoint with System graph");Dormant();
    ShutdownPortableRuntime();Empty(ReadPortableRuntimeSystemServices());
    Refused([&]{(void)ReadPortableRuntimeObjectOwnership(ready.objectPath);},"Shutdown retained rooted GSys authority");
    Refused([&]{(void)ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.System");},"Shutdown retained native System defaults");
    Refused([&]{(void)ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.IntProperty");},"Shutdown retained System-owned IntProperty defaults");
    for(const auto& field:ready.fields)Refused([&]{(void)ReadPortableRuntimeObjectOwnership(field.path);},"Shutdown retained a native System field identity");
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        else throw std::runtime_error("Usage: portable_system_services_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native System integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();Temporary temporary(root);
        Empty(ReadPortableRuntimeSystemServices());Refused([&]{(void)PreparePortableRuntimeSystemServices((root/"System"/"DeusEx.ini").string());},
            "System preparation accepted uninitialized runtime");
        const auto inactive=ExecutePortableRuntimeSystemCommand("SET InputExt T Talk");
        Require(!inactive.execution.passed()&&!inactive.execution.committed&&!inactive.handled&&!inactive.execution.writes,
            "System command fabricated an unprepared global native service");
        GC::Collect();SameGc(initial,"No-runtime System refusal leaked objects");
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");sources.emplace_back(root/"System"/"User.ini");
        packages.reserve(QuestVr::RuntimeScriptPackages.size());
        for(const auto* name:QuestVr::RuntimeScriptPackages) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        FailedNameImport(root,packages,temporary,initial);
        Original(root,packages,map,temporary);ShutdownPortableRuntime();GC::Collect();SameGc(initial,"System domain/default/GSys allocation leases survived shutdown");
        Empty(ReadPortableRuntimeSystemServices());Require(!ReadPortableRuntimeObjectLookup().prepared,"Shutdown retained System-bearing lookup");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned native System integration: "<<checks<<" checks / "<<refusals
            <<" refusals; actual System Class/two CDOs/GSys, six Config fields/two typed array inners, +13 GC/+10 lookup, native layout and headers, "
              "independent original INI instance/default array oracle, detached snapshots/cached config, validated native constructor links, "
              "late config-prefix FName/class/default/lookup/GC rollback, explicit dependency/source refusal, unknown SET non-routing and six "
              "recognized unsupported commands, shared budgets, save/lookup/GC/first-Training retention and complete teardown. "
              "NOT live instance/CDO mutation independence, full FExec, live engine/player startup, VR rendering or crate/loot/gravity gameplay.\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
