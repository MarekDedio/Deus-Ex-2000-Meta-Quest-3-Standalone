#include "Precomp.h"
#include "GC/GC.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "quest_mesh_animation.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Owned integration, not the detached binding coordinator test. Calls the
// actual authored native Function through the public VM entry and inspects the
// resulting native UObject graph. ONLY Maps/00_Training.dx is ever loaded.
// Commercial packages are data: no original EXE/DLL is loaded or executed.
namespace {
namespace fs=std::filesystem;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Value;
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
    auto a=child.begin();
    for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t Fingerprint(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t result=14695981039346656037ull;std::uintmax_t total{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Source exceeds fingerprint bound");
        for(std::streamsize i=0;i<count;++i){result^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);result*=1099511628211ull;}
    }
    Require(input.eof(),"Source fingerprint did not finish");return result;
}
struct SourceGuard {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t fingerprint{};
    explicit SourceGuard(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),fingerprint(Fingerprint(path)){}
    void Unchanged()const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==fingerprint,
            "Owned original source changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,source),"Temporary saves overlap original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto candidate=parent/("quest-con-bind-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){directory=fs::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& path:files)if(path.parent_path()==directory)fs::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    fs::path File(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid temporary leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    Bytes Save(const std::string& leaf) {
        const auto path=File(leaf);Require(SavePortableRuntimeState(path.string()),"Cannot save baseline: "+leaf);
        Bytes result;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,result),"Cannot read baseline");
        return result;
    }
};

