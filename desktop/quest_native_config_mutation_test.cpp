#include "quest_native_config.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace NC=QuestVr::NativeConfig;
using Doc=NC::OrderedIni;
using QuestVr::UnrealString::Encode;
std::size_t checks{},refusals{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
template<class Function>
void Refused(Function&& function,const char* why) {
    bool refused{};try {function();}catch(const std::runtime_error&){refused=true;}
    Require(refused,why);++refusals;
}
std::optional<std::string> Scalar(const Doc& document,std::string_view section,std::string_view key) {
    const auto value=document.Scalar(section,key);
    return value?std::optional<std::string>(std::in_place,*value):std::nullopt;
}
Doc Changed(const Doc& source,std::string_view section,std::string_view key,std::string_view value) {
    auto changed=source.WithString(section,key,value);Require(bool(changed),"Expected a changed document");
    return std::move(*changed);
}
std::string Wide(std::u16string_view text,bool big=false) {
    std::string result;result.push_back(static_cast<char>(big?254u:255u));result.push_back(static_cast<char>(big?255u:254u));
    for(char16_t c:text) {
        result.push_back(static_cast<char>((big?c>>8u:c)&255u));
        result.push_back(static_cast<char>((big?c:c>>8u)&255u));
    }
    return result;
}
void DuplicateAndNoOp() {
    const auto source=Doc::Parse("[S]\nK=first\n[B]\nZ=b\n[s]\nK=second\nK=third\nK=third\n");
    const auto before=source.Serialize();const auto beforeBytes=source.RetainedBytes();
    const auto latest=source.Scalar("s","k");Require(latest=="third","Fixture lost newest duplicate");
    Require(!source.WithString("s","k","THIRD"),"Case-only value changes incorrectly dirty the document");
    Require(!source.WithString("S","K","third"),"Equal value changes incorrectly dirty the document");
    const auto replacement=Changed(source,"s","k","new");
    Require(source.Serialize()==before&&source.RetainedBytes()==beforeBytes&&latest=="third",
        "Copy-on-write mutation invalidated source bytes/capacity/borrowed views");
    Require(replacement.SectionCount()==2u&&replacement.EntryCount()==5u,
        "Updating a duplicate changed pair count or section identity");
    const auto array=replacement.StringArray("S","K");
    Require(array&&*array==std::vector<std::string>{"first","second","third","new"},
        "SetString updated earlier duplicates or changed their source order");
    Require(replacement.Serialize()=="[S]\r\nK=first\r\nK=second\r\nK=third\r\nK=new\r\n\r\n[B]\r\nZ=b\r\n\r\n",
        "Raw section grouping/key spelling/duplicate serialization differs");
    const auto cold=Doc::Parse(replacement.Serialize());
    Require(Scalar(cold,"s","K")=="new"&&cold.StringArray("S","K")==array,
        "Cold reparsing changed ordinary scalar/array contents");
    Require(cold.Serialize()==replacement.Serialize(),"Normalized repeated-section serialization is not stable");
    auto addition=Changed(source,"s","Tail","kept");
    addition=Changed(addition," New "," Key "," value ");
    Require(addition.SectionCount()==3u&&addition.EntryCount()==7u&&
        Scalar(addition," New "," Key ")==" value "&& !Scalar(addition,"New","Key"),
        "New section/key/value whitespace was trimmed or existing sections were duplicated");
    const auto empty=Changed(addition,"","","empty");
    const auto coldEmpty=Doc::Parse(empty.Serialize());
    Require(Scalar(coldEmpty,"","")=="empty","Empty section/key did not retain literal semantics");
}
void CStringAndRawProfile() {
    const auto source=Doc::Parse("[S]\nK=old\n");
    const std::string section("S\0other",7u),key("K\0other",7u),value("new\0other",9u);
    const auto updated=Changed(source,section,key,value);
    Require(updated.SectionCount()==1u&&updated.EntryCount()==1u&&Scalar(updated,"S","K")=="new",
        "SetString did not use complete CString truncation before comparisons/storage");
    const auto alias=Changed(source,"S","Aliases[0]","(Alias=Fire,Command=\"Fire\")");
    const auto coldAlias=Doc::Parse(alias.Serialize());
    Require(Scalar(coldAlias,"S","Aliases[0]")=="(Alias=Fire,Command=\"Fire\")",
        "Struct-shaped quoted members were escaped or parsed as arrays");
    const auto quote=Changed(source,"S","K","\"quoted\"");
    Require(quote.Serialize()=="[S]\r\nK=\"quoted\"\r\n\r\n"&&Scalar(quote,"S","K")=="\"quoted\"",
        "Raw writer invented escaping or removed quotes before serialization");
    const auto coldQuote=Doc::Parse(quote.Serialize());
    Require(Scalar(coldQuote,"S","K")=="quoted",
        "Raw quoted values lost the original parser's intentionally lossy cold-reload rule");
    const auto newline=Changed(source,"S","K","top\r\nExtra=second");
    Require(newline.Serialize()=="[S]\r\nK=top\r\nExtra=second\r\n\r\n",
        "Raw writer escaped or rejected original-valid embedded line breaks");
    const auto coldNewline=Doc::Parse(newline.Serialize());
    Require(Scalar(coldNewline,"S","K")=="top"&&Scalar(coldNewline,"S","Extra")=="second",
        "Raw multiline values did not retain original lossy reload behavior");
    const auto accent=Changed(source,"S","K",Encode(u"\u00c9"));
    Require(accent.WithString("S","K",Encode(u"\u00e9")).has_value(),
        "SetString incorrectly applied Unicode case folding instead of ASCII C-locale comparison");
    const auto cyrillic=Changed(source,"S","K",Encode(u"\u0410"));
    Require(cyrillic.WithString("S","K",Encode(u"\u0430")).has_value(),
        "SetString expanded non-ASCII case equivalence");
    Require(source.Serialize()=="[S]\r\nK=old\r\n\r\n","Raw mutations changed their immutable source");
}
void Encoding() {
    const auto source=Doc::Parse("[S]\nK=old\n");
    for(unsigned unit=1u;unit<=255u;++unit) {
        const auto value=Encode(std::u16string(1u,static_cast<char16_t>(unit)));
        const auto document=Changed(source,"S","K",value);
        const auto bytes=document.Serialize();
        Require(bytes==std::string("[S]\r\nK=")+static_cast<char>(unit)+"\r\n\r\n",
            "8bit-representable code unit was emitted as UTF8/UTF16 or escaped");
        if(unit!='\r'&&unit!='\n'&&unit!='"') {
            const auto cold=Doc::Parse(bytes);
            Require(Scalar(cold,"S","K")==value,"Byte-profile cold reload did not zero-extend the byte");
        }
    }
    const std::vector<std::u16string> wideValues{u"Za\u017c\u00f3\u0142\u0107 \U0001f642",
        std::u16string{static_cast<char16_t>(0xd800u),u'x',static_cast<char16_t>(0xdc00u)},u"\u0100"};
    for(const auto& units:wideValues) {
        const auto text=Encode(units);const auto document=Changed(source,"S","K",text);const auto bytes=document.Serialize();
        Require(bytes.size()>=2u&&static_cast<unsigned char>(bytes[0u])==255u&&static_cast<unsigned char>(bytes[1u])==254u,
            "Non-8bit document did not use readable BOM UTF16LE");
        const auto cold=Doc::Parse(bytes);
        Require(Scalar(cold,"S","K")==text&&cold.Serialize()==bytes,
            "UTF16 code units/split surrogates changed across cold reparse");
    }
    const auto big=Doc::Parse(Wide(u"[\u0421]\n\u041a=\u0416\n",true));
    const auto coldBig=Doc::Parse(big.Serialize());
    Require(Scalar(coldBig,Encode(u"\u0421"),Encode(u"\u041a"))==Encode(u"\u0416"),
        "Big-endian source did not normalize to a readable lossless writer profile");
    const auto split=Encode(std::u16string(1u,static_cast<char16_t>(0xd800u)))+
        Encode(std::u16string(1u,static_cast<char16_t>(0xdc00u)));
    const auto paired=Changed(source,"S","K",split);
    Require(Scalar(paired,"S","K")==Encode(std::u16string{static_cast<char16_t>(0xd800u),static_cast<char16_t>(0xdc00u)}),
        "Mutation did not use the shared canonical UTF8/WTF8 CString transport");
    const auto empty=Doc::Parse("");Require(empty.Serialize().empty(),"Empty document fabricated a BOM or section");
}
struct Trace {
    std::size_t calls{},failAt{},work{},peak{};
    NC::Work Callbacks() {
        return {[this](std::size_t count) {Visit();work+=count;},
            [this](std::size_t bytes) {Visit();Require(bytes>peak,"Admission callback did not use increasing absolute peaks");peak=bytes;}};
    }
    void Visit() {if(++calls==failAt)throw std::runtime_error("Injected shared operation budget failure");}
};
void CallbackAndFailureCuts() {
    const auto source=Doc::Parse("[S]\nK=old\nK=newest\n[Empty]\n");const auto before=source.Serialize();
    Trace success;const auto updated=source.WithString("Added","NewKey","new value",success.Callbacks());
    Require(updated&&success.work>0u&&success.peak>=updated->RetainedBytes(),
        "Mutation did not join shared work/scratch admission before staging its owned document");
    for(std::size_t point=1u;point<=success.calls;++point) {
        Trace rejected;rejected.failAt=point;
        Refused([&]{static_cast<void>(source.WithString("Added","NewKey","new value",rejected.Callbacks()));},
            "Mutation swallowed a shared work/admission failure");
        Require(source.Serialize()==before&&Scalar(source,"S","K")=="newest",
            "Failed copy-on-write mutation modified its immutable source");
    }
    Trace emitted;const auto serialized=updated->Serialize(emitted.Callbacks());
    Require(!serialized.empty()&&emitted.work>serialized.size()&&emitted.peak>=serialized.size(),
        "Serializer did not join shared work/scratch limits");
    for(std::size_t point=1u;point<=emitted.calls;++point) {
        Trace rejected;rejected.failAt=point;
        Refused([&]{static_cast<void>(updated->Serialize(rejected.Callbacks()));},
            "Serialization swallowed a shared work/admission failure");
        Require(updated->Serialize()==serialized&&source.Serialize()==before,
            "Failed serialization mutated source or staged document");
    }
    Trace noOp;Require(!source.WithString("s","k","NEWEST",noOp.Callbacks()),"Case-only no-op changed document");
    Require(noOp.work>0u&&noOp.peak<success.peak,"No-op mutation copied the whole document");
}
void BoundsAndMalformed() {
    const auto ordinary=Doc::Parse("[S]\nK=old\n");const auto before=ordinary.Serialize();
    for(const auto& bad:{std::string(1u,static_cast<char>(0x80u)),std::string("\xc0\x80",2u),
        std::string("\xf4\x90\x80\x80",4u),std::string("x\0\x80",3u),std::string("\xe0\xa0",2u)}) {
        Refused([&]{static_cast<void>(ordinary.WithString(bad,"K","x"));},"Invalid section transport was admitted");
        Refused([&]{static_cast<void>(ordinary.WithString("S",bad,"x"));},"Invalid key transport was admitted");
        Refused([&]{static_cast<void>(ordinary.WithString("S","K",bad));},"Invalid value transport was admitted");
    }
    NC::Limits sections;sections.sections=1u;const auto oneSection=Doc::Parse("[S]\nK=x\n",sections);
    Refused([&]{static_cast<void>(oneSection.WithString("T","K","x"));},"Mutation raised section limit");
    NC::Limits entries;entries.entries=1u;const auto oneEntry=Doc::Parse("[S]\nK=x\n",entries);
    Refused([&]{static_cast<void>(oneEntry.WithString("S","Other","x"));},"Mutation raised entry limit");
    Require(oneEntry.WithString("S","K","changed").has_value(),"Existing-key update incorrectly consumed an entry slot");
    NC::Limits line;line.lineUnits=4u;const auto shortLine=Doc::Parse("[S]\nK=x\n",line);
    Refused([&]{static_cast<void>(shortLine.WithString("S","K","12345"));},"Mutation raised text-unit limit");
    const auto longLine=Changed(shortLine,"S","K","1234");
    Refused([&]{static_cast<void>(longLine.Serialize());},"Serializer ignored complete raw-line limit");
    NC::Limits file;file.fileBytes=14u;const auto shortFile=Doc::Parse("[S]\nK=x\n",file);
    const auto longFile=Changed(shortFile,"S","K","abcdefgh");
    Refused([&]{static_cast<void>(longFile.Serialize());},"Serializer raised file byte limit");
    file.fileBytes=25u;const auto narrow=Doc::Parse("[S]\nK=x\n",file);
    const auto wide=Changed(narrow,"S","K",Encode(u"\u0416"));
    Refused([&]{static_cast<void>(wide.Serialize());},"Serializer counted UTF16 units instead of physical bytes/BOM");
    NC::Limits retained;retained.retainedBytes=256u;const auto small=Doc::Parse("[S]\nK=x\n",retained);
    Refused([&]{static_cast<void>(small.WithString("S","K",std::string(100u,'x')));},"Mutation raised retained document limit");
    NC::Limits work;work.comparisonBytes=5u;const auto bounded=Doc::Parse("[S]\nK=x\n",work);
    Refused([&]{static_cast<void>(bounded.WithString("S","K","y"));},"Mutation ignored local work limit");
    Refused([&]{static_cast<void>(bounded.Serialize());},"Serialization ignored local work limit");
    Require(Scalar(bounded,"S","K")=="x"&&ordinary.Serialize()==before,
        "Malformed/over-budget operations changed source documents");
}
void ManySectionsAndDuplicates() {
    std::string bytes;constexpr std::size_t sectionCount=300u,entryCount=1000u;
    for(std::size_t s=0u;s<sectionCount;++s)bytes+="[S"+std::to_string(s)+"]\nK=initial\n";
    bytes+="[S299]\n";for(std::size_t i=0u;i<entryCount;++i)bytes+="K=v"+std::to_string(i)+"\n";
    const auto source=Doc::Parse(bytes);Trace trace;const auto serialized=source.Serialize(trace.Callbacks());
    Require(source.SectionCount()==sectionCount&&source.EntryCount()==sectionCount+entryCount,
        "Large fixture lost empty sections/duplicate pairs");
    Require(trace.work<serialized.size()*4u+4u*(sectionCount+entryCount),
        "Serialization rescanned every entry for every section instead of using bounded stable grouping");
    const auto cold=Doc::Parse(serialized);
    Require(cold.StringArray("S299","K")==source.StringArray("S299","K")&&cold.Serialize()==serialized,
        "Grouped serialization lost duplicate order or canonical roundtrip stability");
    const auto updated=Changed(source,"s299","k","changed");const auto values=updated.StringArray("S299","K");
    Require(values&&values->size()==entryCount+1u&&values->front()=="initial"&&values->back()=="changed",
        "Large newest-duplicate replacement changed earlier entries");
}
void DeterministicMutationModel() {
    auto document=Doc::Parse("");std::map<std::pair<unsigned,unsigned>,std::string> expected;
    std::uint32_t random=0x51a721e9u;
    const auto next=[&] {random=random*1664525u+1013904223u;return random;};
    for(unsigned iteration=0u;iteration<240u;++iteration) {
        const auto section=next()%9u,key=next()%13u,value=next()%17u;
        const auto names=std::pair{section,key};const auto sectionName="S"+std::to_string(section),keyName="K"+std::to_string(key);
        const auto text="value"+std::to_string(value);const auto before=document.Serialize();
        const auto found=expected.find(names);const bool shouldChange=found==expected.end()||found->second!=text;
        auto changed=document.WithString(sectionName,keyName,text);
        Require(changed.has_value()==shouldChange,"Deterministic SetString model dirty decision differs");
        Require(document.Serialize()==before,"Deterministic mutation modified its borrowed source");
        if(changed)document=std::move(*changed);
        expected[names]=text;Require(document.EntryCount()==expected.size(),"Deterministic mutation duplicated or removed pairs");
        const auto cold=Doc::Parse(document.Serialize());
        for(const auto& [identity,wanted]:expected) {
            Require(Scalar(document,"s"+std::to_string(identity.first),"k"+std::to_string(identity.second))==wanted&&
                Scalar(cold,"S"+std::to_string(identity.first),"K"+std::to_string(identity.second))==wanted,
                "Deterministic mutable/cold config differs from independent last-value model");
        }
    }
}
} // namespace
int main() {
    try {
        DuplicateAndNoOp();CStringAndRawProfile();Encoding();CallbackAndFailureCuts();BoundsAndMalformed();ManySectionsAndDuplicates();DeterministicMutationModel();
        std::cout<<"PASS native config mutation/export: "<<checks<<" checks / "<<refusals<<
            " refusals; immutable source, newest duplicate/case-only no-op, raw CRLF/quote semantics, readable byte/UTF16LE, "
            "shared work/scratch admission and bounded failure cuts. No filesystem writes, GConfig provider binding, SET dispatch or gameplay claim.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"Native config mutation/export failed: "<<error.what()<<'\n';return 1;}
}
