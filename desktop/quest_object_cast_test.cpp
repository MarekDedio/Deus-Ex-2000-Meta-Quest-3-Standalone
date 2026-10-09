#include "quest_object_cast.h"

#include <iostream>
#include <map>

namespace {
using namespace QuestVr;
using Vm::Kind;
std::size_t checks{},rejections{};
void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& operation,const std::string& message) {
    bool rejected{};
    try { operation(); } catch (const std::runtime_error&) { rejected=true; }
    Require(rejected,message); ++rejections;
}
std::string Key(std::string value) {
    for (auto& c:value) if (c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
    return value;
}
Vm::Value Object(const std::string& path={}) { return Vm::Value::Text(Kind::Object,path); }
struct Fixture {
    std::map<std::string,ObjectCastClass> classes;
    std::map<std::string,ObjectCastObject> objects;
    std::vector<std::string> observations;
    ObjectCastResolvers resolvers;
    Fixture() {
        resolvers.resolveClass=[this](const std::string& path) {
            observations.push_back("class:"+path);
            const auto found=classes.find(Key(path));
            if (found==classes.end()) throw std::runtime_error("Missing/non-class declaration: "+path);
            return found->second;
        };
        resolvers.resolveObject=[this](const std::string& path) {
            observations.push_back("object:"+path);
            const auto found=objects.find(Key(path));
            if (found==objects.end()) throw std::runtime_error("Missing operand: "+path);
            return found->second;
        };
        Class("Core.Object","Object");Class("Core.Field","Field","Core.Object");
        Class("Core.Struct","Struct","Core.Field");Class("Core.State","State","Core.Struct");
        Class("Core.Class","Class","Core.State");
        Class("Engine.Actor","Actor","Core.Object");Class("Engine.Pawn","Pawn","Engine.Actor");
        Class("Engine.Inventory","Inventory","Engine.Actor");
        Class("Engine.Ammo","Ammo","Engine.Inventory");
        Class("Game.Pistol","Pistol","Engine.Inventory");
        Class("Game.PawnChild","PawnChild","Engine.Pawn");
        Class("Other.Pawn","pAwN","Core.Object");
        Instance("Map.Pawn","Game.PawnChild");Instance("Map.Pistol","Game.Pistol");
        Instance("Map.Collision","Other.Pawn");
    }
    void Class(const std::string& path,const std::string& name,const std::string& base={}) {
        classes[Key(path)]={path,name,base};objects[Key(path)]={path,"Core.Class",true};
    }
    void Instance(const std::string& path,const std::string& cls) { objects[Key(path)]={path,cls,false}; }
    Vm::Value Cast(const std::string& target,const Vm::Value& value,bool meta,std::size_t limit=128u) {
        return CastAuthoredObject(target,value,meta,resolvers,limit);
    }
};
void Result(const Vm::Value& value,const std::string& path,const std::string& message) {
    Require(value.kind==Kind::Object && value.text==path && value.fields.empty(),message);
}

void NullAndKinds() {
    Fixture fixture;
    for (const bool meta:{false,true}) for (const auto& value:{Vm::Value{},Object()}) {
        fixture.observations.clear();Result(fixture.Cast("Engine.Pawn",value,meta),{},"Null cast is not Object None");
        Require(fixture.observations==std::vector<std::string>{"class:Engine.Pawn"},
            "Null cast did not resolve target first or unnecessarily resolved operand");
    }
    for (const auto kind:{Kind::Byte,Kind::Int,Kind::Bool,Kind::Float,Kind::Name,Kind::String,
            Kind::Vector,Kind::Rotator,Kind::Struct}) for (const bool meta:{false,true}) {
        Vm::Value value;value.kind=kind;fixture.observations.clear();
        Reject([&] {fixture.Cast("Engine.Pawn",value,meta);},"Wrong cast operand kind accepted");
        Require(fixture.observations==std::vector<std::string>{"class:Engine.Pawn"},
            "Wrong-kind cast skipped target validation or resolved an operand object");
    }
    Reject([&] {fixture.Cast("Map.Pawn",Object(),false);},"Instance accepted as target class");
    Reject([&] {fixture.Cast("Missing.Class",Vm::Value{},true);},"Nothing hid missing target");
    Reject([&] {fixture.Cast({},Object(),false);},"Null class target accepted");
    fixture.resolvers.resolveObject={};
    Result(fixture.Cast("Engine.Pawn",Object(),false),{},"Unused object resolver required for None");
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Map.Pawn"),false);},"Missing object resolver accepted");
    fixture.resolvers.resolveClass={};
    Reject([&] {fixture.Cast("Engine.Pawn",Object(),false);},"Missing class resolver accepted");
}
void Membership() {
    Fixture fixture;
    Result(fixture.Cast("engine.pawn",Object("gAmE.pAwNcHiLd"),true),"Game.PawnChild","Inherited MetaCast lost canonical operand");
    Result(fixture.Cast("Game.PawnChild",Object("Game.PawnChild"),true),"Game.PawnChild","Exact MetaCast failed");
    Result(fixture.Cast("Engine.Ammo",Object("Game.Pistol"),true),{},"Unrelated class MetaCast fabricated Ammo");
    Result(fixture.Cast("Engine.Pawn",Object("Map.Pawn"),true),{},"MetaCast used instance Class ancestry");
    Result(fixture.Cast("Engine.Pawn",Object("map.pawn"),false),"Map.Pawn","DynamicCast did not use actual Class ancestry");
    Result(fixture.Cast("Game.PawnChild",Object("Map.Pawn"),false),"Map.Pawn","Exact instance DynamicCast failed");
    Result(fixture.Cast("Engine.Ammo",Object("Map.Pistol"),false),{},"DynamicCast accepted unrelated instance");
    Result(fixture.Cast("Engine.Pawn",Object("Map.Collision"),false),"Map.Collision","DynamicCast wrongly required qualified target identity");
    Result(fixture.Cast("Engine.Pawn",Object("Other.Pawn"),true),{},"MetaCast accepted namespace/name collision");
    Result(fixture.Cast("Core.Class",Object("Game.PawnChild"),false),"Game.PawnChild","Class object's metaclass rejected");
    Result(fixture.Cast("Core.Struct",Object("Game.PawnChild"),false),"Game.PawnChild","Native metaclass intermediate ancestry lost");
    Result(fixture.Cast("Core.Object",Object("Game.PawnChild"),false),"Game.PawnChild","Native metaclass root ancestry lost");
    Result(fixture.Cast("Engine.Pawn",Object("Game.PawnChild"),false),{},"DynamicCast used represented class ancestry");
    const auto source=Object("Map.Pawn");fixture.Cast("Engine.Pawn",source,false);
    Require(source.kind==Kind::Object && source.text=="Map.Pawn" && source.fields.empty(),"Cast mutated input value");
}
void LiteralNames() {
    Fixture fixture;
    // No FriendlyName exists in the cast metadata API. A path-derived name or
    // friendly display name cannot substitute for the explicit declaration Name.
    fixture.Class("Fixture.Target","Literal.Name","Core.Object");
    fixture.Class("Fixture.DifferentPath","lItErAl.NaMe","Core.Object");
    fixture.Class("Fixture.Name","Name","Core.Object");
    fixture.Class("Fixture.FriendlyName","Target","Core.Object");
    fixture.Instance("Map.Literal","Fixture.DifferentPath");fixture.Instance("Map.Leaf","Fixture.Name");
    fixture.Instance("Map.Friendly","Fixture.FriendlyName");
    Result(fixture.Cast("Fixture.Target",Object("Map.Literal"),false),"Map.Literal","Literal dotted class Name was split or case-sensitive");
    Result(fixture.Cast("Fixture.Target",Object("Map.Leaf"),false),{},"Dotted declaration matched only final leaf");
    Result(fixture.Cast("Fixture.Target",Object("Map.Friendly"),false),{},"Path/FriendlyName used instead of declaration Name");
    fixture.Class("Fixture.High","\xc0" "Leaf","Core.Object");
    fixture.Class("Fixture.SameHigh","\xc0" "lEaF","Core.Object");
    fixture.Class("Fixture.OtherHigh","\xe0" "leaf","Core.Object");
    fixture.Instance("Map.SameHigh","Fixture.SameHigh");fixture.Instance("Map.OtherHigh","Fixture.OtherHigh");
    Result(fixture.Cast("Fixture.High",Object("Map.SameHigh"),false),"Map.SameHigh","ASCII class-name folding failed with unchanged high byte");
    Result(fixture.Cast("Fixture.High",Object("Map.OtherHigh"),false),{},"Non-ASCII class name bytes were case folded");
}
void MissingCyclesAndDepth() {
    Fixture fixture;
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Missing.Object"),true);},"Missing operand returned None");
    fixture.Class("Broken.Class","Broken","Missing.Base");fixture.Instance("Map.Broken","Broken.Class");
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Broken.Class"),true);},"Missing MetaCast base returned None");
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Map.Broken"),false);},"Missing DynamicCast base returned None");
    fixture.Instance("Map.MissingClass","Missing.Class");
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Map.MissingClass"),false);},"Missing actual Class returned None");
    fixture.Class("Cycle.A","CycleA","Cycle.B");fixture.Class("Cycle.B","CycleB","Cycle.A");
    fixture.Instance("Map.Cycle","Cycle.A");
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Cycle.A"),true);},"MetaCast cycle returned None");
    Reject([&] {fixture.Cast("Engine.Pawn",Object("Map.Cycle"),false);},"DynamicCast cycle returned None");
    Result(fixture.Cast("Cycle.B",Object("Cycle.A"),true),"Cycle.A","MetaCast visited cycle after early identity match");
    Result(fixture.Cast("Cycle.B",Object("Map.Cycle"),false),"Map.Cycle","DynamicCast visited cycle after early name match");
    Result(fixture.Cast("Broken.Class",Object("Broken.Class"),true),"Broken.Class","Exact MetaCast unnecessarily visited missing base");
    Result(fixture.Cast("Broken.Class",Object("Map.Broken"),false),"Map.Broken","DynamicCast unnecessarily visited missing base after match");
    for (std::size_t i=0u;i<128u;++i)
        fixture.Class("Chain.C"+std::to_string(i),"C"+std::to_string(i),i==127u ? std::string{} : "Chain.C"+std::to_string(i+1u));
    fixture.Instance("Map.Chain","Chain.C0");
    Result(fixture.Cast("Chain.C127",Object("Chain.C0"),true),"Chain.C0","Exactly 128 MetaCast classes rejected");
    Result(fixture.Cast("Chain.C127",Object("Map.Chain"),false),"Map.Chain","Exactly 128 DynamicCast classes rejected");
    Result(fixture.Cast("Engine.Ammo",Object("Chain.C0"),true),{},"Complete 128-class mismatch rejected");
    Result(fixture.Cast("Engine.Ammo",Object("Map.Chain"),false),{},"Complete 128-class dynamic mismatch rejected");
    fixture.classes.at(Key("Chain.C127")).basePath="Chain.C128";fixture.Class("Chain.C128","C128");
    Reject([&] {fixture.Cast("Chain.C128",Object("Chain.C0"),true);},"129 MetaCast classes escaped bound");
    Reject([&] {fixture.Cast("Chain.C128",Object("Map.Chain"),false);},"129 DynamicCast classes escaped bound");
    Result(fixture.Cast("Chain.C127",Object("Chain.C0"),true),"Chain.C0","Early match at hierarchy limit traversed later class");
    Reject([&] {fixture.Cast("Chain.C1",Object("Chain.C0"),true,1u);},"Caller-tightened hierarchy limit ignored");
    Reject([&] {fixture.Cast("Engine.Pawn",Object(),true,0u);},"Zero hierarchy limit accepted");
    Reject([&] {fixture.Cast("Engine.Pawn",Object(),true,129u);},"Hard hierarchy bound could be expanded");
}
void IdentityBoundsAndResolverContracts() {
    Fixture fixture;
    const std::string longText(8193u,'x'),nul("x\0y",3u),maximum(8192u,'x');
    for (const auto& invalid:{longText,nul}) {
        Reject([&] {fixture.Cast(invalid,Object(),true);},"Invalid target identity accepted");
        Reject([&] {fixture.Cast("Engine.Pawn",Object(invalid),true);},"Invalid operand identity accepted");
    }
    fixture.Class(maximum,maximum);fixture.Instance("Map.Maximum",maximum);
    Result(fixture.Cast(maximum,Object("Map.Maximum"),false),"Map.Maximum","Exact 8192-byte target/name bound rejected");
    const std::string maximumObject(8192u,'o');fixture.Instance(maximumObject,"Engine.Pawn");
    Result(fixture.Cast("Engine.Pawn",Object(maximumObject),false),maximumObject,"Exact 8192-byte operand bound rejected");
    for (const auto& invalid:{std::string{},longText,nul}) {
        auto bad=fixture.resolvers;
        bad.resolveClass=[&](const std::string&) {return ObjectCastClass{invalid,"Pawn",{}};};
        Reject([&] {CastAuthoredObject("Engine.Pawn",Object(),true,bad);},"Invalid resolved class identity accepted");
        bad.resolveClass=[&](const std::string&) {return ObjectCastClass{"Engine.Pawn",invalid,{}};};
        Reject([&] {CastAuthoredObject("Engine.Pawn",Object(),true,bad);},"Invalid literal class Name accepted");
        bad=fixture.resolvers;
        bad.resolveObject=[&](const std::string&) {return ObjectCastObject{invalid,"Engine.Pawn",false};};
        Reject([&] {CastAuthoredObject("Engine.Pawn",Object("Map.Pawn"),false,bad);},"Invalid resolved object identity accepted");
        bad.resolveObject=[&](const std::string&) {return ObjectCastObject{"Map.Pawn",invalid,false};};
        Reject([&] {CastAuthoredObject("Engine.Pawn",Object("Map.Pawn"),false,bad);},"Invalid actual Class identity accepted");
    }
    for (const auto& invalid:{longText,nul}) {
        auto bad=fixture.resolvers;
        bad.resolveClass=[&](const std::string&) {return ObjectCastClass{"Engine.Pawn","Pawn",invalid};};
        Reject([&] {CastAuthoredObject("Engine.Pawn",Object(),true,bad);},"Invalid base identity accepted before traversal");
    }
    auto bad=fixture.resolvers;
    bad.resolveClass=[](const std::string&) {return ObjectCastClass{"Other.Pawn","Pawn","Core.Object"};};
    Reject([&] {CastAuthoredObject("Engine.Pawn",Object(),true,bad);},"Class resolver substituted different qualified identity");
    bad=fixture.resolvers;
    bad.resolveObject=[](const std::string&) {return ObjectCastObject{"Other.Object","Engine.Pawn",false};};
    Reject([&] {CastAuthoredObject("Engine.Pawn",Object("Map.Pawn"),false,bad);},"Object resolver substituted another object");
}
} // namespace

int main() {
    try {
        NullAndKinds();Membership();LiteralNames();MissingCyclesAndDepth();IdentityBoundsAndResolverContracts();
        std::cout << "Authored object cast tests passed: " << checks << " checks, " << rejections << " rejections\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Authored object cast test failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
