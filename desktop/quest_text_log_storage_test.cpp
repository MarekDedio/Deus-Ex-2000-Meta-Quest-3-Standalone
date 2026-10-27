#include "quest_native_text_log_storage.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Owned-storage controls only: no source packages, renderer, script events or
// claim that AddLog, text measurement, Tick or Draw have been implemented.
namespace {
using Storage=QuestVr::NativeTextLogStorage;
using Line=QuestVr::NativeTextLogLine;
std::size_t checks{},refusals{};

void Require(const bool value,const char* why) {
    ++checks;if(!value)throw std::runtime_error(why);
}
std::uint32_t Word(const float& value) {return std::bit_cast<std::uint32_t>(value);}
struct SavedLine {
    std::uint32_t expiry{};
    std::string text;
    std::size_t capacity{};
    std::array<std::uint8_t,4u> color{};
};
struct Snapshot {
    bool initialized{};
    std::size_t capacity{},retained{};
    std::vector<SavedLine> lines;
};
Snapshot Capture(const Storage& value) {
    Snapshot result;result.initialized=value.initialized;result.capacity=value.lines.capacity();
    result.retained=value.lines.capacity()*sizeof(Line);
    for(const auto& line:value.lines) {
        result.lines.push_back({Word(line.expiry),line.text,line.text.capacity(),line.color});
        result.retained+=line.text.capacity()+1u;
    }
    Require(value.RetainedBytes()==result.retained,"Native Lines retained accounting differs from independent capacity sum");
    return result;
}
void Same(const Storage& value,const Snapshot& expected) {
    Require(value.initialized==expected.initialized,"Native Lines copy/move/swap changed construction state");
    Require(value.lines.size()==expected.lines.size()&&value.lines.capacity()==expected.capacity,
        "Native Lines copy/move/swap lost logical size or spare vector capacity");
    Require(value.RetainedBytes()==expected.retained,"Native Lines copy/move/swap changed retained-byte accounting");
    for(std::size_t i=0u;i<value.lines.size();++i) {
        const auto& actual=value.lines[i];const auto& original=expected.lines[i];
        Require(Word(actual.expiry)==original.expiry,"Native Lines copy changed raw Float storage");
        Require(actual.color==original.color,"Native Lines copy lost full four-byte Color storage");
        Require(actual.text==original.text&&actual.text.size()==original.text.size(),
            "Native Lines copy changed owned FString bytes, embedded NUL or Unicode");
        Require(actual.text.capacity()==original.capacity,"Native Lines copy shrank or grew an owned FString capacity");
    }
}
void Distinct(const Storage& first,const Storage& second) {
    Require(first.lines.size()==second.lines.size(),"Native Lines ownership comparison has different sizes");
    if(!first.lines.empty())Require(first.lines.data()!=second.lines.data(),"Native Lines copies alias entry allocation");
    for(std::size_t i=0u;i<first.lines.size();++i)
        Require(first.lines[i].text.data()!=second.lines[i].text.data(),"Native Lines copies alias owned FString allocation");
}
void Empty(const Storage& value,const bool initialized) {
    Require(value.initialized==initialized&&value.lines.empty()&&!value.lines.capacity()&&!value.RetainedBytes(),
        "Native Lines empty state retained allocation capacity or changed construction flag");
}
Storage Populated() {
    Storage result;result.Construct();result.lines.reserve(19u);
    const std::array<std::uint32_t,5u> words{0x7fc0a571u,0x80000000u,0x7f812345u,0xff800000u,0x3f800000u};
    const std::array<std::string,5u> texts{
        std::string{},std::string{"short"},std::string{"head\0tail",9u},
        std::string{"\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 \xf0\x9f\x8e\xae"},std::string(301u,'L')};
    for(std::size_t i=0u;i<words.size();++i) {
        auto& line=result.lines.emplace_back();line.expiry=std::bit_cast<float>(words[i]);
        line.text.reserve(texts[i].size()+97u+i*11u);line.text.assign(texts[i].data(),texts[i].size());
        line.color={static_cast<std::uint8_t>(i),0x80u,0xffu,static_cast<std::uint8_t>(0xf0u-i)};
        Require(line.text.capacity()>line.text.size(),"Storage fixture has no actual spare FString capacity");
        Require(Word(line.expiry)==words[i],"Storage fixture did not retain its literal IEEE Float word");
    }
    Require(result.lines.capacity()>result.lines.size(),"Storage fixture has no spare entry capacity");
    Require(result.lines[2u].text.size()==9u&&result.lines[2u].text[4u]=='\0'&&result.lines[3u].text.size()==13u,
        "Storage fixture did not contain the intended embedded NUL/Unicode bytes");
    return result;
}
void Lifecycle() {
    Storage fresh;Empty(fresh,false);const auto original=Capture(fresh);
    bool rejected{};
    try {fresh.Clear();}catch(const std::runtime_error&){rejected=true;}
    Require(rejected,"Unconstructed native Lines accepted Clear");++refusals;Same(fresh,original);
    Storage copied(fresh);Empty(copied,false);
    fresh.Construct();Empty(fresh,true);fresh.Clear();Empty(fresh,true);fresh.Construct();Empty(fresh,true);
    for(const bool reconstruct:{false,true}) {
        auto populated=Populated();Require(populated.RetainedBytes()>0u,"Populated Lines did not retain owned storage");
        if(reconstruct)populated.Construct();else populated.Clear();
        Empty(populated,true);populated.Clear();Empty(populated,true);
    }
    Storage reserved;reserved.Construct();reserved.lines.reserve(37u);
    const auto retained=reserved.lines.capacity()*sizeof(Line);
    Require(reserved.lines.empty()&&reserved.RetainedBytes()==retained&&retained>0u,
        "Reserved-empty Lines omitted its owned vector capacity");
    const auto reservedSnapshot=Capture(reserved);Storage reservedCopy(reserved);Same(reservedCopy,reservedSnapshot);
    reservedCopy.Clear();Empty(reservedCopy,true);Same(reserved,reservedSnapshot);
    reserved.Construct();Empty(reserved,true);
}
void Copies() {
    auto source=Populated();const auto original=Capture(source);
    Storage copied(source);Same(copied,original);Distinct(source,copied);Same(source,original);
    copied.lines[0u].expiry=std::bit_cast<float>(0x7fc01234u);copied.lines[1u].color[3u]=0u;
    copied.lines[2u].text[0u]='X';copied.lines[3u].text.clear();copied.lines.emplace_back();
    Same(source,original);
    auto assigned=Populated();assigned.lines.reserve(41u);assigned.lines[4u].text.assign(701u,'R');
    assigned=source;Same(assigned,original);Distinct(source,assigned);Same(source,original);
    const auto* self=&assigned;assigned=*self;Same(assigned,original);Same(source,original);
    assigned.lines[4u].text[0u]='S';assigned.lines[0u].color[0u]=0xffu;Same(source,original);
    Storage absent;assigned=absent;Empty(assigned,false);Empty(absent,false);Same(source,original);
    auto emptyReserved=Storage{};emptyReserved.Construct();emptyReserved.lines.reserve(53u);
    const auto reserved=Capture(emptyReserved);assigned=emptyReserved;Same(assigned,reserved);Same(emptyReserved,reserved);
    assigned.Clear();Empty(assigned,true);Same(emptyReserved,reserved);Same(source,original);
}
void MovesAndSwap() {
    auto original=Populated();const auto unchanged=Capture(original);
    Storage source(original);const auto saved=Capture(source);const auto* entries=source.lines.data();
    const auto* text=source.lines[4u].text.data();Storage moved(std::move(source));
    Same(moved,saved);Require(moved.lines.data()==entries&&moved.lines[4u].text.data()==text,
        "Native Lines move construction copied owned allocations");Empty(source,true);Same(original,unchanged);
    auto destination=Populated();destination.lines.reserve(67u);destination=std::move(moved);
    Same(destination,saved);Require(destination.lines.data()==entries&&destination.lines[4u].text.data()==text,
        "Native Lines move assignment copied or discarded owned allocations");Empty(moved,true);Same(original,unchanged);
    Storage other;other.lines.reserve(7u);const auto otherSaved=Capture(other);
    destination.Swap(other);Same(destination,otherSaved);Same(other,saved);Same(original,unchanged);
    std::swap(destination,other);Same(destination,saved);Same(other,otherSaved);Same(original,unchanged);
    destination.Clear();Empty(destination,true);other.Construct();Empty(other,true);Same(original,unchanged);
    Storage absent;Storage movedAbsent(std::move(absent));Empty(movedAbsent,false);Empty(absent,false);
}
} // namespace

int main() {
    static_assert(std::is_nothrow_move_constructible_v<Storage>);
    static_assert(std::is_nothrow_move_assignable_v<Storage>);
    static_assert(std::is_nothrow_swappable_v<Storage>);
    static_assert(sizeof(float)==sizeof(std::uint32_t));
    try {
        Lifecycle();Copies();MovesAndSwap();
        std::cout<<"PASS native TextLog owned storage: "<<checks<<" checks, "<<refusals
            <<" refusals; construction/freeing, raw Float/full Color/FString bytes, exact spare-capacity deep copies, "
              "move/swap ownership and retained-byte accounting. No original packages or native log/layout services.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL native TextLog storage: "<<error.what()<<'\n';return 1;}
}
