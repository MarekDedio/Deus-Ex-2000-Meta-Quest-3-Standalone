#include "quest_native_window_navigation.h"
#include "quest_native_work.h"

#include <array>
#include <cstddef>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks{},refusals{};
void Require(bool condition,const char* message) {
    ++checks;if(!condition) throw std::runtime_error(message);
}
using Query=QuestVr::NativeWindowRelativeQuery;
using Link=QuestVr::NativeWindowRelativeLink;
constexpr std::array<Query,4u> queries{Query::TopChild,Query::BottomChild,Query::HigherSibling,Query::LowerSibling};
struct Direction {Link head,step;};
Direction Links(Query query) {
    switch(query) {
    case Query::TopChild:return {Link::LastChild,Link::PrevSibling};
    case Query::BottomChild:return {Link::FirstChild,Link::NextSibling};
    case Query::HigherSibling:return {Link::NextSibling,Link::NextSibling};
    case Query::LowerSibling:return {Link::PrevSibling,Link::PrevSibling};
    default:throw std::runtime_error("Invalid test query");
    }
}
const char* LinkName(Link link) {
    switch(link) {
    case Link::FirstChild:return "first";
    case Link::LastChild:return "last";
    case Link::PrevSibling:return "prev";
    case Link::NextSibling:return "next";
    default:throw std::runtime_error("Invalid test link provider request");
    }
}
std::size_t Slot(Link link) {
    switch(link) {
    case Link::FirstChild:return 0u;
    case Link::LastChild:return 1u;
    case Link::PrevSibling:return 2u;
    case Link::NextSibling:return 3u;
    default:throw std::runtime_error("Invalid test link provider request");
    }
}
struct Node {
    std::size_t id{};
    std::array<Node*,4u> links{};
    Node* parent{};
    bool visible{true};
    bool operator==(const Node&)const=default;
};
class ProviderFailure final:public std::runtime_error {
public:using std::runtime_error::runtime_error;
};
struct Operations {
    std::array<Node,160u> nodes{};
    QuestVr::NativeWork::Budget budget{4096u};
    std::vector<std::string> events;
    std::function<void(Operations&,const std::string&,Node*)> callback;
    std::size_t calls{},failAt{},visibilityReads{};
    Operations() {for(std::size_t index=0u;index<nodes.size();++index) nodes[index].id=index;}
    Node& At(Node* node) {
        if(!node || node->id>=nodes.size() || &nodes[node->id]!=node)
            throw ProviderFailure("Actual Window identity unavailable");
        return *node;
    }
    void Record(const std::string& operation,Node* node) {
        events.push_back(operation+":"+(node?std::to_string(node->id):"None"));
        if(++calls==failAt) throw ProviderFailure("Injected Window provider failure");
        if(callback) callback(*this,operation,node);
    }
    void Visit(Node* node) {
        budget.Step();Record("visit",node);static_cast<void>(At(node));
    }
    Node* ReadLink(Node* node,Link link) {
        Record(std::string("read-")+LinkName(link),node);return At(node).links[Slot(link)];
    }
    bool LocalVisible(Node* node) {
        Record("local",node);++visibilityReads;return At(node).visible;
    }
    void Set(std::size_t node,Link link,std::size_t target) {
        nodes.at(node).links[Slot(link)]=target?&nodes.at(target):nullptr;
    }
};
Node* Run(Operations& operations,Query query,bool visibleOnly=true,std::size_t receiver=1u) {
    // TRUE here is an explicit caller choice. The helper has no default seed.
    return QuestVr::QueryNativeWindowRelative(&operations.nodes.at(receiver),query,visibleOnly,operations);
}
void MixedChain(Operations& operations,Query query) {
    const auto direction=Links(query);
    operations.Set(1u,direction.head,2u);
    operations.Set(2u,direction.step,3u);
    operations.Set(3u,direction.step,4u);
    operations.Set(4u,direction.step,5u);
    operations.nodes[1u].visible=false;
    operations.nodes[2u].visible=false;
    operations.nodes[3u].visible=false;
    operations.nodes[6u].visible=false;
    for(std::size_t index=2u;index<=5u;++index) operations.nodes[index].parent=&operations.nodes[6u];
}
std::vector<std::string> MixedEvents(Query query) {
    const auto direction=Links(query);
    return {"visit:1",std::string("read-")+LinkName(direction.head)+":1",
        "visit:2","local:2",std::string("read-")+LinkName(direction.step)+":2",
        "visit:3","local:3",std::string("read-")+LinkName(direction.step)+":3",
        "visit:4","local:4"};
}

