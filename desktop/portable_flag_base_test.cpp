#include "Precomp.h"
#include "GC/GC.h"
#include "portable_flag_base.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_viewport_input.h"
#include "quest_save_bundle.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <tuple>
namespace {
namespace fs=std::filesystem;std::size_t checks{},refusals{};
void Require(bool v,const std::string& why){++checks;if(!v)throw std::runtime_error(why);}
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits limits;limits.nativeWork=QuestVr::NativeWork::MaximumLimit;return limits;}
struct Files {
    fs::path parent=fs::canonical(fs::temp_directory_path()),directory;
    Files(){directory=parent/("quest-flag-base-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(directory),"Cannot create owned FlagBase temporary folder");directory=fs::canonical(directory);}
    ~Files(){std::error_code ec;if(directory.parent_path()==parent&&directory.filename().string().rfind("quest-flag-base-",0u)==0u)fs::remove_all(directory,ec);}
    std::vector<std::uint8_t> Save(const char* leaf)const{const auto path=directory/leaf;
        Require(SavePortableRuntimeState(path.string()),"Cannot capture FlagBase baseline checkpoint");std::vector<std::uint8_t> bytes;
        Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read checkpoint bytes");return bytes;}
};
void Cases(const Files& files){
    const auto initial=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();const auto revision=GetPortableRuntimeWorldRevision();
    const auto before=files.Save("Before.sav");
    const auto unchanged=[&]{const auto now=GC::GetStats();const auto current=ReadPortableRuntimeObjectLookup();
        Require(now.numObjects==initial.numObjects&&now.memoryUsage==initial.memoryUsage,"FlagBase rollback leaked GC ownership");
        Require(std::tie(current.objects,current.retainedBytes,current.generation)==std::tie(lookup.objects,lookup.retainedBytes,lookup.generation),"FlagBase rollback changed actual weak lookup");
        Require(GetPortableRuntimeWorldRevision()==revision&&ReadPortableRuntimeFlagBases().empty()&&files.Save("After.sav")==before,"FlagBase rollback retained native state/counter/save/revision");};
    const std::vector<std::pair<std::string,std::uint8_t>> queries{{"SKTemp_SkillPointsAvail",2u},{"SKTemp_SkillPointsTotal",2u},{"None",0u},{"",255u}};
    std::optional<PortableFlagBaseSnapshot> baseline;
    for(unsigned repeat=0u;repeat<2u;++repeat){
        const auto probe=ExecutePortableRuntimeFlagBaseProbe("00_Training.LevelInfo0",queries,PortableCoreExecMode::RollbackOnly,Limits());
        Require(probe.completed&&!probe.execution.passed(),"Original FlagBase constructor/query did not complete rollback diagnostic: "+probe.execution.error);
        Require(probe.object.constructorCompleted&&probe.object.defaultCopyVerified&&!probe.object.objectFlags&&probe.object.occupiedBuckets==0u&&
            probe.object.outerPath=="00_Training.LevelInfo0"&&probe.object.classPath=="Extension.FlagBase"&&probe.object.objectIndex,
            "Actual FlagBase constructor violated flags/Outer/class/default/bucket authority");
        Require(probe.matches.size()==queries.size()&&std::none_of(probe.matches.begin(),probe.matches.end(),[](bool v){return v;}),"Empty original FlagBase has fabricated flags");
        if(baseline)Require(*baseline==probe.object,"Rollback changed genuine class counter/MakeUnique/index lease");else baseline=probe.object;
        unchanged();
    }
    for(const auto cut:{0u,1u,100u}){auto limits=Limits();limits.nativeWork=cut;
        const auto refused=ExecutePortableRuntimeFlagBaseProbe("00_Training.LevelInfo0",queries,PortableCoreExecMode::Commit,limits);
        Require(!refused.execution.passed(),"FlagBase work-cut committed");++refusals;unchanged();}
    for(const auto* invalid:{"","@ClassDefault:Engine.Actor","Extension.FlagBase","UnknownOuter"}){
        const auto refused=ExecutePortableRuntimeFlagBaseProbe(invalid,queries,PortableCoreExecMode::Commit,Limits());
        Require(!refused.execution.passed(),"FlagBase accepted invalid actual Outer");++refusals;unchanged();}
    auto committed=ExecutePortableRuntimeFlagBaseProbe("00_Training.LevelInfo0",queries,PortableCoreExecMode::Commit,Limits());
    Require(committed.execution.passed()&&committed.completed,"Actual FlagBase commit failed: "+committed.execution.error);
    const auto live=ReadPortableRuntimeFlagBases();Require(live.size()==1u&&live[0]==committed.object,"Committed FlagBase lacks actual indexed inventory");
    Require(ReadPortableRuntimeDeclaredScriptProperty(live[0].path,"Core.Object.Outer").text==live[0].outerPath&&
        ReadPortableRuntimeDeclaredScriptProperty(live[0].path,"Extension.FlagBase.hashTable",63u).text.empty()&&
        ReadPortableRuntimeDeclaredScriptProperty(live[0].path,"Extension.FlagBase.defaultFlagExpiration").integer==0,"Typed FlagBase fields/header lost native storage");
    Require(GetPortableRuntimeUnsavedScriptState()&&!SavePortableRuntimeState((files.directory/"MustRefuse.sav").string()),"Save silently dropped native FlagBase ownership");
    Require(!fs::exists(files.directory/"MustRefuse.sav"),"Unsupported save created an output file");++refusals;
    bool travelRefused{};try{static_cast<void>(UnloadPortableRuntimeMap());}catch(const std::runtime_error&){travelRefused=true;}
    Require(travelRefused,"Travel silently dropped native FlagBase ownership");++refusals;
    Require(committed.diagnosticLifetime!=nullptr,"Standalone diagnostic did not expose its explicit temporary owner");
    committed.diagnosticLifetime.reset();GC::Collect();
    Require(ReadPortableRuntimeFlagBases().empty(),"Weak FlagBase inventory became a hidden permanent root");
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];else if(argc==1){if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
    else throw std::runtime_error("Usage: portable_flag_base_test [--game-root OWNED_GAME_ROOT]");
    if(root.empty()){std::cout<<"SKIP owned FlagBase: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
    ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();std::vector<PortablePackageTables> packages;
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"})
        packages.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize original FlagBase runtime");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeNativeInputClasses().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        PreparePortableRuntimeInputSchema().prepared&&PreparePortableRuntimeFlagBaseClasses().prepared&&LoadPortableRuntimeMap(map).passed,"Cannot prepare real FlagBase native schema/default/map");
    Require(PreparePortableRuntimeFlagBaseClasses().prepared,"FlagBase preparation not idempotent");Files files;Cases(files);
    ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
    Require(initial.numObjects==final.numObjects&&initial.memoryUsage==final.memoryUsage,"Shutdown leaked FlagBase/Outer/class ownership");
    std::cout<<"PASS owned FlagBase constructor/query checks="<<checks<<" refusals="<<refusals<<"; actual nonActor allocation,CDO-copy,flags0,Outer,lookup/index/counter rollback,CheckFlag emptytable,explicit save/travel guards. NOT Flag/FlagInt node construction or full player startup\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL owned FlagBase: "<<e.what()<<'\n';ShutdownPortableRuntime();return 1;}}
