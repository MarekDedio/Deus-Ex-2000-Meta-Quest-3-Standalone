#include "Precomp.h"
#include "GC/GC.h"
#include "portable_input_schema.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"
#include "quest_unreal_string.h"

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
#include <string_view>
#include <vector>

// Owned source is read as data only. This proves the native Input schema and
// pre-config defaults, not User.ini import, Input instances, viewport Init,
// ResetInput, ReadInput, begun Training or live VR weapon/physics behavior.
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
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned Input schema source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Input schema source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof()&&!file.bad(),"Cannot finish owned Input schema source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Owned Input schema source bytes/size/mtime changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Temporary Input schema fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-input-schema-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated Input schema checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files)if(file.parent_path()==directory)fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    Bytes Snapshot(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid Input schema checkpoint leaf");
        const auto path=directory/(leaf+".sav");Require(path.parent_path()==directory,"Invalid isolated checkpoint target");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot save Input schema checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read Input schema checkpoint");return bytes;
    }
};
void SameGc(const GCStats& expected,const std::string& why) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,why);
}
bool SameLookup(const PortableObjectLookupState& a,const PortableObjectLookupState& b) {
    return a.prepared==b.prepared&&a.objects==b.objects&&a.retainedBytes==b.retainedBytes&&a.generation==b.generation;
}
void Empty(const PortableInputSchemaSnapshot& value) {
    Require(!value.prepared&&!value.defaultsPrepared&&!value.configApplied&&value.fields.empty()&&value.defaults.empty()&&value.links.empty()&&
        value.aliasStruct.objectPath.empty()&&!value.aliasStruct.objectIndex&&value.keyEnumPath.empty()&&!value.keyEnumIndex&&!value.retainedBytes,
        "Unprepared Input schema exposed a partial native graph/default/config state");
}
void SameField(const PortableInputSchemaFieldSnapshot& a,const PortableInputSchemaFieldSnapshot& b) {
    const auto& x=a.field;const auto& y=b.field;
    Require(x.objectPath==y.objectPath&&x.classPath==y.classPath&&x.outerPath==y.outerPath&&x.nextPath==y.nextPath&&
        x.innerPath==y.innerPath&&x.propertyNextPath==y.propertyNextPath&&x.configNextPath==y.configNextPath&&
        x.constructorNextPath==y.constructorNextPath&&x.category==y.category&&x.objectIndex==y.objectIndex&&
        x.classIndex==y.classIndex&&x.outerIndex==y.outerIndex&&x.objectFlags==y.objectFlags&&x.arrayDimension==y.arrayDimension&&
        x.elementSize==y.elementSize&&x.propertyFlags==y.propertyFlags&&x.dataOffset==y.dataOffset&&
        a.referencedStructPath==b.referencedStructPath&&a.referencedStructIndex==b.referencedStructIndex,
        "Input field identity/header/layout/links changed: "+x.objectPath);
}
void SameDefault(const PortableInputDefaultSnapshot& a,const PortableInputDefaultSnapshot& b) {
    Require(a.classPath==b.classPath&&a.superDefaultPath==b.superDefaultPath&&a.friendlyName==b.friendlyName&&a.classIndex==b.classIndex&&
        a.defaultIndex==b.defaultIndex&&a.superDefaultIndex==b.superDefaultIndex&&a.nativeSize==b.nativeSize&&a.classFlags==b.classFlags&&
        a.propertyDimension==b.propertyDimension&&a.propertyElementSize==b.propertyElementSize&&a.propertyFlags==b.propertyFlags&&
        a.propertyOffset==b.propertyOffset&&a.structPropertiesSize==b.structPropertiesSize&&a.hasInputValues==b.hasInputValues&&
        a.execInstalled==b.execInstalled&&a.viewportIndex==b.viewportIndex&&a.enumIndex==b.enumIndex&&a.aliases==b.aliases&&a.bindings==b.bindings,
        "Input native default identity/payload changed: "+a.classPath);
}
void SameSnapshot(const PortableInputSchemaSnapshot& a,const PortableInputSchemaSnapshot& b) {
    Require(a.prepared==b.prepared&&a.defaultsPrepared==b.defaultsPrepared&&a.configApplied==b.configApplied&&
        a.keyEnumPath==b.keyEnumPath&&a.keyEnumIndex==b.keyEnumIndex&&a.retainedBytes==b.retainedBytes&&
        a.strPropertyUniqueCounter==b.strPropertyUniqueCounter&&a.fields.size()==b.fields.size()&&a.defaults.size()==b.defaults.size()&&
        a.links.size()==b.links.size(),"Input schema state, retained accounting, counter or graph counts changed");
    const auto& x=a.aliasStruct;const auto& y=b.aliasStruct;
    Require(x.objectPath==y.objectPath&&x.classPath==y.classPath&&x.outerPath==y.outerPath&&x.friendlyName==y.friendlyName&&
        x.superPath==y.superPath&&x.objectIndex==y.objectIndex&&x.classIndex==y.classIndex&&x.outerIndex==y.outerIndex&&
        x.objectFlags==y.objectFlags&&x.propertiesSize==y.propertiesSize,"Native Alias UStruct header changed");
    for(std::size_t i=0u;i<a.fields.size();++i)SameField(a.fields[i],b.fields[i]);
    for(std::size_t i=0u;i<a.defaults.size();++i)SameDefault(a.defaults[i],b.defaults[i]);
    for(std::size_t i=0u;i<a.links.size();++i) {
        const auto& p=a.links[i];const auto& q=b.links[i];
        Require(p.ownerPath==q.ownerPath&&p.childrenPath==q.childrenPath&&p.ownerIndex==q.ownerIndex&&p.heads==q.heads&&
            p.constructorLinksPrepared==q.constructorLinksPrepared,"Input native Struct/Class linked heads changed");
    }
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> result;for(const auto& actor:GetPortableRuntimeMapActors(true))result.push_back(actor.objectPath+'|'+actor.classPath);return result;
}
void Dormant() {
    const auto begun=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(begun.kind==Kind::Bool&&!begun.boolean,"Input schema preparation began the original first Training world");
}
const PortableInputSchemaLinksSnapshot& Links(const PortableInputSchemaSnapshot& value,std::string_view path) {
    const auto found=std::find_if(value.links.begin(),value.links.end(),[&](const auto& row){return row.ownerPath==path;});
    Require(found!=value.links.end(),"Missing actual Input schema linked-list owner: "+std::string(path));return *found;
}
const PortableInputDefaultSnapshot& Default(const PortableInputSchemaSnapshot& value,std::string_view path) {
    const auto found=std::find_if(value.defaults.begin(),value.defaults.end(),[&](const auto& row){return row.classPath==path;});
    Require(found!=value.defaults.end(),"Missing actual Input native default: "+std::string(path));return *found;
}
void SameClass(const PortableNativeMetadataClassSnapshot& a,const PortableNativeMetadataClassSnapshot& b) {
    Require(a.classPath==b.classPath&&a.baseClassPath==b.baseClassPath&&a.withinClassPath==b.withinClassPath&&a.metaClassPath==b.metaClassPath&&
        a.nativeSize==b.nativeSize&&a.registrationClassFlags==b.registrationClassFlags&&a.registrationObjectFlags==b.registrationObjectFlags&&
        a.objectIndex==b.objectIndex&&a.baseClassIndex==b.baseClassIndex&&a.withinClassIndex==b.withinClassIndex&&a.metaClassIndex==b.metaClassIndex,
        "Input schema replaced a prerequisite native Class header/identity");
}
void SamePackages(const PortableNativePackagesSnapshot& a,const PortableNativePackagesSnapshot& b) {
    Require(a.prepared==b.prepared&&a.defaultObjectIndex==b.defaultObjectIndex&&a.defaultClassIndex==b.defaultClassIndex&&a.packages.size()==b.packages.size(),
        "Input schema changed prerequisite native Package preparation/default identities");
    for(std::size_t i=0u;i<a.packages.size();++i) {
        const auto& x=a.packages[i];const auto& y=b.packages[i];
        Require(x.objectPath==y.objectPath&&x.objectIndex==y.objectIndex&&x.classIndex==y.classIndex&&x.outerIndex==y.outerIndex&&
            x.objectFlags==y.objectFlags&&x.packageFlags==y.packageFlags&&x.legacyHandleAbsent==y.legacyHandleAbsent&&x.bindingAttempted==y.bindingAttempted,
            "Input schema changed an existing genuine Package header/identity/binding state");
    }
}
void LoadedPackages(const PortableNativePackagesSnapshot& scripts,const PortableNativePackagesSnapshot& loaded) {
    auto staticOnly=loaded;
    const auto map=std::find_if(staticOnly.packages.begin(),staticOnly.packages.end(),[](const auto& row){return row.objectPath=="00_Training";});
    Require(map!=staticOnly.packages.end()&&map->objectIndex&&map->classIndex==scripts.defaultClassIndex&&!map->outerIndex&&
        map->objectFlags==4u&&!map->packageFlags&&map->legacyHandleAbsent&&map->bindingAttempted,
        "Training load did not provide its genuine native Package identity/constructor outcome");
    const auto levelInfo=ReadPortableRuntimeObjectOwnership("00_Training.LevelInfo0");
    Require(levelInfo.outerPath=="00_Training"&&levelInfo.outerIndex==map->objectIndex,
        "Authored Training actor does not belong to the actual current map Package");
    staticOnly.packages.erase(map);SamePackages(scripts,staticOnly);
}
void SamePrerequisites(const PortableNativeEngineClassesSnapshot& engine,const PortableNativeMetadataClassesSnapshot& metadata,
    const PortableNativeConfigClassesSnapshot& config,const PortableNativeInputClassesSnapshot& input,const PortableEngineCollisionSnapshot& collision) {
    const auto a=ReadPortableRuntimeNativeEngineClasses();const auto b=ReadPortableRuntimeNativeMetadataClasses();
    const auto c=ReadPortableRuntimeNativeConfigClasses();const auto d=ReadPortableRuntimeNativeInputClasses();
    Require(a.prepared==engine.prepared&&a.configuredGameEngineClass==engine.configuredGameEngineClass&&a.classes.size()==engine.classes.size()&&
        b.prepared==metadata.prepared&&b.boundSerializedClasses==metadata.boundSerializedClasses&&b.classes.size()==metadata.classes.size()&&
        c.prepared==config.prepared&&c.classes.size()==config.classes.size()&&c.consoleClassPath==config.consoleClassPath&&
        c.consoleObjectIndex==config.consoleObjectIndex&&c.consoleMetaClassIndex==config.consoleMetaClassIndex&&
        c.consoleWithinClassIndex==config.consoleWithinClassIndex&&c.consoleClassFlags==config.consoleClassFlags&&c.consoleObjectFlags==config.consoleObjectFlags&&
        d.prepared==input.prepared&&d.classes.size()==input.classes.size()&&d.configuredInputClass==input.configuredInputClass&&d.configName==input.configName,
        "Input schema changed explicit Engine/metadata/config/Input class preparation or selection");
    for(std::size_t i=0u;i<a.classes.size();++i) {
        const auto& x=a.classes[i];const auto& y=engine.classes[i];
        Require(x.classPath==y.classPath&&x.baseClassPath==y.baseClassPath&&x.withinClassPath==y.withinClassPath&&x.nativeSize==y.nativeSize&&
            x.registrationClassFlags==y.registrationClassFlags&&x.registrationObjectFlags==y.registrationObjectFlags,
            "Input schema changed a prerequisite Engine native registration header");
    }
    for(std::size_t i=0u;i<b.classes.size();++i)SameClass(b.classes[i],metadata.classes[i]);
    for(std::size_t i=0u;i<c.classes.size();++i)SameClass(c.classes[i],config.classes[i]);
    for(std::size_t i=0u;i<d.classes.size();++i)SameClass(d.classes[i],input.classes[i]);
    const auto current=ReadPortableRuntimeEngineCollision();
    Require(current.prepared==collision.prepared&&current.defaults.size()==collision.defaults.size()&&current.fields.size()==collision.fields.size()&&
        current.engineIndex==collision.engineIndex&&current.engineClassIndex==collision.engineClassIndex&&current.transientIndex==collision.transientIndex&&
        current.cylinderIndex==collision.cylinderIndex&&current.cylinderClassIndex==collision.cylinderClassIndex,
        "Input schema replaced or expanded the separate collision/default domain");
    for(std::size_t i=0u;i<current.fields.size();++i) {
        PortableInputSchemaFieldSnapshot x,y;x.field=current.fields[i];y.field=collision.fields[i];SameField(x,y);
    }
    for(std::size_t i=0u;i<current.defaults.size();++i) {
        const auto& x=current.defaults[i];const auto& y=collision.defaults[i];
        Require(x.classPath==y.classPath&&x.classIndex==y.classIndex&&x.defaultIndex==y.defaultIndex&&x.superClassIndex==y.superClassIndex&&
            x.nativeSize==y.nativeSize&&x.classFlags==y.classFlags&&x.configuredClasses==y.configuredClasses&&x.cacheSizeMegs==y.cacheSizeMegs&&
            x.useSound==y.useSound&&x.urlValid==y.urlValid&&x.currentTickRate==y.currentTickRate&&x.serverActors==y.serverActors&&x.serverPackages==y.serverPackages,
            "Input schema changed an existing native collision/default identity or payload");
    }
}
std::array<std::string,3u> InheritedHeads(const std::vector<PortablePackageTables>& packages) {
    std::array<std::string,3u> heads;
    // Independent source traversal: each real Class.Children list, closest
    // superclass first. Native Alias is not inserted into Input.Children.
    for(const auto* name:{"Core.Subsystem","Core.Object"}) {
        const std::string path=name;const auto dot=path.find('.');
        const auto table=std::find_if(packages.begin(),packages.end(),[&](const auto& p){return fs::path(p.sourcePath).stem().string()==path.substr(0u,dot);});
        Require(table!=packages.end(),"Missing actual inherited Input source table");
        const auto index=FindPortableExport(*table,path.substr(dot+1u));const auto cls=LoadPortableClassDescriptor(*table,index);
        auto child=cls.state.children;std::set<std::int32_t> seen;
        while(child) {
            Require(child>0&&static_cast<std::size_t>(child)<=table->exports.size()&&seen.size()<65536u&&seen.insert(child).second,
                "Original inherited Input field chain is invalid/cyclic/over-budget");
            const auto slot=static_cast<std::size_t>(child-1);const auto& entry=table->exports[slot];
            Require(entry.ObjOuter==static_cast<std::int32_t>(index+1u),"Original Input inherited field lost its declaring Outer");
            const auto type=GetPortableObjectPath(*table,entry.ObjClass);
            if(type.size()>=8u&&type.substr(type.size()-8u)=="Property") {
                const auto property=LoadPortablePropertyDescriptor(*table,slot);
                const auto fieldPath=fs::path(table->sourcePath).stem().string()+'.'+GetPortableObjectPath(*table,child);
                if(heads[0u].empty())heads[0u]=fieldPath;
                if(heads[1u].empty()&&(property.flags&0x4000u))heads[1u]=fieldPath;
                if(heads[2u].empty()&&(property.flags&0x400000u))heads[2u]=fieldPath;
                child=property.nextField;
            }else if(type=="Core.Function")child=LoadPortableFunctionScript(*table,slot).nextField;
            else if(type=="Core.State")child=LoadPortableStateDescriptor(*table,slot).nextField;
            else child=LoadPortableFieldLinks(*table,slot).nextField;
        }
    }
    return heads;
}
void LookupAndHeader(const std::string& path,std::uint32_t index,const std::string& cls,std::uint32_t clsIndex,
    const std::string& outer,std::uint32_t outerIndex,std::uint32_t flags) {
    const auto owner=ReadPortableRuntimeObjectOwnership(path);
    Require(index&&owner.objectPath==path&&owner.objectIndex==index&&owner.classPath==cls&&owner.classIndex==clsIndex&&
        owner.outerPath==outer&&owner.outerIndex==outerIndex&&!owner.serializedOuterZero,"Native Input object lacks actual Class/Outer/index authority: "+path);
    const auto generation=ReadPortableRuntimeObjectLookup().generation;
    for(const auto& spelling:{path,Fold(path)}) {
        const auto found=FindPortableRuntimeObject({spelling,clsIndex,0u,true,true});
        Require(found&&found->objectPath==path&&found->objectIndex==index&&found->classIndex==clsIndex&&found->outerIndex==outerIndex&&
            found->generation==generation,"Native Input object missing from canonical/folded class-filtered lookup: "+path);
    }
    const auto leaf=path.substr(path.rfind('.')+1u);const auto direct=FindPortableRuntimeObject({Fold(leaf),clsIndex,outerIndex,false,true});
    Require(direct&&direct->objectIndex==index&&direct->outerIndex==outerIndex,"Native Input direct-Outer lookup changed identity: "+path);
    const auto read=[&](const char* property,Kind kind) {
        const auto result=ReadPortableRuntimeDeclaredScriptProperty(path,std::string("Core.Object.")+property);
        Require(result.kind==kind,"Native Input header property has wrong kind: "+path+' '+property);return result;
    };
    Require(read("Name",Kind::Name).text==leaf&&read("Class",Kind::Object).text==cls&&
        read("Outer",Kind::Object).text==outer&&static_cast<std::uint32_t>(read("ObjectFlags",Kind::Int).integer)==flags,
        "Native Input generic header read disagrees with actual storage: "+path);
    Refused([&]{(void)ReadPortableRuntimeDeclaredScriptProperty(path,"Engine.Actor.Location");},"Input metadata accepted an unrelated Actor declaring property");
    Refused([&]{(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},"Native Input schema manufactured serialized authored state metadata");
}
void Schema(const PortableInputSchemaSnapshot& value,const std::vector<PortablePackageTables>& packages) {
    Require(value.prepared&&value.defaultsPrepared&&!value.configApplied&&value.fields.size()==258u&&value.defaults.size()==5u&&
        value.links.size()==3u&&value.retainedBytes>0u,"Input schema is not the complete pre-config 259-object/five-default phase");
    const auto actor=ReadPortableRuntimeEnum("Engine.Actor.EInputKey"),console=ReadPortableRuntimeEnum("Engine.Console.EInputKey");
    Require(actor.names.size()==255u&&actor.objectIndex&&actor.objectIndex!=console.objectIndex&&actor.names!=console.names&&
        value.keyEnumPath==actor.objectPath&&value.keyEnumIndex==actor.objectIndex&&actor.outerPath=="Engine.Actor"&&actor.classPath=="Core.Enum",
        "Input schema did not retain the actual Actor Enum identity independently of default.Enum/Console");
    const auto enumExport=FindPortableExport(packages[1u],"Actor.EInputKey");
    const auto source=LoadPortableEnumDescriptor(packages[1u],enumExport);
    const auto actorClass=ReadPortableRuntimeObjectOwnership("Engine.Actor"),enumClass=ReadPortableRuntimeObjectOwnership("Core.Enum");
    Require(enumExport+1u==2290u&&source.names==actor.names&&source.names.size()==255u&&source.names.front()=="IK_None"&&source.names.back()=="IK_OEMClear"&&
        actor.outerIndex==actorClass.objectIndex&&actor.classIndex==enumClass.objectIndex,
        "Runtime Input key authority differs from all actual255 owned source names");
    std::set<int> nameIndices;for(const auto& name:actor.names)
        Require(!name.IsNone()&&nameIndices.insert(name.GetCompareIndex()).second,"Actor key source contains NAME_None or duplicate FName identities");
    Require(actor.names[239u].ToString()=="UK_Unknown10F","Original key239 spelling changed; do not normalize its UK_ prefix to IK_");
    const auto input=ReadPortableRuntimeObjectOwnership("Engine.Input"),extension=ReadPortableRuntimeObjectOwnership("Extension.InputExt");
    const auto structure=ReadPortableRuntimeObjectOwnership("Core.Struct");const auto& alias=value.aliasStruct;
    Require(alias.objectPath=="Engine.Input.Alias"&&alias.classPath=="Core.Struct"&&alias.outerPath=="Engine.Input"&&
        alias.classIndex==structure.objectIndex&&alias.outerIndex==input.objectIndex&&alias.friendlyName=="Alias"&&alias.superPath.empty()&&
        alias.objectFlags==0u&&alias.propertiesSize==16u,"Alias is not the actual native UStruct with null Super and logical size16");
    std::set<std::uint32_t> objects{alias.objectIndex};Require(alias.objectIndex!=0u,"Alias UStruct has a null allocation index");
    LookupAndHeader(alias.objectPath,alias.objectIndex,alias.classPath,alias.classIndex,alias.outerPath,alias.outerIndex,0u);
    const auto inherited=InheritedHeads(packages);
    for(std::size_t i=0u;i<value.fields.size();++i) {
        const auto& row=value.fields[i];const auto& field=row.field;
        const bool aliasName=i==0u,aliasCommand=i==1u,array=i==2u,raw=i>=3u;
        std::string expectedPath,expectedClass,expectedOuter,category;std::uint32_t dimension=1u,size=12u,offset=4u,flags=0x404000u;
        if(aliasName) {expectedPath="Engine.Input.Alias.Alias";expectedClass="Core.NameProperty";expectedOuter=alias.objectPath;category="None";size=4u;offset=0u;flags=0x4000u;}
        if(aliasCommand) {expectedPath="Engine.Input.Alias.Command";expectedClass="Core.StrProperty";expectedOuter=alias.objectPath;category="None";}
        if(array) {expectedPath="Engine.Input.Aliases";expectedClass="Core.StructProperty";expectedOuter=input.objectPath;category="Aliases";dimension=40u;size=16u;offset=0x2cu;}
        if(raw) {
            const auto key=i-3u;const auto spelling=actor.names[key].ToString();
            const auto units=QuestVr::UnrealString::Decode(spelling);
            Require(units.size()>=3u,"Actual Actor key is shorter than original native suffix offset");
            // Original StaticInitInput strips three UTF16 units without an
            // IK_ prefix test. Key239 really is UK_Unknown10F in owned Engine.u.
            const auto suffix=QuestVr::UnrealString::Encode(std::u16string_view(units).substr(3u));
            if(key==0u) {
                Require(suffix=="None"&&value.strPropertyUniqueCounter>0,"NAME_None raw key was skipped or unique-name counter was not retained");
                expectedPath="Engine.Input.StrProperty"+std::to_string(value.strPropertyUniqueCounter-1);
            }else expectedPath="Engine.Input."+suffix;
            expectedClass="Core.StrProperty";expectedOuter=input.objectPath;category="RawKeys";offset=0x2acu+12u*static_cast<std::uint32_t>(key);
        }
        const auto cls=ReadPortableRuntimeObjectOwnership(expectedClass);
        Require(field.objectPath==expectedPath&&field.classPath==expectedClass&&field.outerPath==expectedOuter&&field.classIndex==cls.objectIndex&&
            field.outerIndex==(i<2u?alias.objectIndex:input.objectIndex)&&field.objectFlags==4u&&field.arrayDimension==dimension&&field.elementSize==size&&
            field.dataOffset==offset&&field.propertyFlags==flags&&field.category==category&&field.innerPath.empty()&&
            field.objectIndex&&objects.insert(field.objectIndex).second,"Input native property creation order/header/declared Link0 layout differs: "+expectedPath);
        Require(row.referencedStructPath==(array?alias.objectPath:std::string{})&&row.referencedStructIndex==(array?alias.objectIndex:0u),
            "Input StructProperty used Array.Inner/fabricated descriptor instead of actual Alias Struct");
        const auto categoryIdentity=NameString::FindCompareIndex(field.category);
        Require(categoryIdentity&&((*categoryIdentity==0)==(category=="None")),
            "Native Cpp Property category did not create/preserve its genuine FName identity");
        std::string next,propertyNext,configNext,constructorNext;
        if(aliasCommand) {next=value.fields[0u].field.objectPath;propertyNext=next;configNext=next;}
        if(array) {propertyNext=inherited[0u];configNext=inherited[1u];constructorNext=inherited[2u];}
        if(raw) {next=value.fields[i-1u].field.objectPath;propertyNext=next;configNext=next;constructorNext=next;}
        Require(field.nextPath==next&&field.propertyNextPath==propertyNext&&field.configNextPath==configNext&&field.constructorNextPath==constructorNext,
            "Input Children/Property/Config/Constructor link order differs: "+expectedPath);
        LookupAndHeader(field.objectPath,field.objectIndex,field.classPath,field.classIndex,field.outerPath,field.outerIndex,4u);
    }
    Require(objects.size()==259u,"Input native Struct/properties share an allocation identity");
    const auto objectRef=static_cast<std::int32_t>(FindPortableExport(packages[0u],"Object")+1u);
    // Native-only Field/Struct are not serialized UClass exports. Reuse the
    // original Vector UStruct's actual signed class import in the Core table.
    const auto vectorExport=FindPortableExport(packages[0u],"Object.Vector");
    const auto structRef=packages[0u].exports.at(vectorExport).ObjClass;
    Require(structRef<0&&GetPortableObjectPath(packages[0u],structRef)=="Core.Struct",
        "Original Core.Object.Vector does not carry an actual native Struct class import");
    const auto cast=[&](const std::string& path,std::int32_t reference,const std::string& expected) {
        const auto inputValue=Value::Text(Kind::Object,Fold(path));
        const auto result=CastPortableRuntimeObject("Core.Object",reference,inputValue,false);
        Require(result.kind==Kind::Object&&result.text==expected&&inputValue.text==Fold(path),"Native Input cast lost actual Class/super identity");
    };
    cast(alias.objectPath,objectRef,alias.objectPath);cast(alias.objectPath,structRef,alias.objectPath);
    for(const auto& row:value.fields)cast(row.field.objectPath,objectRef,row.field.objectPath);
    Refused([&]{(void)ReadPortableRuntimeDispatchClass(alias.objectPath);},"Alias UStruct manufactured authored dispatch metadata");
    for(const auto* path:{"Engine.Input","Extension.InputExt"}) {
        Refused([&]{(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},"Input schema manufactured a serialized UClass descriptor");
        Refused([&]{(void)ReadPortableRuntimeDispatchClass(path);},"Input schema manufactured authored native-only Class dispatch");
    }
    const auto& a=Links(value,alias.objectPath);const auto& p=Links(value,input.objectPath);const auto& x=Links(value,extension.objectPath);
    const auto& command=value.fields[1u].field.objectPath;const auto& last=value.fields.back().field.objectPath;
    Require(a.ownerIndex==alias.objectIndex&&a.childrenPath==command&&a.heads==std::array<std::string,3u>{command,command,command}&&a.constructorLinksPrepared,
        "Alias linked heads do not start at Command with constructor-only Command");
    Require(p.ownerIndex==input.objectIndex&&p.childrenPath==last&&p.heads==std::array<std::string,3u>{last,last,last}&&p.constructorLinksPrepared&&
        x.ownerIndex==extension.objectIndex&&x.childrenPath.empty()&&x.heads==p.heads&&x.constructorLinksPrepared,
        "Input/InputExt lists do not retain exact original reverse creation order and inherited shared identities");
}
void Defaults(const PortableInputSchemaSnapshot& value,const PortableEngineCollisionSnapshot& collision) {
    constexpr std::array<const char*,5u> paths{"Core.Struct","Core.NameProperty","Core.StructProperty","Engine.Input","Extension.InputExt"};
    constexpr std::array<const char*,5u> supers{"Core.Field","Core.Property","Core.Property","Core.Subsystem","Engine.Input"};
    constexpr std::array<std::uint32_t,5u> sizes{0x6cu,0x5cu,0x64u,0xfb0u,0xfb0u};
    std::set<std::uint32_t> classes,defaults;
    Require(collision.defaults.size()==11u&&collision.fields.size()==4u,"Input expanded collision's closed eleven-default/four-field contract");
    for(std::size_t i=0u;i<paths.size();++i) {
        const auto& row=Default(value,paths[i]);const auto cls=ReadPortableRuntimeObjectOwnership(paths[i]);
        const auto cdo=ReadPortableRuntimeObjectOwnership(std::string("@ClassDefault:")+paths[i]);
        const auto super=ReadPortableRuntimeObjectOwnership(std::string("@ClassDefault:")+supers[i]);
        Require(row.classIndex==cls.objectIndex&&row.classIndex&&classes.insert(row.classIndex).second&&row.defaultIndex==cdo.objectIndex&&
            row.defaultIndex&&defaults.insert(row.defaultIndex).second&&row.defaultIndex!=row.classIndex&&cdo.classIndex==row.classIndex&&!cdo.outerIndex&&
            row.superDefaultPath==std::string("@ClassDefault:")+supers[i]&&row.superDefaultIndex==super.objectIndex&&row.nativeSize==sizes[i]&&
            row.classFlags==(i<3u?0x12u:0x1eu),"Native Input default lacks its genuine Class, direct Super-default and allocation identity");
        Require(row.propertyDimension==0u&&row.propertyElementSize==0u&&row.propertyFlags==0u&&row.propertyOffset==0u&&
            row.structPropertiesSize==0u&&!row.viewportIndex&&!row.enumIndex&&!row.execInstalled&&row.hasInputValues==(i>=3u),
            "Pre-config native default ran instance constructors or invented Enum/Viewport/FExec/property storage");
        if(i==0u)Require(row.friendlyName=="None","Core.Struct pre-config default FriendlyName was not zero-extended NAME_None");
        Require(!FindPortableRuntimeObject({std::string("@ClassDefault:")+paths[i],0u,0u,true,false}),"Input default leaked into named object lookup");
        const auto name=ReadPortableClassDefault(paths[i],"Name"),type=ReadPortableClassDefault(paths[i],"Class");
        const auto outer=ReadPortableClassDefault(paths[i],"Outer"),flags=ReadPortableClassDefault(paths[i],"ObjectFlags");
        Require(name.kind==Kind::Name&&name.text=="None"&&type.kind==Kind::Object&&type.text==paths[i]&&
            outer.kind==Kind::Object&&outer.text.empty()&&flags.kind==Kind::Int&&flags.integer==0,"Native default header is not original None/Class/null/zero");
        if(i<3u)continue;
        for(std::size_t key=0u;key<row.bindings.size();++key) {
            Require(row.bindings[key].empty(),"Pre-config Input binding imported User.ini prematurely");
            const auto& field=value.fields[key+3u].field;const auto leaf=field.objectPath.substr(field.objectPath.rfind('.')+1u);
            const auto binding=ReadPortableClassDefault(paths[i],leaf);
            Require(binding.kind==Kind::String&&binding.text.empty(),"Public native Input default raw-key read is absent or nonempty");
        }
        for(std::uint32_t j=0u;j<40u;++j) {
            Require(row.aliases[j].aliasCompareIndex==0&&row.aliases[j].alias=="None"&&row.aliases[j].command.empty(),
                "Pre-config Alias default is not the actual None-name/empty-string value");
            auto alias=ReadPortableClassDefault(paths[i],"Aliases",j);
            const auto nameValue=alias.fields.find("alias"),commandValue=alias.fields.find("command");
            Require(alias.kind==Kind::Struct&&alias.fields.size()==2u&&nameValue!=alias.fields.end()&&commandValue!=alias.fields.end()&&
                nameValue->second.kind==Kind::Name&&nameValue->second.text=="None"&&commandValue->second.kind==Kind::String&&commandValue->second.text.empty(),
                "Public native Aliases fixed-array read did not expose the genuine typed Struct members");
            alias.fields["alias"]=Value::Text(Kind::Name,"DetachedMutation");alias.fields["command"]=Value::Text(Kind::String,"DetachedCommand");
            Require(!QuestVr::Vm::Equal(alias,ReadPortableClassDefault(paths[i],"Aliases",j)),"Native Alias read shared mutable default storage");
        }
        Refused([&]{(void)ReadPortableClassDefault(paths[i],"Aliases",40u);},"Native Input fixed Aliases bounds were not enforced");
        const auto firstKey=value.fields[3u].field.objectPath;const auto firstName=firstKey.substr(firstKey.rfind('.')+1u);
        Refused([&]{(void)ReadPortableClassDefault(paths[i],firstName,1u);},"Native Input scalar raw-key bounds were not enforced");
        Refused([&]{(void)ReadPortableClassDefault(paths[i],"None");},"NAME_None raw-key declaration bypassed genuine unique-name allocation");
        Refused([&]{(void)ReadPortableClassDefault(paths[i],"Enum");},"Input native Enum pointer acquired an invented reflected property");
        Refused([&]{(void)ReadPortableClassDefault(paths[i],"Viewport");},"Input native Viewport pointer acquired an invented reflected property");
    }
    const auto& input=Default(value,"Engine.Input");const auto& extension=Default(value,"Extension.InputExt");
    Require(input.aliases==extension.aliases&&input.bindings==extension.bindings&&input.defaultIndex!=extension.defaultIndex&&
        extension.superDefaultIndex==input.defaultIndex,"InputExt did not copy genuine parent defaults independently");
}
void MissingPhases(const std::vector<PortablePackageTables>& packages,const std::string& ini,Temporary& temporary) {
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize original Input schema source packages");
    for(unsigned phase=0u;phase<7u;++phase) {
        if(phase==1u)Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare five Engine classes");
        if(phase==2u)Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare thirteen metadata classes");
        if(phase==3u)Require(PreparePortableRuntimeNativeConfigClasses().prepared,"Cannot prepare four config constraints");
        if(phase==4u)Require(PreparePortableRuntimeNativePackages().prepared,"Cannot prepare actual Packages");
        if(phase==5u)Require(PreparePortableRuntimeNativeInputClasses().prepared,"Cannot prepare two actual Input classes");
        if(phase==6u)Require(PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare actual object lookup");
        const auto before=temporary.Snapshot("MissingBefore"+std::to_string(phase));const auto gc=GC::GetStats();
        const auto lookup=ReadPortableRuntimeObjectLookup();const auto revision=GetPortableRuntimeWorldRevision();
        const auto engine=ReadPortableRuntimeNativeEngineClasses();const auto metadata=ReadPortableRuntimeNativeMetadataClasses();
        const auto config=ReadPortableRuntimeNativeConfigClasses();const auto input=ReadPortableRuntimeNativeInputClasses();
        const auto collision=ReadPortableRuntimeEngineCollision();const auto packageState=ReadPortableRuntimeNativePackages();
        const auto schema=ReadPortableRuntimeInputSchema();Empty(schema);
        Refused([&]{(void)PreparePortableRuntimeInputSchema();},"Input schema silently prepared a missing explicit native prerequisite");
        SameSnapshot(schema,ReadPortableRuntimeInputSchema());
        Require(!ReadPortableRuntimeEngineCollision().prepared&&SameLookup(lookup,ReadPortableRuntimeObjectLookup()),
            "Missing-phase Input refusal changed collision/lookup publication");
        SamePrerequisites(engine,metadata,config,input,collision);SamePackages(packageState,ReadPortableRuntimeNativePackages());
        Refused([&]{(void)ReadPortableRuntimeObjectOwnership("Engine.Input.Alias");},"Missing-phase refusal published Alias UStruct");
        if(phase>=5u)Refused([&]{(void)ReadPortableClassDefault("Engine.Input","Name");},"Missing-phase refusal manufactured native Input default storage");
        GC::Collect();SameGc(gc,"Missing-phase Input refusal leaked native schema/default/domain/index leases");
        Require(temporary.Snapshot("MissingAfter"+std::to_string(phase))==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
            "Missing-phase Input refusal changed checkpoint/revision/world");
    }
    Require(PreparePortableRuntimeEngineCollision(ini).prepared,"Cannot prepare explicit genuine collision/default prerequisite");
}
void Original(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    MissingPhases(packages,(root/"System"/"DeusEx.ini").string(),temporary);
    const auto engine=ReadPortableRuntimeNativeEngineClasses();const auto metadata=ReadPortableRuntimeNativeMetadataClasses();
    const auto config=ReadPortableRuntimeNativeConfigClasses();const auto inputClasses=ReadPortableRuntimeNativeInputClasses();
    const auto collision=ReadPortableRuntimeEngineCollision();const auto preMapPackages=ReadPortableRuntimeNativePackages();
    Require(engine.prepared&&engine.classes.size()==5u&&metadata.prepared&&metadata.classes.size()==13u&&
        config.prepared&&config.classes.size()==4u&&inputClasses.prepared&&inputClasses.classes.size()==2u,
        "Wrong explicit native class dependency closure for Input schema");
    const auto scriptGc=GC::GetStats();Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant first Training map");Dormant();
    const auto packageState=ReadPortableRuntimeNativePackages();LoadedPackages(preMapPackages,packageState);
    const auto checkpoint=temporary.Snapshot("OwnedBefore");const auto world=WorldPaths();const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"),sound=ReadPortableClassDefault("Engine.Engine","UseSound");
    const auto prepared=PreparePortableRuntimeInputSchema();Schema(prepared,packages);Defaults(prepared,ReadPortableRuntimeEngineCollision());
    SamePrerequisites(engine,metadata,config,inputClasses,collision);SamePackages(packageState,ReadPortableRuntimeNativePackages());
    const auto preparedGc=GC::GetStats();const auto preparedLookup=ReadPortableRuntimeObjectLookup();
    Require(preparedGc.numObjects==gc.numObjects+265u&&preparedGc.memoryUsage>gc.memoryUsage,
        "Input phase did not allocate exactly259 schema objects, five native defaults and one rooted domain");
    Require(preparedLookup.prepared&&preparedLookup.generation==lookup.generation&&preparedLookup.objects==lookup.objects+259u&&
        preparedLookup.retainedBytes>lookup.retainedBytes,"Input schema did not publish exactly259 genuine weak lookup candidates");
    for(unsigned repeat=0u;repeat<3u;++repeat) {
        SameSnapshot(prepared,PreparePortableRuntimeInputSchema());SameSnapshot(prepared,ReadPortableRuntimeInputSchema());
        Require(SameLookup(preparedLookup,PreparePortableRuntimeObjectLookup()),"Lookup reprepare duplicated/replaced Input schema candidates");
        SamePrerequisites(engine,metadata,config,inputClasses,collision);SamePackages(packageState,ReadPortableRuntimeNativePackages());
        GC::Collect();SameGc(preparedGc,"Repeated Input schema preparation/read/lookup/GC changed the native graph");
    }
    auto detached=ReadPortableRuntimeInputSchema();detached.prepared=false;detached.configApplied=true;detached.keyEnumIndex=0u;
    detached.aliasStruct.friendlyName="Detached";detached.fields[0u].field.outerIndex=0u;detached.links[1u].heads[0u]="Detached.Head";
    for(auto& row:detached.defaults)if(row.hasInputValues) {row.aliases[0u].alias="Detached";row.aliases[0u].command="Changed";row.bindings[0u]="Changed";}
    SameSnapshot(prepared,ReadPortableRuntimeInputSchema());ClearPortableRuntimeClient();
    Require(!HasPortableRuntimeClient(),"Input schema/default preparation fabricated a Client or Input instance");
    SameSnapshot(prepared,ReadPortableRuntimeInputSchema());
    SamePrerequisites(engine,metadata,config,inputClasses,collision);SamePackages(packageState,ReadPortableRuntimeNativePackages());
    Require(temporary.Snapshot("OwnedAfter")==checkpoint&&WorldPaths()==world&&GetPortableRuntimeWorldRevision()==revision&&
        ReadPortableRuntimeLevelTime()==0.0&&!GetPortableRuntimeUnsavedScriptState()&&
        QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"))&&
        QuestVr::Vm::Equal(sound,ReadPortableClassDefault("Engine.Engine","UseSound")),
        "Input schema/defaults contaminated dormant Training checkpoint/revision/actors/clock/dirty state/Engine defaults");Dormant();
    const auto warmRevision=GetPortableRuntimeWorldRevision();
    Require(LoadPortableRuntimeState((temporary.directory/"OwnedBefore.sav").string()),"Cannot warm-restore original dormant Training checkpoint");
    Require(GetPortableRuntimeWorldRevision()==warmRevision+1u,"Warm checkpoint replacement did not advance revision once");
    SameSnapshot(prepared,ReadPortableRuntimeInputSchema());SamePrerequisites(engine,metadata,config,inputClasses,collision);
    LoadedPackages(preMapPackages,ReadPortableRuntimeNativePackages());
    const auto warmLookup=ReadPortableRuntimeObjectLookup();
    Require(warmLookup.prepared&&warmLookup.generation==preparedLookup.generation&&warmLookup.objects==preparedLookup.objects,"Warm restore changed Input lookup identity/count");
    GC::Collect();SameGc(preparedGc,"Warm restore lost/duplicated native Input schema/default roots");
    Require(temporary.Snapshot("OwnedWarm")==checkpoint&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,"Warm restore contaminated dormant Training state");Dormant();
    Require(UnloadPortableRuntimeMap()>0u&&WorldPaths().empty(),"Cannot unload first Training map");GC::Collect();
    SameSnapshot(prepared,PreparePortableRuntimeInputSchema());SamePrerequisites(engine,metadata,config,inputClasses,collision);
    SamePackages(preMapPackages,ReadPortableRuntimeNativePackages());
    Require(GC::GetStats().numObjects==scriptGc.numObjects+265u,"Training unload did not retain exact engine-session Input schema/default graph");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload first Training with prepared Input schema");
    const auto reloadedPackages=ReadPortableRuntimeNativePackages();LoadedPackages(preMapPackages,reloadedPackages);
    const auto reloadedGc=GC::GetStats();SameSnapshot(prepared,ReadPortableRuntimeInputSchema());
    SameSnapshot(prepared,PreparePortableRuntimeInputSchema());Schema(prepared,packages);Defaults(prepared,ReadPortableRuntimeEngineCollision());
    SamePrerequisites(engine,metadata,config,inputClasses,collision);SamePackages(reloadedPackages,ReadPortableRuntimeNativePackages());
    GC::Collect();SameGc(reloadedGc,"Training reload lost/duplicated native Input schema/default roots");
    Require(temporary.Snapshot("OwnedReload")==checkpoint&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,"Training reload changed original dormant checkpoint");Dormant();
    ShutdownPortableRuntime();Empty(ReadPortableRuntimeInputSchema());
    for(const auto& row:prepared.fields)Refused([&]{(void)ReadPortableRuntimeObjectOwnership(row.field.objectPath);},"Shutdown retained a native Input schema identity");
    Refused([&]{(void)ReadPortableRuntimeObjectOwnership(prepared.aliasStruct.objectPath);},"Shutdown retained native Alias UStruct");
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        else throw std::runtime_error("Usage: portable_input_schema_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native Input schema integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        Empty(ReadPortableRuntimeInputSchema());Refused([&]{(void)PreparePortableRuntimeInputSchema();},"Input schema accepted uninitialized runtime");
        GC::Collect();SameGc(baseline,"No-runtime Input schema refusal leaked native objects");
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;sources.emplace_back(root/"System"/"DeusEx.ini");
        for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
            "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Original(root,packages,map,temporary);ShutdownPortableRuntime();GC::Collect();
        SameGc(baseline,"Input schema/default domain roots or allocation-index leases survived runtime shutdown");
        Empty(ReadPortableRuntimeInputSchema());Require(!ReadPortableRuntimeObjectLookup().prepared,"Shutdown retained Input-bearing object lookup");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned native Input schema integration: "<<checks<<" checks, "<<refusals
            <<" refusals; actual Actor255-key Enum, native Alias UStruct/258 properties/+259 lookup/+265 GC graph, exact declared layout/list order, "
              "five pre-config defaults, actual header/default/property visibility, detached typed aliases/bindings, atomic missing-phase refusals, "
              "first Training dormant checkpoint/warm restore/map/GC/shutdown lifetime. configApplied=false; NOT User.ini import, Input instances, "
              "viewport Init/ResetInput, native ReadInput, world startup, VR rendering or live crate gameplay.\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