void DirectionsAndEmptyLinks() {
    for(const auto query:queries) for(const bool visibleOnly:{false,true}) {
        Operations operations;
        operations.Set(1u,Link::FirstChild,2u);operations.Set(1u,Link::LastChild,4u);
        operations.Set(1u,Link::PrevSibling,5u);operations.Set(1u,Link::NextSibling,6u);
        const std::size_t expected=query==Query::TopChild?4u:query==Query::BottomChild?2u:
            query==Query::HigherSibling?6u:5u;
        const auto before=operations.nodes;
        Require(Run(operations,query,visibleOnly)==&operations.nodes[expected],"Relative query returned the wrong physical link identity");
        std::vector<std::string> events{"visit:1",std::string("read-")+LinkName(Links(query).head)+":1",
            "visit:"+std::to_string(expected)};
        if(visibleOnly) events.push_back("local:"+std::to_string(expected));
        Require(operations.events==events,"Relative query changed receiver/head/candidate/local read order");
        Require(operations.nodes==before,"Read-only query changed the Window graph");

        Operations empty;empty.nodes[1u].visible=false;
        Require(Run(empty,query,visibleOnly)==nullptr,"Empty actual link did not return exact None");
        Require(empty.events==std::vector<std::string>{"visit:1",std::string("read-")+LinkName(Links(query).head)+":1"} &&
            empty.visibilityReads==0u && empty.budget.Read().used==1u,
            "Empty query read receiver visibility, visited None or skipped receiver validation/work");
    }
}

void LocalVisibilityFiltering() {
    for(const auto query:queries) {
        Operations operations;MixedChain(operations,query);
        const auto before=operations.nodes;
        Require(Run(operations,query)==&operations.nodes[4u],"Filtering did not skip hidden candidates under the hidden parent");
        Require(operations.events==MixedEvents(query) && operations.visibilityReads==3u,
            "Filtering inspected parent/effective state, the wrong direction or nodes after the first visible candidate");
        Require(operations.nodes==before,"Visibility filtering changed source visibility or links");

        Operations unfiltered;MixedChain(unfiltered,query);
        unfiltered.callback=[](Operations&,const std::string& operation,Node*) {
            if(operation=="local") throw ProviderFailure("Unfiltered query requested visibility");
        };
        Require(Run(unfiltered,query,false)==&unfiltered.nodes[2u] && unfiltered.visibilityReads==0u,
            "Explicit false did not return the exact hidden first link without visibility reads");
        Require(unfiltered.events.size()==3u,"Unfiltered query traversed beyond the first candidate");

        Operations hidden;MixedChain(hidden,query);hidden.nodes[4u].visible=false;hidden.nodes[5u].visible=false;
        Require(Run(hidden,query)==nullptr && hidden.visibilityReads==4u,
            "An exhausted nonempty hidden chain did not return exact None");
        auto expected=MixedEvents(query);const auto step=std::string("read-")+LinkName(Links(query).step);
        expected.insert(expected.end(),{step+":4","visit:5","local:5",step+":5"});
        Require(hidden.events==expected,"All-hidden exhaustion skipped a live step or read a null candidate");
    }
}

void LiveReadsAndBetweenInvocationChanges() {
    for(const auto query:queries) {
        Operations operations;const auto direction=Links(query);
        operations.Set(1u,direction.head,2u);
        Require(Run(operations,query)==&operations.nodes[2u],"Initial actual link query failed");
        operations.Set(1u,direction.head,3u);
        Require(Run(operations,query)==&operations.nodes[3u],"A repeated query reused the previous head snapshot");
        operations.Set(1u,direction.head,0u);
        Require(Run(operations,query)==nullptr,"A repeated empty query reused a previous result");

        Operations live;MixedChain(live,query);
        live.callback=[direction](Operations& target,const std::string& operation,Node* node) {
            if(operation=="visit" && node==&target.nodes[1u]) target.Set(1u,direction.head,3u);
            if(operation=="local" && node==&target.nodes[3u]) target.Set(3u,direction.step,5u);
        };
        Require(Run(live,query)==&live.nodes[5u],"Query froze its head/next link before the live providers ran");
        Require(live.events==std::vector<std::string>{"visit:1",std::string("read-")+LinkName(direction.head)+":1",
            "visit:3","local:3",std::string("read-")+LinkName(direction.step)+":3","visit:5","local:5"},
            "Query traversed a stale head or stale successor after provider mutation");
    }

    // The script hides an icon, clears its client, then asks THAT live icon for
    // its lower sibling. Hide/callback work is not a frozen child-list query.
    Operations operations;operations.Set(1u,Link::LastChild,4u);
    operations.Set(4u,Link::PrevSibling,3u);operations.Set(3u,Link::PrevSibling,2u);
    auto* current=Run(operations,Query::TopChild);
    Require(current==&operations.nodes[4u],"Clear-augmentation fixture did not begin at top");
    current->visible=false;operations.Set(4u,Link::PrevSibling,2u);
    current=QuestVr::QueryNativeWindowRelative(current,Query::LowerSibling,true,operations);
    Require(current==&operations.nodes[2u],"Lower sibling after Hide observed an obsolete child-list snapshot");
    current->visible=false;
    Require(QuestVr::QueryNativeWindowRelative(current,Query::LowerSibling,true,operations)==nullptr,
        "Live final icon did not reach the actual None link");
}

