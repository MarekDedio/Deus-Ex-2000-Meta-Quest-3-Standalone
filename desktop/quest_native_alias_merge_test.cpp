#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Concrete standard-container regression, independent of runtime/UObject
// initialization. The runtime uses these same standard allocator/hash/equal
// dictionary types, with a pointer mapped value instead of opaque uintptr_t.
namespace AllocationFault {
thread_local bool denied{};
thread_local std::size_t attempts{};
void* Allocate(std::size_t bytes) {
    ++attempts;
    if(denied) throw std::bad_alloc();
    if(auto* memory=std::malloc(bytes?bytes:1u)) return memory;
    throw std::bad_alloc();
}
}
void* operator new(std::size_t bytes) {return AllocationFault::Allocate(bytes);}
void* operator new[](std::size_t bytes) {return AllocationFault::Allocate(bytes);}
// Keep the genuine malloc/free boundary visible to optimized GCC's strict
// mismatched-new-delete diagnostic, as in quest_static_find_object_test.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}

namespace {
using Map=std::unordered_map<std::string,std::uintptr_t>;
std::size_t checks{},refusals{},merges{},largeNamespaces{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition) throw std::runtime_error(message);
}
std::string Fold(std::string value) {
    for(char& c:value) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    return value;
}
constexpr std::array<const char*,9u> aliases{{
    "Core.FloatProperty","Core.NameProperty","Engine.GameEngine.ServerActors",
    "Engine.GameEngine.ServerActors.StrProperty0","Engine.GameEngine.ServerPackages",
    "Engine.GameEngine.ServerPackages.StrProperty0","Transient",
    "Transient.DeusExGameEngine0","Transient.Primitive0"}};

// Mirrors production's staged-first lookup only after allocation prechecks
// establish that live and staged identities cannot share a folded key.
std::uintptr_t Indexed(const Map& live,const Map& staged,const std::string& folded) {
    const auto next=staged.find(folded);if(next!=staged.end()) return next->second;
    const auto old=live.find(folded);return old==live.end()?0u:old->second;
}
bool Admit(const Map& liveQualified,const Map& liveVm,const Map& nextQualified,
    const Map& nextVm,const std::string& path) {
    return nextQualified.size()+liveQualified.size()<1'000'000u &&
        nextVm.size()+liveVm.size()<1'000'000u && !nextQualified.count(path) &&
        !liveQualified.count(path) && !Indexed(liveVm,nextVm,Fold(path));
}

struct Observed {
    std::string key;
    const Map::value_type* node{};
    const std::string* string{};
    const char* bytes{};
    std::uintptr_t value{};
};
std::vector<Observed> Observe(const Map& map) {
    std::vector<Observed> result;result.reserve(map.size());
    for(const auto& entry:map) result.push_back({entry.first,&entry,&entry.first,entry.first.data(),entry.second});
    return result;
}
void Unchanged(const Map& map,const std::vector<Observed>& observations) {
    for(const auto& entry:observations) {
        const auto found=map.find(entry.key);
        Require(found!=map.end(),"Original/transferred node disappeared");
        Require(&*found==entry.node,"Reserve/merge moved an existing allocated node");
        Require(&found->first==entry.string&&found->first.data()==entry.bytes,
            "Reserve/merge copied or moved a stored key string");
        Require(found->second==entry.value,"Reserve/merge changed an opaque object identity");
    }
}
struct DictionaryTriple {Map qualified,vm,packages;};
std::array<std::size_t,3u> Buckets(const DictionaryTriple& maps) {
    return {maps.qualified.bucket_count(),maps.vm.bucket_count(),maps.packages.bucket_count()};
}
// Same concrete capacity predicate as the production delta publication. It
// avoids reserve's optional shrink when the existing buckets already suffice.
std::size_t EnsureMergeCapacity(Map& destination,std::size_t incoming) {
    if(!incoming)return 0u;
    const auto required=destination.size()+incoming;
    const auto capacity=static_cast<long double>(destination.bucket_count())*
        static_cast<long double>(destination.max_load_factor());
    // An untouched empty container can expose its singleton/dummy bucket
    // while its first-insertion growth policy is not initialized yet.
    if(!destination.empty()&&static_cast<long double>(required)<=capacity) return 0u;
    const auto previous=destination.bucket_count();destination.reserve(required);return previous;
}
void MergeDenied(DictionaryTriple& live,DictionaryTriple& next) {
    bool failed{};AllocationFault::attempts=0u;AllocationFault::denied=true;
    try {
        live.qualified.merge(next.qualified);
        live.vm.merge(next.vm);
        live.packages.merge(next.packages);
    }catch(...) {failed=true;}
    AllocationFault::denied=false;
    Require(!failed,"Preallocated delta-node merge attempted a throwing operation");
    Require(AllocationFault::attempts==0u,"Preallocated delta-node merge called global new/new[]");
    merges+=3u;
}

