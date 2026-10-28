#include "quest_config_file_backend.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>

namespace {bool denyAllocations{};}
#if defined(__GNUC__)
#define CONFIG_ALLOC_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define CONFIG_ALLOC_NOINLINE __declspec(noinline)
#else
#define CONFIG_ALLOC_NOINLINE
#endif
CONFIG_ALLOC_NOINLINE void* operator new(std::size_t size) {
    if(denyAllocations)throw std::bad_alloc();
    if(auto* value=std::malloc(size?size:1u))return value;
    throw std::bad_alloc();
}
CONFIG_ALLOC_NOINLINE void* operator new[](std::size_t size){return ::operator new(size);}
CONFIG_ALLOC_NOINLINE void operator delete(void* value) noexcept {std::free(value);}
CONFIG_ALLOC_NOINLINE void operator delete[](void* value) noexcept {std::free(value);}
CONFIG_ALLOC_NOINLINE void operator delete(void* value,std::size_t) noexcept {std::free(value);}
CONFIG_ALLOC_NOINLINE void operator delete[](void* value,std::size_t) noexcept {std::free(value);}
#undef CONFIG_ALLOC_NOINLINE

namespace {
using namespace QuestVr::ConfigFile;
unsigned checks{},refusals{};
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class Action>void Refuse(Action&& action,const char* message) {
    bool refused{};try{action();}catch(const std::exception&){refused=true;++refusals;}
    Require(refused,message);
}
Bytes Text(std::string_view value){return Bytes(value.begin(),value.end());}
void WriteFixture(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream output(path,std::ios::binary|std::ios::trunc);Require(bool(output),"Cannot create generated fixture leaf");
    output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    output.close();Require(bool(output),"Generated fixture write failed");
}
Bytes RawRead(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);Require(bool(input),"Cannot read generated fixture leaf");
    const auto size=input.tellg();Require(size>=0&&static_cast<std::uint64_t>(size)<=MaximumBytes,"Fixture read bound exceeded");
    Bytes result(static_cast<std::size_t>(size));input.seekg(0);
    input.read(reinterpret_cast<char*>(result.data()),size);Require(bool(input),"Generated fixture read truncated");return result;
}
struct Fixture {
    std::filesystem::path root,app,seedDirectory,seed;
    Bytes initial=Text("[Extension.InputExt]\r\nT=\r\n");
    std::filesystem::file_time_type initialTime;
    Fixture() {
        const auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;attempt<20u;++attempt) {
            const auto proposed=parent/("deusex-owned-config-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if(std::filesystem::create_directory(proposed)){root=std::filesystem::canonical(proposed);break;}
        }
        Require(!root.empty()&&root.parent_path()==parent&&root.filename().string().rfind("deusex-owned-config-",0u)==0u,
            "Could not create bounded isolated fixture root");
        app=root/"application";seedDirectory=root/"source";seed=seedDirectory/"User.ini";
        Require(std::filesystem::create_directory(app)&&std::filesystem::create_directory(seedDirectory),"Cannot create fixture subdirectories");
        WriteFixture(seed,initial);initialTime=std::filesystem::last_write_time(seed);
    }
    std::filesystem::path Target() const{return app/"Config"/"User.ini";}
    std::filesystem::path Stage() const{return app/"Config"/"User.ini.stage";}
    void UnchangedSeed() const {
        Require(RawRead(seed)==initial&&std::filesystem::last_write_time(seed)==initialTime,"Backend changed the immutable seed bytes/mtime");
    }
    ~Fixture() {
        // Only known generated leaves/directories, never recursive removal.
        std::error_code ignored;
        for(const auto* name:{"User.ini.stage","User.ini.lock","User.ini","foreign.ini"})std::filesystem::remove(app/"Config"/name,ignored);
        for(const auto* name:{"source-link","application-link"})std::filesystem::remove(root/name,ignored);
        std::filesystem::remove(seed,ignored);std::filesystem::remove(app/"Config",ignored);
        std::filesystem::remove(seedDirectory,ignored);std::filesystem::remove(app,ignored);std::filesystem::remove(root,ignored);
    }
};
struct Fault {
    FaultPoint denied{};bool armAllocationDenial{};
    static bool Allow(void* context,FaultPoint point) noexcept {
        auto& value=*static_cast<Fault*>(context);
        if(value.armAllocationDenial&&point==FaultPoint::BeforePublish)denyAllocations=true;
        return point!=value.denied;
    }
    Faults Hooks(){return {this,Allow};}
};

