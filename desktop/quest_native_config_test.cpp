#include "quest_native_config.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace NC=QuestVr::NativeConfig;
using Doc=NC::OrderedIni;
using Values=std::vector<std::string>;
std::size_t checks{},refusals{};
void Require(bool v,const char* why) {++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refused(F&& fn,const char* why) {bool refused{};try{fn();}catch(const std::runtime_error&){refused=true;}Require(refused,why);++refusals;}
void Scalar(const Doc& doc,std::string_view section,std::string_view key,std::optional<std::string_view> expected) {
    Require(doc.Scalar(section,key)==expected,"Scalar last-value/raw-byte semantics differ");
}
void Array(const Doc& doc,std::string_view section,std::string_view key,std::optional<Values> expected) {
    Require(doc.StringArray(section,key)==expected,"Array source-order/absent-section semantics differ");
}
void Semantics() {
    const auto d=Doc::Parse("Unsectioned=ignored\r\n\r\n[S]\nK=first\nK=second\n[s]\rK=third\nK=third\n");
    Require(d.SectionCount()==1u&&d.EntryCount()==4u&&d.RetainedBytes()>0u,"Duplicate sections did not merge / duplicate values were deduplicated");
    Scalar(d,"S","k","third");Array(d,"s","K",Values{"first","second","third","third"});
    Scalar(d,"Missing","k",std::nullopt);Array(d,"Missing","k",std::nullopt);
    Scalar(d,"S","Missing",std::nullopt);Array(d,"S","Missing",Values{});
    const auto raw=Doc::Parse("[ S ]\n K = value \nK=\" a=b ; // c \"\n;K=comment-is-key\n//K=also-key\n=empty-key\nK=\"\n");
    Scalar(raw,"S","K",std::nullopt);Scalar(raw," S "," K "," value ");
    Scalar(raw," S ",";K","comment-is-key");Scalar(raw," S ","//K","also-key");Scalar(raw," S ","","empty-key");
    Scalar(raw," S ","K","");Array(raw," S ","K",Values{" a=b ; // c ",""});
    const auto headers=Doc::Parse("[S]\nK=one\n[Other] \nK=two\n [Other]\nK=three\n[]\nK=empty-section\n[[nested]]\nK=nested\n");
    Array(headers,"S","K",Values{"one","two","three"});Scalar(headers,"","K","empty-section");Scalar(headers,"[nested]","K","nested");
    const auto keys=Doc::Parse("[S]\n+K=add\n-K=remove\n.K=dot\n!K=clear\nK[0]=indexed\nK=literal\n\tK=tab\n");
    Array(keys,"S","K",Values{"literal"});Scalar(keys,"S","\tK","tab");Scalar(keys,"S","+K","add");
    const auto quotes=Doc::Parse("[S]\nA=\"\"\nB=\"one\nC=two\"\nD=\"\"three\"\"\nE=\"escaped\\n\\\"keep\"\n");
    Scalar(quotes,"S","A","");Scalar(quotes,"S","B","\"one");Scalar(quotes,"S","C","two\"");
    Scalar(quotes,"S","D","\"three\"");Scalar(quotes,"S","E","escaped\\n\\\"keep");
    for(const auto& newline:Values{"\r","\n","\r\n","\n\r","\r\r\n\r\n\n"}) {
        const auto e=Doc::Parse(newline+"[S]"+newline+"K=one"+newline+"K=two"+newline);
        Array(e,"S","K",Values{"one","two"});Scalar(e,"S","K","two");
    }
    const auto layers=Doc::Parse("[Engine.Engine]\nCacheSizeMegs=2\nServerActors=parent\n[Engine.GameEngine]\nServerActors=base\n[DeusEx.DeusExGameEngine]\nCacheSizeMegs=4\nServerActors=first command with spaces\nServerActors=second\n");
    Scalar(layers,"Engine.Engine","CacheSizeMegs","2");Scalar(layers,"DeusEx.DeusExGameEngine","CacheSizeMegs","4");
    Array(layers,"Engine.GameEngine","ServerPackages",Values{});Array(layers,"Missing.Engine","ServerActors",std::nullopt);
    Array(layers,"DeusEx.DeusExGameEngine","ServerActors",Values{"first command with spaces","second"});
    Require(Doc::Parse("").SectionCount()==0u,"Empty config invented a section");
}
void Safety() {
    using Member=std::size_t NC::Limits::*;
    for(const Member m:{&NC::Limits::fileBytes,&NC::Limits::lineUnits,&NC::Limits::sections,&NC::Limits::entries,&NC::Limits::retainedBytes,&NC::Limits::comparisonBytes}) {
        auto limit=NC::Limits{};limit.*m=0u;Refused([&]{(void)Doc::Parse("",limit);},"Disabled bound accepted");
        limit=NC::Limits{};++(limit.*m);Refused([&]{(void)Doc::Parse("",limit);},"Expanded bound accepted");
    }
    for(unsigned c=0;c<256u;++c) {
        const auto source=std::string("[S]\nK=")+static_cast<char>(c);
        const auto doc=Doc::Parse(source);Require(doc.EntryCount()==1u,"Byte-widened value/terminator dropped record");
        const auto value=c==0u||c=='\r'||c=='\n'||c=='"'?std::string{}:QuestVr::UnrealString::FromAnsi(std::string(1u,static_cast<char>(c)));
        Scalar(doc,"S","K",value);
    }
    Require(Doc::Parse("\xef\xbb\xbf[S]\nK=V").SectionCount()==0u,"UTF8 BOM was incorrectly decoded/stripped");
    Scalar(Doc::Parse(std::string("[S]\nK=a\0\nK=b",13u)),"S","K","a");
    auto limits=NC::Limits{};limits.fileBytes=4u;Require(Doc::Parse("[S]\n",limits).SectionCount()==1u,"Exact source byte limit refused");
    Refused([&]{(void)Doc::Parse("[S]\n\n",limits);},"Source byte limit ignored");
    limits=NC::Limits{};limits.lineUnits=3u;Require(Doc::Parse("[S]\nK=v",limits).EntryCount()==1u,"Exact line limit refused");
    Refused([&]{(void)Doc::Parse("[SS]",limits);},"Line byte limit ignored");
    limits=NC::Limits{};limits.sections=1u;Require(Doc::Parse("[S]\n[s]",limits).SectionCount()==1u,"Repeated section consumed count twice");
    Refused([&]{(void)Doc::Parse("[S]\n[T]",limits);},"Section count ignored");
    limits=NC::Limits{};limits.entries=1u;Require(Doc::Parse("[S]\nK=v",limits).EntryCount()==1u,"Exact entry limit refused");
    Refused([&]{(void)Doc::Parse("[S]\nK=v\nK=v",limits);},"Duplicate-entry count ignored");
    const auto exact=Doc::Parse("[S]\nK=v").RetainedBytes();limits=NC::Limits{};limits.retainedBytes=exact;
    Require(Doc::Parse("[S]\nK=v",limits).RetainedBytes()==exact,"Exact retained capacity limit refused");
    --limits.retainedBytes;Refused([&]{(void)Doc::Parse("[S]\nK=v",limits);},"Retained capacity limit ignored");
    limits=NC::Limits{};limits.comparisonBytes=1u;const auto limited=Doc::Parse("[S]",limits);
    Refused([&]{(void)limited.Scalar("S","K");},"Lookup byte-comparison budget ignored");
    Refused([&]{(void)limited.StringArray("S","K");},"Array byte-comparison budget ignored");
    Refused([&]{(void)Doc::Parse("[S]\n[S]",limits);},"Parser section-comparison budget ignored");
    const auto original=Doc::Parse("[S]\nK=ok");
    Scalar(original,std::string("S\0x",3u),"K","ok");
    Refused([&]{(void)original.Scalar(std::string(1u,static_cast<char>(0x80u)),"K");},"Malformed lookup transport accepted");
    limits=NC::Limits{};limits.lineUnits=3u;const auto names=Doc::Parse("[S]\nK=v",limits);
    Scalar(names,"SSS","K",std::nullopt);
    Refused([&]{(void)names.Scalar("SSSS","K");},"Decoded lookup unit limit ignored");
    Refused([&]{(void)names.StringArray("S","KKKK");},"Array lookup unit limit ignored");
    Scalar(original,"S","K","ok");
}
std::string WideFile(std::u16string_view units,bool bigEndian) {
    std::string result=bigEndian?std::string("\xfe\xff",2u):std::string("\xff\xfe",2u);
    for(const auto unit:units) {
        const auto low=static_cast<char>(unit&255u),high=static_cast<char>(unit>>8u);
        result.push_back(bigEndian?high:low);result.push_back(bigEndian?low:high);
    }
    return result;
}
void EncodingAndScalarLimit() {
    Require(Doc::Parse(std::string("\xff\xfe",2u)).SectionCount()==0u&&Doc::Parse(std::string("\xfe\xff",2u)).SectionCount()==0u,
        "UTF16 BOM-only document invented data");
    std::u16string source=u"[S]\r\nK=\"wide \u00e9 \u6c34 ";source.push_back(0xd83du);source.push_back(0xde00u);source+=u"\"\nK=last\n";
    for(const bool big:{false,true}) {
        const auto doc=Doc::Parse(WideFile(source,big));Scalar(doc,"S","K","last");
        std::u16string first=u"wide \u00e9 \u6c34 ";first.push_back(0xd83du);first.push_back(0xde00u);
        Array(doc,"S","K",Values{QuestVr::UnrealString::Encode(first),"last"});
        const auto terminated=Doc::Parse(WideFile(std::u16string(u"[S]\nK=one\0\nK=two",16u),big));Scalar(terminated,"S","K","one");
    }
    Require(Doc::Parse("\xff\xfe[S]\nK=x").SectionCount()==0u,"Odd UTF16 BOM file did not fall back to byte widening");
    Require(Doc::Parse("\xfe\xff[S]\nK=x").SectionCount()==0u,"Odd UTF16 BE BOM file did not fall back to byte widening");
    std::u16string broken=u"[S]\nK=";broken.push_back(0xd800u);broken.push_back(u'a');broken.push_back(0xdc00u);
    const auto isolated=Doc::Parse(WideFile(broken,false));Scalar(isolated,"S","K",QuestVr::UnrealString::Encode(broken.substr(6u)));
    const auto cLocale=Doc::Parse("[\xc9]\nK=V");Scalar(cLocale,QuestVr::UnrealString::FromAnsi(std::string(1u,static_cast<char>(0xc9u))),"k","V");
    Scalar(cLocale,QuestVr::UnrealString::FromAnsi(std::string(1u,static_cast<char>(0xe9u))),"k",std::nullopt);
    for(const auto size:{1022u,1023u,1024u,4096u}) {
        const std::string value(size,'a');const auto doc=Doc::Parse("[S]\nK="+value);
        Scalar(doc,"S","K",value);Require(doc.PropertyScalar("S","K")==std::optional<std::string>(value.substr(0u,1023u)),"Scalar property cap did not match1023 UTF16 units");
        Array(doc,"S","K",Values{value});
    }
    std::u16string longValue(1022u,u'a');longValue.push_back(0xd83du);longValue.push_back(0xde00u);
    const auto doc=Doc::Parse(WideFile(std::u16string(u"[S]\nK=")+longValue,false));
    Require(doc.PropertyScalar("S","K")==std::optional<std::string>(QuestVr::UnrealString::Encode(longValue.substr(0u,1023u))),"Scalar cap repaired/lost a split surrogate");
    Require(!doc.PropertyScalar("S","Absent"),"Missing scalar property invented empty override");
}
std::uint32_t Next(std::uint32_t& n) {n^=n<<13u;n^=n>>17u;n^=n<<5u;return n;}
void Random() {
    std::uint32_t seed=0x179d1234u;
    for(unsigned trial=0u;trial<256u;++trial) {
        std::string source;std::map<std::string,std::map<std::string,Values>> expected;
        for(unsigned i=0u;i<160u;++i) {
            const auto section="S"+std::to_string(Next(seed)%9u),key="K"+std::to_string(Next(seed)%11u);
            const auto value="value "+std::to_string(Next(seed))+" = keep // ; ";
            source+='['+section+"]\r\n"+key+'='+value+'\n';expected[section][key].push_back(value);
        }
        const auto doc=Doc::Parse(source);Require(doc.SectionCount()==expected.size()&&doc.EntryCount()==160u,"Random repeated section/entry count differs");
        for(const auto& [section,keys]:expected)for(const auto& [key,values]:keys) {
            Array(doc,section,key,values);Scalar(doc,section,key,values.back());
        }
    }
}
struct Temporary {
    std::filesystem::path parent,directory;
    Temporary() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {const auto p=parent/("quest-native-config-test-"+std::to_string(stamp)+'-'+std::to_string(i));if(std::filesystem::create_directory(p)){directory=std::filesystem::canonical(p);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create scoped config fixture directory");
    }
    ~Temporary() {std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().starts_with("quest-native-config-test-"))std::filesystem::remove_all(actual,error);}
    void Write(const std::string& bytes) const {std::ofstream out(directory/"System.ini",std::ios::binary|std::ios::trunc);Require(bool(out),"Cannot create config fixture");out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();Require(bool(out),"Cannot finish config fixture");}
};
std::string Read(const std::filesystem::path& path) {std::ifstream in(path,std::ios::binary);Require(bool(in),"Cannot inspect config source");return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
void Files() {
    Temporary t;const auto file=t.directory/"System.ini";Require(!NC::ReadOrderedIni(file),"Missing file invented config");
    Refused([&]{(void)NC::ReadOrderedIni(t.directory);},"Directory treated as config");
    t.Write("");const auto empty=NC::ReadOrderedIni(file);Require(empty&&empty->SectionCount()==0u,"Empty physical file was treated as missing");
    const std::string source="[S]\nK=1\nK=2\n";t.Write(source);const auto modified=std::filesystem::last_write_time(file);
    const auto doc=NC::ReadOrderedIni(file);Require(bool(doc),"Existing source not read");Scalar(*doc,"S","K","2");Array(*doc,"S","K",Values{"1","2"});
    Require(Read(file)==source&&std::filesystem::last_write_time(file)==modified,"Config reader modified source bytes/mtime");
    auto limits=NC::Limits{};limits.fileBytes=source.size()-1u;Refused([&]{(void)NC::ReadOrderedIni(file,limits);},"File byte budget ignored");
    t.Write("[S]\nK=changed\n");Scalar(*doc,"S","K","2");Scalar(*NC::ReadOrderedIni(file),"S","K","changed");
}
void Owned(const std::filesystem::path& root) {
    const auto file=std::filesystem::canonical(root/"System"/"DeusEx.ini");const auto before=Read(file);const auto modified=std::filesystem::last_write_time(file);
    const auto doc=NC::ReadOrderedIni(file);Require(bool(doc),"Owned original INI missing");
    Scalar(*doc,"Engine.Engine","GameEngine","DeusEx.DeusExGameEngine");Scalar(*doc,"DeusEx.DeusExGameEngine","CacheSizeMegs","4");Scalar(*doc,"DeusEx.DeusExGameEngine","UseSound","True");
    Array(*doc,"DeusEx.DeusExGameEngine","ServerActors",Values{"IpDrv.UdpBeacon","IpServer.UdpServerQuery",
        "IpServer.UdpServerUplink MasterServerAddress=master0.gamespy.com MasterServerPort=27900",
        "IpServer.UdpServerUplink MasterServerAddress=master.epicgames.com MasterServerPort=27900",
        "IpServer.UdpServerUplink MasterServerAddress=master.fragaholic.com MasterServerPort=27900"});
    Array(*doc,"DeusEx.DeusExGameEngine","ServerPackages",Values{});
    Require(Read(file)==before&&std::filesystem::last_write_time(file)==modified,"Owned INI source changed");
}
}
int main(int argc,char** argv) {
    try {Semantics();Safety();EncodingAndScalarLimit();Random();Files();if(argc==3&&std::string(argv[1])=="--game-root")Owned(argv[2]);else Require(argc==1,"Usage: quest_native_config_test [--game-root OWNED_GAME_ROOT]");
        std::cout<<"PASS native ordered config "<<checks<<" checks, "<<refusals<<" refusals; shipped-backend default-C-locale profile, byte/UTF16 decoding, scalar latest/unit cap/array source order, raw whitespace/quote rules and bounded read-only file data. Not packed-launcher factory, typed class imports/default buffers, instances or crate gameplay.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"Native ordered config failed: "<<error.what()<<'\n';return 1;}
}
