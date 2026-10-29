#include "Precomp.h"
#include "GC/GC.h"
#include "portable_string_to_name.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_viewport_input.h"
#include "quest_save_bundle.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>

namespace {
namespace fs=std::filesystem;
using QuestVr::Vm::Kind;using QuestVr::Vm::Value;
std::size_t checks{},refusals{};
constexpr const char* Receiver="@ClassDefault:Extension.ExtensionObject";
void Require(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
std::uint64_t Hash(const fs::path& path){
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read owned source");
    std::uint64_t hash=14695981039346656037ull;std::array<char,65536u> bytes{};
    while(file){file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        for(std::streamsize i=0;i<file.gcount();++i){hash^=static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    Require(file.eof()&&!file.bad(),"Owned source read failed");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type stamp;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),stamp(fs::last_write_time(path)),hash(Hash(path)){}
    void Unchanged()const{Require(fs::file_size(path)==size&&fs::last_write_time(path)==stamp&&Hash(path)==hash,"Original source changed: "+path.string());}
};
struct Temporary {
    fs::path parent,directory;
    Temporary():parent(fs::canonical(fs::temp_directory_path())),directory(parent/(
        "quest-original-string-name-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))){
        Require(fs::create_directory(directory),"Cannot create isolated checkpoint directory");directory=fs::canonical(directory);}
    ~Temporary(){std::error_code ignored;
        if(directory.parent_path()==parent&&directory.filename().string().rfind("quest-original-string-name-",0u)==0u)
            fs::remove_all(directory,ignored);}
    std::vector<std::uint8_t> Save(const char* leaf)const{
        Require(std::string(leaf)=="Before.sav"||std::string(leaf)=="After.sav","Invalid isolated checkpoint leaf");
        const auto path=directory/leaf;Require(SavePortableRuntimeState(path.string()),"Cannot capture whole world checkpoint");
        std::vector<std::uint8_t> bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read world checkpoint");return bytes;
    }
};
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits limits;limits.nativeWork=QuestVr::NativeWork::MaximumLimit;return limits;}
struct Baseline {
    const Temporary& files;std::vector<std::uint8_t> save;PortableObjectLookupState lookup;GCStats gc;std::uint64_t revision{};
    explicit Baseline(const Temporary& value):files(value),save(files.Save("Before.sav")),lookup(ReadPortableRuntimeObjectLookup()),gc(GC::GetStats()),revision(GetPortableRuntimeWorldRevision()){}
    void Unchanged()const{
        const auto now=ReadPortableRuntimeObjectLookup();const auto current=GC::GetStats();
        Require(std::tie(now.prepared,now.generation,now.objects,now.retainedBytes)==
            std::tie(lookup.prepared,lookup.generation,lookup.objects,lookup.retainedBytes),"Name call changed UObject lookup ownership");
        Require(current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage,"Name call changed native GC ownership");
        Require(GetPortableRuntimeWorldRevision()==revision&&files.Save("After.sav")==save,"Name call changed world save/revision");
    }
};
PortableStringToNameResult Call(const std::string& value,bool indexed=false,PortableCoreExecMode mode=PortableCoreExecMode::Commit,
    const QuestVr::Vm::Limits& limits=Limits()){
    return ExecutePortableRuntimeStringToName(Receiver,Value::Text(Kind::String,value),mode,limits,indexed);
}
void Success(const PortableStringToNameResult& value,const std::string& canonical,int identity=-1){
    Require(value.completed&&value.execution.passed(),"Original native1024 failed: "+value.execution.error);
    Require(value.canonicalName==canonical&&value.execution.value.kind==Kind::Name&&value.execution.value.text==canonical,"Original name result lost first spelling");
    const auto found=NameString::FindCompareIndex(canonical);
    Require(found&&*found==value.compareIndex&&(identity<0||value.compareIndex==identity),"Name result has no actual shared arena identity");
}
void Refusal(const PortableStringToNameResult& value,const char* reason){
    Require(!value.completed&&!value.execution.passed(),reason);++refusals;
}
void Cases(const Temporary& files){
    Baseline baseline(files);
    Require(!NameString::FindCompareIndex("QuestOriginalNameMixedCaseProbe"),"New-name fixture already exists in original arena");
    auto incrementalLimit=Limits();incrementalLimit.nativeWork=8192u;
    const auto fresh=Call("QuestOriginalNameMixedCaseProbe",false,PortableCoreExecMode::Commit,incrementalLimit);
    Success(fresh,"QuestOriginalNameMixedCaseProbe");
    // CommittedGrowthBytes is NONNEGATIVE NET arena growth, not a per-name
    // allocation counter. Replacing an overallocated Names buffer may shrink
    // its spare capacity even while actual new names/nodes are admitted.
    Require(fresh.forwardNativeWork>0u&&fresh.forwardNativeWork<=8192u&&fresh.processNameCreditBytes>=fresh.arenaGrowthBytes,
        "New name traversal/growth credit was not charged");
    std::cout<<"FIRST NAME work="<<fresh.forwardNativeWork<<" netGrowth="<<fresh.arenaGrowthBytes<<" credit="<<fresh.processNameCreditBytes<<'\n';
    baseline.Unchanged();
    Success(Call("questoriginalnamemixedcaseprobe",true),"QuestOriginalNameMixedCaseProbe",fresh.compareIndex);baseline.Unchanged();
    Success(Call("QUESTORIGINALNAMEMIXEDCASEPROBE"),"QuestOriginalNameMixedCaseProbe",fresh.compareIndex);baseline.Unchanged();
    for(const auto* value:{"","None","nOnE","NONE"}){Success(Call(value,true),"None",0);baseline.Unchanged();}
    Success(Call("Digits01234_Underscore-Hyphen",true),"Digits01234_Underscore-Hyphen");baseline.Unchanged();
    const std::string longName="QuestOriginalLong"+std::string(128u,'x');Success(Call(longName),longName);baseline.Unchanged();
    const std::string terminated=std::string("QuestOriginalNulProbe")+std::string(1u,'\0')+".invalid after terminator";
    Success(Call(terminated,true),"QuestOriginalNulProbe");baseline.Unchanged();
    for(const auto& bad:std::array<std::string,4u>{"QuestOriginal.Bad","QuestOriginal Bad","QuestOriginal:Bad",std::string("QuestOriginal\xc3\xa9")}){
        const auto before=NameString::FindCompareIndex(bad);const auto refused=Call(bad,true);
        Refusal(refused,"Invalid original CString bypassed actual GError boundary");
        Require(refused.execution.error.find("GError")!=std::string::npos&&NameString::FindCompareIndex(bad)==before,"Invalid name was interned or failed at unrelated boundary");baseline.Unchanged();}
    Refusal(ExecutePortableRuntimeStringToName(Receiver,Value::Integer(3),PortableCoreExecMode::Commit,Limits()),"NonString argument accepted");baseline.Unchanged();
    for(const bool indexed:{false,true})for(unsigned shape=0u;shape<3u;++shape){
        const auto candidate="QuestOriginalContainerTail"+std::to_string(shape)+(indexed?"Indexed":"Declared");
        auto malformed=Value::Text(Kind::String,candidate);
        if(shape==0u)malformed.fields.emplace("hidden",Value::Text(Kind::String,std::string(262144u,'x')));
        else {malformed.intArray.reserve(262144u);if(shape==1u)malformed.intArray.push_back(71);}
        const auto rejected=ExecutePortableRuntimeStringToName(Receiver,malformed,PortableCoreExecMode::Commit,Limits(),indexed);
        Refusal(rejected,"String's hidden container tail was copied before admission");
        Require(rejected.execution.error.find("without container tails")!=std::string::npos&&
            !NameString::FindCompareIndex(candidate),"Malformed String failed after name publication or at unrelated boundary");baseline.Unchanged();
    }
    for(const bool indexed:{false,true}){
        auto limited=Limits();limited.stringBytes=4u;
        const auto rejected=Call("QuestOriginalStringByteLimit",indexed,PortableCoreExecMode::Commit,limited);
        Refusal(rejected,"Indexed/declared String input bypassed the VM byte limit");
        Require(!NameString::FindCompareIndex("QuestOriginalStringByteLimit"),"Oversize String was interned before limit admission");baseline.Unchanged();
    }
    Refusal(ExecutePortableRuntimeStringToName("@ClassDefault:Engine.Actor",Value::Text(Kind::String,"WrongReceiverName"),PortableCoreExecMode::Commit,Limits(),true),"NonExtensionObject receiver accepted");
    Require(!NameString::FindCompareIndex("WrongReceiverName"),"Wrong receiver published a name");baseline.Unchanged();
    const auto rollback=Call("QuestOriginalWorldRollbackName",true,PortableCoreExecMode::RollbackOnly);
    Require(rollback.completed&&!rollback.execution.passed()&&rollback.processNameCreditBytes>=fresh.processNameCreditBytes&&
        NameString::FindCompareIndex(rollback.canonicalName)==rollback.compareIndex,"World rollback lost successful process-lifetime name identity");baseline.Unchanged();
    const auto retained=Call("questoriginalworldrollbackname");Success(retained,"QuestOriginalWorldRollbackName",rollback.compareIndex);
    Require(retained.processNameCreditBytes>=rollback.processNameCreditBytes,"Later root refunded process name credit");baseline.Unchanged();
    Require(retained.processNameCreditBytes>0u,"Positive NET name arena growth never entered the process-lifetime ledger");
    for(const auto amount:{0u,1u,100u}){
        const auto candidate="QuestOriginalWorkCut"+std::to_string(amount);auto limits=Limits();limits.nativeWork=amount;
        Refusal(Call(candidate,true,PortableCoreExecMode::Commit,limits),"Native work cut published a name");
        Require(!NameString::FindCompareIndex(candidate),"Failed staging changed real arena identity");baseline.Unchanged();}
    auto tiny=Limits();tiny.retainedBytes=64u;
    Refusal(Call("QuestOriginalVmByteCut",false,PortableCoreExecMode::Commit,tiny),"VM retained-byte cut accepted call");baseline.Unchanged();
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
    else if(argc==1){if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
    else throw std::runtime_error("Usage: portable_string_to_name_test [--game-root OWNED_GAME_ROOT]");
    if(root.empty()){std::cout<<"SKIP owned StringToName: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
    root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();
    Refusal(Call("QuestOriginalBeforeRuntime"),"Name call without original runtime accepted");
    std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}){
        sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"System"/"DeusEx.ini");sources.emplace_back(root/"Maps"/"00_Training.dx");
    const auto map=LoadPortablePackageTables(sources.back().path.string());Temporary files;
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize original StringToName runtime");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        PreparePortableRuntimeInputSchema().prepared&&LoadPortableRuntimeMap(map).passed,"Cannot prepare original metadata/firstTraining");
    Cases(files);ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
    Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Shutdown retained StringToName GC ownership");
    for(const auto& source:sources)source.Unchanged();
    std::cout<<"PASS owned StringToName: "<<checks<<" checks / "<<refusals<<" refusals; original declared/indexed dispatch, canonical real name identity, None/NUL/long names, GError boundary, world save/revision/lookup/GC unchanged, explicit process-lifetime growth. NOT complete startup or live Quest gameplay\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL owned StringToName: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
