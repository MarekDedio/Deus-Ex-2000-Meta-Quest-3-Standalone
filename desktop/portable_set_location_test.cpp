#include "Precomp.h"
#include "GC/GC.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

// Actual first Training ONLY. Dormant native267 controls do not prove original
// Login, begun physics, live weapon damage or surviving loot. Owned commercial
// files are read-only data; never execute or load original DLL/EXE code.
namespace {
namespace fs=std::filesystem;using Value=QuestVr::Vm::Value;using Kind=QuestVr::Vm::Kind;
using Bytes=std::vector<std::uint8_t>;std::size_t checks{};
void Need(bool v,const std::string& why){++checks;if(!v)throw std::runtime_error(why);}
std::uint64_t Fingerprint(const fs::path& path){
    std::ifstream input(path,std::ios::binary);Need(bool(input),"Cannot fingerprint original");
    std::array<char,16384> buffer;std::uint64_t hash=14695981039346656037ull;std::size_t total{};
    while(input){input.read(buffer.data(),buffer.size());const auto n=input.gcount();total+=static_cast<std::size_t>(n);
        Need(total<=512u*1024u*1024u,"Fingerprint bound exceeded");
        for(std::streamsize i=0;i<n;++i){hash^=static_cast<unsigned char>(buffer[i]);hash*=1099511628211ull;}}
    Need(input.eof(),"Fingerprint did not finish");return hash;
}
struct Guard {fs::path path;std::uintmax_t size;fs::file_time_type time;std::uint64_t hash;
    explicit Guard(fs::path p):path(fs::canonical(p)),size(fs::file_size(path)),time(fs::last_write_time(path)),hash(Fingerprint(path)){}
    void Unchanged()const{Need(fs::file_size(path)==size&&fs::last_write_time(path)==time&&Fingerprint(path)==hash,"Owned original changed: "+path.string());}};
struct Temporary {
    fs::path parent,path;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source){parent=fs::canonical(fs::temp_directory_path());Need(parent!=parent.root_path(),"Unsafe temporary root");
        auto part=parent.begin();bool overlaps=true;
        for(auto original=source.begin();original!=source.end();++original,++part) {
            if(part==parent.end()){overlaps=false;break;}
            auto a=part->string(),b=original->string();
            for(auto& c:a)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
            for(auto& c:b)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
            if(a!=b){overlaps=false;break;}
        }
        Need(!overlaps,"Temporary checkpoints overlap owned original installation");
        path=parent/("quest-set-location-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Need(fs::create_directory(path),"Cannot create unique temporary directory");path=fs::canonical(path);Need(path.parent_path()==parent,"Temporary parent changed");}
    ~Temporary(){ShutdownPortableRuntime();std::error_code ec;for(const auto& file:files)if(file.parent_path()==path)fs::remove(file,ec);
        if(path.parent_path()==parent)fs::remove(path,ec);}
    Bytes Save(const std::string& leaf){const auto file=path/(leaf+".sav");if(std::find(files.begin(),files.end(),file)==files.end())files.push_back(file);
        Need(SavePortableRuntimeState(file.string()),"Checkpoint capture failed");Bytes bytes;
        Need(QuestVr::ReadBoundedSaveFile(file.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Checkpoint read failed");return bytes;}
};
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits l;l.instructions=1'000'000;l.nativeWork=1'048'576;l.writes=65536;l.retainedBytes=128u<<20u;return l;}
Value Read(const std::string& actor,const char* key){return ReadPortableActorDeclaredScriptProperty(actor,key);}
bool Equal(const std::array<float,3>& a,const std::array<float,3>& b){
    for(unsigned i=0;i<3;++i) {
        if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;
    }
    return true;
}
void Run(const fs::path& root){
    Temporary tmp(root);std::vector<Guard> guards;std::vector<PortablePackageTables> packages;
    for(const auto* name:QuestVr::RuntimeScriptPackages){guards.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(guards.back().path.string()));}
    guards.emplace_back(root/"System"/"DeusEx.ini");const auto ini=guards.back().path.string();
    guards.emplace_back(root/"System"/"User.ini");guards.emplace_back(root/"Maps"/"00_Training.dx");
    const auto map=LoadPortablePackageTables(guards.back().path.string());
    try{
        Need(InitializePortableRuntime(packages,0x13579bdfu,QuestVr::OriginalLinkerLoadProfile{false,true,true}).passed,"Runtime init failed");
        Need(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini).prepared,"Native prerequisites failed");
        Need(LoadPortableRuntimeMap(map).passed,"First Training load failed");
        const std::string actor="00_Training.CrateBreakableMedGeneral2",level="00_Training.LevelInfo0";
        Need(!Read(level,"Engine.LevelInfo.bBegunPlay").boolean,"Test must keep actual world dormant");
        const auto original=Read(actor,"Engine.Actor.Location").vector;
        const auto rotation=Read(actor,"Engine.Actor.Rotation").rotation;
        const auto old=Read(actor,"Engine.Actor.OldLocation").vector;
        const auto col=Read(actor,"Engine.Actor.ColLocation").vector;
        const auto baseline=tmp.Save("baseline");const auto revision=GetPortableRuntimeWorldRevision();
        const auto order=ReadPortableWorldActorOrder();const auto lookup=ReadPortableRuntimeObjectLookup();GC::Collect();const auto gc=GC::GetStats();
        const auto restored=[&]{Need(tmp.Save("rollback")==baseline,"Full saved state did not roll back");
            Need(GetPortableRuntimeWorldRevision()==revision&&ReadPortableWorldActorOrder()==order,"Rollback changed world revision/slots");
            const auto current=ReadPortableRuntimeObjectLookup();Need(current.objects==lookup.objects&&current.retainedBytes==lookup.retainedBytes,"Rollback changed lookup storage");
            GC::Collect();const auto currentGc=GC::GetStats();Need(currentGc.numObjects==gc.numObjects&&currentGc.memoryUsage==gc.memoryUsage,"Rollback changed GC ownership");};
        const auto call=[&](const std::string& who,std::array<float,3> at,const QuestVr::Vm::Limits& limits){
            return ExecutePortableActorFunction(who,"Engine.Actor.SetLocation",{{Value::Vector(at),{}}},limits);};
        const auto invalid=call(level,{std::numeric_limits<float>::quiet_NaN(),0,0},Limits());
        Need(invalid.status==QuestVr::Vm::Status::Invalid&&!invalid.committed&&invalid.writes==0,
            "Public canonical Vector guard must reject nonfinite input before native dispatch");restored();
        const auto fixed=call(level,{0,0,0},Limits());
        Need(fixed.passed()&&fixed.committed&&fixed.value.kind==Kind::Bool&&!fixed.value.boolean&&fixed.writes==0,
            "Static ineligible267 must not query/write: "+fixed.error+" writes="+std::to_string(fixed.writes));restored();
        auto target=original;target[2]+=8.f;
        auto zero=Limits();zero.writes=0;const auto failed=call(actor,target,zero);
        Need(failed.status==QuestVr::Vm::Status::Budget&&!failed.committed&&failed.error=="VM write limit","Native267 zero-write refusal failed: "+failed.error);restored();
        auto partial=Limits();partial.writes=4;const auto late=call(actor,target,partial);
        Need(late.status==QuestVr::Vm::Status::Budget&&!late.committed&&late.error=="VM write limit","Native267 partial vector-write refusal failed: "+late.error);restored();
        const auto moved=call(actor,target,Limits());
        Need(moved.passed()&&moved.committed&&moved.value.kind==Kind::Bool&&moved.value.boolean,"Actual267 clear relocation failed: "+moved.error);
        const auto placed=Read(actor,"Engine.Actor.Location").vector;
        Need(Equal(placed,target)&&Equal(Read(actor,"Engine.Actor.OldLocation").vector,target),"Native267 did not store full Location/OldLocation absolute vectors");
        Need(Read(actor,"Engine.Actor.bJustTeleported").boolean&&Read(actor,"Engine.Actor.Rotation").rotation==rotation&&
            Equal(Read(actor,"Engine.Actor.ColLocation").vector,col),"Native267 changed unrelated rotation/no-hash ColLocation or omitted teleport flag");
        Need(GetPortableRuntimeWorldRevision()==revision+1&&!Read(level,"Engine.LevelInfo.bBegunPlay").boolean,"Native267 commit failed revision/dormant-world ownership");
        Need(!ReadPortableRuntimeRetailCollision().initialized,"World-only native267 created a collision hash");
        const auto after=tmp.Save("committed");Need(after!=baseline,"Committed267 not captured in state");
        Need(LoadPortableRuntimeState((tmp.path/"baseline.sav").string()),"Cannot restore baseline checkpoint");
        Need(Equal(Read(actor,"Engine.Actor.Location").vector,original)&&Equal(Read(actor,"Engine.Actor.OldLocation").vector,old),"Checkpoint failed both native267 vectors");
        std::cout<<"PASS actual first Training native267: static inactive input, zero/partial-write rollback, real clear FindSpot/encroachment, absolute Location+OldLocation, teleport flag, one revision and checkpoint; checks="<<checks<<"\n";
        std::cout<<"NOT original Login completion, active-hash ownership, based-child callback integration, live gravity/melee/loot or headset acceptance.\n";
    }catch(...){ShutdownPortableRuntime();for(const auto& g:guards)g.Unchanged();throw;}
    ShutdownPortableRuntime();for(const auto& g:guards)g.Unchanged();std::cout<<"UNCHANGED owned sources="<<guards.size()<<"; first Training only\n";
}
}
int main(int argc,char** argv){try{Need(argc<=2,"Usage: portable_set_location_test [OWNED_ROOT]");const auto* env=std::getenv("DEUSEX_GAME_ROOT");
    const fs::path root=argc==2?fs::path(argv[1]):env?fs::path(env):fs::path{};
    if(root.empty()){std::cout<<"SKIP: supply owned game root for actual first Training267\n";return 77;}
    Run(fs::canonical(root));return 0;}catch(const std::exception& e){ShutdownPortableRuntime();std::cerr<<e.what()<<'\n';return 1;}}