void AllocationDenialControl() {
    bool failed{};AllocationFault::attempts=0u;AllocationFault::denied=true;
    try {auto* probe=::operator new(64u);::operator delete(probe);}catch(const std::bad_alloc&) {failed=true;}
    AllocationFault::denied=false;
    Require(failed&&AllocationFault::attempts==1u,"Allocation denial observer did not reject global new");
}

void Namespace(std::size_t count,unsigned round) {
    DictionaryTriple live,next;
    // Long keys avoid relying solely on short-string storage. The existing
    // namespace contains mixed-case aliases and distinct nonzero identities.
    for(std::size_t i=0u;i<count;++i) {
        const auto path="ExistingPackage"+std::to_string(round)+'.'+std::string(96u,'X')+std::to_string(i);
        const auto identity=static_cast<std::uintptr_t>(i+1u);
        Require(live.qualified.emplace(path,identity).second,"Duplicate existing qualified fixture name");
        Require(live.vm.emplace(Fold(path),identity).second,"Duplicate existing folded fixture name");
    }
    const auto packageCount=count?std::min<std::size_t>(count,4095u):0u;
    for(std::size_t i=0u;i<packageCount;++i)
        Require(live.packages.emplace("package"+std::to_string(i),static_cast<std::uintptr_t>(i+1u)).second,
            "Duplicate existing Package fixture name");
    const auto oldQualified=Observe(live.qualified),oldVm=Observe(live.vm),oldPackages=Observe(live.packages);
    for(std::size_t i=0u;i<aliases.size();++i) {
        const std::string path=aliases[i];
        Require(Admit(live.qualified,live.vm,next.qualified,next.vm,path),"Fresh native alias refused preflight");
        const auto identity=static_cast<std::uintptr_t>(1'000'000u+i);
        Require(next.qualified.emplace(path,identity).second&&next.vm.emplace(Fold(path),identity).second,
            "Cannot preallocate native namespace nodes");
        Require(Indexed(live.vm,next.vm,Fold(path))==identity,"Staged lookup failed before publication");
    }
    Require(next.packages.emplace("transient",1'000'006u).second,"Cannot preallocate actual Transient Package alias");
    const auto newQualified=Observe(next.qualified),newVm=Observe(next.vm),newPackages=Observe(next.packages);
    EnsureMergeCapacity(live.qualified,next.qualified.size());
    EnsureMergeCapacity(live.vm,next.vm.size());
    EnsureMergeCapacity(live.packages,next.packages.size());
    const auto liveBuckets=Buckets(live),nextBuckets=Buckets(next);
    MergeDenied(live,next);
    Require(next.qualified.empty()&&next.vm.empty()&&next.packages.empty(),"Admitted source delta did not drain completely");
    Require(Buckets(live)==liveBuckets&&Buckets(next)==nextBuckets,"Node merge rehashed a source/destination dictionary");
    Require(live.qualified.size()==count+aliases.size()&&live.vm.size()==count+aliases.size()&&
        live.packages.size()==packageCount+1u,"Three dictionary membership totals differ after merge");
    Unchanged(live.qualified,oldQualified);Unchanged(live.vm,oldVm);Unchanged(live.packages,oldPackages);
    Unchanged(live.qualified,newQualified);Unchanged(live.vm,newVm);Unchanged(live.packages,newPackages);
    for(std::size_t i=0u;i<aliases.size();++i)
        Require(Indexed(live.vm,next.vm,Fold(aliases[i]))==1'000'000u+i,"Published folded alias resolves a different identity");

    // An emptied staging dictionary can be reused without disturbing the
    // previous generation of nodes; reserve must be renewed for this batch.
    next.qualified.emplace("Transient.SecondLogicalViewport",2'000'000u);
    next.vm.emplace("transient.secondlogicalviewport",2'000'000u);
    EnsureMergeCapacity(live.qualified,1u);EnsureMergeCapacity(live.vm,1u);
    EnsureMergeCapacity(live.packages,0u);
    const auto repeatedBuckets=Buckets(live);
    MergeDenied(live,next);
    Require(Buckets(live)==repeatedBuckets&&next.qualified.empty()&&next.vm.empty()&&next.packages.empty(),
        "Repeated delta merge rehashed or retained transferred nodes");
    Unchanged(live.qualified,newQualified);Unchanged(live.vm,newVm);Unchanged(live.packages,newPackages);
    if(count>=4096u) ++largeNamespaces;
}