void RoundTripAndRollback() {
    Fixture fixture;
    {
        auto backend=Backend::Bind(fixture.app,fixture.seed);
        const auto first=backend.Read();Require(!first.Owned()&&first.Generation()==0u&&first.Data()==fixture.initial,"First read did not use the exact read-only seed");
        Require(!std::filesystem::exists(fixture.Target())&&!std::filesystem::exists(fixture.Stage()),"Read created a config/stage");
        Refuse([&]{static_cast<void>(backend.Prepare(Text("new"),first));},"Default rollback-only preparation wrote to disk");
        Require(std::filesystem::is_empty(backend.Directory()),"Rollback-only mode created a lock/stage/target");
        {
            auto pending=backend.Prepare(Text("not committed"),first,PublicationMode::CommitAllowed);
            Require(std::filesystem::exists(fixture.Stage())&&!std::filesystem::exists(fixture.Target()),"Preparation published before commit");
            Refuse([&]{static_cast<void>(backend.Read());},"Nested read blocked instead of refusing a held backend");
        }
        Require(!std::filesystem::exists(fixture.Stage())&&!std::filesystem::exists(fixture.Target()),"Prepared destructor retained an owned stage or published target");
        const Bytes binary{0xffu,0xfeu,'[',0u,'x',0u,']',0u,13u,0u,10u,0u,0u,0u};
        auto pending=backend.Prepare(binary,first,PublicationMode::CommitAllowed);
        const auto result=pending.Publish();Require(result.disposition==Disposition::Durable&&result.generation==1u,"First publication did not report durability/generation");
        Require(pending.Publish().disposition==result.disposition&&pending.Publish().generation==result.generation,"Repeated publication lost its authoritative committed disposition");
        pending.Cancel();const auto second=backend.Read();
        Require(second.Owned()&&second.Data()==binary&&second.Generation()==1u,"Committed raw bytes/generation did not reopen");
        Refuse([&]{static_cast<void>(backend.Prepare(Text("stale"),first,PublicationMode::CommitAllowed));},"Stale same-binding snapshot was accepted");
        Require(!std::filesystem::exists(fixture.Stage()),"Stale snapshot created a stage");
        auto empty=backend.Prepare({},second,PublicationMode::CommitAllowed);Require(empty.Publish().Published(),"Empty native INI could not be published");empty.Cancel();
        Require(backend.Read().Data().empty(),"Empty raw config was not preserved");
        Require(backend.RetainedBytes()>sizeof(Backend)&&second.RetainedBytes()>sizeof(Snapshot)&&backend.PreparePeakBytes(0u,second)>0u,
            "Backend/snapshot/preparation storage admission omitted owned storage");
        fixture.UnchangedSeed();
    }
    {
        auto cold=Backend::Bind(fixture.app,fixture.seed);const auto reopened=cold.Read();
        Require(reopened.Owned()&&reopened.Data().empty()&&reopened.Generation()==0u,"Cold binding lost target or invented persisted session generation");
    }
    fixture.UnchangedSeed();
}
void FailureBoundaries() {
    for(const auto point:{FaultPoint::CreateStage,FaultPoint::WriteStage,FaultPoint::FlushStage,FaultPoint::Readback,FaultPoint::BeforePublish,
        FaultPoint::AfterPublish,FaultPoint::DirectoryFlush}) {
        Fixture fixture;
        {
            auto backend=Backend::Bind(fixture.app,fixture.seed);const auto before=backend.Read();const auto next=Text("[x]\r\nk=value\r\n");
            Fault injected{point};
            if(point==FaultPoint::CreateStage||point==FaultPoint::WriteStage||point==FaultPoint::FlushStage||point==FaultPoint::Readback) {
                Refuse([&]{static_cast<void>(backend.Prepare(next,before,PublicationMode::CommitAllowed,injected.Hooks()));},"Injected preparation failure was accepted");
                Require(!std::filesystem::exists(fixture.Target())&&!std::filesystem::exists(fixture.Stage()),"Preparation failure leaked stage/target");
            }else {
                auto pending=backend.Prepare(next,before,PublicationMode::CommitAllowed,injected.Hooks());const auto result=pending.Publish();pending.Cancel();
                if(point==FaultPoint::BeforePublish)Require(!result.Published()&&!std::filesystem::exists(fixture.Target()),"Pre-rename failure published or reported publication");
                else Require(result.disposition==Disposition::PublishedDurabilityUnknown&&result.generation==1u&&RawRead(fixture.Target())==next,
                    "Post-rename durability failure incorrectly requested memory rollback");
                Require(!std::filesystem::exists(fixture.Stage()),"Publication failure retained an owned stage");
            }
            fixture.UnchangedSeed();
        }
    }
    Fixture fixture;
    {
        auto backend=Backend::Bind(fixture.app,fixture.seed);const auto before=backend.Read();
        // A never-used invalid enum value disables point rejection and arms the
        // global allocator immediately before rename. Publish must not allocate.
        Fault injected{static_cast<FaultPoint>(255u),true};
        auto pending=backend.Prepare(Text("no allocation after preparation"),before,PublicationMode::CommitAllowed,injected.Hooks());
        const auto result=pending.Publish();denyAllocations=false;
        Require(result.disposition==Disposition::Durable&&result.generation==1u,"Publication allocated or threw after its last preparation boundary");
        pending.Cancel();fixture.UnchangedSeed();
    }
}
void ConcurrentBindingsAndStaleDisk() {
    Fixture fixture;
    {
        auto first=Backend::Bind(fixture.app,fixture.seed),other=Backend::Bind(fixture.app,fixture.seed);
        const auto a=first.Read(),b=other.Read();
        Refuse([&]{static_cast<void>(other.Prepare(Text("foreign authority"),a,PublicationMode::CommitAllowed));},"Another binding's snapshot was accepted");
        auto pending=first.Prepare(Text("first"),a,PublicationMode::CommitAllowed);
        Refuse([&]{static_cast<void>(other.Prepare(Text("second"),b,PublicationMode::CommitAllowed));},"Concurrent publisher ignored its native file lock");
        pending.Cancel();auto replacement=other.Prepare(Text("second"),b,PublicationMode::CommitAllowed);
        Require(replacement.Publish().Published(),"Publisher could not acquire released writer lock");replacement.Cancel();
        Refuse([&]{static_cast<void>(first.Prepare(Text("stale disk"),a,PublicationMode::CommitAllowed));},"Cross-binding disk change was overwritten by stale snapshot");
        const auto current=first.Read();WriteFixture(fixture.Target(),Text("external completed edit"));
        Refuse([&]{static_cast<void>(first.Prepare(Text("stale existing"),current,PublicationMode::CommitAllowed));},"Completed external edit was overwritten");
        const auto latest=first.Read();auto staged=first.Prepare(Text("proposal"),latest,PublicationMode::CommitAllowed);
        WriteFixture(fixture.Target(),Text("edit after preparation"));Require(!staged.Publish().Published(),"Publish skipped final target-version validation");staged.Cancel();
        Require(RawRead(fixture.Target())==Text("edit after preparation")&&!std::filesystem::exists(fixture.Stage()),"Version conflict damaged target or leaked stage");
        fixture.UnchangedSeed();
    }
}
void MoveAndRecoveryOwnership() {
    Fixture firstFixture,secondFixture;
    {
        auto first=Backend::Bind(firstFixture.app,firstFixture.seed),second=Backend::Bind(secondFixture.app,secondFixture.seed);
        auto pending=first.Prepare(Text("discarded by move assignment"),first.Read(),PublicationMode::CommitAllowed);
        auto replacement=second.Prepare(Text("moved publication"),second.Read(),PublicationMode::CommitAllowed);
        pending=std::move(replacement);
        Require(!std::filesystem::exists(firstFixture.Stage())&&std::filesystem::exists(secondFixture.Stage()),"Move assignment leaked an old owned stage");
        Require(pending.Publish().Published(),"Moved prepared publication lost authority");pending.Cancel();
        const auto committed=second.Read();Require(committed.Owned()&&committed.Data()==Text("moved publication"),"Move published into the wrong backend");
        WriteFixture(secondFixture.Stage(),Text("abandoned uncommitted proposal"));
        Require(second.DiscardAbandonedStage(PublicationMode::CommitAllowed)&&second.Read().Data()==committed.Data()&&
            second.Read().Generation()==committed.Generation(),"Explicit abandoned-stage recovery changed committed target/generation");
        Require(first.Read().Data()==firstFixture.initial&&!std::filesystem::exists(firstFixture.Target()),"Move cleanup changed the discarded backend's target");
        firstFixture.UnchangedSeed();secondFixture.UnchangedSeed();
    }
}
void UnsafePathsAndBounds() {
    {
        Fixture fixture;Refuse([&]{static_cast<void>(Backend::Bind({},fixture.seed));},"Empty application root accepted");
        Refuse([&]{static_cast<void>(Backend::Bind(fixture.app.root_path(),fixture.seed));},"Filesystem root accepted as application data");
        Refuse([&]{static_cast<void>(Backend::Bind(fixture.app,fixture.seed,0u));},"Zero file limit accepted");
        Refuse([&]{static_cast<void>(Backend::Bind(fixture.app,fixture.seed,MaximumBytes+1u));},"Raised file limit accepted");
        auto backend=Backend::Bind(fixture.app,fixture.seed);const auto snapshot=backend.Read();
        Refuse([&]{static_cast<void>(backend.Prepare(Bytes(MaximumBytes+1u),snapshot,PublicationMode::CommitAllowed));},"Oversized staged data accepted");
        Require(!std::filesystem::exists(fixture.Stage()),"Oversized ingress created stage");fixture.UnchangedSeed();
    }
    for(bool stage:{false,true}) {
        Fixture fixture;Require(std::filesystem::create_directory(fixture.app/"Config"),"Cannot create unsafe-leaf fixture");
        const auto leaf=stage?fixture.Stage():fixture.Target();std::filesystem::create_hard_link(fixture.seed,leaf);
        Refuse([&]{auto backend=Backend::Bind(fixture.app,fixture.seed);if(stage)static_cast<void>(backend.Prepare(Text("unsafe"),backend.Read(),PublicationMode::CommitAllowed));},
            "Seed/owned hardlink alias accepted");fixture.UnchangedSeed();
    }
    {
        Fixture fixture;Require(std::filesystem::create_directory(fixture.app/"Config"),"Cannot create non-file fixture");
        Require(std::filesystem::create_directory(fixture.Target()),"Cannot create target-directory fixture");
        Refuse([&]{static_cast<void>(Backend::Bind(fixture.app,fixture.seed));},"Directory accepted as a regular config target");fixture.UnchangedSeed();
    }
    {
        Fixture fixture;Require(std::filesystem::create_directory(fixture.app/"Config"),"Cannot create orphan-stage fixture");
        const auto orphan=Text("uncommitted orphan; preserve for explicit recovery");WriteFixture(fixture.Stage(),orphan);
        auto backend=Backend::Bind(fixture.app,fixture.seed);const auto initial=backend.Read();
        Require(!initial.Owned()&&initial.Data()==fixture.initial,"Cold read preferred uncommitted orphan stage");
        Refuse([&]{static_cast<void>(backend.Prepare(Text("new"),initial,PublicationMode::CommitAllowed));},"Existing orphan stage was silently overwritten");
        Require(RawRead(fixture.Stage())==orphan&&!std::filesystem::exists(fixture.Target()),"Refusal/destructor removed an unowned orphan stage");fixture.UnchangedSeed();
        Refuse([&]{static_cast<void>(backend.DiscardAbandonedStage());},"Default rollback recovery touched an abandoned stage");
        Require(backend.DiscardAbandonedStage(PublicationMode::CommitAllowed)&&!std::filesystem::exists(fixture.Stage())&&
            !std::filesystem::exists(fixture.Target()),"Explicit owned-stage recovery damaged target or failed to remove orphan");
        auto retried=backend.Prepare(Text("recovered"),initial,PublicationMode::CommitAllowed);Require(retried.Publish().Published(),"Recovered backend could not commit again");retried.Cancel();
    }
#ifndef _WIN32
    for(bool stage:{false,true}) {
        Fixture fixture;Require(std::filesystem::create_directory(fixture.app/"Config"),"Cannot create symlink fixture");
        std::filesystem::create_symlink(fixture.seed,stage?fixture.Stage():fixture.Target());
        Refuse([&]{auto backend=Backend::Bind(fixture.app,fixture.seed);if(stage)static_cast<void>(backend.Prepare(Text("unsafe"),backend.Read(),PublicationMode::CommitAllowed));},
            "Owned leaf symlink accepted");fixture.UnchangedSeed();
    }
    {
        Fixture fixture;std::filesystem::create_directory_symlink(fixture.app,fixture.root/"application-link");
        Refuse([&]{static_cast<void>(Backend::Bind(fixture.root/"application-link",fixture.seed));},"Application ancestor symlink accepted");
        std::filesystem::create_symlink(fixture.seed,fixture.root/"source-link");
        Refuse([&]{static_cast<void>(Backend::Bind(fixture.app,fixture.root/"source-link"));},"Seed symlink accepted");fixture.UnchangedSeed();
    }
#endif
}
} // namespace

int main() {
    static_assert(std::is_nothrow_move_constructible_v<Prepared> && std::is_nothrow_move_assignable_v<Prepared>);
    static_assert(noexcept(std::declval<Prepared&>().Publish())&&noexcept(std::declval<Prepared&>().Cancel()));
    try {
        RoundTripAndRollback();FailureBoundaries();ConcurrentBindingsAndStaleDisk();MoveAndRecoveryOwnership();UnsafePathsAndBounds();
        std::cout<<"Owned config backend passed: "<<checks<<" checks / "<<refusals<<" refusals; real temporary files, immutable seed, "
            "exclusive staging, raw-byte/cold reopen, version conflicts and explicit post-rename durability disposition. NOT native SET/FExec integration.\n";return 0;
    }catch(const std::exception& error){denyAllocations=false;std::cerr<<error.what()<<'\n';return 1;}
}
