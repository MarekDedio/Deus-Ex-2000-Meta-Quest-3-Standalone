#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Actual first-map dynamic payloads, not completed native UObject destruction,
// original executable code, published world startup, usable loot or gravity.
namespace {
namespace fs=std::filesystem;
using Value=QuestVr::Vm::Value;using Kind=QuestVr::Vm::Kind;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool test,const std::string& why){++checks;if(!test)throw std::runtime_error(why);}
std::uint64_t Hash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint original source");
    std::uint64_t hash=14695981039346656037ull;std::array<char,16384u> bytes{};
    while(file){file.read(bytes.data(),bytes.size());for(std::streamsize i=0;i<file.gcount();++i){hash^=static_cast<unsigned char>(bytes[i]);hash*=1099511628211ull;}}
    Require(file.eof(),"Original fingerprint did not reach EOF");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size;fs::file_time_type time;std::uint64_t hash;
    explicit Source(fs::path file):path(fs::canonical(file)),size(fs::file_size(path)),time(fs::last_write_time(path)),hash(Hash(path)){}
    void Unchanged()const{Require(size==fs::file_size(path)&&time==fs::last_write_time(path)&&hash==Hash(path),"Original source changed");}
};
struct Temporary {
    fs::path directory;std::vector<fs::path> files;
    Temporary(){const auto parent=fs::canonical(fs::temp_directory_path());const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i){const auto next=parent/("quest-property-teardown-"+std::to_string(stamp)+'-'+std::to_string(i));if(fs::create_directory(next)){directory=fs::canonical(next);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent&&directory!=directory.root_path(),"Cannot create isolated checkpoint directory");}
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;for(const auto& file:files)if(file.parent_path()==directory)fs::remove(file,error);if(!directory.empty())fs::remove(directory,error);}
    Bytes Save(const std::string& leaf){const auto path=directory/(leaf+".sav");if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot save complete runtime state");Bytes bytes;
        Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read complete checkpoint");return bytes;}
};
struct Identity {std::string path,cls;std::uint32_t index{},classIndex{},outerIndex{};bool operator==(const Identity&)const=default;};
std::vector<Identity> Actors(){std::vector<Identity> result;for(const auto& row:GetPortableRuntimeMapActors(true)){
    const auto owner=ReadPortableRuntimeObjectOwnership(row.objectPath);result.push_back({row.objectPath,row.classPath,owner.objectIndex,owner.classIndex,owner.outerIndex});}return result;}
