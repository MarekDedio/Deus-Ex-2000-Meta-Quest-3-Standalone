#include "quest_system_config.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace SC=QuestVr::SystemConfig;
namespace NC=QuestVr::NativeConfig;
std::size_t checks{},refusals{};
void Require(bool condition,const char* why) {++checks;if(!condition)throw std::runtime_error(why);}
template<class F>void Refused(F&& action,const char* why) {
    bool refused{};try{action();}catch(const std::runtime_error&){refused=true;}
    Require(refused,why);++refusals;
}

struct Operations {
    // Independent test-only catalog. Production supplies actual FName arena
    // identities, never this mock's small integer tokens or string vector.
    using Name=std::uint32_t;
    std::vector<std::u16string> names{u"None"},calls;
    std::vector<std::size_t> admissions;
    std::size_t steps{},workLimit=std::numeric_limits<std::size_t>::max();
    std::size_t byteLimit=std::numeric_limits<std::size_t>::max();
    std::size_t throwOnName=std::numeric_limits<std::size_t>::max();
    void Step() {if(steps==workLimit)throw std::runtime_error("Mock work refusal");++steps;}
    void Admit(std::size_t absolutePeakBytes) {
        if(absolutePeakBytes>byteLimit)throw std::runtime_error("Mock admission refusal");
        admissions.push_back(absolutePeakBytes);
    }
    Name AddName(std::u16string_view spelling) {
        if(calls.size()==throwOnName)throw std::runtime_error("Mock real-arena refusal");
        calls.emplace_back(spelling);
        if(spelling.empty())return 0u;
        for(std::size_t i=0u;i<names.size();++i)
            if(QuestVr::UnrealString::EqualInsensitive(names[i],spelling))return static_cast<Name>(i);
        names.emplace_back(spelling);return static_cast<Name>(names.size()-1u);
    }
};
using Values=SC::Values<Operations::Name>;
Values Seed() {
    Values value;value.purgeCacheDays=7;value.savePath="old-save";value.cachePath="old-cache";
    value.cacheExt="old-ext";value.paths={"old-path"};value.suppress={77u};return value;
}
void ZeroAndMissing() {
    Operations operations;
    const auto empty=SC::Load(NC::OrderedIni::Parse(""),operations);
    Require(empty.purgeCacheDays==0&&empty.savePath.empty()&&empty.cachePath.empty()&&empty.cacheExt.empty(),"Invented scalar CDO defaults");
    Require(empty.paths.empty()&&empty.suppress.empty()&&operations.calls.empty(),"Invented array/name CDO defaults");
    const auto seed=Seed();
    const auto missing=SC::Load(NC::OrderedIni::Parse("[Other]\nPaths=foreign\n"),operations,seed);
    Require(missing.purgeCacheDays==7&&missing.savePath=="old-save"&&missing.cachePath=="old-cache"&&missing.cacheExt=="old-ext", "Absent section overwrote scalars");
    Require(missing.paths==seed.paths&&missing.suppress==seed.suppress,"Absent section did not retain arrays");
    const auto present=SC::Load(NC::OrderedIni::Parse("[Core.System]\nUnrelated=yes\n"),operations,seed);
    Require(present.paths.empty()&&present.suppress.empty(),"Present section missing keys did not clear arrays");
    Require(present.savePath==seed.savePath&&present.purgeCacheDays==seed.purgeCacheDays,"Missing scalar did not retain baseline");
    Require(seed.paths==std::vector<std::string>{"old-path"}&&seed.suppress==std::vector<Operations::Name>{77u},"Returned-value load mutated input baseline");
}
void DefaultProfileAndReload() {
    const auto document=NC::OrderedIni::Parse(
        "[Core.System]\nPurgeCacheDays=30\nSavePath=..\\Save\nCachePath=..\\Cache\nCacheExt=.uxx\n"
        "Paths=..\\System\\*.u\nPaths=..\\Maps\\*.dx\nPaths=..\\Textures\\*.utx\nPaths=..\\Sounds\\*.uax\nPaths=..\\Music\\*.umx\n"
        "Suppress=DevLoad\nSuppress=DevSave\nSuppress=DevNetTraffic\nSuppress=DevGarbage\nSuppress=DevKill\n"
        "Suppress=DevReplace\nSuppress=DevSound\nSuppress=DevCompile\nSuppress=DevBind\nSuppress=DevBsp\n");
    Operations operations;const auto value=SC::Load(document,operations);
    Require(value.purgeCacheDays==30&&value.savePath=="..\\Save"&&value.cachePath=="..\\Cache"&&value.cacheExt==".uxx","Original System scalar profile differs");
    Require(value.paths==std::vector<std::string>{"..\\System\\*.u","..\\Maps\\*.dx","..\\Textures\\*.utx","..\\Sounds\\*.uax","..\\Music\\*.umx"},"Original Paths source order differs");
    Require(operations.calls==std::vector<std::u16string>{u"DevLoad",u"DevSave",u"DevNetTraffic",u"DevGarbage",u"DevKill",u"DevReplace",u"DevSound",u"DevCompile",u"DevBind",u"DevBsp"},"Original Suppress source order differs");
    Require(value.suppress.size()==10u&&value.suppress.front()==1u&&value.suppress.back()==10u,"Suppress did not retain typed actual-name callback results");
    const auto copy=value;const auto reload=SC::Load(document,operations,value);
    Require(reload.paths==copy.paths&&reload.suppress==copy.suppress,"Reload appended instead of replacing arrays");
    Require(operations.calls.size()==20u,"Reload skipped actual Name imports");
    Require(!operations.admissions.empty()&&*std::max_element(operations.admissions.begin(),operations.admissions.end())>=SC::RetainedBytes(reload),"Returned payload escaped memory admission");
    Require(operations.steps>document.EntryCount(),"Config work was not charged");
}
void OrderedDuplicatesAndNoExtraSyntax() {
    Operations operations;
    const auto value=SC::Load(NC::OrderedIni::Parse(
        "[core.system]\nSavePath=first\nPaths=one\nSuppress=Alpha\nPaths=\n"
        "[CORE.SYSTEM]\nsavepath=last\nPaths=one\nPaths=two words  \nSuppress=alpha\nSuppress=\n"
        "+Paths=not-an-append-command\nPaths[0]=not-an-index-command\n"),operations);
    Require(value.savePath=="last","Scalar did not select newest case-insensitive duplicate");
    Require(value.paths==std::vector<std::string>{"one","","one","two words  "},"Array order/duplicates/empty/whole-string semantics differ");
    Require(value.suppress==std::vector<Operations::Name>{1u,1u,0u},"Name identity/duplicate/empty semantics differ");
    Require(operations.calls==std::vector<std::u16string>{u"Alpha",u"alpha",u""},"Array names not imported in source order");
}
void ScalarAndArrayImport() {
    const auto longText=std::string(2048u,'x');Operations operations;
    const auto value=SC::Load(NC::OrderedIni::Parse("[Core.System]\nSavePath="+longText+"\nPaths="+longText+"\nPurgeCacheDays=--12\n"),operations,Seed());
    Require(value.savePath==std::string(1023u,'x'),"Scalar GetString1024 truncation omitted");
    Require(value.paths==std::vector<std::string>{longText},"Array StrProperty was scalar-truncated/tokenized");
    Require(value.purgeCacheDays==0,"Integer --12 native parser semantics replaced by generic conversion");
    for(const auto& text:{""," 12","+12","no-value"}) {
        Operations local;const auto retained=SC::Load(NC::OrderedIni::Parse(std::string("[Core.System]\nPurgeCacheDays=")+text+"\n"),local,Seed());
        Require(retained.purgeCacheDays==7,"Non-numeric Integer import overwrote baseline");
    }
    for(const auto& [text,expected]:std::vector<std::pair<std::string,std::int32_t>>{
        {"12-34x",12},{"-",0},{"-2147483648",std::numeric_limits<std::int32_t>::min()},{"2147483647",std::numeric_limits<std::int32_t>::max()}}) {
        Operations local;const auto imported=SC::Load(NC::OrderedIni::Parse("[Core.System]\nPurgeCacheDays="+text+"\n"),local);
        Require(imported.purgeCacheDays==expected,"Integer parser bound/consumed-prefix semantics differ");
    }
}
void Utf16ScalarBoundary() {
    auto text=std::u16string(u"[Core.System]\nSavePath=")+std::u16string(1022u,u'x');
    text.push_back(static_cast<char16_t>(0xd83du));text.push_back(static_cast<char16_t>(0xde00u));
    text+=u"\nPaths=";text.push_back(static_cast<char16_t>(0xd83du));text.push_back(static_cast<char16_t>(0xde00u));
    text+=u"\nCachePath=\u00e9\n";
    std::string file;file.push_back(static_cast<char>(255u));file.push_back(static_cast<char>(254u));
    for(const auto unit:text) {file.push_back(static_cast<char>(unit&255u));file.push_back(static_cast<char>((unit>>8u)&255u));}
    Operations operations;const auto value=SC::Load(NC::OrderedIni::Parse(file),operations);
    const auto scalar=QuestVr::UnrealString::Decode(value.savePath);
    Require(scalar.size()==1023u&&scalar.back()==static_cast<char16_t>(0xd83du),"Scalar truncated codepoints rather than UTF16 units");
    Require(value.paths==std::vector<std::string>{QuestVr::UnrealString::Encode(std::u16string{static_cast<char16_t>(0xd83du),static_cast<char16_t>(0xde00u)})},"Array UTF16 supplementary pair changed");
    Require(value.cachePath==QuestVr::UnrealString::Encode(u"\u00e9"),"Native file units did not retain shared FString transport");
}
void NameFailuresAndPrefix() {
    Operations operations;const auto overlong=std::string(1023u,'a');
    const auto value=SC::Load(NC::OrderedIni::Parse("[Core.System]\nSuppress=Alpha.tail\nSuppress= leading\nSuppress=_word\n"
        "Suppress=\"unterminated\nSuppress="+overlong+"\nSuppress=Beta\nPaths=still imported\nSavePath=after failures\n"),operations);
    Require(operations.calls==std::vector<std::u16string>{u"Alpha",u" ",u"_",u"Beta"},"Name ReadToken flags0 grammar or failure handling differs");
    Require(value.suppress==std::vector<Operations::Name>{1u,2u,3u,0u,0u,4u},"Failed Name cells were removed/retained old values");
    Require(value.paths==std::vector<std::string>{"still imported"}&&value.savePath=="after failures","Ordinary Name import failure aborted later fields");
    Operations failing;failing.throwOnName=1u;
    Refused([&]{static_cast<void>(SC::Load(NC::OrderedIni::Parse("[Core.System]\nSuppress=First\nSuppress=Second\n"),failing));},"Actual arena refusal was swallowed");
    Require(failing.calls==std::vector<std::u16string>{u"First"}&&failing.names.back()==u"First","Helper invented rollback of real arena prefix effects");
    Operations overflow;
    Refused([&]{static_cast<void>(SC::Load(NC::OrderedIni::Parse("[Core.System]\nSuppress=BeforeInteger\nPurgeCacheDays=2147483648\n"),overflow));},"Unestablished signed Integer overflow was accepted");
    Require(overflow.calls==std::vector<std::u16string>{u"BeforeInteger"},"System ConfigLink order did not import Suppress before PurgeCacheDays");
}
void ResourceRefusalAndImmutableSource() {
    const auto document=NC::OrderedIni::Parse("[Core.System]\nSuppress=Alpha\nPaths=a long path\n");
    Operations noBytes;noBytes.byteLimit=0u;
    Refused([&]{static_cast<void>(SC::Load(document,noBytes));},"No memory admission did not refuse");
    Require(noBytes.calls.empty()&&noBytes.steps==0u,"Memory refusal happened after Name/work effects");
    Operations noWork;noWork.workLimit=0u;
    Refused([&]{static_cast<void>(SC::Load(document,noWork));},"Work budget did not refuse");
    Require(noWork.calls.empty(),"Work refusal happened after Name effects");
    Operations allowed;const auto value=SC::Load(document,allowed);
    Require(value.paths==std::vector<std::string>{"a long path"}&&document.Scalar("Core.System","Suppress")=="Alpha","Load mutated immutable source after refusal/success");
    Require(std::is_sorted(allowed.admissions.begin(),allowed.admissions.end())&&
        std::adjacent_find(allowed.admissions.begin(),allowed.admissions.end())==allowed.admissions.end(),"Single-load admissions were not increasing absolute peaks");
}
}

int main() {
    try {
        ZeroAndMissing();DefaultProfileAndReload();OrderedDuplicatesAndNoExtraSyntax();
        ScalarAndArrayImport();Utf16ScalarBoundary();NameFailuresAndPrefix();ResourceRefusalAndImmutableSource();
        std::cout<<"PASS quest_system_config checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL quest_system_config: "<<error.what()<<'\n';return 1;}
}