void LongChainsAndBudgetRefusals() {
    for(const auto query:queries) {
        Operations operations;const auto direction=Links(query);constexpr std::size_t last=131u;
        operations.Set(1u,direction.head,2u);
        for(std::size_t index=2u;index<last;++index) {
            operations.nodes[index].visible=false;operations.Set(index,direction.step,index+1u);
        }
        Require(Run(operations,query)==&operations.nodes[last] && operations.visibilityReads==130u &&
            operations.budget.Read().used==131u,"Lawful long chain hit an arbitrary traversal cap or skipped per-candidate work");

        Operations cycle;MixedChain(cycle,query);cycle.Set(3u,direction.step,2u);cycle.budget.Reset(7u);
        const auto before=cycle.nodes;Node* result=&cycle.nodes[9u];bool refused{};
        try {result=Run(cycle,query);} catch(const QuestVr::NativeWork::Exhausted&) {refused=true;++refusals;}
        Require(refused && result==&cycle.nodes[9u] && cycle.nodes==before,
            "Cyclic hidden query fabricated None/result or swallowed budget refusal");
        Require(cycle.budget.Read().used==7u && cycle.budget.Read().exhausted,
            "Cyclic hidden query bypassed the enclosing cumulative work budget");

        for(const std::size_t maximum:{0u,1u}) {
            Operations limited;limited.Set(1u,direction.head,2u);limited.budget.Reset(maximum);bool failed{};
            try {static_cast<void>(Run(limited,query,false));}
            catch(const QuestVr::NativeWork::Exhausted&) {failed=true;++refusals;}
            Require(failed && limited.visibilityReads==0u && limited.budget.Read().used==maximum,
                "Unfiltered query skipped receiver/candidate admission before returning a pointer");
        }
        // A visible repeated node is a legitimate immediate result; only work
        // exhaustion, not a visited set, bounds hidden cycles in this helper.
        Operations repeated;repeated.Set(1u,direction.head,1u);
        Require(Run(repeated,query)==&repeated.nodes[1u] && repeated.budget.Read().used==2u,
            "An immediate visible repeated identity was rejected as a synthetic cycle");
    }
}

void ProviderFailuresAndInvalidQueries() {
    for(const auto query:queries) for(const bool visibleOnly:{false,true}) {
        Operations baseline;MixedChain(baseline,query);static_cast<void>(Run(baseline,query,visibleOnly));
        for(std::size_t failure=1u;failure<=baseline.calls;++failure) {
            Operations operations;MixedChain(operations,query);operations.failAt=failure;
            const auto before=operations.nodes;Node* result=&operations.nodes[9u];bool refused{};
            try {result=Run(operations,query,visibleOnly);}
            catch(const ProviderFailure& error) {refused=std::string(error.what())=="Injected Window provider failure";++refusals;}
            Require(refused && result==&operations.nodes[9u],"Unavailable provider was swallowed or converted into an empty query");
            Require(operations.events==std::vector<std::string>(baseline.events.begin(),baseline.events.begin()+failure) &&
                operations.calls==failure,"Query executed an operation after a provider failure");
            Require(operations.nodes==before,"Query/provider failure mutated the physical Window graph");
        }
    }
    for(const auto query:queries) {
        for(const bool nullReceiver:{false,true}) {
            Operations operations;Node foreign;foreign.id=1u;bool refused{};
            try {static_cast<void>(QuestVr::QueryNativeWindowRelative(nullReceiver?nullptr:&foreign,query,true,operations));}
            catch(const ProviderFailure&) {refused=true;++refusals;}
            Require(refused && operations.events==std::vector<std::string>{nullReceiver?"visit:None":"visit:1"},
                "Invalid receiver became None or reached a link read before provider validation");
        }
        Operations operations;MixedChain(operations,query);Node foreign;foreign.id=1u;
        operations.nodes[1u].links[Slot(Links(query).head)]=&foreign;bool refused{};
        try {static_cast<void>(Run(operations,query));} catch(const ProviderFailure&) {refused=true;++refusals;}
        Require(refused && operations.visibilityReads==0u && operations.events.size()==3u,
            "Invalid linked candidate was returned or inspected before Visit validation");
    }
    for(const auto query:{static_cast<Query>(-1),static_cast<Query>(99)}) {
        Operations operations;bool refused{};
        try {static_cast<void>(Run(operations,query));} catch(const std::invalid_argument&) {refused=true;++refusals;}
        Require(refused && operations.events.empty() && operations.budget.Read().used==0u,
            "Unknown relative-query enum selected a default link or invoked a provider");
    }
}
} // namespace

int main() {
    try {
        DirectionsAndEmptyLinks();LocalVisibilityFiltering();LiveReadsAndBetweenInvocationChanges();
        LongChainsAndBudgetRefusals();ProviderFailuresAndInvalidQueries();
        std::cout<<"Native Window relative queries passed: "<<checks<<" checks, "<<refusals<<" refusals\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