void Conflicts() {
    DictionaryTriple live,next;
    live.qualified.emplace("Existing.MixedCase",11u);live.vm.emplace("existing.mixedcase",11u);
    live.packages.emplace("transient",21u);
    next.qualified.emplace("New.StagedCase",31u);next.vm.emplace("new.stagedcase",31u);
    const auto oldQualified=Observe(live.qualified),oldVm=Observe(live.vm),oldPackages=Observe(live.packages);
    const auto stagedQualified=Observe(next.qualified),stagedVm=Observe(next.vm);
    for(const std::string path:{"Existing.MixedCase","EXISTING.MIXEDCASE","New.StagedCase","NEW.STAGEDCASE"}) {
        Require(!Admit(live.qualified,live.vm,next.qualified,next.vm,path),"Exact/folded live or staged collision was admitted");
        ++refusals;
    }
    Require(live.packages.count("transient")!=0u,"Actual Package collision was overlooked");++refusals;
    Unchanged(live.qualified,oldQualified);Unchanged(live.vm,oldVm);Unchanged(live.packages,oldPackages);
    Unchanged(next.qualified,stagedQualified);Unchanged(next.vm,stagedVm);

    // Document why production preflight and the post-merge delta-empty invariant
    // matter: std::merge itself silently retains an equivalent-key source node.
    Map destination,source;
    destination.emplace("occupied",41u);source.emplace("occupied",42u);source.emplace("fresh",43u);
    destination.reserve(destination.size()+source.size());
    const auto original=Observe(destination),incoming=Observe(source);
    bool failed{};AllocationFault::attempts=0u;AllocationFault::denied=true;
    try {destination.merge(source);}catch(...) {failed=true;}
    AllocationFault::denied=false;
    Require(!failed&&AllocationFault::attempts==0u,"Conflict control merge unexpectedly allocated");
    Require(source.size()==1u&&source.at("occupied")==42u&&destination.at("occupied")==41u&&destination.at("fresh")==43u,
        "Concrete merge conflict did not preserve the original and unadopted identities");
    Unchanged(destination,original);
    for(const auto& observed:incoming)
        Unchanged(observed.key=="occupied"?source:destination,std::vector<Observed>{observed});
}
void ExistingSpareCapacity() {
    Map live,next;live.reserve(8192u);live.emplace("Already.Loaded",71u);
    for(std::size_t i=0u;i<aliases.size();++i)next.emplace(aliases[i],100u+i);
    const auto previousBuckets=live.bucket_count();const auto observed=Observe(live),incoming=Observe(next);
    bool failed{};std::size_t overlap{};AllocationFault::attempts=0u;AllocationFault::denied=true;
    try {overlap=EnsureMergeCapacity(live,next.size());live.merge(next);}catch(...) {failed=true;}
    AllocationFault::denied=false;
    Require(!failed&&AllocationFault::attempts==0u&&overlap==0u,
        "Existing spare capacity still allocated a replacement bucket array");
    Require(live.bucket_count()==previousBuckets&&next.empty(),"Spare-capacity merge shrank or rehashed existing buckets");
    Unchanged(live,observed);Unchanged(live,incoming);++merges;
}
}

int main() {
    try {
        AllocationDenialControl();
        for(unsigned round=0u;round<3u;++round)
            for(const std::size_t count:{0u,1u,31u,4096u,65536u}) Namespace(count,round);
        Conflicts();ExistingSpareCapacity();
        std::cout<<"PASS concrete native alias delta merge: "<<checks<<" checks, "<<refusals<<" preflight refusals, "
            <<merges<<" allocation-denied map merges, "<<largeNamespaces<<" large namespaces; zero merge allocations.\n";
        return 0;
    }catch(const std::exception& error) {
        AllocationFault::denied=false;
        std::cerr<<"FAIL concrete native alias delta merge after "<<checks<<" checks: "<<error.what()<<'\n';return 1;
    }
}
