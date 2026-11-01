#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_input_exec.h"
#include "portable_process_loop.h"
#include "quest_save_bundle.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;
constexpr std::uint64_t Client=5011u,Viewport=5012u;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits limits;limits.nativeWork=QuestVr::NativeWork::MaximumLimit;return limits;}
struct Temporary {
    fs::path parent=fs::canonical(fs::temp_directory_path()),path;
    Temporary(){path=parent/("quest-input-exec-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(path),"Cannot create isolated input-exec fixture");path=fs::canonical(path);}
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;const auto actual=fs::weakly_canonical(path,error);
        if(!error&&actual==path&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-input-exec-",0u)==0u)
            fs::remove_all(actual,error);}
    fs::path Config(){const auto target=path/"User.ini";std::ofstream file(target,std::ios::binary);
        Require(bool(file),"Cannot create explicit non-commercial User.ini fixture");
        for(const auto* section:{"Engine.Input","Extension.InputExt"})file<<'['<<section<<"]\n"
            <<"Aliases[0]=(Command=\"KEYNAME 1|KEYNAME 2\",Alias=QuestLineProbe)\n"
            <<"Aliases[1]=(Command=\"//comment|KEYNAME 65|\",Alias=QuestCommentProbe)\n"
            <<"Aliases[2]=(Command=\"QuestRecursiveProbe|KEYNAME 66\",Alias=QuestRecursiveProbe)\n"
            <<"Aliases[3]=(Command=\"BUTTON bFire|KEYNAME 67\",Alias=QuestActionProbe)\n";
        file.close();Require(bool(file),"Cannot finish explicit input configuration");return target;}
    std::vector<std::uint8_t> Save(const char* name)const {const auto target=path/name;
        Require(SavePortableRuntimeState(target.string()),"Cannot capture complete dormant input checkpoint");
        std::vector<std::uint8_t> bytes;Require(QuestVr::ReadBoundedSaveFile(target.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read complete input checkpoint");return bytes;}
};
void Original(const fs::path& root,Temporary& temp) {
    std::vector<PortablePackageTables> packages;
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"})
        packages.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
    Require(InitializePortableRuntime(packages).passed&&PreparePortableRuntimeNativeEngineClasses().prepared&&
        PreparePortableRuntimeNativeMetadataClasses().prepared&&PreparePortableRuntimeNativeConfigClasses().prepared&&
        PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeNativeInputClasses().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        PreparePortableRuntimeInputSchema().prepared&&PreparePortableRuntimeInputConfig(temp.Config().string()).configApplied,
        "Cannot prepare actual configured native Input authority");
    auto client=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(client,Client);
    RegisterPortableRuntimeViewport(Client,{Viewport,1024u,1152u,3u,2u});
    Require(!PreparePortableRuntimePlayerObjects(Viewport).bound,"Input commands fixture fabricated a possessed player");
    const auto input=PreparePortableRuntimeViewportInput(Viewport,PortableInputLoggingProfile::Ordinary);
    Require(input.constructed&&!input.initialized&&!input.nativeViewportIndex&&input.action==0&&input.delta==0.0f&&
        input.aliases[0].alias=="QuestLineProbe"&&input.aliases[0].command=="KEYNAME 1|KEYNAME 2",
        "Configured alias fixture changed native constructor/Init state");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed,
        "Cannot load dormant first Training");
    const auto save=temp.Save("before.sav");const auto revision=GetPortableRuntimeWorldRevision();
    const auto index=ReadPortableRuntimeObjectLookup();const auto gc=GC::GetStats();
    const auto unchanged=[&]{GC::Collect();const auto now=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
        const auto current=ReadPortableRuntimeViewportInput(Viewport);
        Require(now.numObjects==gc.numObjects&&now.memoryUsage==gc.memoryUsage&&
            std::tie(index.objects,index.retainedBytes,index.generation)==std::tie(lookup.objects,lookup.retainedBytes,lookup.generation)&&
            GetPortableRuntimeWorldRevision()==revision&&temp.Save("after.sav")==save&&
            !current.initialized&&!current.nativeViewportIndex&&current.objectIndex==input.objectIndex&&current.aliases==input.aliases&&
            current.bindings==input.bindings&&current.keys==input.keys&&current.action==input.action&&current.delta==input.delta&&
            current.retainedBytes==input.retainedBytes&&!current.sourceSequence,
            "Input alias diagnostic changed complete save/lookup/GC/native protocol state");};
    const auto run=[&](const char* command,bool late=false,const QuestVr::Vm::Limits& limits=Limits()) {
        auto result=ProbePortableRuntimeInputCommands(Viewport,command,late,limits);unchanged();return result;};
    const auto properties=ProbePortableRuntimeInputProperties("DeusEx.JCDentonMale",Limits());unchanged();
    Require(properties.completed&&!properties.execution.committed&&properties.properties==28u&&
        properties.bytes==13u&&properties.floats==15u&&properties.execution.writes==0u&&
        properties.execution.error=="Original Input property metadata diagnostic complete; rollback required",
        "Actual player inherited Input cache failed before native actions: "+properties.execution.error);
    std::cout<<"Actual inherited player Input cache properties="<<properties.properties<<" bytes="<<properties.bytes
        <<" floats="<<properties.floats<<" work="<<properties.execution.nativeWork->used<<"; no fabricated Pawn/Init\n";
    const auto missing=run("QuestLineProbe");
    Require(!missing.completed&&!missing.execution.passed()&&!missing.execution.error.empty(),"Absent running capability became genuine false/success");++refusals;
    auto loop=std::make_shared<QuestVr::ProcessLoopControl>();BindPortableRuntimeProcessLoop(loop);
    const auto ready=run("QuestLineProbe");
    Require(ready.completed&&ready.command.handled&&ready.command.output.empty()&&
        ready.execution.error=="Original Input commands diagnostic complete; rollback required",
        "Real pre-loop false did not skip its command list");
    loop->EnterExecution();
    const auto first=run("QuestLineProbe");
    Require(first.completed&&first.command.handled&&first.command.output=="LeftMouseRightMouse"&&
        first.execution.error=="Original Input commands diagnostic complete; rollback required"&&!first.execution.committed,
        "Actual configured alias did not execute its two real native key queries: "+first.execution.error);
    const auto second=run("QuestLineProbe");
    Require(second.command.output==first.command.output&&second.execution.writes==first.execution.writes&&
        second.execution.nativeWork&&first.execution.nativeWork&&second.execution.nativeWork->used==first.execution.nativeWork->used,
        "Repeated native alias invocation changed state/work or left its recursion guard set");
    const auto comment=run("QuestCommentProbe");Require(comment.completed&&comment.command.output=="A","Comment/empty parsed line blocked later real key query");
    const auto recursive=run("QuestRecursiveProbe");Require(recursive.completed&&recursive.command.output=="B","Actual shared alias guard did not decline recursive line then continue");
    const auto late=run("QuestLineProbe",true);Require(late.completed&&late.command.output==first.command.output&&
        late.execution.error=="Original Input commands late failure after actual native dispatch","Late failure was swallowed or occurred before native output");++refusals;
    const auto action=run("QuestActionProbe");
    Require(!action.completed&&!action.execution.error.empty(),"Unprepared action/Init service was turned into a fake decline");++refusals;
    for(const auto cut:{std::size_t{0u},std::size_t{1u},first.execution.nativeWork->used/2u,first.execution.nativeWork->used-1u}) {
        auto limits=Limits();limits.nativeWork=cut;const auto result=run("QuestLineProbe",false,limits);
        Require(!result.completed&&result.execution.nativeWork&&result.execution.nativeWork->exhausted,
            "Actual native alias route escaped shared work limit");++refusals;
    }
    loop->LeaveExecution();const auto stopped=run("QuestLineProbe");
    Require(stopped.completed&&stopped.command.handled&&stopped.command.output.empty(),"Actual loop stop did not return genuine false");
    loop->Revoke();const auto revoked=run("QuestLineProbe");
    Require(!revoked.completed&&!revoked.execution.error.empty(),"Revoked loop capability was treated as genuine false");++refusals;
    const auto query=ProbePortableRuntimeInputQueryLease(Viewport,{},Limits());
    Require(query.completed&&query.execution.passed()&&query.execution.committed&&query.command.handled&&
        query.command.output=="LeftMouse","Actual native query-only lease baseline failed");unchanged();
    const auto revokedClient=ProbePortableRuntimeInputQueryLease(Viewport,[&]{client->Revoke();},Limits());
    Require(revokedClient.completed&&!revokedClient.execution.passed()&&!revokedClient.execution.committed&&
        revokedClient.command.output=="LeftMouse"&&revokedClient.execution.error.find("Native client lifetime revoked")!=std::string::npos,
        "Input-handled query published after its actual viewport client was revoked");++refusals;
    const auto stale=ProbePortableRuntimeInputQueryLease(Viewport,{},Limits());
    Require(!stale.completed&&!stale.execution.passed(),"Revoked native Input query became a neutral/missing success");++refusals;
    const auto afterRevoke=ReadPortableRuntimeObjectLookup();
    Require(GetPortableRuntimeWorldRevision()==revision&&temp.Save("revoked.sav")==save&&
        std::tie(index.objects,index.retainedBytes,index.generation)==std::tie(afterRevoke.objects,afterRevoke.retainedBytes,afterRevoke.generation),
        "Query-only lease failure changed saved world or native lookup state");
    std::cout<<"Actual constructed Input alias nativeWork="<<first.execution.nativeWork->used<<" writes="<<first.execution.writes
        <<"; original configured alias/ParseLine/key-query/output path, NOT initialized input/controller events\n";
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try {
    if(argc==1){std::cout<<"SKIP owned Input command route: supply original game root\n";return 77;}
    Require(argc==2,"Usage: portable_input_exec_test GAME_ROOT");ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();
    {Temporary temp;Original(fs::canonical(argv[1]),temp);}GC::Collect();const auto after=GC::GetStats();
    Require(initial.numObjects==after.numObjects&&initial.memoryUsage==after.memoryUsage,"Input command shutdown leaked ownership");
    std::cout<<"PASS owned constructed Input command route checks="<<checks<<" refusals="<<refusals
        <<"; actual configured alias/native queries/output and full rollback. NOT Quest mapping, Init/ReadInput/startup/gameplay\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL owned Input command route: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
