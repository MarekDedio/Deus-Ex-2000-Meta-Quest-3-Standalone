#include "quest_native_tile_layout.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Independent small, hand-computed protocol fixtures. This is an EXPLICIT
// MOCK transaction/provider, not a VM journal, native UObject graph, general
// arithmetic oracle, original Training startup or headset acceptance.
namespace {
using Row=QuestVr::NativeTileRow;
using Child=std::uintptr_t;
std::size_t checks{},cases{},refusals{};
void Require(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
struct Node {
    bool visible{true},alive{true};Child previous{},next{};
    float width{10.0f},height{5.0f};
    std::map<std::string,float> held{{"holdX",-0.0f},{"holdY",-31.0f},{"holdWidth",-41.0f},{"holdHeight",-51.0f}};
};
struct State {
    Child first{},last{};std::map<Child,Node> nodes;
    std::map<std::string,std::uint8_t> bytes{{"orientation",0u},{"hDirection",0u},{"vDirection",0u},{"hChildAlign",0u},{"vChildAlign",0u}};
    std::map<std::string,bool> flags{{"bWrap",false},{"bFillParent",false},{"bEqualWidth",false},{"bEqualHeight",false}};
    std::map<std::string,float> floats{{"hMargin",2.0f},{"vMargin",3.0f},{"minorSpacing",1.0f},{"majorSpacing",4.0f}};
    std::vector<Row> rows{{99,-12,4}};
};
bool Exact(const State& a,const State& b){
    if(a.first!=b.first||a.last!=b.last||a.bytes!=b.bytes||a.flags!=b.flags||a.rows!=b.rows||a.nodes.size()!=b.nodes.size()||a.floats.size()!=b.floats.size())return false;
    for(const auto& [key,value]:a.floats){const auto found=b.floats.find(key);if(found==b.floats.end()||Bits(value)!=Bits(found->second))return false;}
    for(const auto& [id,node]:a.nodes){const auto found=b.nodes.find(id);if(found==b.nodes.end())return false;const auto& other=found->second;
        if(node.visible!=other.visible||node.alive!=other.alive||node.previous!=other.previous||node.next!=other.next||
            Bits(node.width)!=Bits(other.width)||Bits(node.height)!=Bits(other.height)||node.held.size()!=other.held.size())return false;
        for(const auto& [key,value]:node.held){const auto held=other.held.find(key);if(held==other.held.end()||Bits(value)!=Bits(held->second))return false;}
    }return true;
}
struct Query {Child child{};char kind{};float constraint{};};
struct Operations {
    using Child=::Child;
    State state;
    std::vector<std::string> trace;
    std::vector<Query> queries;
    std::function<void(Operations&,Child,char)> callback;
    std::size_t work{},writes{},workLimit{10000u},writeLimit{10000u};
    bool poisoned{},active{},committed{},rolledBack{};
    void Step(){if(poisoned)throw std::runtime_error("Mock Tile root already failed");
        if(work>=workLimit){poisoned=true;throw std::runtime_error("Mock Tile work limit");}++work;}
    void Store(const std::string& event){if(poisoned)throw std::runtime_error("Mock Tile root already failed");
        if(writes>=writeLimit){poisoned=true;throw std::runtime_error("Mock Tile write limit");}++writes;trace.push_back(event);}
    Node& Get(Child id){const auto found=state.nodes.find(id);
        if(!id||found==state.nodes.end()||!found->second.alive){poisoned=true;throw std::runtime_error("Mock Tile dangling/dead child");}
        return found->second;}
    std::uint8_t ReadTileByte(const char* name){trace.push_back("byte:"+std::string(name));return state.bytes.at(name);}
    bool ReadTileBool(const char* name){trace.push_back("bool:"+std::string(name));return state.flags.at(name);}
    QuestVr::NativeTileFlags ReadTileFlags(){trace.emplace_back("packed");return {state.flags.at("bWrap"),state.flags.at("bFillParent"),
        state.flags.at("bEqualWidth"),state.flags.at("bEqualHeight")};}
    float ReadTileFloat(const char* name){trace.push_back("float:"+std::string(name));return state.floats.at(name);}
    float ReadHeldFloat(Child child,const char* name){trace.push_back("r:"+std::to_string(child)+'.'+name);return Get(child).held.at(name);}
    void WriteHeldFloat(Child child,const char* name,float value){auto& node=Get(child);Store("w:"+std::to_string(child)+'.'+name);node.held.at(name)=value;}
    bool ReadChildVisible(Child child){trace.push_back("visible:"+std::to_string(child));return Get(child).visible;}
    Child LastChild(){trace.emplace_back("last");return state.last;}
    Child PreviousSibling(Child child){trace.push_back("prev:"+std::to_string(child));return Get(child).previous;}
    Child FirstVisibleChild(){trace.emplace_back("first-visible");return Visible(state.first);}
    Child NextVisibleSibling(Child child){trace.push_back("next-visible:"+std::to_string(child));return Visible(Get(child).next);}
    Child Visible(Child child){while(child){Step();auto& node=Get(child);if(node.visible)return child;child=node.next;}return {};}
    void QueryPreferredSize(Child child){
        trace.push_back("qS:"+std::to_string(child));queries.push_back({child,'S',0.0f});Get(child);
        if(callback)callback(*this,child,'S');
        const auto width=Get(child).width,height=Get(child).height;
        WriteHeldFloat(child,"holdWidth",width);WriteHeldFloat(child,"holdHeight",height);
    }
    float QueryPreferredWidth(Child child,float constraint){
        trace.push_back("qW:"+std::to_string(child));queries.push_back({child,'W',constraint});
        Require(Bits(Get(child).held.at("holdHeight"))==Bits(constraint),"Fixed child height was not stored before its width query");
        if(callback)callback(*this,child,'W');
        return Get(child).width;
    }
    float QueryPreferredHeight(Child child,float constraint){
        trace.push_back("qH:"+std::to_string(child));queries.push_back({child,'H',constraint});
        Require(Bits(Get(child).held.at("holdWidth"))==Bits(constraint),"Fixed child width was not stored before its height query");
        if(callback)callback(*this,child,'H');
        return Get(child).height;
    }
    void ClearRows(){Store("rows:clear");state.rows.clear();}
    void AppendRow(Row row){Store("rows:append");state.rows.push_back(row);}
    Row ReadRow(std::size_t index){trace.push_back("rows:read:"+std::to_string(index));return state.rows.at(index);}
    void WriteRowChildCount(std::size_t index,std::int32_t count){Store("rows:count:"+std::to_string(index));state.rows.at(index).childCount=count;}
    std::size_t RowCount(){trace.emplace_back("rows:size");return state.rows.size();}
    void ParentByte(const char* name,std::uint8_t value){Store("callback:byte:"+std::string(name));state.bytes.at(name)=value;}
    void ParentBool(const char* name,bool value){Store("callback:bool:"+std::string(name));state.flags.at(name)=value;}
    void ParentFloat(const char* name,float value){Store("callback:float:"+std::string(name));state.floats.at(name)=value;}
    void VisibleChild(Child child,bool value){Store("callback:visible:"+std::to_string(child));Get(child).visible=value;}
    void Previous(Child child,Child value){Store("callback:prev:"+std::to_string(child));Get(child).previous=value;}
    void Next(Child child,Child value){Store("callback:next:"+std::to_string(child));Get(child).next=value;}
};
Operations Fixture(std::initializer_list<std::pair<float,float>> sizes={{10,5},{20,7},{6,3}}){
    Operations operations;Child previous{};
    for(const auto& [width,height]:sizes){const auto id=static_cast<Child>(operations.state.nodes.size()+1u);Node node;
        node.previous=previous;node.width=width;node.height=height;operations.state.nodes.emplace(id,node);
        if(previous)operations.state.nodes.at(previous).next=id;else operations.state.first=id;previous=id;}
    operations.state.last=previous;return operations;
}
void Compute(Operations& operations,bool ws,float& width,bool hs,float& height){
    QuestVr::ComputeNativeTileChildSizes(operations,ws,width,hs,height);
}
bool Transaction(Operations& operations,bool ws,float& width,bool hs,float& height,bool late=false,bool caught=false){
    Require(!operations.active,"Nested explicit mock Tile transaction");operations.active=true;
    const auto before=operations.state;const auto oldWidth=width,oldHeight=height;
    try{if(caught){try{Compute(operations,ws,width,hs,height);}catch(...){operations.poisoned=true;}}
        else Compute(operations,ws,width,hs,height);
        if(late)throw std::runtime_error("Mock late Tile coordinator failure");
        if(operations.poisoned)throw std::runtime_error("Caught Tile failure poisoned its mock root");
        operations.active=false;operations.committed=true;++cases;return true;
    }catch(...){operations.state=before;width=oldWidth;height=oldHeight;operations.active=false;operations.rolledBack=true;
        ++cases;++refusals;return false;}
}
void Layout(Operations& operations,bool ws,float& width,bool hs,float& height){Require(Transaction(operations,ws,width,hs,height),"Finite mock Tile layout refused");}
void Rows(const Operations& operations,std::initializer_list<Row> expected){Require(operations.state.rows==std::vector<Row>(expected),"Literal Tile row records differ");}
void Held(const Operations& operations,Child child,float x,float y,float width,float height){const auto& held=operations.state.nodes.at(child).held;
    Require(Bits(held.at("holdX"))==Bits(x)&&Bits(held.at("holdY"))==Bits(y)&&Bits(held.at("holdWidth"))==Bits(width)&&Bits(held.at("holdHeight"))==Bits(height),
        "Literal Tile held geometry differs for child "+std::to_string(child));}
std::vector<std::string> Selected(const Operations& operations,const std::function<bool(const std::string&)>& select){
    std::vector<std::string> result;for(const auto& event:operations.trace)if(select(event))result.push_back(event);return result;}
void EmptyHiddenAndOrdering(){
    {auto operations=Fixture({});float w=91,h=92;Layout(operations,false,w,false,h);Rows(operations,{});
        Require(w==4&&h==6&&operations.queries.empty(),"Empty Tile fabricated a row/query or lost doubled margins");}
    {auto operations=Fixture({{1,1},{2,2},{3,3},{4,4},{5,5},{6,6},{7,7},{8,8},{9,9},{10,10}});
        for(auto& [id,node]:operations.state.nodes)node.visible=false;
        float w=91,h=92;Layout(operations,false,w,false,h);Rows(operations,{});Require(w==4&&h==6&&operations.queries.empty(),"All-hidden Tile queried a child");
        std::vector<std::string> expected;
        for(unsigned id=10u;id>0u;--id)for(const auto* name:{"holdWidth","holdHeight","holdX","holdY"})expected.push_back("w:"+std::to_string(id)+'.'+name);
        const auto actual=Selected(operations,[](const auto& event){return event.starts_with("w:");});
        Require(actual==expected&&actual.size()==40u,"Ten hidden children lost original forty zero-store order");
        for(const auto& [id,node]:operations.state.nodes)for(const auto& [name,value]:node.held)Require(Bits(value)==0u,"Hidden held field was not positive zero");}
    {auto operations=Fixture();operations.state.nodes.at(2).visible=false;float w=91,h=92;Layout(operations,false,w,false,h);
        Rows(operations,{{5,17,2}});Require(w==21&&h==11,"Mixed local visibility changed content size");
        Require(operations.queries.size()==2u&&operations.queries[0u].child==3u&&operations.queries[1u].child==1u,"Queries did not visit reverse live sibling order");
        const auto writes=Selected(operations,[](const auto& event){return event.starts_with("w:");});
        Require(writes==std::vector<std::string>{"w:3.holdWidth","w:3.holdHeight","w:2.holdWidth","w:2.holdHeight","w:2.holdX","w:2.holdY","w:1.holdWidth","w:1.holdHeight"},
            "Mixed hidden/visible stores were reordered or globally copied out");}
    {auto operations=Fixture();float w=100,h=60;Layout(operations,true,w,true,h);Rows(operations,{{7,38,0}});
        Held(operations,1,2,3,10,5);Held(operations,2,13,3,20,7);Held(operations,3,34,3,6,3);
        const auto placement=Selected(operations,[](const auto& event){return event.starts_with("w:")&&(event.ends_with("holdX")||event.ends_with("holdY"));});
        Require(placement==std::vector<std::string>{"w:1.holdY","w:1.holdX","w:2.holdY","w:2.holdX","w:3.holdY","w:3.holdX"},
            "Placement was not forward or lost original Y-before-X stores");Require(w==100&&h==60,"Both-specified layout overwrote preferred outputs");}
}
void ByteBranches(){
    for(unsigned value=0u;value<256u;++value){
        {auto operations=Fixture();operations.state.bytes.at("orientation")=static_cast<std::uint8_t>(value);float w=91,h=92;
            Layout(operations,false,w,false,h);if(value==1u){Rows(operations,{{20,17,3}});Require(w==24&&h==23,"Vertical Byte1 sizing differs");}
            else{Rows(operations,{{7,38,3}});Require(w==42&&h==13,"Non1 orientation Byte did not take horizontal branch");}}
        for(const bool vertical:{false,true})for(const bool horizontalAxis:{false,true}){
            auto operations=Fixture({{10,5},{17,12}});operations.state.bytes.at("orientation")=vertical?1u:0u;
            operations.state.bytes.at(horizontalAxis?"hDirection":"vDirection")=static_cast<std::uint8_t>(value);
            float w=100,h=60;Layout(operations,true,w,true,h);const bool reverse=value==1u;
            if(vertical){Held(operations,1,horizontalAxis&&reverse?81.0f:2.0f,!horizontalAxis&&reverse?52.0f:3.0f,10,5);
                Held(operations,2,horizontalAxis&&reverse?81.0f:2.0f,!horizontalAxis&&reverse?39.0f:9.0f,17,12);}
            else{Held(operations,1,horizontalAxis&&reverse?88.0f:2.0f,!horizontalAxis&&reverse?45.0f:3.0f,10,5);
                Held(operations,2,horizontalAxis&&reverse?70.0f:13.0f,!horizontalAxis&&reverse?45.0f:3.0f,17,12);}
        }
        for(const bool vertical:{false,true}){
            auto operations=Fixture({{10,5},{17,12}});operations.state.bytes.at("orientation")=vertical?1u:0u;
            operations.state.bytes.at(vertical?"hChildAlign":"vChildAlign")=static_cast<std::uint8_t>(value);
            // Also set the inactive alignment to a hostile raw Byte: only the
            // current orientation's cross-axis alignment is applicable.
            operations.state.bytes.at(vertical?"vChildAlign":"hChildAlign")=255u;
            float w=100,h=60;Layout(operations,true,w,true,h);
            const float offset=value==1u?3.0f:value==2u?7.0f:0.0f;
            if(vertical){Held(operations,1,2+offset,3,value==3u?17.0f:10.0f,5);Held(operations,2,2,9,17,12);}
            else{Held(operations,1,2,3+offset,10,value==3u?12.0f:5.0f);Held(operations,2,13,3,17,12);}
        }
    }
}
void WrapFillEquality(){
    for(const bool vertical:{false,true})for(const bool wrap:{false,true})for(const bool fill:{false,true})
        for(const bool ws:{false,true})for(const bool hs:{false,true}){
        auto operations=Fixture({{10,5},{20,7}});operations.state.bytes.at("orientation")=vertical?1u:0u;
        operations.state.flags.at("bWrap")=wrap;operations.state.flags.at("bFillParent")=fill;
        float w=100,h=60;Layout(operations,ws,w,hs,h);
        const bool fixed=!wrap&&fill&&(vertical?ws:hs);
        for(const auto& query:operations.queries){Require(query.kind==(fixed?(vertical?'H':'W'):'S'),"Fill fixed-axis query branch changed");
            if(fixed)Require(query.constraint==(vertical?96.0f:54.0f),"Fill constraint did not subtract doubled cross margin");}
        if(fixed&&vertical){Rows(operations,{{96,13,ws&&hs?0:2}});if(!ws)Require(w==100,"Inactive width output unexpectedly changed");if(!hs)Require(h==19,"Fixed-width content height differs");}
        else if(fixed){Rows(operations,{{54,31,ws&&hs?0:2}});if(!ws)Require(w==35,"Fixed-height content width differs");}
        else if(vertical){Rows(operations,{{20,13,ws&&hs?0:2}});if(!ws)Require(w==24,"Vertical preferred width differs");if(!hs)Require(h==19,"Vertical preferred height differs");}
        else{Rows(operations,{{7,31,ws&&hs?0:2}});if(!ws)Require(w==35,"Horizontal preferred width differs");if(!hs)Require(h==13,"Horizontal preferred height differs");}
        if(ws)Require(w==100,"Specified width was overwritten");
        if(hs)Require(h==60,"Specified height was overwritten");
    }
    for(const bool vertical:{false,true})for(const bool ew:{false,true})for(const bool eh:{false,true}){
        auto operations=Fixture({{10,5},{20,7}});operations.state.bytes.at("orientation")=vertical?1u:0u;
        operations.state.flags.at("bEqualWidth")=ew;operations.state.flags.at("bEqualHeight")=eh;
        float w=91,h=92;Layout(operations,false,w,false,h);
        Require(operations.state.nodes.at(1).held.at("holdWidth")== (ew?20.0f:10.0f)&&
            operations.state.nodes.at(1).held.at("holdHeight")== (eh?7.0f:5.0f),"Fresh equality maxima were not applied forward");
        const auto stores=Selected(operations,[](const auto& event){return event.starts_with("w:");});
        std::vector<std::string> expected{"w:2.holdWidth","w:2.holdHeight","w:1.holdWidth","w:1.holdHeight"};
        for(unsigned id:{1u,2u}){if(ew)expected.push_back("w:"+std::to_string(id)+".holdWidth");if(eh)expected.push_back("w:"+std::to_string(id)+".holdHeight");}
        Require(stores==expected,"Equality stores lost first-child order or width-before-height");
    }
    {auto operations=Fixture({{10,5},{20,7},{6,3}});operations.state.flags.at("bWrap")=true;float w=35,h=92;
        Layout(operations,true,w,false,h);Rows(operations,{{7,31,2},{3,6,1}});Require(w==35&&h==20,"Exact wrap equality or row major spacing differs");}
    {auto operations=Fixture({{1000000,5},{1,7}});operations.state.floats.at("minorSpacing")=0;float w=0,h=0;
        Layout(operations,false,w,false,h);Rows(operations,{{5,1000000,1},{7,1,1}});Require(w==1000004&&h==22,
            "bWrap=false lost finite million-unit sentinel wrap or invented an empty initial row");}
    {auto operations=Fixture({{10,5},{20,7}});operations.state.flags.at("bWrap")=true;float w=-5,h=0;
        Layout(operations,true,w,false,h);Rows(operations,{{5,10,1},{7,20,1}});Require(w==-5&&h==22,"Negative wrap bound was clamped or rejected");}
    for(const bool vertical:{false,true}){auto operations=Fixture({{10,5}});operations.state.bytes.at("orientation")=vertical?1u:0u;
        operations.state.flags.at("bFillParent")=true;float w=1,h=1;Layout(operations,true,w,true,h);
        Require(operations.queries.size()==1u&&Bits(operations.queries[0u].constraint)==0u,"Negative fill constraint was not clamped to positive zero");}
}
void IntegerRowsAndReplacement(){
    {auto operations=Fixture({{1.75f,2.75f},{1.75f,2.75f},{1.75f,2.75f}});operations.state.floats.at("minorSpacing")=0;
        float w=0,h=0;Layout(operations,false,w,false,h);Rows(operations,{{2,3,3}});Require(w==7&&h==8,"Tile failed per-child INT truncation");}
    {auto operations=Fixture({{-2.75f,-3.5f},{0.5f,0.25f}});operations.state.floats.at("minorSpacing")=-0.5f;
        operations.state.floats.at("hMargin")=-1;operations.state.floats.at("vMargin")=-2;
        float w=0,h=0;Layout(operations,false,w,false,h);Rows(operations,{{0,-2,2}});Require(w==-2&&h==-4,"Finite negative sizes/spacings/margins were clamped or accumulated as Float");}
    {auto operations=Fixture({{0,0},{0,0}});operations.state.floats.at("hMargin")=-0.0f;operations.state.floats.at("vMargin")=-0.0f;
        operations.state.floats.at("minorSpacing")=-0.0f;float w=91,h=92;Layout(operations,false,w,false,h);Rows(operations,{{0,0,2}});
        Require(Bits(w)==0u&&Bits(h)==0u,"Zero rows or doubled signed-zero margin output differs");}
    {auto operations=Fixture();float w=0,h=0;Layout(operations,false,w,false,h);Rows(operations,{{7,38,3}});
        operations.committed=false;operations.state.nodes.at(2).visible=false;operations.state.nodes.at(3).visible=false;
        w=100;h=60;Layout(operations,true,w,true,h);Rows(operations,{{5,10,0}});Held(operations,1,2,3,10,5);
        Require(operations.state.rows.size()==1u,"Second call retained old native rows or unconsumed placement counts");}
}
void LiveCallbacks(){
    {auto operations=Fixture({{10,5},{20,7}});operations.callback=[](Operations& self,Child child,char){
            if(child==2u){self.ParentBool("bEqualWidth",true);self.ParentBool("bEqualHeight",true);}
            if(child==1u)self.WriteHeldFloat(2u,"holdWidth",30.0f);};
        float w=0,h=0;Layout(operations,false,w,false,h);Rows(operations,{{7,41,2}});
        Require(w==45&&h==13&&operations.state.nodes.at(2).held.at("holdWidth")==20,
            "Equality rereads did not capture flags after query or did not overwrite later held mutation with the actual captured maximum");}
    {auto operations=Fixture({{10,5},{20,7}});operations.callback=[](Operations& self,Child child,char){if(child==2u){
            self.ParentByte("orientation",1u);self.ParentBool("bWrap",true);self.ParentFloat("vMargin",1.0f);self.VisibleChild(1u,false);}};
        float w=91,h=92;Layout(operations,false,w,false,h);Rows(operations,{{20,7,1}});
        Require(w==24&&h==9&&operations.queries.size()==1u,"Post-query orientation/margins/visibility were snapshotted too early");
        Held(operations,1,0,0,0,0);}
    {auto operations=Fixture();operations.callback=[](Operations& self,Child child,char){if(child==3u)self.Previous(3u,1u);};
        float w=0,h=0;Layout(operations,false,w,false,h);
        Require(operations.queries.size()==2u&&operations.queries[0].child==3u&&operations.queries[1].child==1u,
            "Reverse pass used a precollected predecessor rather than its live link");
        // Child2 remains forward-visible and keeps its genuine pre-query held
        // negative values. This is a deliberately inconsistent but live graph,
        // not permission to silently synthesize its missing query.
        Require(operations.state.nodes.at(2).held.at("holdWidth")==-41.0f,"Skipped reverse child was fabricated/queried");}
    {auto operations=Fixture();operations.callback=[](Operations& self,Child child,char){if(child==1u){self.Next(1u,3u);self.VisibleChild(2u,false);}};
        float w=0,h=0;Layout(operations,false,w,false,h);Rows(operations,{{5,17,2}});Require(w==21&&h==11,
            "Forward row pass ignored callback-updated sibling links/visibility");}
    {auto operations=Fixture({{10,5},{20,7}});operations.state.bytes.at("orientation")=1u;operations.state.flags.at("bFillParent")=true;
        operations.callback=[](Operations& self,Child child,char kind){Require(kind=='H',"Initial fixed-width query mode changed after callback");
            if(child==2u){self.ParentByte("orientation",0u);self.ParentBool("bFillParent",false);self.ParentBool("bWrap",true);}};
        float w=30,h=0;Layout(operations,true,w,false,h);Rows(operations,{{26,26,1},{26,26,1}});Require(w==30&&h==62,
            "Captured initial fill/query constraint or post-callback row orientation was recomputed incorrectly");
        Require(operations.queries[0].constraint==26&&operations.queries[1].constraint==26,"Initial fill constraint changed during reverse callbacks");}
}
void RefusalAndRollback(){
    const auto exercise=[](Operations operations,bool placement){float w=placement?35.0f:91.0f,h=placement?60.0f:92.0f;
        const auto baseline=operations.state;const auto oldW=w,oldH=h;auto warm=operations;float warmW=w,warmH=h;
        Require(Transaction(warm,placement,warmW,placement,warmH),"Warm mock Tile budget witness refused");
        for(std::size_t cut=0u;cut<warm.writes;++cut){auto limited=operations;limited.writeLimit=cut;float cw=w,ch=h;
            Require(!Transaction(limited,placement,cw,placement,ch)&&limited.writes==cut&&limited.rolledBack&&Exact(limited.state,baseline)&&
                Bits(cw)==Bits(oldW)&&Bits(ch)==Bits(oldH),"Mock Tile write cut retained held/row/callback/output state");}
        for(std::size_t cut=0u;cut<warm.work;++cut){auto limited=operations;limited.workLimit=cut;float cw=w,ch=h;
            Require(!Transaction(limited,placement,cw,placement,ch,true,true)&&limited.work==cut&&limited.rolledBack&&Exact(limited.state,baseline)&&
                Bits(cw)==Bits(oldW)&&Bits(ch)==Bits(oldH),"Mock Tile work cut/caught poison retained a partial root");}
        auto late=operations;Require(!Transaction(late,placement,w,placement,h,true)&&late.rolledBack&&Exact(late.state,baseline)&&
            Bits(w)==Bits(oldW)&&Bits(h)==Bits(oldH),"Late mock Tile root failure failed complete row/geometry/output undo");
    };
    auto rich=Fixture();rich.state.flags.at("bWrap")=true;rich.state.flags.at("bEqualWidth")=true;rich.state.flags.at("bEqualHeight")=true;
    exercise(rich,false);exercise(rich,true);
    auto mutating=Fixture();mutating.callback=[](Operations& self,Child child,char){if(child==3u){self.ParentBool("bEqualWidth",true);self.VisibleChild(1u,false);}};
    exercise(mutating,false);
    for(const unsigned mode:{0u,1u,2u,3u}){auto operations=Fixture();operations.workLimit=100u;
        if(mode==0u)operations.state.nodes.at(1u).previous=3u;
        if(mode==1u)operations.state.nodes.at(3u).next=1u;
        if(mode==2u)operations.state.nodes.at(3u).previous=999u;
        if(mode==3u)operations.callback=[](Operations& self,Child child,char){if(child==3u){self.Store("callback:dead");self.Get(3u).alive=false;}};
        const auto before=operations.state;float w=91,h=92;
        Require(!Transaction(operations,false,w,false,h)&&operations.rolledBack&&Exact(operations.state,before)&&w==91&&h==92,
            "Cyclic/dangling/dead live Tile graph did not refuse boundedly and restore its fixture");}
    for(const auto* field:{"hMargin","vMargin","minorSpacing","majorSpacing"}){
        auto operations=Fixture();operations.state.floats.at(field)=std::numeric_limits<float>::infinity();const auto before=operations.state;
        operations.state.flags.at("bWrap")=true;const auto active=operations.state;
        float w=15,h=92;Require(!Transaction(operations,true,w,false,h)&&Exact(operations.state,active),"Reached nonfinite Tile field was silently accepted/clamped");}
    {auto operations=Fixture();operations.callback=[](Operations&,Child child,char){if(child==1u)throw std::runtime_error("Mock late child query refusal");};
        const auto before=operations.state;float w=91,h=92;Require(!Transaction(operations,false,w,false,h)&&Exact(operations.state,before)&&operations.queries.size()==3u,
            "Late child query failure continued layout or leaked earlier reverse-query writes");}
}
} // namespace
int main(){try{EmptyHiddenAndOrdering();ByteBranches();WrapFillEquality();IntegerRowsAndReplacement();LiveCallbacks();RefusalAndRollback();
    std::cout<<"PASS EXPLICIT MOCK Tile layout protocol: "<<checks<<" checks / "<<cases<<" transactions / "<<refusals
        <<" refusals; literal geometry/row expectations, all raw Byte branches, reverse live queries/forward placement, forty hidden stores, "
        "wrap/fill/equality/reentry, bounded graph refusal and mock shared-cut/late rollback. NOT x87 oracle/native VM/owned HUD/startup/Quest acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL mock Tile layout protocol: "<<error.what()<<'\n';return 1;}}
