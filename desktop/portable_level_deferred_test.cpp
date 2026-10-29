#include "Precomp.h"
#include "portable_model_geometry.h"
#include "surreal_portable_package_tables.h"

#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

// Data-only admission: original serialized Level head, NOT world startup,
// native cleanup, live Actor ownership, loot/gravity or headset acceptance.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F> void Refused(F&& action,const char* why){bool no{};try{action();}catch(const std::runtime_error&){no=true;}Require(no,why);++refusals;}
void U32(Bytes& bytes,std::uint32_t value){for(unsigned shift=0u;shift<32u;shift+=8u)bytes.push_back(static_cast<std::uint8_t>(value>>shift));}
void Index(Bytes& bytes,std::int32_t value){
    const bool negative=value<0;auto magnitude=static_cast<std::uint32_t>(negative?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(negative?128u:0u));magnitude>>=6u;
    if(magnitude)first|=64u;
    bytes.push_back(first);
    while(magnitude){auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude)next|=128u;bytes.push_back(next);}
}
void AnsiString(Bytes& bytes,std::string_view text){
    Require(text.size()<static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()),"Generated ANSI string size exceeds compact bound");
    Index(bytes,static_cast<std::int32_t>(text.size()+1u));
    for(const unsigned char unit:text)bytes.push_back(unit);
    bytes.push_back(0u);
}
void WideString(Bytes& bytes,std::u16string_view text){
    Require(text.size()<static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()),"Generated UTF16 string size exceeds compact bound");
    Index(bytes,-static_cast<std::int32_t>(text.size()+1u));
    for(const auto unit:text){bytes.push_back(static_cast<std::uint8_t>(unit));bytes.push_back(static_cast<std::uint8_t>(unit>>8u));}
    bytes.push_back(0u);bytes.push_back(0u);
}
std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
bool Within(const fs::path& child,const fs::path& parent){auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b)if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;return true;}
std::uint64_t Hash(const fs::path& path){
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint original first Training");
    std::uint64_t hash=14695981039346656037ull;std::array<char,16384u> buffer{};std::uintmax_t size{};
    while(file){file.read(buffer.data(),buffer.size());const auto count=file.gcount();size+=static_cast<std::uintmax_t>(count);
        Require(size<=512u*1024u*1024u,"Original fingerprint exceeds bounded source size");
        for(std::streamsize i=0;i<count;++i){hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    Require(file.eof(),"Cannot finish original fingerprint");return hash;
}
struct Fixture {
    fs::path parent,directory,file;PortablePackageTables package;
    explicit Fixture(const fs::path& original={}){
        parent=fs::canonical(fs::temp_directory_path());Require(parent!=parent.root_path(),"Temporary parent cannot be a filesystem root");
        if(!original.empty())Require(!Within(parent,fs::canonical(original)),"Temporary decoder fixture overlaps originals");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i){const auto next=parent/("quest-level-deferred-"+std::to_string(stamp)+'-'+std::to_string(i));if(fs::create_directory(next)){directory=fs::canonical(next);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot isolate generated Level fixture");
        file=directory/"generated-level.bin";package.sourcePath=file.string();package.version=68u;
        for(const auto* name:{"None","Engine","Level","Model","Actor","MyLevel","RootModel","Actor0"})package.names.push_back({NameString(name),0u});
        package.imports={{0,0,0,1},{0,0,-1,2},{0,0,-1,3},{0,0,-1,4}};
        package.exports={{-2,0,0,5,ObjectFlags{},0,0},{-3,0,0,6,ObjectFlags{},0,0},{-4,0,0,7,ObjectFlags{},0,0}};
    }
    ~Fixture(){std::error_code error;if(!directory.empty()&&directory.parent_path()==parent){fs::remove(file,error);fs::remove(directory,error);}}
    void Write(const Bytes& bytes){std::ofstream out(file,std::ios::binary|std::ios::trunc);Require(bool(out),"Cannot create generated decoder bytes");
        out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));out.close();Require(bool(out),"Cannot finish generated decoder bytes");
        package.exports[0].ObjOffset=0;package.exports[0].ObjSize=static_cast<std::int32_t>(bytes.size());}
};
struct Body {
    Bytes bytes;std::size_t prefix{},reachCount{},start{},end{},time{},preliminary{},head{},travel{};
    std::array<std::size_t,16u> tail{};
    explicit Body(std::int32_t first=3,std::uint32_t timeBits=0xc0200000u,
        std::optional<std::int32_t> initial={},std::int32_t slotZero=0){
        Index(bytes,0);U32(bytes,3u);U32(bytes,4u);Index(bytes,3);Index(bytes,0);Index(bytes,3);
        for(unsigned i=0;i<4u;++i)Index(bytes,0);
        Index(bytes,0);U32(bytes,7777u);U32(bytes,0u);Index(bytes,2);prefix=bytes.size();
        reachCount=bytes.size();Index(bytes,2);
        for(unsigned i=0;i<2u;++i){U32(bytes,100u+i);if(!i)start=bytes.size();Index(bytes,i?-4:3);if(!i)end=bytes.size();Index(bytes,i?0:3);
            U32(bytes,40u);U32(bytes,70u);U32(bytes,0x100020u+i);bytes.push_back(i?0xffu:0u);}
        time=bytes.size();U32(bytes,timeBits);preliminary=bytes.size();head=preliminary;Index(bytes,first);
        for(std::size_t index=0u;index<tail.size();++index) {
            tail[index]=bytes.size();
            Index(bytes,index==0u?slotZero:index==1u?initial.value_or(first):0);
        }
        travel=bytes.size();Index(bytes,0); // Actual v68 travel-map count.
    }
};
void Generated(const fs::path& original){
    Fixture fixture(original);const Body good;fixture.Write(good.bytes);
    const auto read=[&]{return ReadPortableLevel68DeferredState(fixture.package,65536u);};
    const auto value=read();Require(value.firstDeleted==3&&value.reachSpecs==2u&&value.firstDeletedOffset==good.head&&
        std::bit_cast<std::uint32_t>(value.serializedTime)==0xc0200000u&&value.travelMap.empty()&&
        value.travelMapOffset==good.travel&&value.travelMapEndOffset==good.bytes.size(),
        "Generated original native sequence/offset/value/travel-map changed");
    for(const auto first:{0,-4,2}){const Body body(first,0x80000000u);fixture.Write(body.bytes);const auto result=read();
        Require(result.firstDeleted==first&&std::bit_cast<std::uint32_t>(result.serializedTime)==0x80000000u,
            "Decoder discarded valid compact reference or signed-zero time");}
    for(const auto pair:{std::array<std::int32_t,2u>{0,3},std::array<std::int32_t,2u>{3,0}}) {
        const Body body(pair[0u],0xc0200000u,pair[1u]);fixture.Write(body.bytes);const auto result=read();
        Require(result.firstDeleted==pair[0u]&&result.firstDeletedOffset==body.head&&body.head==body.preliminary,
            "Unrelated hexadecimal +0xA0 reference overwrote actual +0x100 FirstDeleted");
    }
    // Independent native-address oracle from the actual LEA encodings:
    // [EDI+0x100] versus [EDI+ESI*4+0x9C], ESI=0..15. Exhaust every later
    // slot with different valid local/import/None references and both heads.
    constexpr std::array<std::size_t,16u> nativeOffsets{
        0x9cu,0xa0u,0xa4u,0xa8u,0xacu,0xb0u,0xb4u,0xb8u,
        0xbcu,0xc0u,0xc4u,0xc8u,0xccu,0xd0u,0xd4u,0xd8u};
    for(const auto first:{0,3})for(std::size_t slot=0u;slot<nativeOffsets.size();++slot)
        for(const auto reference:{0,2,3,-4}) {
            const Body body(first);auto bytes=body.bytes;
            Require(nativeOffsets[slot]!=0x100u,"Independent native pointer offset unexpectedly aliases FirstDeleted");
            bytes[body.tail[slot]]=static_cast<std::uint8_t>(reference<0?0x80u+static_cast<unsigned>(-reference):reference);
            fixture.Write(bytes);const auto parsed=read();
            Require(parsed.firstDeleted==first&&parsed.firstDeletedOffset==body.head,
                "A valid unrelated native pointer changed the real serialized deferred queue head");
        }
    // Source-table reference ranges admit these longer compact imports without
    // inventing a runtime UObject. Later variable-length references must not
    // move the already-read actual FirstDeleted witness offset.
    const auto smallImports=fixture.package.imports;
    fixture.package.imports.resize(8192u,smallImports.front());
    const Body variable(3,0xc0200000u,-65,-8192);fixture.Write(variable.bytes);const auto moved=read();
    Require(moved.firstDeleted==3&&moved.firstDeletedOffset==good.head&&moved.firstDeletedOffset==variable.head&&
        variable.tail[1u]==variable.head+4u,
        "Later variable-width unrelated references changed FirstDeleted value/offset");
    fixture.package.imports=smallImports;
    // The native map archives ordered key/value FString pairs, not its hash
    // links. Preserve duplicates, empty source strings, ANSI zero-extension,
    // isolated surrogates and units after embedded NULs without eager lookup.
    auto travel=good.bytes;travel.resize(good.travel);Index(travel,4);
    AnsiString(travel,"Player");AnsiString(travel,"First");
    AnsiString(travel,"player");AnsiString(travel,"");
    const std::string ansiUnits{'A',static_cast<char>(0x80u),static_cast<char>(0xffu),'\0','Z'};
    const std::u16string wideUnits{u'B',static_cast<char16_t>(0xd800u),u'\0',static_cast<char16_t>(0xdc00u),u'Q'};
    AnsiString(travel,ansiUnits);WideString(travel,wideUnits);
    Index(travel,0);Index(travel,0); // Zero-length native FString storage.
    const auto travelEnd=travel.size();travel.insert(travel.end(),{0x7fu,0xffu,0x80u}); // Later Level arrays remain outside this API.
    fixture.Write(travel);const auto entries=read();
    Require(entries.travelMap==std::vector<PortableLevel68TravelEntry>{{u"Player",u"First"},{u"player",u""},
        {std::u16string{u'A',static_cast<char16_t>(0x80u),static_cast<char16_t>(0xffu),u'\0',u'Z'},wideUnits},{u"",u""}}&&
        entries.travelMapOffset==good.travel&&entries.travelMapEndOffset==travelEnd,
        "Actual travel pair order/duplicates/empty/ANSI/UTF16 units or exact prefix boundary changed");
    for(std::size_t size=good.travel;size<travelEnd;++size){fixture.Write(Bytes(travel.begin(),travel.begin()+size));
        Refused(read,"Truncated nonempty travel count/key/value was accepted");}
    auto missing=good.bytes;missing.resize(good.travel);fixture.Write(missing);
    Refused(read,"Missing travel archive count fabricated an empty runtime map");
    for(const auto count:{-1,1'000'001,std::numeric_limits<std::int32_t>::max()}) {
        auto malformed=missing;Index(malformed,count);malformed.insert(malformed.end(),8u,0u);fixture.Write(malformed);
        Refused(read,"Negative/excessive travel count was accepted");
    }
    for(const auto unicode:{false,true}) {
        auto malformed=missing;Index(malformed,1);Index(malformed,unicode?-2:2);
        malformed.push_back('X');if(unicode)malformed.push_back(0u);
        malformed.push_back('Y');if(unicode)malformed.push_back(0u);
        Index(malformed,0);fixture.Write(malformed);
        Refused(read,"Unterminated archived travel FString was accepted");
    }
    for(const auto count:{std::numeric_limits<std::int32_t>::max(),-std::numeric_limits<std::int32_t>::max()}) {
        auto malformed=missing;Index(malformed,1);Index(malformed,count);Index(malformed,0);fixture.Write(malformed);
        Refused(read,"Oversized/truncated travel string magnitude was accepted");
    }
    auto many=missing;Index(many,80);
    const std::string heapString(80u,'a');
    for(std::size_t i=0u;i<80u;++i){AnsiString(many,heapString);AnsiString(many,heapString);}
    fixture.Write(many);const auto retained=read();
    Require(retained.travelMap.size()==80u&&retained.travelMap.back().value==std::u16string(80u,u'a'),
        "Cumulatively bounded owned travel string payload changed");
    Refused([&]{static_cast<void>(ReadPortableLevel68DeferredState(fixture.package,32768u));},
        "Travel entry/string allocations bypassed cumulative body/actor/string working budget");
    auto longValue=missing;Index(longValue,1);Index(longValue,0);AnsiString(longValue,std::string(70'000u,'x'));
    fixture.Write(longValue);const auto longParsed=ReadPortableLevel68DeferredState(fixture.package,262144u);
    Require(longParsed.travelMap.size()==1u&&longParsed.travelMap.front().key.empty()&&
        longParsed.travelMap.front().value==std::u16string(70'000u,u'x'),
        "Travel payload was silently constrained to URL's unrelated65536-unit string bound");
    for(std::size_t size=0u;size<good.bytes.size();++size){fixture.Write(Bytes(good.bytes.begin(),good.bytes.begin()+size));
        Refused(read,"Truncated Level native deferred prefix was accepted");}
    fixture.Write(Bytes(good.bytes.begin(),good.bytes.begin()+good.prefix));
    Require(FindPortableRootModel68Export(fixture.package)==1u&&ReadPortableLevel68ActorOrder(fixture.package)==std::vector<std::int32_t>({3,0,3}),
        "Existing model-only prefix consumers now require deferred tail");Refused(read,"Model-only prefix fabricated a None head");
    for(const auto offset:{good.start,good.end})for(const auto invalid:{std::uint8_t{5u},std::uint8_t{0x85u}}){auto bytes=good.bytes;bytes[offset]=invalid;fixture.Write(bytes);
        Refused(read,"ReachSpec source/destination reference outside actual tables was accepted");}
    for(const auto count:{std::uint8_t{0x81u},std::uint8_t{63u}}){auto bytes=good.bytes;bytes[good.reachCount]=count;fixture.Write(bytes);
        Refused(read,"Negative/oversized remaining ReachSpec count was accepted");}
    for(const auto first:{4,-5}){const Body body(first);fixture.Write(body.bytes);Refused(read,"FirstDeleted outside actual export/import tables was accepted");}
    for(const auto first:{4,-5}){const Body body(0,0xc0200000u,first);fixture.Write(body.bytes);
        Refused(read,"Invalid unrelated +0xA0 reference escaped whole-prefix validation");}
    for(const auto offset:good.tail)for(const auto invalid:{std::uint8_t{4u},std::uint8_t{0x85u}}) {
        auto invalidTail=good.bytes;invalidTail[offset]=invalid;fixture.Write(invalidTail);
        Refused(read,"One of sixteen native pointer references escaped source-table bounds");
    }
    for(const auto bits:{0x7f800000u,0xff800000u,0x7fc00001u}){const Body body(0,bits);fixture.Write(body.bytes);Refused(read,"Nonfinite serialized Level time was accepted");}
    for(const auto offset:{good.head,good.tail[1u]}) {
        Bytes bytes(good.bytes.begin(),good.bytes.begin()+offset);bytes.insert(bytes.end(),{0x7fu,0xffu,0xffu,0xffu,0x7fu});
        bytes.insert(bytes.end(),good.bytes.begin()+offset+1u,good.bytes.end());fixture.Write(bytes);
        Refused(read,"Overflowing deferred/unrelated compact native reference was accepted");
    }
    fixture.Write(good.bytes);const auto baseline=fixture.package;
    fixture.package.version=69u;Refused(read,"Unsupported Level version accepted");fixture.package=baseline;
    fixture.package.licenseeMode=1u;Refused(read,"Unsupported Level licensee accepted");fixture.package=baseline;
    fixture.package.exports[1].ObjClass=-4;Refused(read,"Non-Model source reference accepted");fixture.package=baseline;
    fixture.package.exports[2].ObjClass=-2;Refused(read,"Ambiguous actual Level selection accepted");fixture.package=baseline;
    for(const auto limit:{0u,1u,32u})Refused([&]{static_cast<void>(ReadPortableLevel68DeferredState(fixture.package,limit));},"Deferred decoder bypassed caller working budget");
    std::cout<<"PASS generated native Level deferred/travel prefix: "<<checks<<" checks / "<<refusals<<" bounded malformed/truncated refusals\n";
}
void Owned(const fs::path& root){
    const auto path=fs::canonical(root/"Maps"/"00_Training.dx");const auto size=fs::file_size(path);
    const auto hash=Hash(path);const auto modified=fs::last_write_time(path);
    const auto map=LoadPortablePackageTables(path.string());const auto result=ReadPortableLevel68DeferredState(map,65536u);
    // Independent literal witnesses from static ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md,
    // NOT expected values produced by a production table/decoder.
    Require(result.firstDeleted==0&&result.reachSpecs==60u&&result.firstDeletedOffset==4370509u&&
        std::bit_cast<std::uint32_t>(result.serializedTime)==0x4324ee0du&&result.travelMap.empty()&&
        result.travelMapOffset==4370527u&&result.travelMapEndOffset==4370528u,
        "First Training differs from original deferred/travel-state witnesses");
    std::ifstream file(path,std::ios::binary);file.seekg(4370505);std::array<unsigned char,7u> witness{};
    file.read(reinterpret_cast<char*>(witness.data()),witness.size());Require(bool(file)&&witness==std::array<unsigned char,7u>{0x0du,0xeeu,0x24u,0x43u,0u,0u,0u},
        "Independent raw time/FirstDeleted/unrelated native reference source bytes changed");
    file.seekg(4370527);std::array<unsigned char,4u> travelWitness{};
    file.read(reinterpret_cast<char*>(travelWitness.data()),travelWitness.size());
    Require(bool(file)&&travelWitness==std::array<unsigned char,4u>{0u,0x59u,0x04u,0x5bu},
        "Independent raw first Training empty travel count/prefix boundary changed");
    // Repeat the conflicting-reference controls on an isolated copy of the
    // actual first map, not just the generated minimum format. Commercial
    // bytes remain data-only, temporary and never part of the repository.
    Require(size<64u*1024u*1024u,"Owned map copy exceeds independent test byte bound");
    Bytes original(static_cast<std::size_t>(size));std::ifstream source(path,std::ios::binary);
    source.read(reinterpret_cast<char*>(original.data()),static_cast<std::streamsize>(original.size()));
    Require(bool(source),"Cannot read bounded owned map data-only fixture");
    Fixture copy(root);auto copiedTables=map;copiedTables.sourcePath=copy.file.string();
    for(const auto pair:{std::array<std::uint8_t,2u>{0u,3u},std::array<std::uint8_t,2u>{3u,0u}}) {
        auto changed=original;changed.at(4370509u)=pair[0u];changed.at(4370511u)=pair[1u];
        {std::ofstream output(copy.file,std::ios::binary|std::ios::trunc);
            output.write(reinterpret_cast<const char*>(changed.data()),static_cast<std::streamsize>(changed.size()));
            Require(bool(output),"Cannot write isolated owned-map reference fixture");}
        const auto parsed=ReadPortableLevel68DeferredState(copiedTables,65536u);
        Require(parsed.firstDeleted==pair[0u]&&parsed.firstDeletedOffset==4370509u&&parsed.travelMap.empty()&&
            parsed.travelMapOffset==4370527u&&parsed.travelMapEndOffset==4370528u,
            "Actual Training copy incorrectly aliases native0xA0 and0x100 archive destinations");
    }
    Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Hash(path)==hash,"Original Training bytes/size/mtime changed");
    std::cout<<"PASS ONLY first Training actual serialized deferred head: None / 60 ReachSpecs / absolute4370509 (native0x100, NOT unrelated0xA0/4370511) / Float4324EE0D / empty travel count4370527, end4370528; original unchanged. No runtime/startup/cleanup/physics claim.\n";
}
}
int main(int argc,char** argv){try{
    Require(argc<=2,"Usage: portable_level_deferred_test [OWNED_ROOT | --generated-only]");
    const bool generatedOnly=argc==2&&std::string(argv[1])=="--generated-only";
    const char* env=std::getenv("DEUSEX_GAME_ROOT");const fs::path root=argc==2&&!generatedOnly?fs::path(argv[1]):env&&!generatedOnly?fs::path(env):fs::path{};
    Generated(root);if(generatedOnly)return 0;
    if(root.empty()){std::cout<<"SKIP original first Training: supply owned source root\n";return 77;}
    Owned(fs::canonical(root));std::cout<<"PASS Level deferred admission: "<<checks<<" checks / "<<refusals<<" refusals\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL Level deferred admission: "<<error.what()<<'\n';return 1;}}