Value Read(const std::string& path,const char* key){return ReadPortableActorDeclaredScriptProperty(path,key);}
void Values(const PortablePropertyTeardownProbeResult& result,bool distributor,std::size_t writes) {
    const std::array<const char*,4u> actor{"BindName","BarkBindName","FamiliarName","UnfamiliarName"};
    const auto count=distributor?12u:6u;Require(result.values.size()==count,"Actual ConstructorLink did not match original independent census");
    for(std::size_t i=0u;i<result.values.size();++i){const auto& row=result.values[i];Value expected=row.before;
        if(distributor&&i<8u){
            Require(row.propertyPath=="DeusEx.PickupDistributor.NanoKeyData"&&row.arrayIndex==i&&row.before.kind==Kind::Struct,
                "Nested ConstructorLink order/dimension differs from actual original NanoKeyData[8]");
            Require(row.before.fields.size()==4u&&row.before.fields.at("description").kind==Kind::String&&
                row.before.fields.at("scriptedpawntag").kind==Kind::Name&&row.before.fields.at("keyid").kind==Kind::Name&&
                row.before.fields.at("skincolor").kind==Kind::Byte,"Original nested struct members have wrong typed layout");
            if(i<writes)expected.fields.at("description")=Value::Text(Kind::String,{});
        }else{
            const std::string key=distributor?std::string("Engine.Actor.")+actor[i-8u]:i<2u?
                std::string("DeusEx.DeusExDecoration.")+(i?"ItemName":"ItemArticle"):std::string("Engine.Actor.")+actor[i-2u];
            Require(row.propertyPath==key&&row.arrayIndex==0u&&row.before.kind==Kind::String,"String constructor field/order differs from original independent census");
            if(i<writes)expected=Value::Text(Kind::String,{});
        }
        Require(QuestVr::Vm::Equal(row.after,expected),"DestroyValue changed wrong payload/nonconstructor member or execution prefix");
    }
}
void Run(const fs::path& root){
    Temporary temporary;std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    for(const auto* name:QuestVr::RuntimeScriptPackages){const auto file=root/"System"/(std::string(name)+".u");sources.emplace_back(file);packages.push_back(LoadPortablePackageTables(file.string()));}
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(InitializePortableRuntime(packages,0x13579bdfu).passed,"Cannot initialize real original scripts");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare actual Engine classes");
    const auto metadata=PreparePortableRuntimeNativeMetadataClasses();
    Require(metadata.prepared&&metadata.classes.size()==14u&&metadata.classes[11u].classPath=="Core.StructProperty"&&
        metadata.classes[11u].nativeSize==0x64u,"Missing actual registered native StructProperty Class authority");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load ONLY first Training map");
    const auto original=temporary.Save("Authored");
    // An authentic live animation clock is an unrelated payload and must not
    // be restarted by native string/struct destruction.
    const auto waiting=ExecutePortableActorFunction("00_Training.Doctor1","PlayWaiting");
    Require(waiting.passed()&&waiting.committed,"Cannot run actual Doctor.PlayWaiting clock control");
    const auto before=temporary.Save("Before");const auto revision=GetPortableRuntimeWorldRevision();
    const auto actors=Actors();const auto lookup=ReadPortableRuntimeObjectLookup();const auto hash=ReadPortableRuntimeRetailCollision();
    GC::Collect();const auto gc=GC::GetStats();
    const auto rollback=[&]{Require(temporary.Save("After")==before&&GetPortableRuntimeWorldRevision()==revision&&Actors()==actors,
        "Property teardown changed full save/revision/actor slots or identities");
        const auto actual=ReadPortableRuntimeObjectLookup();const auto collision=ReadPortableRuntimeRetailCollision();
        Require(actual.prepared==lookup.prepared&&actual.objects==lookup.objects&&actual.retainedBytes==lookup.retainedBytes&&actual.generation==lookup.generation,
            "Property teardown changed UObject weak lookup authority");
        Require(collision.initialized==hash.initialized&&collision.enabled==hash.enabled&&collision.seed==hash.seed&&collision.tag==hash.tag,
            "Property teardown changed native hash/RNG state");GC::Collect();const auto current=GC::GetStats();
        Require(current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage,"Property teardown leaked native UObject roots");};
    for(const auto* leaf:{"PickupDistributor0","CrateBreakableMedGeneral1","CrateBreakableMedGeneral2"}){
        const std::string path=std::string("00_Training.")+leaf;const bool distributor=std::string(leaf)=="PickupDistributor0";
        const auto tag=Read(path,"Engine.Actor.Tag"),location=Read(path,"Engine.Actor.Location"),flags=Read(path,"Core.Object.ObjectFlags");
        const auto sourceFlags=static_cast<std::uint32_t>(map.exports.at(FindPortableExport(map,leaf)).ObjFlags);
        Require(static_cast<std::uint32_t>(flags.integer)==sourceFlags,
            "Absent ObjectFlags overlay did not read the actual original native header");
        const auto full=ProbePortableActorPropertyTeardown(path);const std::size_t count=distributor?12u:6u;
        Require(full.execution.passed()&&!full.execution.committed&&full.completed&&full.execution.writes==count,
            "Actual ExitProperties failed: "+full.execution.error);Values(full,distributor,count);rollback();
        if(!distributor)Require(!full.values[1u].before.text.empty(),"Crate did not exercise a nonempty original ItemName");
        for(std::size_t prefix=0u;prefix<count;++prefix){QuestVr::Vm::Limits limits;limits.writes=prefix;
            const auto partial=ProbePortableActorPropertyTeardown(path,limits);
            Require(!partial.execution.passed()&&!partial.execution.committed&&!partial.completed&&partial.execution.status==QuestVr::Vm::Status::Budget&&
                partial.execution.writes==prefix,"Partial DestroyValue failed outside exact caller write prefix: "+partial.execution.error);
            Values(partial,distributor,prefix);rollback();++refusals;}
        const auto warm=ProbePortableActorPropertyTeardown(path);
        Require(warm.execution.passed()&&warm.completed,"Warmed real teardown failed");Values(warm,distributor,count);rollback();
        const auto work=warm.execution.nativeWork;Require(work.has_value()&&work->used>1u,"Teardown omitted actual borrowed native-work usage");
        for(const auto prefix:{std::size_t{0u},work->used/2u,work->used-1u}){QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
            const auto partial=ProbePortableActorPropertyTeardown(path,limits);
            Require(!partial.execution.passed()&&!partial.execution.committed&&!partial.completed&&partial.execution.nativeWork&&
                partial.execution.nativeWork->exhausted,"Teardown failed to obey caller native-work prefix");rollback();++refusals;}
        Require(QuestVr::Vm::Equal(Read(path,"Engine.Actor.Tag"),tag)&&QuestVr::Vm::Equal(Read(path,"Engine.Actor.Location"),location)&&
            QuestVr::Vm::Equal(Read(path,"Core.Object.ObjectFlags"),flags),"Payload probe changed nonconstructor fields");
        std::cout<<"PASS actual "<<path<<" ConstructorLink: "<<count<<" original payload stores; complete and every write-prefix rollback; borrowed native work="<<work->used<<'\n';
    }
    Require(LoadPortableRuntimeState((temporary.directory/"Authored.sav").string())&&temporary.Save("RestoredAuthored")==original,
        "Cannot restore original authored/Doctor clock baseline");
    ShutdownPortableRuntime();for(const auto& source:sources)source.Unchanged();
    std::cout<<"UNCHANGED "<<sources.size()<<" original sources; ONLY first Training loaded. No full native destruction/linker/deferred ownership, startup, loot or gravity claim.\n";
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    Require(argc<=2,"Usage: portable_property_teardown_test [OWNED_GAME_ROOT]");const char* env=std::getenv("DEUSEX_GAME_ROOT");
    const fs::path root=argc==2?fs::path(argv[1]):env?fs::path(env):fs::path{};
    if(root.empty()){std::cout<<"SKIP owned first-Training property teardown: supply source root\n";return 77;}
    ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();Run(fs::canonical(root));ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
    Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,"Shutdown leaked native teardown objects");
    std::cout<<"PASS real first-Training payload teardown: "<<checks<<" checks / "<<refusals<<" refusal prefixes\n";return 0;
}catch(const std::exception& error){ShutdownPortableRuntime();std::cerr<<"FAIL property teardown: "<<error.what()<<'\n';return 1;}}