// Independent authored-data oracle: tagged instance -> concrete class defaults
// -> typed zero. This does not call the production binding coordinator or use
// the runtime's conversation presentation index to predict accepted entries.
struct SourceObject {const PortablePackageTables* table{};std::size_t index{};};
struct Oracle {
    std::map<std::string,const PortablePackageTables*> tables;
    std::map<std::string,SourceObject> objects;
    std::map<std::string,PortablePropertyStream> streams;
    std::map<std::string,PortableClassDescriptor> classes;
    Oracle(const std::vector<PortablePackageTables>& scripts,const PortablePackageTables& map) {
        for(const auto& table:scripts)Add(table);
        Add(map);
    }
    void Add(const PortablePackageTables& table) {
        const auto package=fs::path(table.sourcePath).stem().string();
        Require(tables.emplace(Fold(package),&table).second,"Duplicate source package");
        for(std::size_t i=0u;i<table.exports.size();++i)
            Require(objects.emplace(Fold(package+'.'+GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u))),
                SourceObject{&table,i}).second,"Duplicate source identity");
    }
    const SourceObject& Find(const std::string& path)const {
        const auto found=objects.find(Fold(path));Require(found!=objects.end(),"Authored oracle identity missing: "+path);return found->second;
    }
    std::string Qualified(const PortablePackageTables& table,std::int32_t reference)const {
        if(!reference)return {};
        const auto path=GetPortableObjectPath(table,reference);
        return reference>0?fs::path(table.sourcePath).stem().string()+'.'+path:path;
    }
    std::optional<Value> Tagged(const PortablePackageTables& table,
        const std::vector<PortableTaggedProperty>& tags,const std::string& leaf,Kind kind) {
        const PortableTaggedProperty* selected{};
        for(const auto& tag:tags)if(Fold(tag.name.ToString())==Fold(leaf)&&tag.arrayIndex==0u) {
            Require(!selected,"Duplicate source property: "+leaf);selected=&tag;
        }
        if(!selected)return {};
        if(kind==Kind::Object){Require(selected->type==5u,"Source object tag type differs");
            return Value::Text(kind,Qualified(table,DecodePortableObjectReference(*selected)));}
        if(kind==Kind::String){Require(selected->type==13u,"Source string tag type differs");
            return Value::Text(kind,DecodePortableVmStringProperty(*selected));}
        if(kind==Kind::Name){Require(selected->type==6u,"Source FName tag type differs");
            return Value::Text(kind,DecodePortableNameProperty(table,*selected));}
        Require(kind==Kind::Int&&selected->type==2u&&selected->value.size()==4u,"Source Int tag type differs");
        std::int32_t value{};std::memcpy(&value,selected->value.data(),4u);return Value::Integer(value);
    }
    Value Read(const std::string& path,const std::string& leaf,Kind kind) {
        const auto& object=Find(path);auto stream=streams.find(Fold(path));
        if(stream==streams.end())stream=streams.emplace(Fold(path),LoadPortableExportProperties(*object.table,object.index)).first;
        if(const auto value=Tagged(*object.table,stream->second.properties,leaf,kind))return *value;
        auto at=Qualified(*object.table,object.table->exports.at(object.index).ObjClass);std::set<std::string> visited;
        while(!at.empty()) {
            Require(visited.size()<128u&&visited.insert(Fold(at)).second,"Authored oracle class chain is cyclic");
            const auto& cls=Find(at);Require(!cls.table->exports.at(cls.index).ObjClass,"Authored class reference is not UClass");
            auto desc=classes.find(Fold(at));
            if(desc==classes.end())desc=classes.emplace(Fold(at),LoadPortableClassDescriptor(*cls.table,cls.index)).first;
            if(const auto value=Tagged(*cls.table,desc->second.defaults,leaf,kind))return *value;
            at=Qualified(*cls.table,cls.table->exports.at(cls.index).ObjBase);
        }
        return kind==Kind::Int?Value::Integer(0):Value::Text(kind,kind==Kind::Name?"None":"");
    }
    struct Conversation {std::string path,owner,name;std::int32_t count{};};
    std::vector<Conversation> Conversations() {
        std::vector<Conversation> result;std::set<std::string> seen;
        auto at=Read("DeusExConText.ConList_Mission00","conversations",Kind::Object).text;
        while(!at.empty()) {
            Require(result.size()<65536u&&seen.insert(Fold(at)).second,"Original ConItem list is cyclic/oversized");
            const auto con=Read(at,"ConObject",Kind::Object).text;Require(!con.empty(),"Original ConItem has no conversation");
            result.push_back({con,Read(con,"conOwnerName",Kind::String).text,Read(con,"conName",Kind::Name).text,
                Read(con,"ownerRefCount",Kind::Int).integer});
            at=Read(at,"Next",Kind::Object).text;
        }
        Require(result.size()==470u,"First Training source list changed from audited 470 entries");return result;
    }
    std::vector<std::string> Accepted(const std::string& actor,const std::vector<Conversation>& source) {
        const auto bind=Read(actor,"BindName",Kind::String).text,bark=Read(actor,"BarkBindName",Kind::String).text;
        std::vector<std::string> result;
        for(const auto& con:source) {
            for(const auto& text:{con.owner,con.name,bind,bark})
                Require(std::all_of(text.begin(),text.end(),[](unsigned char c){return c>0u&&c<128u;}),
                    "This independent source oracle needs a new non-ASCII/C-string witness");
            const bool isBark=con.name.find("_Bark")!=std::string::npos;
            const bool accepted=(isBark&&Fold(con.owner)==Fold(bark))||
                (Fold(con.owner)==Fold(bind)&&(bark.empty()||!isBark));
            if(accepted)result.push_back(con.path);
        }
        std::reverse(result.begin(),result.end());return result;
    }
};

