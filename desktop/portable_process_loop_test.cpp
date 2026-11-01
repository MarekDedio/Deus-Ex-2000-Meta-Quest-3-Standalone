#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_process_loop.h"
#include "portable_object_lookup.h"
#include "quest_save_bundle.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;using Control=QuestVr::ProcessLoopControl;
std::size_t checks{},refusals{};
void Check(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F&& operation) {
    bool refused{};try{operation();}catch(const std::exception&){refused=true;}
    Check(refused,"Missing/foreign process authority succeeded");++refusals;
}
struct Files {
    fs::path parent=fs::canonical(fs::temp_directory_path()),path;
    Files(){path=parent/("quest-process-loop-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Check(fs::create_directory(path),"Cannot create process-loop checkpoint directory");path=fs::canonical(path);}
    ~Files(){ShutdownPortableRuntime();std::error_code error;const auto resolved=fs::weakly_canonical(path,error);
        if(!error&&resolved==path&&resolved.parent_path()==parent&&resolved.filename().string().rfind("quest-process-loop-",0u)==0u)
            fs::remove_all(resolved,error);}
    std::vector<std::uint8_t> Save(const char* name)const {
        const auto file=path/name;Check(SavePortableRuntimeState(file.string()),"Cannot save process-loop baseline");
        std::vector<std::uint8_t> data;Check(QuestVr::ReadBoundedSaveFile(file.string(),QuestVr::kMaximumSaveRuntimeBytes,data),"Cannot read process-loop baseline");return data;
    }
};
}
int main(int argc,char** argv){try {
    if(argc==1){std::cout<<"SKIP owned process-loop binding: supply original GOTY root\n";return 77;}
    if(argc!=2)throw std::runtime_error("Usage: portable_process_loop_test OWNED_GAME_ROOT");
    const fs::path root=fs::canonical(argv[1]);ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();
    Files files;std::vector<PortablePackageTables> packages;
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"})
        packages.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
    const auto prepare=[&] {
        Check(InitializePortableRuntime(packages).passed,"Cannot initialize process-loop fixture runtime");
        Check(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
            LoadPortableRuntimeMap(map).passed,"Cannot prepare actual process-loop lookup/map generation");
    };
    auto control=std::make_shared<Control>();Refuse([&]{BindPortableRuntimeProcessLoop(control);});prepare();
    Refuse([]{static_cast<void>(ReadPortableRuntimeProcessLoop());});
    const auto absent=ProbePortableRuntimeProcessLoop();Check(!absent.execution.passed()&&!absent.completed,"Absent running authority became false");++refusals;
    Refuse([]{BindPortableRuntimeProcessLoop({});});
    BindPortableRuntimeProcessLoop(control);BindPortableRuntimeProcessLoop(control);
    const auto bound=ReadPortableRuntimeProcessLoop();Check(bound.identity==control->Identity()&&bound.lookupGeneration==ReadPortableRuntimeObjectLookup().generation&&
        !bound.running&&bound.state==Control::State::Ready&&bound.retainedBytes>=Control::RetainedBytes(),"Bound pre-loop identity/state/storage is not actual");
    Refuse([&]{BindPortableRuntimeProcessLoop(std::make_shared<Control>());});
    const auto before=files.Save("before.sav");const auto revision=GetPortableRuntimeWorldRevision();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto actors=ReadPortableWorldActorOrder();const auto gc=GC::GetStats();
    const auto unchanged=[&] {
        const auto current=ReadPortableRuntimeObjectLookup();const auto now=GC::GetStats();
        Check(std::tie(current.objects,current.retainedBytes,current.generation)==std::tie(lookup.objects,lookup.retainedBytes,lookup.generation)&&
            now.numObjects==gc.numObjects&&now.memoryUsage==gc.memoryUsage&&GetPortableRuntimeWorldRevision()==revision&&ReadPortableWorldActorOrder()==actors&&
            files.Save("after.sav")==before,"Process capability serialized or mutated save/lookup/Actor/GC/revision state");
    };
    const auto ready=ProbePortableRuntimeProcessLoop();Check(ready.execution.passed()&&ready.execution.committed&&ready.completed&&!ready.first&&!ready.second,"Ready false did not use real binding");unchanged();
    control->EnterExecution();const auto running=ProbePortableRuntimeProcessLoop();
    Check(running.execution.passed()&&running.completed&&running.first&&running.second&&running.identity==bound.identity&&running.lookupGeneration==bound.lookupGeneration,"Executing state not observed through actual runtime root");unchanged();
    const auto stop=ProbePortableRuntimeProcessLoop([&]{control->LeaveExecution();});
    Check(stop.execution.passed()&&stop.execution.committed&&stop.completed&&stop.first&&!stop.second,"Legitimate bool change was frozen or invalidated at publication");unchanged();
    const auto late=ProbePortableRuntimeProcessLoop({},true);Check(!late.execution.passed()&&late.completed&&late.execution.error=="Process loop diagnostic late failure; outer rollback required","Late rollback missed actual process observations");++refusals;unchanged();
    for(const auto cut:{std::size_t{0u},std::size_t{1u}}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=cut;const auto partial=ProbePortableRuntimeProcessLoop({},false,limits);
        Check(!partial.execution.passed()&&partial.execution.nativeWork&&partial.execution.nativeWork->exhausted,"Process observation bypassed shared work");++refusals;unchanged();
    }
    const auto rebind=ProbePortableRuntimeProcessLoop([&]{BindPortableRuntimeProcessLoop(control);});
    Check(!rebind.execution.passed()&&!rebind.completed,"Runtime binding changed inside a live root");++refusals;unchanged();
    const auto shutdown=ProbePortableRuntimeProcessLoop([]{ShutdownPortableRuntime();});
    Check(!shutdown.execution.passed()&&!shutdown.completed,"Runtime shutdown retired an active root");++refusals;unchanged();
    const auto revoked=ProbePortableRuntimeProcessLoop({},false,{},[&]{control->Revoke();});
    Check(!revoked.execution.passed()&&!revoked.execution.committed&&revoked.completed,"Commit published after actual loop capability revoke");++refusals;unchanged();
    Refuse([]{static_cast<void>(ReadPortableRuntimeProcessLoop());});
    Refuse([&]{BindPortableRuntimeProcessLoop(control);});
    ShutdownPortableRuntime();prepare();Refuse([]{static_cast<void>(ReadPortableRuntimeProcessLoop());});
    Refuse([&]{BindPortableRuntimeProcessLoop(control);});
    auto replacement=std::make_shared<Control>();BindPortableRuntimeProcessLoop(replacement);
    const auto next=ReadPortableRuntimeProcessLoop();Check(next.identity!=bound.identity&&next.lookupGeneration!=bound.lookupGeneration,"Reinitialized runtime reused stale binding generation");
    Check(UnloadPortableRuntimeMap()>0u&&LoadPortableRuntimeMap(map).passed,"Cannot replace only first Training map");
    const auto afterMap=ReadPortableRuntimeProcessLoop();Check(afterMap.identity==next.identity&&afterMap.lookupGeneration==next.lookupGeneration,"Map replacement invented a new process-loop lifetime");
    ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
    Check(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Process-loop shutdown leaked runtime GC ownership");
    std::cout<<"PASS owned process-loop binding checks="<<checks<<" refusals="<<refusals
        <<"; genuine runtime/lookup generation, external state changes, root/publication/revoke/rollback/map/reinit ownership. NOT original Input dispatch or Android hook timing\n";return 0;
}catch(const std::exception& error){ShutdownPortableRuntime();std::cerr<<"FAIL owned process-loop binding: "<<error.what()<<'\n';return 1;}}
