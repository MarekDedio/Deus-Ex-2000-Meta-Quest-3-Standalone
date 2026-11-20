#include "quest_save_url_config.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Pure option tests; no actual FURL construction, User cache, file
// publication, runtime startup, or Quest gameplay is exercised here.
namespace {
using QuestVr::DeriveOriginalLoadMapOptions;
using QuestVr::OriginalLoadMapOptions;
using QuestVr::VisitOriginalLoadMapUserOptions;
using Row=std::pair<std::u16string_view,std::u16string_view>;
std::size_t checks{};

void Require(bool condition,const char* message) {
    ++checks;
    if(!condition)throw std::runtime_error(message);
}

template<class Function>
bool Refused(Function&& f) {
    try { f(); } catch(const std::runtime_error&) { return true; }
    return false;
}

void OrderedOptions() {
    const std::vector<std::u16string> source{u"Voice=V",u"CLASS=OldClass",u"nAME=First",
        u"Team=x=y",u"Face=",u"OverrideClass=New",u"skin=S",u"Name=Second",
        u"Class=LastClass",u"TEAM=last",u"Name",u"NAMEPLUS=no",u" Name=no",u"team =no",
        u"Name\u0131=no",u"=Name",u"Name==payload",u"Other=Ignored"};
    const auto before=source;
    const std::vector<Row> expected{{u"nAME",u"First"},{u"Name",u"Second"},{u"Name",u"=payload"},
        {u"Team",u"x=y"},{u"TEAM",u"last"},{u"CLASS",u"OldClass"},{u"Class",u"LastClass"},
        {u"skin",u"S"},{u"Face",u""},{u"Voice",u"V"},{u"OverrideClass",u"New"}};
    const std::vector<std::size_t> indices{2u,7u,16u,3u,9u,1u,8u,6u,4u,0u,5u};
    std::vector<Row> actual;
    std::size_t work{};
    const auto count=VisitOriginalLoadMapUserOptions(source,[&](std::size_t units){work+=units;},
        [&](auto key,auto value){actual.emplace_back(key,value);});
    Require(count==expected.size()&&actual==expected,"SaveURLConfig order, match, or suffix differs");
    for(std::size_t i=0u;i<actual.size();++i) {
        const auto& option=source[indices[i]];
        Require(actual[i].first.data()==option.data()&&
            actual[i].second.data()==option.data()+option.find(u'=')+1u,
            "Visitor did not borrow actual key/suffix views");
    }
    Require(work>source.size()&&source==before,"Traversal omitted work or changed its source");
}

void BoundsAndEmpty() {
    std::size_t visits{};
    const auto step=[](std::size_t){};
    const auto visit=[&](auto,auto){++visits;};
    const std::vector<std::u16string> none,wrong{u"Name",u"Other=x",u"VoiCe ",u"Class ="};
    Require(VisitOriginalLoadMapUserOptions(none,step,visit)==0u&&
        VisitOriginalLoadMapUserOptions(wrong,step,visit)==0u&&visits==0u,
        "Empty/nonmatching option set produced a visit");

    // Count UTF-16 code units, not Unicode scalars or UTF-8 bytes. Isolated
    // surrogate units are legal FString data; an embedded NUL is not admitted.
    std::vector<std::u16string> legal(QuestVr::OriginalSaveUrlConfigMaxOptions);
    legal[0]=u"Name=";
    legal[0].append(QuestVr::OriginalSaveUrlConfigMaxOptionUnits-legal[0].size(),char16_t{0xd800u});
    Require(VisitOriginalLoadMapUserOptions(legal,step,[&](auto key,auto value){
        ++visits;Require(key==u"Name"&&value.size()==1018u&&value.back()==char16_t{0xd800u},
            "Maximum-width native option truncated or changed its UTF-16 units");
    })==1u&&visits==1u,"Limits rejected an admitted width/count");
    legal.emplace_back();
    Require(Refused([&]{VisitOriginalLoadMapUserOptions(legal,step,visit);})&&visits==1u,
        "Over-count options visited the provider");
    legal={u"Name=AlreadyValid",std::u16string(1024u,u'x')};
    Require(Refused([&]{VisitOriginalLoadMapUserOptions(legal,step,visit);})&&visits==1u,
        "Later over-width option reached the provider before validation");
    legal[1]=u"Face=Prefix";
    legal[1].push_back(u'\0');
    legal[1]+=u"Suffix";
    Require(Refused([&]{VisitOriginalLoadMapUserOptions(legal,step,visit);})&&visits==1u,
        "Embedded NUL was silently truncated or visited");
}

void PropagateRefusal() {
    const std::vector<std::u16string> source{u"Name=First",u"Name=Second",u"Team=Later"};
    std::size_t steps{},visits{};
    Require(Refused([&]{VisitOriginalLoadMapUserOptions(source,[&](std::size_t){
        if(++steps==2u)throw std::runtime_error("budget rejected validation");
    },[&](auto,auto){++visits;});})&&visits==0u,"Admission failure was swallowed or emitted a visit");
    Require(Refused([&]{VisitOriginalLoadMapUserOptions(source,[](std::size_t){},[&](auto,auto) {
        if(++visits==2u)throw std::runtime_error("caller refused second visit");
    });})&&visits==2u,"Caller failure was swallowed or later matches were visited");
}

void InitGameAndGameClass() {
    const auto derive=[](const std::vector<std::u16string>& options) {
        return DeriveOriginalLoadMapOptions(options,[](std::size_t){},[](std::size_t){});
    };
    const auto none=derive({});
    Require(none.initGame.empty()&&none.gameClass.empty(),"Absent URL options changed empty destinations");
    const std::vector<std::u16string> source{u"gAmE=First,GAME=Ignored",u"xGAME=No 9GAME=No",u"",u"Name=JC",
        u"_gaME=\"Package.With spaces, tabs\tand?\" GAME=ignored",u"x12Game=no"};
    const auto options=derive(source);
    Require(options.initGame==u"?gAmE=First,GAME=Ignored?xGAME=No 9GAME=No??Name=JC?_gaME=\"Package.With spaces, tabs\tand?\" GAME=ignored?x12Game=no",
        "InitGame lost option order, embedded syntax, or empty option");
    Require(options.gameClass==u"Package.With spaces, tabs\tand?",
        "Core Parse did not use last matching option, first token within it, and an exact quoted suffix");
    Require(derive({u"GAME=Earlier",u"Xgame=Unrelated"}).gameClass==u"Earlier",
        "A nonmatching option cleared a previously written GAME value");
    Require(derive({u"GAME=Earlier",u"GAME= GAME=Later"}).gameClass.empty()&&
        derive({u"GAME=Earlier",u"game=\"\""}).gameClass.empty()&&
        derive({u"GAME=Earlier",u"game="}).gameClass.empty(),
        "A real empty parse did not replace the previously written GAME value");
    Require(derive({u"\u00c9GAME=Map.Game;Mode=still-suffix"}).gameClass==u"Map.Game;Mode=still-suffix",
        "Non-ASCII prefix or nondelimiter punctuation was treated as an alnum or terminator");
    Require(derive({u"GAME=\"Space without ending quote"}).gameClass==u"Space without ending quote",
        "Quoted payload without a closing quote did not stop at the end of the option");
    for(const auto delimiter:{u' ',u'\r',u'\n',u'\t',u','}) {
        auto option=std::u16string(u"prefix-GAME=Package.Game");
        option.push_back(delimiter);
        option.append(u"Unused");
        Require(derive({option}).gameClass==u"Package.Game","Core Parse lost an unquoted terminator");
    }
    Require(derive({u"GAME=Package\vGame\fClass"}).gameClass==u"Package\vGame\fClass",
        "Core Parse treated all CRT whitespace as terminators");
}

void InitGameBoundsAndAdmission() {
    std::vector<std::u16string> source{u"GAME="};
    source[0].append(QuestVr::OriginalLoadMapGameClassMaxUnits-1u,u'x');
    source[0].append(u"\U0001f642");
    source[0].append(QuestVr::OriginalLoadMapInitGameMaxUnits-source[0].size()-1u,u'y');
    const auto original=source;
    const auto step=[](std::size_t){};
    constexpr auto expectedBytes=sizeof(OriginalLoadMapOptions)+sizeof(char16_t)*
        (QuestVr::OriginalLoadMapInitGameMaxUnits+QuestVr::OriginalLoadMapGameClassMaxUnits+2u);
    std::vector<std::size_t> admissions;
    const auto admit=[&](std::size_t peak){admissions.push_back(peak);};
    const auto result=DeriveOriginalLoadMapOptions(source,step,admit);
    Require(result.initGame==u"?"+source[0]&&result.initGame.size()==1023u&&
        result.gameClass.size()==255u&&result.gameClass.back()==char16_t{0xd83du},
        "LoadMap did not preserve exact 1023-unit append and 255-unit GAME copy bounds");
    Require(admissions.size()==2u&&admissions.front()==expectedBytes&&
        admissions.back()==sizeof(OriginalLoadMapOptions)+sizeof(char16_t)*
            (result.initGame.capacity()+result.gameClass.capacity()+2u)&&source==original,
        "LoadMap did not pre-admit exact prospective storage and account for actual storage without mutating the FURL");

    admissions.clear();
    source.emplace_back();
    Require(Refused([&]{DeriveOriginalLoadMapOptions(source,step,admit);})&&admissions.empty(),
        "A later empty option crossed the original joined-buffer bound before allocation");
    source={u"Name=Fred",std::u16string{u'g',u'\0',u'='}};
    Require(Refused([&]{DeriveOriginalLoadMapOptions(source,step,admit);})&&admissions.empty(),
        "An embedded NUL was silently truncated by InitGame derivation");
    source={u"GAME=OriginalClass"};
    std::size_t calls{};
    Require(Refused([&]{DeriveOriginalLoadMapOptions(source,step,[&](std::size_t) {
        ++calls;
        throw std::runtime_error("root budget refused allocation");
    });})&&calls==1u,"InitGame continued after prospective allocation was refused");
}
}

int main() {
    try {
        OrderedOptions();BoundsAndEmpty();PropagateRefusal();InitGameAndGameClass();InitGameBoundsAndAdmission();
        std::cout<<"PASS original LoadMap InitGame, GAME=, and SaveURLConfig options: "<<checks<<" checks, no file I/O\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"FAIL original LoadMap option rules: "<<error.what()<<'\n';
        return 1;
    }
}
