#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include "GC/GC.h"
#include "quest_object_indices.h"

namespace {
std::size_t checks{},refusals{},leafDestructions{},failedDestructions{},memberDestructions{},leaseDestructions{};
using Pool=QuestVr::ObjectIndexPool;

void Require(bool condition,const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Exception,class Action>
void Refuse(Action action,const char* message) {
    bool refused{};
    try {action();} catch(const Exception&) {refused=true;}
    Require(refused,message);++refusals;
}
template<class Object>
constexpr std::size_t ObjectBytes=sizeof(GCAllocation)+sizeof(Object);

void VerifyState(std::initializer_list<GCObject*> expected,std::size_t memory,const char* message) {
    std::set<GCObject*> wanted(expected),observed;
    Require(wanted.size()==expected.size(),"GC fixture contains duplicate expected object identities");
    for (auto* object:GC::GetObjects()) {
        Require(observed.size()<1024u,"GC allocation list is cyclic after failed construction");
        Require(observed.insert(object).second,"GC allocation list repeats a node after failed construction");
        Require(wanted.count(object)==1u,message);
    }
    Require(observed==wanted,message);
    const auto stats=GC::GetStats();
    Require(stats.numObjects==wanted.size() && stats.memoryUsage==memory,message);
}

struct OriginalFailure {int code{};};
struct Member {
    ~Member() {++memberDestructions;}
};
class Leaf final:public GCObject {
public:
    explicit Leaf(unsigned identity):identity(identity) {}
    ~Leaf() override {++leafDestructions;}
    unsigned identity{};
protected:
    GCAllocation* Mark(GCAllocation* list) override {return list;}
};
class ThrowingObject final:public GCObject {
public:
    explicit ThrowingObject(int code) {throw OriginalFailure{code};}
    ~ThrowingObject() override {++failedDestructions;}
protected:
    GCAllocation* Mark(GCAllocation* list) override {return list;}
private:
    Member member;
};
class NestedThrow final:public GCObject {
public:
    NestedThrow(GCRoot<Leaf>& root,Leaf*& unrooted) {
        root.set(GC::Alloc<Leaf>(101u));
        unrooted=GC::Alloc<Leaf>(102u);
        throw OriginalFailure{201};
    }
    ~NestedThrow() override {++failedDestructions;}
protected:
    GCAllocation* Mark(GCAllocation* list) override {return list;}
private:
    Member member;
};
class InnerThrow final:public GCObject {
public:
    explicit InnerThrow(GCRoot<Leaf>& root) {
        root.set(GC::Alloc<Leaf>(301u));
        throw OriginalFailure{302};
    }
    ~InnerThrow() override {++failedDestructions;}
protected:
    GCAllocation* Mark(GCAllocation* list) override {return list;}
private:
    Member member;
};
class OuterThrow final:public GCObject {
public:
    explicit OuterThrow(GCRoot<Leaf>& root) {static_cast<void>(GC::Alloc<InnerThrow>(root));}
    ~OuterThrow() override {++failedDestructions;}
protected:
    GCAllocation* Mark(GCAllocation* list) override {return list;}
private:
    Member member;
};
class LeaseObject final:public GCObject {
public:
    explicit LeaseObject(Pool& pool,bool failAfterAcquire=false):lease(pool.Acquire()) {
        if (failAfterAcquire) throw OriginalFailure{401};
    }
    ~LeaseObject() override {++leaseDestructions;}
    Pool::Lease lease;
protected:
    GCAllocation* Mark(GCAllocation* list) override {return list;}
private:
    Member member;
};

void SimpleFailuresAndSurvivingObjects() {
    VerifyState({},0u,"Standalone GC did not start empty");
    const auto initialMembers=memberDestructions;
    for (int repeat=0;repeat<128;++repeat) {
        bool caught{};
        try {static_cast<void>(GC::Alloc<ThrowingObject>(repeat));}
        catch(const OriginalFailure& failure) {caught=failure.code==repeat;}
        Require(caught,"GC allocation did not preserve the constructor's original exception");
        VerifyState({},0u,"Failed constructor retained allocation-list ownership or statistics");
        GC::Collect();
        VerifyState({},0u,"Collection after failed construction retained an incomplete object");
    }
    Require(memberDestructions==initialMembers+128u && failedDestructions==0u,
        "GC re-destroyed incomplete objects or skipped language-level member unwind");

    GCRoot<Leaf> first(GC::Alloc<Leaf>(1u)),second(GC::Alloc<Leaf>(2u));
    Refuse<OriginalFailure>([] {static_cast<void>(GC::Alloc<ThrowingObject>(9));},
        "Failed constructor between surviving roots did not throw");
    VerifyState({first.get(),second.get()},2u*ObjectBytes<Leaf>,
        "Failed head construction changed a previously rooted allocation");
    GC::Collect();
    Require(first->identity==1u && second->identity==2u,"Collection damaged a surviving object after constructor failure");
    VerifyState({first.get(),second.get()},2u*ObjectBytes<Leaf>,
        "Collection after constructor failure removed a rooted predecessor");
    first.set(nullptr);second.set(nullptr);GC::Collect();
    VerifyState({},0u,"Surviving predecessor allocations were not reclaimed after roots cleared");
}

void NestedAllocationsAndInteriorUnlink() {
    GCRoot<Leaf> prior(GC::Alloc<Leaf>(100u)),child;
    Leaf* unrooted{};
    const auto beforeDestroyed=leafDestructions,beforeMembers=memberDestructions;
    Refuse<OriginalFailure>([&] {static_cast<void>(GC::Alloc<NestedThrow>(child,unrooted));},
        "Nested child fixture did not throw from its parent constructor");
    Require(child && child->identity==101u && unrooted && unrooted->identity==102u,
        "Parent construction failure destroyed a successfully constructed nested child");
    VerifyState({prior.get(),child.get(),unrooted},3u*ObjectBytes<Leaf>,
        "Interior parent unlink dropped a nested allocation or retained incomplete parent accounting");
    Require(memberDestructions==beforeMembers+1u && failedDestructions==0u && leafDestructions==beforeDestroyed,
        "Parent construction failure invoked the wrong destructor or destroyed successful child objects");
    GC::Collect();
    Require(leafDestructions==beforeDestroyed+1u && child->identity==101u && prior->identity==100u,
        "Collection after interior unlink failed to distinguish rooted and unrooted nested children");
    VerifyState({prior.get(),child.get()},2u*ObjectBytes<Leaf>,
        "Collection retained an unrooted child or lost either surviving root");
    prior.set(nullptr);child.set(nullptr);GC::Collect();
    VerifyState({},0u,"Nested successful child allocations leaked after roots cleared");

    const auto nestedMembers=memberDestructions;
    GCRoot<Leaf> grandchild;
    Refuse<OriginalFailure>([&] {static_cast<void>(GC::Alloc<OuterThrow>(grandchild));},
        "Nested constructor failures did not propagate");
    Require(memberDestructions==nestedMembers+2u && failedDestructions==0u && grandchild && grandchild->identity==301u,
        "Nested constructor unwind skipped member destruction or destroyed a surviving rooted grandchild");
    VerifyState({grandchild.get()},ObjectBytes<Leaf>,
        "Two interior construction failures did not unlink both exact allocation nodes");
    GC::Collect();
    VerifyState({grandchild.get()},ObjectBytes<Leaf>,
        "Collection after nested failures visited an incomplete parent allocation");
    grandchild.set(nullptr);GC::Collect();
    VerifyState({},0u,"Nested surviving grandchild was not reclaimed");
}

void PoolRefusalAndTransactionRollback() {
    {
        Pool tight(1u);
        GCRoot<LeaseObject> retained(GC::Alloc<LeaseObject>(tight));
        const auto index=retained->lease.Index();
        for (unsigned repeat=0u;repeat<64u;++repeat) {
            Refuse<std::length_error>([&] {static_cast<void>(GC::Alloc<LeaseObject>(tight));},
                "GC fixture did not refuse exhausted native object-index capacity");
            VerifyState({retained.get()},ObjectBytes<LeaseObject>,
                "Index Acquire refusal retained an incomplete GC allocation or changed its statistics");
            Require(tight.LiveCount()==1u && retained->lease.Index()==index,
                "Index Acquire refusal changed a surviving object's allocation authority");
            GC::Collect();
            VerifyState({retained.get()},ObjectBytes<LeaseObject>,
                "Collection after index refusal lost a rooted object or visited incomplete construction");
        }
        retained.set(nullptr);GC::Collect();
        Require(tight.LiveCount()==0u,"Collecting a successful indexed object failed to release its lease");
    }
    VerifyState({},0u,"Index-capacity fixture retained GC allocations");

    Pool pool(4u);
    GCRoot<LeaseObject> original(GC::Alloc<LeaseObject>(pool)),provisional;
    auto transaction=pool.BeginTransaction(1u);
    provisional.set(GC::Alloc<LeaseObject>(pool));
    Refuse<std::length_error>([&] {static_cast<void>(GC::Alloc<LeaseObject>(pool));},
        "GC fixture did not refuse the active root's native index journal overflow");
    VerifyState({original.get(),provisional.get()},2u*ObjectBytes<LeaseObject>,
        "Active journal refusal corrupted successfully constructed old/new GC allocations");
    transaction.Rollback();
    Require(original->lease.Index()==1u && !provisional->lease && pool.LiveCount()==1u && pool.AllocatedSlots()==1u,
        "GC constructor refusal prevented exact enclosing native-index rollback");
    provisional.set(nullptr);GC::Collect();
    VerifyState({original.get()},ObjectBytes<LeaseObject>,
        "Collecting invalidated provisional construction damaged the restored old object");

    const auto beforeMembers=memberDestructions,beforeLeaseDestructions=leaseDestructions;
    auto retired=pool.BeginTransaction(2u);
    retired.Retire(original->lease);
    Refuse<OriginalFailure>([&] {static_cast<void>(GC::Alloc<LeaseObject>(pool,true));},
        "Indexed construction did not fail after same-root reuse of a retired slot");
    VerifyState({original.get()},ObjectBytes<LeaseObject>,
        "Post-Acquire construction failure leaked GC allocation ownership or statistics");
    Require(pool.LiveCount()==0u && !original->lease && memberDestructions==beforeMembers+1u &&
        leaseDestructions==beforeLeaseDestructions,
        "Post-Acquire constructor unwind leaked its provisional lease or invoked an incomplete object's destructor");
    retired.Rollback();
    Require(original->lease.Index()==1u && pool.LiveCount()==1u && pool.AllocatedSlots()==1u,
        "Post-Acquire constructor failure prevented restoring the exact retired native slot");
    GC::Collect();
    VerifyState({original.get()},ObjectBytes<LeaseObject>,
        "Collection after native retirement rollback damaged the restored original allocation");
    original.set(nullptr);GC::Collect();
    Require(pool.LiveCount()==0u,"Final rooted object collection retained native allocation authority");
    VerifyState({},0u,"GC/native-index transaction fixture retained allocation list entries or statistics");
}
}

int main() {
    try {
        SimpleFailuresAndSurvivingObjects();NestedAllocationsAndInteriorUnlink();PoolRefusalAndTransactionRollback();
        Require(failedDestructions==0u,"GC destroyed an incompletely constructed object");
        std::cout<<"GC constructor allocation rollback: "<<checks<<" checks and "<<refusals<<" refusal controls passed.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<"\n";return 1;
    } catch(...) {
        std::cerr<<"Unexpected constructor exception escaped the GC regression fixture.\n";return 1;
    }
}
