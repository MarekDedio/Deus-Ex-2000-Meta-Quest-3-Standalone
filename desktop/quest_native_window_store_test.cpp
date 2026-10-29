#include "quest_native_window_store.h"

#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
using Kind=QuestVr::Vm::Kind;
using Value=QuestVr::Vm::Value;
std::size_t checks{};
void Require(bool condition,const char* why) {++checks;if(!condition)throw std::runtime_error(why);}
Value Payload(Kind kind,std::string text={}) {Value value;value.kind=kind;value.text=std::move(text);return value;}
bool Candidate(std::string_view property,const Value& value) {
    return QuestVr::NativeWindowStore::IsPlainStoreCandidate(property,value);
}
void Candidates() {
    struct Case {std::string_view property;Kind kind;std::string_view text;bool expected;};
    const std::array<Case,28u> cases{{
        {"extension.window.bisvisible",Kind::Bool,"",true},
        {"extension.window.order",Kind::Byte,"",true},
        {"extension.window.x",Kind::Float,"",true},
        {"extension.window.counter",Kind::Int,"",true},
        {"extension.textwindow.text",Kind::String,"",true},
        {"extension.textwindow.text",Kind::String,"hello",true},
        {"extension.window.label",Kind::Name,"None",true},
        {"extension.window.label",Kind::Name,"MixedCase",true},
        {"extension.window.label",Kind::Name,"mixedcase",true},
        {"extension.window.owner",Kind::Object,"",true},
        {"extension.window.owner",Kind::Object,"None",false},
        {"extension.window.owner",Kind::Object,"00_Training.Player0",false},
        {"extension.window.owner",Kind::Object," ",false},
        {"extension.window.position",Kind::Vector,"",false},
        {"extension.window.orientation",Kind::Rotator,"",false},
        {"extension.window.color",Kind::Struct,"",false},
        {"extension.window.indices",Kind::IntArray,"",false},
        {"extension.window.void",Kind::Nothing,"",false},
        {"core.object.objectflags",Kind::Int,"",false},
        {"core.object.name",Kind::Name,"Window0",false},
        {"core.object.class",Kind::Object,"",false},
        {"core.object.outer",Kind::Object,"",false},
        {"core.object.objectinternal",Kind::Int,"",false},
        {"core.object.anyheader",Kind::Bool,"",false},
        {"core.object",Kind::Byte,"",false},
        {"",Kind::Float,"",false},
        {"deusex.hudwindow.label",Kind::String,"player health",true},
        {"deusex.hudwindow.value",Kind::Float,"",true}
    }};
    for(const auto& test:cases)
        Require(Candidate(test.property,Payload(test.kind,std::string(test.text)))==test.expected,"Plain native window candidate kind/header classification differs");
}
void PredicateDoesNotReplaceStorageComparison() {
    const auto key="extension.window.scalar";
    auto positiveZero=Payload(Kind::Float),negativeZero=Payload(Kind::Float);
    negativeZero.floating=std::bit_cast<float>(std::uint32_t{0x80000000u});
    Require(Candidate(key,positiveZero)&&Candidate(key,negativeZero),"Predicate interpreted float signed-zero storage identity");
    auto nanA=Payload(Kind::Float),nanB=Payload(Kind::Float);
    nanA.floating=std::bit_cast<float>(std::uint32_t{0x7fc00001u});
    nanB.floating=std::bit_cast<float>(std::uint32_t{0x7fc00002u});
    Require(Candidate(key,nanA)&&Candidate(key,nanB),"Predicate interpreted float NaN payload equality");
    auto first=Payload(Kind::Name,"OriginalCase"),second=Payload(Kind::Name,"originalcase");
    Require(Candidate(key,first)&&Candidate(key,second)&&first.text!=second.text,"Name spelling was normalized or compared by candidate filter");
    auto scalar=Payload(Kind::Int);scalar.integer=42;
    scalar.floating=std::bit_cast<float>(std::uint32_t{0x80000000u});
    scalar.boolean=true;scalar.text="inactive text";scalar.rotation={1,2,3};
    scalar.vector={1.0f,2.0f,3.0f};scalar.intArray.push_back(7);
    scalar.fields.emplace("inactive",Payload(Kind::Object,"00_Training.Foreign0"));
    Require(Candidate(key,scalar),"Candidate filter incorrectly replaced full inactive-payload comparison");
    Require(scalar.integer==42&&std::bit_cast<std::uint32_t>(scalar.floating)==0x80000000u&&scalar.boolean&&
        scalar.text=="inactive text"&&scalar.rotation==std::array<std::int32_t,3u>{1,2,3}&&
        scalar.vector==std::array<float,3u>{1.0f,2.0f,3.0f}&&scalar.intArray.size()==1u&&scalar.intArray[0u]==7&&
        scalar.fields.at("inactive").text=="00_Training.Foreign0","Predicate mutated inactive storage");
    auto none=Payload(Kind::Object);none.integer=99;none.fields.emplace("inactive",Payload(Kind::Int));
    Require(Candidate("extension.window.parent",none)&&none.integer==99&&none.fields.size()==1u,"Object None candidate discarded inactive fields");
    auto invalid=Payload(static_cast<Kind>(999));
    Require(!Candidate(key,invalid),"Unknown VM value kind entered plain store path");
}
}

int main() {
    static_assert(noexcept(QuestVr::NativeWindowStore::IsPlainStoreCandidate({},std::declval<const Value&>())));
    try {
        Candidates();PredicateDoesNotReplaceStorageComparison();
        std::cout<<"PASS quest_native_window_store checks="<<checks<<" (eligibility only; caller retains bit-exact comparison/write validation)\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL quest_native_window_store: "<<error.what()<<'\n';return 1;}
}
