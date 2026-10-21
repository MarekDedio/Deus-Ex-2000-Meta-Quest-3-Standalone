#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Actual original map/header/linker storage, always rolled back. This does not
// establish deferred Actor deletion, cleanup, startup, usable loot or gravity.
namespace {
namespace fs=std::filesystem;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};

void Require(bool value,const std::string& why) {
    ++checks;
    if(!value)throw std::runtime_error(why);
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
    std::ifstream input(path,std::ios::binary);
    Require(bool(input),"Cannot fingerprint original source: "+path.string());
    std::array<char,16384u> buffer{};
    std::uint64_t hash=14695981039346656037ull;
    std::uintmax_t total{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto count=input.gcount();total+=static_cast<std::uintmax_t>(count);
        Require(total<=512u*1024u*1024u,"Original fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {
            hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash*=1099511628211ull;
        }
    }
    Require(input.eof(),"Original source fingerprint did not finish");return hash;
}
struct Source {
    fs::path path;
    std::uintmax_t size{};
    fs::file_time_type modified;
    std::uint64_t hash{};
    explicit Source(const fs::path& file):path(fs::canonical(file)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(Fingerprint(path)){}
    void Unchanged()const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==hash,
            "Owned source bytes/size/timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;
    std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,original),
            "Temporary checkpoints overlap original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-object-destroy-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& path:files)if(path.parent_path()==directory)fs::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    Bytes Save(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot capture full runtime checkpoint");Bytes bytes;
        Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read full runtime checkpoint");return bytes;
    }
};
struct Identity {
    std::string path,cls;
    std::uint32_t index{},classIndex{},outerIndex{};
    bool operator==(const Identity&)const=default;
};
std::vector<Identity> Actors() {
    std::vector<Identity> result;
    for(const auto& row:GetPortableRuntimeMapActors(true)) {
        const auto owner=ReadPortableRuntimeObjectOwnership(row.objectPath);
        result.push_back({row.objectPath,row.classPath,owner.objectIndex,owner.classIndex,owner.outerIndex});
    }
    return result;
}
Value Read(const std::string& path,const char* key) {
    return ReadPortableActorDeclaredScriptProperty(path,key);
}
void Completed(const PortableObjectDestroyProbeResult& result,const PortableObjectLinkerSnapshot& before,
    std::size_t writes) {
    Require(result.execution.passed()&&!result.execution.committed&&result.completed&&
        result.returned==1&&result.repeated==0&&result.execution.writes==writes,
        "Actual ConditionalDestroy/repeated fast path failed: "+result.execution.error);
    Require(result.before==before,"Probe initial native linker/header snapshot differs from public source witness");
    const auto& after=result.after;
    Require(after.bound&&!after.attached&&after.sourceExport&&after.linkerIdentity==0u&&
        after.sourceLinkerIdentity==before.sourceLinkerIdentity&&after.exportObjectPath.empty(),
        "UObject.Destroy did not detach the exact original source export attachment");
    Require(after.sourcePath==before.sourcePath&&after.sourceExportIndex==before.sourceExportIndex&&
        after.objectIndex==before.objectIndex&&after.outerIndex==before.outerIndex,
        "Native destruction erased authored provenance or retired UObject/Outer identity");
    Require(before.outerIndex<=static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())&&
        after.nativeIndex==static_cast<std::int32_t>(before.outerIndex),
        "UObject.Destroy did not replace linker index with the current Outer's actual object index");
    Require(after.flags==(before.flags|0x80800000u),
        "ConditionalDestroy/DebugDestroy flags discarded unrelated original header bits");
}
void Run(const fs::path& root) {
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    for(const auto* name:QuestVr::RuntimeScriptPackages) {
        const auto file=root/"System"/(std::string(name)+".u");
        sources.emplace_back(file);packages.push_back(LoadPortablePackageTables(file.string()));
    }
    sources.emplace_back(root/"Maps"/"00_Training.dx");
    const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(InitializePortableRuntime(packages,0x13579bdfu).passed,"Cannot initialize real original scripts");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&
        PreparePortableRuntimeNativeMetadataClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare actual native Class/Package/lookup ownership");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load ONLY first Training map");
    const auto authored=temporary.Save("Authored");
    const auto waiting=ExecutePortableActorFunction("00_Training.Doctor1","PlayWaiting");
    Require(waiting.passed()&&waiting.committed,"Cannot establish genuine unrelated Doctor animation clock");
    const auto before=temporary.Save("Before");const auto revision=GetPortableRuntimeWorldRevision();
    const auto actors=Actors();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto hash=ReadPortableRuntimeRetailCollision();GC::Collect();const auto gc=GC::GetStats();
    bool distinctCacheWitness{};
    for(const auto* leaf:{"PickupDistributor0","CrateBreakableMedGeneral1","CrateBreakableMedGeneral2"}) {
        const std::string path=std::string("00_Training.")+leaf;
        const auto original=ReadPortableRuntimeObjectLinker(path);
        const auto identity=ReadPortableRuntimeObjectOwnership(path);
        const auto sourceIndex=FindPortableExport(map,leaf);
        Require(original.bound&&original.attached&&original.sourceExport&&original.linkerIdentity&&
            original.linkerIdentity==original.sourceLinkerIdentity&&original.exportObjectPath==path,
            "Original Training Actor lacks an actual attached native source linker slot");
        Require(original.sourceExportIndex==sourceIndex&&
            original.nativeIndex==static_cast<std::int32_t>(sourceIndex)&&fs::equivalent(original.sourcePath,map.sourcePath),
            "Native linker index/source differs from independently decoded original export table");
        Require(original.objectIndex==identity.objectIndex&&original.outerIndex==identity.outerIndex&&
            original.outerIndex&&identity.outerPath=="00_Training",
            "Native linker cache fixture lacks real current map Package Outer identity");
        Require(original.flags==static_cast<std::uint32_t>(map.exports.at(sourceIndex).ObjFlags)&&
            !(original.flags&0x00800000u),"Original linker fixture has fabricated/destroyed header flags");
        distinctCacheWitness|=sourceIndex!=original.outerIndex&&sourceIndex!=original.objectIndex;
        const auto tag=Read(path,"Engine.Actor.Tag"),location=Read(path,"Engine.Actor.Location");
        const auto rollback=[&] {
            Require(ReadPortableRuntimeObjectLinker(path)==original,
                "ConditionalDestroy rollback lost native linker attachment/index/header storage");
            Require(temporary.Save("After")==before&&GetPortableRuntimeWorldRevision()==revision&&Actors()==actors,
                "ConditionalDestroy changed full checkpoint/revision/Actor slots or identities");
            const auto actual=ReadPortableRuntimeObjectLookup();const auto collision=ReadPortableRuntimeRetailCollision();
            Require(actual.prepared==lookup.prepared&&actual.objects==lookup.objects&&
                actual.retainedBytes==lookup.retainedBytes&&actual.generation==lookup.generation,
                "ConditionalDestroy retired/changed actual UObject weak lookup authority");
            PortableObjectLookupRequest request;request.name=path;
            const auto found=FindPortableRuntimeObject(request);
            Require(found&&found->objectIndex==identity.objectIndex&&found->classIndex==identity.classIndex&&
                found->outerIndex==identity.outerIndex,"ConditionalDestroy rollback changed exact original lookup identity");
            Require(collision.initialized==hash.initialized&&collision.enabled==hash.enabled&&
                collision.seed==hash.seed&&collision.tag==hash.tag,"ConditionalDestroy changed native hash/RNG state");
            Require(QuestVr::Vm::Equal(Read(path,"Engine.Actor.Tag"),tag)&&
                QuestVr::Vm::Equal(Read(path,"Engine.Actor.Location"),location),
                "ConditionalDestroy changed nonconstructor map payloads");
            GC::Collect();const auto current=GC::GetStats();
            Require(current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage,
                "ConditionalDestroy leaked or prematurely collected native UObjects");
        };
        const std::size_t writes=std::string(leaf)=="PickupDistributor0"?14u:8u;
        const auto full=ProbePortableActorConditionalDestroy(path);Completed(full,original,writes);rollback();
        for(std::size_t prefix=0u;prefix<writes;++prefix) {
            QuestVr::Vm::Limits limits;limits.writes=prefix;
            const auto partial=ProbePortableActorConditionalDestroy(path,limits);
            Require(!partial.execution.passed()&&!partial.execution.committed&&
                partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==prefix,
                "ConditionalDestroy failed outside exact caller VM write prefix: "+partial.execution.error);
            rollback();++refusals;
        }
        const auto warm=ProbePortableActorConditionalDestroy(path);Completed(warm,original,writes);rollback();
        Require(warm.execution.nativeWork&&warm.execution.nativeWork->used>1u&&warm.execution.nativeWork->used<=512u,
            "Warmed ConditionalDestroy has missing/unbounded native-work prefix census");
        const auto used=warm.execution.nativeWork->used;
        bool exportClearedBeforeLinker{},detachedBeforeCache{},cachedBeforeReturn{};
        for(std::size_t prefix=0u;prefix<used;++prefix) {
            QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
            const auto partial=ProbePortableActorConditionalDestroy(path,limits);
            Require(!partial.execution.passed()&&!partial.execution.committed&&partial.execution.nativeWork&&
                partial.execution.nativeWork->exhausted,"ConditionalDestroy ignored borrowed caller native-work prefix");
            const auto& observed=partial.after;
            exportClearedBeforeLinker|=observed.attached&&observed.exportObjectPath.empty();
            detachedBeforeCache|=!observed.attached&&observed.nativeIndex==-1;
            cachedBeforeReturn|=!observed.attached&&observed.nativeIndex==static_cast<std::int32_t>(original.outerIndex);
            rollback();++refusals;
        }
        Require(exportClearedBeforeLinker&&detachedBeforeCache&&cachedBeforeReturn,
            "Native-work failures did not witness original export-clear/linker/index/outer-cache ordering");
        std::cout<<"PASS actual "<<path<<": attached export "<<sourceIndex<<" -> detached/cache "<<original.outerIndex
            <<"; "<<writes<<" VM writes, "<<used<<" every native-work prefix, repeated ConditionalDestroy0; full native/storage rollback\n";
    }
    Require(distinctCacheWitness,"Actual fixtures did not distinguish source export, object and Outer cache indices");
    Require(LoadPortableRuntimeState((temporary.directory/"Authored.sav").string())&&
        temporary.Save("RestoredAuthored")==authored,"Cannot restore original map/Doctor clock baseline");
    ShutdownPortableRuntime();for(const auto& source:sources)source.Unchanged();
    std::cout<<"UNCHANGED "<<sources.size()<<" original sources; ONLY first Training loaded. "
        "No deferred deletion/cleanup, complete startup, usable loot, gravity or VR acceptance claim.\n";
}
}
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        Require(argc<=2,"Usage: portable_object_destroy_test [OWNED_GAME_ROOT]");
        const auto* environment=std::getenv("DEUSEX_GAME_ROOT");
        const fs::path root=argc==2?fs::path(argv[1]):environment?fs::path(environment):fs::path{};
        if(root.empty()){std::cout<<"SKIP owned first-Training native object destruction: supply source root\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();
        Run(fs::canonical(root));ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,
            "Shutdown leaked native object/linker state");
        std::cout<<"PASS real first-Training native object destruction: "<<checks<<" checks / "<<refusals<<" refusal prefixes\n";
        return 0;
    }catch(const std::exception& error) {
        ShutdownPortableRuntime();std::cerr<<"FAIL owned native object destruction: "<<error.what()<<'\n';return 1;
    }
}
