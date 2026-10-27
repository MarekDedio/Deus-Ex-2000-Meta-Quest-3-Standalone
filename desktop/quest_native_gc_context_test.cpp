#include "quest_native_gc_context.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace Policy=QuestVr::NativeGcContext;
unsigned checks{},refusals{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
template<class Call> void Refuses(Call&& call,const char* message) {
    bool refused{};try {call();} catch(const std::exception&) {refused=true;}
    Require(refused,message);++refusals;
}
using Node=std::uint32_t;
constexpr Node receiver=1u,source=2u;
constexpr std::array<const char*,27u> expectedFields{
    "Canvas","gcClipRect","Style","bSmoothed","bDrawEnabled","bMasked",
    "bTranslucent","bModulated","bTextTranslucent","PolyFlags","textPolyFlags",
    "tileColor","tilePlane","TextColor","textPlane","normalFont","boldFont",
    "underlineTexture","underlineHeight","baselineOffset","textVSpacing",
    "HAlign","VAlign","bWordWrap","bParseMetachars","hMultiplier","vMultiplier"};
std::size_t FieldIndex(std::string_view name) {
    for(std::size_t i=0u;i<expectedFields.size();++i)if(name==expectedFields[i])return i;
    throw std::runtime_error("Mock GC field identity is not authenticated");
}
bool Packed48(std::size_t index) {return index>=3u&&index<=8u;}
bool Packed98(std::size_t index) {return index==23u||index==24u;}
bool Byte(std::size_t index) {return index==2u||index==21u||index==22u;}
std::size_t WordCount(std::size_t index) {
    return index==1u?6u:(index==12u||index==14u)?4u:1u;
}
struct Cell {
    std::array<std::uint32_t,6u> words{};
    bool operator==(const Cell&)const=default;
};
struct Gc {
    std::array<Cell,27u> fields{};
    std::uint32_t flags48{},flags98{},freeFlags{};
    std::int32_t count{1};
    Node owner{},stack{},free{},identity{},outer{};
    std::uint32_t classIdentity{0xface1234u};
    bool operator==(const Gc&)const=default;
};
Gc MakeGc(Node id,std::uint32_t seed) {
    Gc result;result.identity=id;result.outer=0x100u+id;
    result.flags48=0xa5a55ac0u|(seed&63u);
    result.flags98=0x5ac3a5a4u|((seed>>6u)&3u);
    result.freeFlags=0xacc0ff00u|(seed&1u);
    for(std::size_t i=0u;i<result.fields.size();++i) {
        for(std::size_t j=0u;j<result.fields[i].words.size();++j)
            result.fields[i].words[j]=0x9e3779b9u*static_cast<std::uint32_t>(i+1u)+seed+
                static_cast<std::uint32_t>(j)*0x100000u;
        if(Byte(i))result.fields[i].words[0u]&=255u;
    }
    return result;
}
Cell View(const Gc& object,std::size_t index) {
    Cell value;
    if(Packed48(index))value.words[0u]=(object.flags48>>(index-3u))&1u;
    else if(Packed98(index))value.words[0u]=(object.flags98>>(index-23u))&1u;
    else std::copy_n(object.fields[index].words.begin(),WordCount(index),value.words.begin());
    return value;
}
void Store(Gc& object,std::size_t index,const Cell& value) {
    if(Packed48(index)) {
        const auto mask=1u<<(index-3u);
        object.flags48^=(object.flags48^(value.words[0u]?mask:0u))&mask;
    } else if(Packed98(index)) {
        const auto mask=1u<<(index-23u);
        object.flags98^=(object.flags98^(value.words[0u]?mask:0u))&mask;
    } else {
        std::copy_n(value.words.begin(),WordCount(index),object.fields[index].words.begin());
        if(Byte(index))object.fields[index].words[0u]&=255u;
    }
}
bool SameContext(const Gc& left,const Gc& right) {
    for(std::size_t i=0u;i<expectedFields.size();++i)if(View(left,i)!=View(right,i))return false;
    return true;
}
struct Graph {
    std::map<Node,Gc> nodes;
    Node next{3u};
    unsigned allocations{};
    std::uint64_t revision{};
    bool operator==(const Graph&)const=default;
};
// Explicit mock ownership/storage provider, not a native UObject/Canvas or a
// second production GC. Read/write events independently expose policy order.
struct Operations {
    using Node=std::uint32_t;
    Graph graph;
    std::vector<std::string> events;
    std::size_t work{},writes{};
    std::size_t workLimit{std::numeric_limits<std::size_t>::max()};
    std::size_t writeLimit{std::numeric_limits<std::size_t>::max()};
    unsigned allocationLimit{std::numeric_limits<unsigned>::max()};
    bool allocationReturnsNone{},throwAfterAllocation{};
    std::function<void(Operations&,std::string_view)> callback;
    Gc& Actual(Node node) {
        const auto found=graph.nodes.find(node);
        if(!node||found==graph.nodes.end())throw std::runtime_error("Mock live GC identity is invalid");
        return found->second;
    }
    void Step() {
        if(work>=workLimit)throw std::runtime_error("Mock shared root native work exhausted");
        ++work;
    }
    void Event(std::string event) {
        events.push_back(std::move(event));
        if(callback)callback(*this,events.back());
    }
    void WriteEvent(Node node,const char* field) {
        static_cast<void>(Actual(node));
        if(writes>=writeLimit)throw std::runtime_error("Mock shared root write budget exhausted");
        ++writes;++graph.revision;Event(std::string("write.")+field+':'+std::to_string(node));
    }
    Node ReadOwner(Node node) {auto& object=Actual(node);Event("read.owner:"+std::to_string(node));return object.owner;}
    std::int32_t ReadCount(Node node) {auto& object=Actual(node);Event("read.count:"+std::to_string(node));return object.count;}
    Node ReadFree(Node node) {auto& object=Actual(node);Event("read.free:"+std::to_string(node));return object.free;}
    Node ReadStack(Node node) {auto& object=Actual(node);Event("read.stack:"+std::to_string(node));return object.stack;}
    void WriteOwner(Node node,Node value) {WriteEvent(node,"owner");Actual(node).owner=value;}
    void WriteCount(Node node,std::int32_t value) {WriteEvent(node,"count");Actual(node).count=value;}
    void WriteFree(Node node,Node value) {WriteEvent(node,"free");Actual(node).free=value;}
    void WriteStack(Node node,Node value) {WriteEvent(node,"stack");Actual(node).stack=value;}
    void WriteFreeFlag(Node node,bool value) {
        WriteEvent(node,"freeFlag");auto& bits=Actual(node).freeFlags;
        bits^=(bits^(value?1u:0u))&1u;
    }
    Cell ReadContextField(Node node,const char* name) {
        auto& object=Actual(node);const auto index=FieldIndex(name);
        Event("read.context:"+std::to_string(node)+':'+name);return View(object,index);
    }
    void WriteContextField(Node node,const char* name,const Cell& value) {
        static_cast<void>(Actual(node));const auto index=FieldIndex(name);
        if(writes>=writeLimit)throw std::runtime_error("Mock shared root context write budget exhausted");
        ++writes;++graph.revision;Event("write.context:"+std::to_string(node)+':'+name);
        Store(Actual(node),index,value);
    }
    void CopyContext(Node destination,Node from) {
        Event("copy.context:"+std::to_string(destination)+':'+std::to_string(from));
        Policy::Copy(*this,destination,from);
    }
    Node AllocateCopy(Node from) {
        static_cast<void>(Actual(from));Event("allocate.copy:"+std::to_string(from));
        if(allocationReturnsNone)return 0u;
        if(graph.allocations>=allocationLimit)throw std::runtime_error("Mock root retained-object admission exhausted");
        const auto id=graph.next++;
        auto object=MakeGc(id,0xdead0000u+id);
        for(std::size_t i=0u;i<expectedFields.size();++i)Store(object,i,View(Actual(from),i));
        object.count=1;object.owner=0u;object.stack=0u;object.free=0u;object.freeFlags&=~1u;
        object.outer=from;object.classIdentity=Actual(from).classIdentity;
        graph.nodes.emplace(id,std::move(object));++graph.allocations;++graph.revision;
        if(throwAfterAllocation)throw std::runtime_error("Mock actual-copy-constructor provider failed after allocation");
        return id;
    }
};
Operations Pair() {
    Operations result;
    result.graph.nodes.emplace(receiver,MakeGc(receiver,0x12345678u));
    result.graph.nodes.emplace(source,MakeGc(source,0x87654321u));
    return result;
}
void ResetEvents(Operations& operations) {
    operations.events.clear();operations.work=0u;operations.writes=0u;
}
std::vector<std::string> CopyEvents(Node destination,Node from) {
    std::vector<std::string> result;
    for(const auto* name:expectedFields) {
        result.push_back("read.context:"+std::to_string(from)+':'+name);
        result.push_back("write.context:"+std::to_string(destination)+':'+name);
    }
    return result;
}
void ContextCopies() {
    for(std::size_t i=0u;i<expectedFields.size();++i)
        Require(std::string_view(Policy::ContextFields[i])==expectedFields[i],"CopyGC field identity/order changed");
    for(std::uint32_t incoming=0u;incoming<256u;++incoming)
        for(std::uint32_t old=0u;old<256u;++old) {
            auto f=Pair();auto& destination=f.Actual(receiver);auto& from=f.Actual(source);
            destination.flags48=(destination.flags48&~63u)|(old&63u);
            destination.flags98=(destination.flags98&~3u)|((old>>6u)&3u);
            from.flags48=(from.flags48&~63u)|(incoming&63u);
            from.flags98=(from.flags98&~3u)|((incoming>>6u)&3u);
            const auto prior=destination,sourceBefore=from;
            Policy::Copy(f,receiver,source);
            Require(SameContext(destination,from),"CopyGC lost one of the27 semantic fields");
            Require(destination.flags48==((prior.flags48&~63u)|(from.flags48&63u))&&
                destination.flags98==((prior.flags98&~3u)|(from.flags98&3u)),"CopyGC overwrote unrelated packed flag bits");
            Require(from==sourceBefore,"CopyGC mutated its source context");
            Require(destination.count==prior.count&&destination.owner==prior.owner&&destination.stack==prior.stack&&
                destination.free==prior.free&&destination.freeFlags==prior.freeFlags&&destination.identity==prior.identity&&
                destination.outer==prior.outer&&destination.classIdentity==prior.classIdentity,
                "CopyGC copied protocol fields or UObject identity");
            Require(f.work==54u&&f.writes==27u,"CopyGC skipped stores or changed charged read/write count");
        }
    auto order=Pair();Policy::Copy(order,receiver,source);
    Require(order.events==CopyEvents(receiver,source),"CopyGC read/store ordering changed");
    for(unsigned byte=0u;byte<256u;++byte) {
        auto f=Pair();auto& from=f.Actual(source);
        from.fields[2u].words[0u]=byte;
        from.fields[21u].words[0u]=255u-byte;
        from.fields[22u].words[0u]=byte^0x96u;
        from.fields[11u].words[0u]=byte|(byte<<8u)|(byte<<16u)|(byte<<24u);
        from.fields[13u].words[0u]=~from.fields[11u].words[0u];
        Policy::Copy(f,receiver,source);
        Require(SameContext(f.Actual(receiver),from),"CopyGC clamped raw Byte alignment/Style or lost Color alpha");
    }
    const std::array<std::uint32_t,15u> words{
        0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,0x3f800000u,0xbf800000u,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc01234u,0x7f801234u,0xff801234u};
    for(const auto bits:words) {
        auto f=Pair();
        for(const auto index:{1u,12u,14u,18u,19u,20u})
            for(std::size_t channel=0u;channel<WordCount(index);++channel)
                f.Actual(source).fields[index].words[channel]=bits;
        Policy::Copy(f,receiver,source);
        Require(SameContext(f.Actual(receiver),f.Actual(source)),"CopyGC changed raw Float/Plane/ClipRect words");
    }
    auto self=Pair();auto before=self.graph;
    Policy::Copy(self,receiver,receiver);before.revision+=27u;
    Require(self.graph==before&&self.writes==27u&&self.events==CopyEvents(receiver,receiver),
        "Self CopyGC invented an equality shortcut or changed state");
    auto freshReads=Pair();const auto first=View(freshReads.Actual(source),0u);
    freshReads.callback=[](Operations& f,std::string_view event) {
        if(event=="write.context:1:Canvas") {
            f.Actual(source).fields[0u].words[0u]=0xaaaau;
            f.Actual(source).fields[20u].words[0u]=0x7f801234u;
        }
    };
    Policy::Copy(freshReads,receiver,source);
    Require(View(freshReads.Actual(receiver),0u)==first&&freshReads.Actual(receiver).fields[20u].words[0u]==0x7f801234u,
        "CopyGC preloaded the whole context or reread a consumed field after its write");
}
void PushPopLifecycle() {
    auto f=Pair();const auto original=f.Actual(receiver);
    const auto pushed=Policy::Push(f,receiver);const auto node=f.Actual(receiver).stack;
    Require(pushed==1&&node==3u&&f.graph.allocations==1u&&f.graph.nodes.size()==3u,"Fresh PushGC result/allocation identity changed");
    Require(f.Actual(receiver).count==2&&f.Actual(receiver).free==0u&&f.Actual(node).owner==receiver&&
        !f.Actual(node).stack&&!f.Actual(node).free&&f.Actual(node).count==1&&!(f.Actual(node).freeFlags&1u)&&
        f.Actual(node).outer==receiver&&SameContext(f.Actual(node),original),"Fresh PushGC snapshot/protocol fields changed");
    f.Actual(receiver).fields[20u].words[0u]=0x7fc01234u;
    ResetEvents(f);Policy::Pop(f,receiver);
    Require(f.Actual(receiver).count==1&&!f.Actual(receiver).stack&&f.Actual(receiver).free==node&&
        f.Actual(node).count==1&&f.Actual(node).owner==receiver&&(f.Actual(node).freeFlags&1u),
        "No-arg PopGC failed to return the saved node to the real free list");
    Require(SameContext(f.Actual(receiver),original),"PopGC did not restore saved context");
    const auto freePadding=f.Actual(node).freeFlags&~1u;
    ResetEvents(f);Require(Policy::Push(f,receiver)==1,"Reused PushGC returned a new depth");
    Require(f.graph.allocations==1u&&f.graph.nodes.size()==3u&&f.Actual(receiver).stack==node&&
        !f.Actual(receiver).free&&!(f.Actual(node).freeFlags&1u)&&(f.Actual(node).freeFlags&~1u)==freePadding,
        "PushGC allocated instead of reusing free context or overwrote free padding");
    std::vector<std::string> expected{
        "read.owner:1","read.count:1","read.free:1","read.stack:3","write.free:1","copy.context:3:1"};
    const auto copied=CopyEvents(node,receiver);expected.insert(expected.end(),copied.begin(),copied.end());
    for(const auto* event:{"write.owner:3","write.free:3","write.freeFlag:3","read.count:1","write.count:3",
            "read.stack:1","write.stack:3","read.count:1","write.stack:1","write.count:1"})expected.emplace_back(event);
    Require(f.events==expected,"Reused PushGC read/store order changed");
    const auto oldest=f.Actual(receiver);
    f.Actual(receiver).fields[20u].words[0u]=0x80000000u;
    const auto middle=f.Actual(receiver);
    Require(Policy::Push(f,receiver)==2&&f.graph.allocations==2u,"Nested PushGC result/identity changed");
    const auto newest=f.Actual(receiver).stack;
    f.Actual(receiver).fields[20u].words[0u]=0xff800000u;
    ResetEvents(f);Policy::PopTo(f,receiver,1);
    Require(f.Actual(receiver).count==1&&!f.Actual(receiver).stack&&f.Actual(receiver).free==node&&
        f.Actual(node).stack==newest&&f.Actual(node).count==2&&f.Actual(newest).count==1&&
        (f.Actual(node).freeFlags&1u)&&(f.Actual(newest).freeFlags&1u),"Nested threshold pop free-list order/count changed");
    Require(SameContext(f.Actual(receiver),oldest)&&SameContext(f.Actual(newest),middle),"Nested PopGC restored the newest instead of final popped context");
    Require(std::count(f.events.begin(),f.events.end(),"copy.context:1:3")==1&&
        std::count(f.events.begin(),f.events.end(),"copy.context:1:4")==0,"Nested pop copied intermediate contexts");
    Require(f.events.back()=="write.context:1:vMultiplier","PopGC restored context before finishing protocol stores");
}
void GuardsCountsAndSentinel() {
    const auto minimum=std::numeric_limits<std::int32_t>::min(),maximum=std::numeric_limits<std::int32_t>::max();
    Require(Policy::Increment(maximum)==minimum&&Policy::Decrement(minimum)==maximum,"GC count rollover used signed-overflow UB/clamping");
    const std::array<std::int32_t,7u> values{minimum,-17,-1,0,1,17,maximum};
    for(const auto count:values) {
        auto f=Pair();f.Actual(receiver).count=count;f.Actual(receiver).owner=source;
        const auto before=f.graph;
        Require(Policy::Push(f,receiver)==count&&f.graph==before&&f.events==
            std::vector<std::string>{"read.owner:1","read.count:1"},"Owner-bound PushGC forwarded or changed state");
        ResetEvents(f);Policy::PopTo(f,receiver,minimum);
        std::vector<std::string> expected{"read.count:1"};if(minimum<count)expected.emplace_back("read.owner:1");
        Require(f.graph==before&&f.events==expected,"PopGC initial signed/owner guard order changed");
        auto empty=Pair();empty.Actual(receiver).count=count;const auto emptyBefore=empty.graph;
        Policy::Pop(empty,receiver);
        Require(empty.graph==emptyBefore&&empty.writes==0u,"No-arg empty PopGC decremented or invented a context");
    }
    for(const auto current:values)for(const auto saved:values)for(const auto target:values) {
        auto f=Pair();f.Actual(receiver).count=current;f.Actual(receiver).stack=source;
        f.Actual(source).count=saved;f.Actual(source).owner=receiver;const auto before=f.graph;
        Policy::PopTo(f,receiver,target);
        const bool popped=target<current&&saved>=target;
        if(popped) {
            Require(f.Actual(receiver).count==saved&&!f.Actual(receiver).stack&&f.Actual(receiver).free==source&&
                SameContext(f.Actual(receiver),before.nodes.at(source)),"Signed threshold PopGC clamped or required an exact count match");
        } else Require(f.graph==before&&f.writes==0u,"No-op signed threshold PopGC mutated state");
    }
    auto wrapping=Pair();wrapping.Actual(receiver).count=maximum;
    Require(Policy::Push(wrapping,receiver)==maximum&&wrapping.Actual(receiver).count==minimum,
        "PushGC failed machine-word count wrap");
    auto freeWrapping=Pair();freeWrapping.Actual(receiver).count=4;freeWrapping.Actual(receiver).stack=source;
    freeWrapping.Actual(source).count=3;freeWrapping.Actual(source).owner=receiver;
    freeWrapping.graph.nodes.emplace(3u,MakeGc(3u,33u));freeWrapping.graph.next=4u;
    freeWrapping.Actual(receiver).free=3u;freeWrapping.Actual(3u).count=maximum;
    Policy::PopTo(freeWrapping,receiver,3);
    Require(freeWrapping.Actual(source).count==minimum,"Popped free-list serial failed machine-word increment wrap");
    auto makeSentinel=[] {
        auto f=Pair();f.Actual(receiver).count=10;f.Actual(receiver).stack=source;
        f.Actual(source).count=8;f.Actual(source).owner=receiver;return f;
    };
    auto omitted=makeSentinel(),explicitMinusOne=makeSentinel(),literalMinusOne=makeSentinel();
    Policy::PopScript(omitted,receiver);Policy::PopScript(explicitMinusOne,receiver,-1);
    Policy::PopTo(literalMinusOne,receiver,-1);
    Require(omitted.graph==explicitMinusOne.graph&&omitted.events==explicitMinusOne.events,
        "Omitted/explicit-1 script PopGC selected different operations");
    Require(omitted.events.size()>2u&&omitted.events[0u]=="read.stack:1"&&omitted.events[1u]=="read.count:2"&&
        literalMinusOne.events[0u]=="read.count:1", "PopGC wrapper confused no-arg and literal integer-1 overloads");
    auto absent=Pair();absent.allocationReturnsNone=true;const auto before=absent.graph;
    Require(Policy::Push(absent,receiver)==1&&absent.graph==before&&absent.writes==0u,
        "Failed native allocation fabricated a context or advanced count");
}
void RereadsAndFaults() {
    auto f=Pair();Policy::Push(f,receiver);Policy::Pop(f,receiver);ResetEvents(f);
    f.callback=[](Operations& operations,std::string_view event) {
        if(event=="copy.context:3:1")operations.Actual(receiver).count=7;
        if(event=="write.stack:3")operations.Actual(receiver).count=11;
    };
    Require(Policy::Push(f,receiver)==11&&f.Actual(receiver).count==12&&f.Actual(3u).count==7,
        "PushGC failed live count rereads after delegated context copy and saved fields");
    const std::array<std::function<void(Operations&)>,4u> invalid{
        [](Operations& operations) {Policy::Copy(operations,receiver,0u);},
        [](Operations& operations) {Policy::Copy(operations,0u,source);},
        [](Operations& operations) {operations.Actual(receiver).free=99u;static_cast<void>(Policy::Push(operations,receiver));},
        [](Operations& operations) {operations.Actual(receiver).count=3;operations.Actual(receiver).stack=99u;Policy::PopTo(operations,receiver,1);}};
    for(const auto& call:invalid) {auto operations=Pair();Refuses([&] {call(operations);},"Invalid mock live GC identity was accepted");}
}
struct Scenario {
    std::function<Operations()> fixture;
    std::function<void(Operations&)> call;
};
// Pure fixture's explicit outer-root owner. Production rollback is separately
// tested through authentic native declarations/VM; the policy never undoes.
bool InMockRoot(Operations& operations,const std::function<void(Operations&)>& call,bool lateFailure=false) {
    auto before=operations.graph;
    try {
        call(operations);
        if(lateFailure)throw std::runtime_error("Explicit late enclosing-root failure");
        return true;
    } catch(const std::exception&) {
        operations.graph.nodes.swap(before.nodes);
        operations.graph.next=before.next;operations.graph.allocations=before.allocations;
        operations.graph.revision=before.revision;return false;
    }
}
void SharedBudgetAndRollback() {
    const auto reused=[] {auto f=Pair();Policy::Push(f,receiver);Policy::Pop(f,receiver);ResetEvents(f);return f;};
    const auto nested=[] {
        auto f=Pair();Policy::Push(f,receiver);f.Actual(receiver).fields[20u].words[0u]=0x7fc01234u;
        Policy::Push(f,receiver);ResetEvents(f);return f;
    };
    const std::array<Scenario,6u> cases{{
        {Pair,[](Operations& f) {Policy::Copy(f,receiver,source);}},
        {Pair,[](Operations& f) {static_cast<void>(Policy::Push(f,receiver));}},
        {reused,[](Operations& f) {static_cast<void>(Policy::Push(f,receiver));}},
        {nested,[](Operations& f) {Policy::PopTo(f,receiver,1);}},
        {nested,[](Operations& f) {Policy::Pop(f,receiver);}},
        {nested,[](Operations& f) {Policy::PopScript(f,receiver,-1);}}}};
    for(const auto& scenario:cases) {
        auto complete=scenario.fixture();scenario.call(complete);
        Require(complete.work==complete.events.size(),"Mock provider events no longer expose one operation per charged Step");
        for(std::size_t cut=0u;cut<complete.work;++cut) {
            auto f=scenario.fixture();const auto before=f.graph;f.workLimit=cut;
            Require(!InMockRoot(f,scenario.call),"Native work cutoff was swallowed");++refusals;
            Require(f.work==cut&&f.events==std::vector<std::string>(complete.events.begin(),complete.events.begin()+cut),
                "Native work cutoff reordered or executed post-failure GC operations");
            Require(f.graph==before,"Outer mock root lost node identity/context/list/name/count rollback on work cutoff");
        }
        for(std::size_t cut=0u;cut<complete.writes;++cut) {
            auto f=scenario.fixture();const auto before=f.graph;f.writeLimit=cut;
            Require(!InMockRoot(f,scenario.call),"Write cutoff was swallowed");++refusals;
            Require(f.writes==cut&&f.events.size()<=complete.events.size()&&
                std::equal(f.events.begin(),f.events.end(),complete.events.begin()),"Write cutoff executed compensating or post-failure operations");
            Require(f.graph==before,"Outer mock root lost exact graph beforeimage on write cutoff");
        }
        for(std::size_t at=0u;at<complete.events.size();++at) {
            auto f=scenario.fixture();const auto before=f.graph;
            f.callback=[at](Operations& operations,std::string_view) {
                if(operations.events.size()==at+1u)throw std::runtime_error("Explicit provider failure");
            };
            Require(!InMockRoot(f,scenario.call),"Delegated GC provider failure was swallowed");++refusals;
            Require(f.events==std::vector<std::string>(complete.events.begin(),complete.events.begin()+at+1u),
                "Provider failure retried, compensated or continued GC operations");
            Require(f.graph==before,"Outer mock root lost exact graph after delegated provider failure");
        }
        auto late=scenario.fixture();const auto before=late.graph;
        Require(!InMockRoot(late,scenario.call,true)&&late.graph==before&&late.events==complete.events,
            "Late enclosing failure did not undo complete native context/stack ownership");++refusals;
    }
    for(const bool afterAllocation:{false,true}) {
        auto f=Pair();const auto before=f.graph;
        if(afterAllocation)f.throwAfterAllocation=true;else f.allocationLimit=0u;
        const auto call=[](Operations& operations) {static_cast<void>(Policy::Push(operations,receiver));};
        Require(!InMockRoot(f,call)&&f.graph==before,"Allocation admission/copy-constructor failure escaped outer rollback");++refusals;
    }
}
} // namespace

int main() {
    try {
        ContextCopies();PushPopLifecycle();GuardsCountsAndSentinel();RereadsAndFaults();SharedBudgetAndRollback();
        std::cout<<"PASS original GC context policy: "<<checks<<" checks, "<<refusals
            <<" refusal controls; exact27-field copies, raw payloads/masked bits, fresh/reused/nested stack, signed counts, "
            "sentinel, provider faults and mock enclosing-root undo. Not native UObject/VM, font metrics, startup or rendered HUD acceptance.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL original GC context policy: "<<error.what()<<'\n';return 1;
    }
}