Value Read(const std::string& object,const char* property,Kind kind) {
    const auto result=ReadPortableRuntimeDeclaredScriptProperty(object,property);
    Require(result.kind==kind,std::string("Declared runtime type differs: ")+object+" / "+property);return result;
}
QuestVr::Vm::Limits Ample() {
    QuestVr::Vm::Limits result;result.nativeWork=262144u;return result;
}
QuestVr::Vm::Result Bind(const std::string& actor,const QuestVr::Vm::Limits& limits=Ample()) {
    return ExecutePortableActorFunction(actor,"ConBindEvents",{},limits);
}
void Passed(const QuestVr::Vm::Result& result,const std::string& why) {
    Require(result.passed()&&result.committed,why+": "+result.error+" at "+result.function+':'+std::to_string(result.offset));
    Require(result.nativeWork&&result.nativeWork->used&& !result.nativeWork->exhausted,"Actual native work accounting is absent");
}
struct Node {
    std::string path,con,next,name,classPath,outer;std::uint32_t index{},classIndex{},outerIndex{},flags{};
    bool operator==(const Node&)const=default;
};
std::vector<Node> Chain(const std::string& actor) {
    std::vector<Node> result;std::set<std::string> paths;std::set<std::uint32_t> indices;
    auto at=Read(actor,"Engine.Actor.ConListItems",Kind::Object).text;
    const auto cls=ReadPortableRuntimeObjectOwnership("ConSys.ConListItem");
    const auto outer=ReadPortableRuntimeObjectOwnership("DeusExConText");
    while(!at.empty()) {
        Require(result.size()<65536u&&paths.insert(Fold(at)).second,"Runtime ConListItem chain is cyclic/oversized");
        const auto identity=ReadPortableRuntimeObjectOwnership(at);
        Require(identity.objectIndex&&indices.insert(identity.objectIndex).second,"Live node lacks unique native index");
        Node node{at,Read(at,"ConSys.ConListItem.con",Kind::Object).text,
            Read(at,"ConSys.ConListItem.Next",Kind::Object).text,Read(at,"Core.Object.Name",Kind::Name).text,
            Read(at,"Core.Object.Class",Kind::Object).text,Read(at,"Core.Object.Outer",Kind::Object).text,
            identity.objectIndex,identity.classIndex,identity.outerIndex,
            std::bit_cast<std::uint32_t>(Read(at,"Core.Object.ObjectFlags",Kind::Int).integer)};
        Require(node.classPath=="ConSys.ConListItem"&&identity.classPath==node.classPath&&node.classIndex==cls.objectIndex,
            "Native list node is not the exact actual ConSys.ConListItem class");
        Require(node.outer=="DeusExConText"&&identity.outerPath==node.outer&&node.outerIndex==outer.objectIndex,
            "Native list node did not use ConversationList.GetOuter package");
        Require(node.flags==4u&&(node.flags&0x4000u)==0u,"Original list node must be RF_Public, not RF_Transient");
        Require(!node.con.empty()&&!node.name.empty()&&node.path==node.outer+'.'+node.name,
            "Native list node lost original named/package identity");
        const auto found=FindPortableRuntimeObject({node.name,cls.objectIndex,outer.objectIndex,false,true});
        Require(found&&found->objectPath==node.path&&found->objectIndex==node.index&&found->classIndex==node.classIndex&&
            found->outerIndex==node.outerIndex,"Exact actual native class/Outer lookup cannot find the live node");
        result.push_back(node);at=node.next;
    }
    return result;
}
void Expected(const std::vector<Node>& chain,const std::vector<std::string>& expected) {
    Require(chain.size()==expected.size(),"Real binding node count differs from independent source oracle");
    for(std::size_t i=0u;i<chain.size();++i)Require(chain[i].con==expected[i],"Real binding source order/multiplicity differs");
}
void Gone(const std::vector<Node>& old,const std::vector<Node>& current) {
    for(const auto& node:old) {
        Require(std::none_of(current.begin(),current.end(),[&](const auto& now){return now.path==node.path;}),
            "Rebind reused a deleted ConListItem name");
        Require(!FindPortableRuntimeObject({node.name,node.classIndex,node.outerIndex,false,true}),
            "Deleted ConListItem is still visible through native weak lookup");
        bool rejected{};try{static_cast<void>(ReadPortableRuntimeDeclaredScriptProperty(node.path,"ConSys.ConListItem.con"));}
        catch(const std::exception&){rejected=true;}
        Require(rejected,"Deleted ConListItem remains readable by path");++refusals;
    }
}
std::vector<std::string> NextNames(const std::vector<Node>& newest) {
    Require(!newest.empty(),"Native counter witness needs a positive binding");
    const std::string prefix="ConListItem";const auto& head=newest.front();
    Require(head.name.starts_with(prefix)&&head.name.size()>prefix.size(),"Native unique name has unexpected concrete-class prefix");
    std::uint32_t counter{};
    for(const char c:head.name.substr(prefix.size())) {
        Require(c>='0'&&c<='9'&&counter<1000000u,"Native unique suffix is not a bounded original decimal counter");
        counter=counter*10u+static_cast<std::uint32_t>(c-'0');
    }
    ++counter;std::vector<std::string> result;
    while(result.size()<newest.size()) {
        Require(counter<1000000u,"Independent next-name collision scan exceeds bound");
        const auto candidate=prefix+std::to_string(counter++);
        // StaticFindObject checks ANY concrete class in the actual Outer.
        if(!FindPortableRuntimeObject({candidate,0u,head.outerIndex,false,false}))result.push_back(candidate);
    }
    std::reverse(result.begin(),result.end());return result;
}
using Counts=std::map<std::string,std::int32_t>;
Counts CountSnapshot(const std::vector<Oracle::Conversation>& source) {
    Counts result;for(const auto& con:source)result.emplace(con.path,Read(con.path,"ConSys.Conversation.ownerRefCount",Kind::Int).integer);
    return result;
}
void Balanced(const std::vector<Oracle::Conversation>& source,const std::vector<std::vector<Node>>& bindings) {
    std::map<std::string,std::uint32_t> frequency;
    for(const auto& chain:bindings)for(const auto& node:chain)++frequency[node.con];
    for(const auto& con:source)Require(std::bit_cast<std::uint32_t>(Read(con.path,"ConSys.Conversation.ownerRefCount",Kind::Int).integer)==
        std::bit_cast<std::uint32_t>(con.count)+frequency[con.path],"Native ownerRefCount differs from all live binding multiplicities");
}
PortableActorSnapshot Actor(const std::string& path) {
    const auto actors=GetPortableRuntimeMapActors(true);
    const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& actor){return actor.objectPath==path;});
    Require(found!=actors.end(),"Actual first Training Actor missing: "+path);return *found;
}
bool SameAnimation(const PortableActorAnimationSnapshot& a,const PortableActorAnimationSnapshot& b) {
    if(std::tie(a.sequence,a.frame,a.rate,a.last,a.minRate,a.tweenRate,a.oldRate,a.loop,a.notify,a.finished,
            a.previous.vertexOffset0,a.previous.vertexOffset1,a.previous.fraction)!=
        std::tie(b.sequence,b.frame,b.rate,b.last,b.minRate,b.tweenRate,b.oldRate,b.loop,b.notify,b.finished,
            b.previous.vertexOffset0,b.previous.vertexOffset1,b.previous.fraction))return false;
    for(std::size_t i=0u;i<a.blends.size();++i) {
        const auto& x=a.blends[i];const auto& y=b.blends[i];
        if(std::tie(x.sequence,x.frame,x.rate,x.last,x.minRate,x.tweenRate,x.oldRate,x.loop,
                x.previous.vertexOffset0,x.previous.vertexOffset1,x.previous.fraction)!=
            std::tie(y.sequence,y.frame,y.rate,y.last,y.minRate,y.tweenRate,y.oldRate,y.loop,
                y.previous.vertexOffset0,y.previous.vertexOffset1,y.previous.fraction))return false;
    }
    return true;
}
bool SameLookup(const PortableObjectLookupState& a,const PortableObjectLookupState& b) {
    return a.prepared==b.prepared&&a.objects==b.objects&&a.generation==b.generation&&a.retainedBytes==b.retainedBytes;
}

void Run(const fs::path& root) {
    Temporary temporary(root);std::vector<SourceGuard> guards;std::vector<PortablePackageTables> scripts;
    guards.reserve(QuestVr::RuntimeScriptPackages.size()+1u);scripts.reserve(QuestVr::RuntimeScriptPackages.size());
    for(const auto* name:QuestVr::RuntimeScriptPackages) {
        guards.emplace_back(root/"System"/(std::string(name)+".u"));scripts.push_back(LoadPortablePackageTables(guards.back().path.string()));
    }
    guards.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(guards.back().path.string());
    try {
        Oracle oracle(scripts,map);const auto source=oracle.Conversations();
        const std::string jaime="00_Training.JaimeReyes2",troop="00_Training.UNATCOTroop0",secondTroop="00_Training.UNATCOTroop3",
            doctor="00_Training.Doctor1",plant="00_Training.Plant2";
        const auto expectedJaime=oracle.Accepted(jaime,source),expectedTroop=oracle.Accepted(troop,source),
            expectedSecondTroop=oracle.Accepted(secondTroop,source);
        Require(expectedJaime.size()==4u&&expectedTroop.size()==2u&&expectedSecondTroop==expectedTroop,
            "Positive original first-Training binding controls changed");
        Require(oracle.Accepted(doctor,source).empty()&&oracle.Accepted(plant,source).empty(),"Empty authored controls changed");
        Require(InitializePortableRuntime(scripts).passed,"Cannot initialize exact Quest script source set");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared,"Actual metadata/Package/weak lookup prerequisites failed");
        Require(LoadPortableRuntimeMap(map).passed,"Cannot load ONLY actual first Training map");
        Require(!Read("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Test unexpectedly forced original world startup");
        for(const auto& actor:{jaime,troop,secondTroop,doctor,plant})Require(Chain(actor).empty(),"Fresh map already has a bound native list");
        Balanced(source,{});const auto baseline=temporary.Save("Baseline");const auto revision=GetPortableRuntimeWorldRevision();
        const auto lookup=ReadPortableRuntimeObjectLookup();GC::Collect();const auto gc=GC::GetStats();
        const auto initialRollback=[&] {
            Require(temporary.Save("InitialRollback")==baseline,"Refused first bind changed checkpoint/property/clock/RNG state");
            Require(GetPortableRuntimeWorldRevision()==revision&&SameLookup(lookup,ReadPortableRuntimeObjectLookup()),
                "Refused first bind changed revision or actual weak lookup census");
            for(const auto& actor:{jaime,troop,secondTroop,doctor,plant})Require(Chain(actor).empty(),"Refused first bind retained native head");
            Balanced(source,{});GC::Collect();const auto now=GC::GetStats();
            Require(now.numObjects==gc.numObjects&&now.memoryUsage==gc.memoryUsage,"Refused first bind leaked native GC objects");
        };
        for(const bool writeLimit:{false,true}) {
            auto limits=Ample();if(writeLimit)limits.writes=1u;else limits.nativeWork=0u;
            const auto failed=Bind(jaime,limits);
            Require(!failed.passed()&&!failed.committed&&failed.status==QuestVr::Vm::Status::Budget,
                "First bind ignored shared "+std::string(writeLimit?"write":"native-work")+" budget");
            ++refusals;initialRollback();
        }
        Passed(Bind(plant),"Actual decoration native2101 empty authored binding failed");
        Require(temporary.Save("EmptyDecoration")==baseline,"Empty original decoration binding serialized invented state");

        // A real authored helper establishes native animation state; binding an
        // empty Doctor list must not recreate/reset its animation clock.
        const auto beforeDoctor=Actor(doctor);const auto mesh=GetPortableRuntimeMesh(beforeDoctor.meshPath);
        Require(mesh.animation&&!mesh.animation->sequences.empty(),"Original Doctor animation asset unavailable");
        const auto sequence=std::find_if(mesh.animation->sequences.begin(),mesh.animation->sequences.end(),[&](const auto& value) {
            return value.numFrames>1&&QuestVr::HasUsableMeshAnimationSpan(*mesh.animation,value);
        });
        Require(sequence!=mesh.animation->sequences.end(),"Original Doctor has no usable animation sequence");
        const auto animation=ExecutePortableActorFunction(doctor,"LoopAnimPivot",{
            {Value::Text(Kind::Name,sequence->name),{}},{Value::Float(2.0f),{}},{Value::Float(0.2f),{}},
            {Value::Float(0.5f),{}},{Value::Vector({0.0f,0.0f,0.0f}),{}}},Ample());
        Require(animation.passed()&&animation.committed,"Original Doctor LoopAnimPivot failed: "+animation.error);
        const auto pose=Actor(doctor);const auto clockCheckpoint=temporary.Save("DoctorClock");
        Passed(Bind(doctor),"Actual ScriptedPawn native2102 empty Doctor binding failed");
        Require(Chain(doctor).empty()&&SameAnimation(pose.animation,Actor(doctor).animation)&&
            temporary.Save("DoctorAfterBind")==clockCheckpoint,"Conversation head store reset/serialized the native Doctor clock");
        Require(LoadPortableRuntimeState(temporary.File("Baseline").string()),"Cannot restore untouched baseline before positive bindings");

        auto result=Bind(jaime);Passed(result,"Actual Jaime native2102 binding failed");auto jaimeChain=Chain(jaime);
        Expected(jaimeChain,expectedJaime);Balanced(source,{jaimeChain});
        Require(GetPortableRuntimeScriptStatePresent()&&GetPortableRuntimeUnsavedScriptState(),
            "Actual native conversation links/counts were falsely reported as clean/absent state");
        const auto denied=temporary.File("UnsupportedArchive");
        Require(!SavePortableRuntimeState(denied.string())&&!fs::exists(denied),
            "Unimplemented native conversation archive silently saved dangling native paths/counts");++refusals;
        for(const bool wrongOwner:{false,true}) {
            bool rejected{};try {
                static_cast<void>(ReadPortableRuntimeDeclaredScriptProperty(jaimeChain.front().path,
                    wrongOwner?"Engine.Actor.ConListItems":"ConSys.ConListItem.con",wrongOwner?0u:1u));
            }catch(const std::exception&){rejected=true;}
            Require(rejected,"Native node property read accepted foreign declaration or non-scalar array index");++refusals;
        }
        const auto liveModel=ReadPortableRuntimeNativeModel();bool unloadRejected{};
        try{static_cast<void>(UnloadPortableRuntimeMap());}catch(const std::exception&){unloadRejected=true;}
        Require(unloadRejected&&Chain(jaime)==jaimeChain&&
            ReadPortableRuntimeNativeModel().mapGeneration==liveModel.mapGeneration,
            "Unimplemented map conversation retirement discarded a live binding/world");++refusals;
        Require(!LoadPortableRuntimeState(temporary.File("Baseline").string())&&Chain(jaime)==jaimeChain,
            "Unimplemented conversation checkpoint replacement discarded live native bindings");++refusals;
        Balanced(source,{jaimeChain});
        auto old=jaimeChain;result=Bind(jaime);Passed(result,"Actual Jaime purge/rebind failed");jaimeChain=Chain(jaime);
        Expected(jaimeChain,expectedJaime);Gone(old,jaimeChain);Balanced(source,{jaimeChain});
        Passed(Bind(troop),"Actual first Trooper positive/native binding failed");auto troopChain=Chain(troop);
        Expected(troopChain,expectedTroop);Balanced(source,{jaimeChain,troopChain});
        Passed(Bind(secondTroop),"Actual second shared-conversation Actor binding failed");auto secondTroopChain=Chain(secondTroop);
        Expected(secondTroopChain,expectedSecondTroop);Balanced(source,{jaimeChain,troopChain,secondTroopChain});
        for(const auto& a:troopChain)for(const auto& b:secondTroopChain)Require(a.index!=b.index&&a.path!=b.path,
            "Two actors incorrectly share a mutable native ConListItem node");

        // A successful real rebind supplies its actual root accounting, not a
        // guessed mock budget. N-1 refuses late, after purge/allocation work.
        old=troopChain;const auto accounting=Bind(troop);Passed(accounting,"Measured actual rebind failed");
        troopChain=Chain(troop);Expected(troopChain,expectedTroop);Gone(old,troopChain);
        Require(accounting.writes>old.size()*2u&&accounting.nativeWork->used>1u,"Measured positive rebind did not perform real writes/work");
        const auto nextNames=NextNames(troopChain);
        for(const bool writeLimit:{true,false}) {
            GC::Collect();const auto beforeGc=GC::GetStats();const auto beforeLookup=ReadPortableRuntimeObjectLookup();
            const auto beforeRevision=GetPortableRuntimeWorldRevision();const auto beforeCounts=CountSnapshot(source);
            auto limits=Ample();if(writeLimit)limits.writes=accounting.writes-1u;
            else limits.nativeWork=accounting.nativeWork->used-1u;
            const auto failed=Bind(troop,limits);
            Require(!failed.passed()&&!failed.committed&&failed.status==QuestVr::Vm::Status::Budget&&failed.writes>old.size()*2u,
                "Late rebind did not refuse after actual purge/new node work: "+failed.error);++refusals;
            Require(Chain(jaime)==jaimeChain&&Chain(troop)==troopChain&&Chain(secondTroop)==secondTroopChain&&CountSnapshot(source)==beforeCounts,
                "Late rebind refusal lost old binding identity/links/refcounts");
            Require(GetPortableRuntimeWorldRevision()==beforeRevision&&SameLookup(beforeLookup,ReadPortableRuntimeObjectLookup()),
                "Late rebind refusal changed revision/weak lookup census");
            GC::Collect();const auto afterGc=GC::GetStats();
            Require(afterGc.numObjects==beforeGc.numObjects&&afterGc.memoryUsage==beforeGc.memoryUsage,
                "Late rebind refusal leaked newly allocated nodes or collected the restored old binding");
            Balanced(source,{jaimeChain,troopChain,secondTroopChain});
        }
        old=troopChain;Passed(Bind(troop),"Rebind after two rolled-back allocation/deletion roots failed");
        troopChain=Chain(troop);Expected(troopChain,expectedTroop);Gone(old,troopChain);
        Require(troopChain.size()==nextNames.size(),"Post-rollback naming witness size changed");
        for(std::size_t i=0u;i<nextNames.size();++i)Require(troopChain[i].name==nextNames[i],
            "Failed rebind consumed the original concrete UClass unique-name counter");
        GC::Collect();Require(Chain(jaime)==jaimeChain&&Chain(secondTroop)==secondTroopChain&&Chain(troop)==troopChain,
            "Committed actor head/con/Next edges do not root genuine native list objects");
        Balanced(source,{jaimeChain,troopChain,secondTroopChain});
        std::cout<<"PASS actual first Training native2101/2102: 470-entry source oracle; Jaime4 (including actual FName barks), shared Troopers2+2; "
            "exact native class/flags/packageOuter/indices/lookup, real purge/rebind, ownerRefCount balance, late write/work rollback, "
            "old graph GC, Doctor clock and explicit unsupported archive refusal; checks="<<checks<<" refusals="<<refusals<<'\n';
        std::cout<<"NOT proof of native2100 possessed-player execution, conversation archive support, complete startup, crates or playable Training.\n";
    }catch(...) {
        ShutdownPortableRuntime();for(const auto& guard:guards)guard.Unchanged();throw;
    }
    ShutdownPortableRuntime();for(const auto& guard:guards)guard.Unchanged();
    std::cout<<"UNCHANGED original sources="<<guards.size()<<"; only first Training map loaded\n";
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        Require(argc<=2,"Usage: portable_conversation_bind_test [OWNED_GAME_ROOT] (ONLY Maps/00_Training.dx)");
        const auto* environment=std::getenv("DEUSEX_GAME_ROOT");
        const fs::path root=argc==2?fs::path(argv[1]):environment?fs::path(environment):fs::path{};
        if(root.empty()){std::cout<<"SKIP: supply owned Deus Ex root for actual first-Training conversation binding\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Run(fs::canonical(root));
        ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(after.numObjects==before.numObjects&&after.memoryUsage==before.memoryUsage,"Runtime shutdown leaked native conversation objects");
        return 0;
    }catch(const std::exception& error){ShutdownPortableRuntime();std::cerr<<"FAIL actual conversation binding: "<<error.what()<<'\n';return 1;}
}
