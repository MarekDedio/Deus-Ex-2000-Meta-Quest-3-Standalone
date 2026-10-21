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
#include <optional>
#include <stdexcept>
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
    Bytes bytes;std::size_t prefix{},reachCount{},start{},end{},time{},preliminary{},head{};
    std::array<std::size_t,16u> tail{};
    explicit Body(std::int32_t first=3,std::uint32_t timeBits=0xc0200000u,
        std::optional<std::int32_t> initial={},std::int32_t slotZero=0){
        Index(bytes,0);U32(bytes,3u);U32(bytes,4u);Index(bytes,3);Index(bytes,0);Index(bytes,3);
        for(unsigned i=0;i<4u;++i)Index(bytes,0);
        Index(bytes,0);U32(bytes,7777u);U32(bytes,0u);Index(bytes,2);prefix=bytes.size();
        reachCount=bytes.size();Index(bytes,2);
        for(unsigned i=0;i<2u;++i){U32(bytes,100u+i);if(!i)start=bytes.size();Index(bytes,i?-4:3);if(!i)end=bytes.size();Index(bytes,i?0:3);
            U32(bytes,40u);U32(bytes,70u);U32(bytes,0x100020u+i);bytes.push_back(i?0xffu:0u);}
        time=bytes.size();U32(bytes,timeBits);preliminary=bytes.size();Index(bytes,initial.value_or(first));
        for(std::size_t index=0u;index<tail.size();++index) {
            tail[index]=bytes.size();
            if(index==1u)head=bytes.size();
            Index(bytes,index==0u?slotZero:index==1u?first:0);
        }
    }
};
void Generated(const fs::path& original){
    Fixture fixture(original);const Body good;fixture.Write(good.bytes);
    const auto read=[&]{return ReadPortableLevel68DeferredState(fixture.package,65536u);};
    const auto value=read();Require(value.firstDeleted==3&&value.reachSpecs==2u&&value.firstDeletedOffset==good.head&&
        std::bit_cast<std::uint32_t>(value.serializedTime)==0xc0200000u,"Generated original native sequence/offset/value changed");
    for(const auto first:{0,-4,2}){const Body body(first,0x80000000u);fixture.Write(body.bytes);const auto result=read();
        Require(result.firstDeleted==first&&std::bit_cast<std::uint32_t>(result.serializedTime)==0x80000000u,
            "Decoder discarded valid compact reference or signed-zero time");}
    for(const auto pair:{std::array<std::int32_t,2u>{0,3},std::array<std::int32_t,2u>{3,0}}) {
        const Body body(pair[1u],0xc0200000u,pair[0u]);fixture.Write(body.bytes);const auto result=read();
        Require(result.firstDeleted==pair[1u]&&result.firstDeletedOffset==body.head,
            "Serialized +100 alias asserted equality or retained the preliminary value instead of last store");
    }
    // Source-table reference ranges admit these longer compact imports without
    // inventing a runtime UObject. The final offset must follow encoded lengths.
    const auto smallImports=fixture.package.imports;
    fixture.package.imports.resize(8192u,smallImports.front());
    const Body variable(3,0xc0200000u,-65,-8192);fixture.Write(variable.bytes);const auto moved=read();
    Require(moved.firstDeleted==3&&moved.firstDeletedOffset==good.preliminary+5u&&moved.firstDeletedOffset==variable.head,
        "Final alias offset ignored two-byte preliminary/three-byte preceding compact reference");
    fixture.package.imports=smallImports;
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
        Refused(read,"Invalid preliminary reference was hidden by a valid later alias");}
    for(const auto offset:good.tail)for(const auto invalid:{std::uint8_t{4u},std::uint8_t{0x85u}}) {
        auto invalidTail=good.bytes;invalidTail[offset]=invalid;fixture.Write(invalidTail);
        Refused(read,"One of sixteen native pointer references escaped source-table bounds");
    }
    for(const auto bits:{0x7f800000u,0xff800000u,0x7fc00001u}){const Body body(0,bits);fixture.Write(body.bytes);Refused(read,"Nonfinite serialized Level time was accepted");}
    for(const auto offset:{good.preliminary,good.head}) {
        Bytes bytes(good.bytes.begin(),good.bytes.begin()+offset);bytes.insert(bytes.end(),{0x7fu,0xffu,0xffu,0xffu,0x7fu});
        bytes.insert(bytes.end(),good.bytes.begin()+offset+1u,good.bytes.end());fixture.Write(bytes);
        Refused(read,"Overflowing preliminary/final compact deferred reference was accepted");
    }
    fixture.Write(good.bytes);const auto baseline=fixture.package;
    fixture.package.version=69u;Refused(read,"Unsupported Level version accepted");fixture.package=baseline;
    fixture.package.licenseeMode=1u;Refused(read,"Unsupported Level licensee accepted");fixture.package=baseline;
    fixture.package.exports[1].ObjClass=-4;Refused(read,"Non-Model source reference accepted");fixture.package=baseline;
    fixture.package.exports[2].ObjClass=-2;Refused(read,"Ambiguous actual Level selection accepted");fixture.package=baseline;
    for(const auto limit:{0u,1u,32u})Refused([&]{static_cast<void>(ReadPortableLevel68DeferredState(fixture.package,limit));},"Deferred decoder bypassed caller working budget");
    std::cout<<"PASS generated native Level prefix: "<<checks<<" checks / "<<refusals<<" bounded malformed/truncated refusals\n";
}
void Owned(const fs::path& root){
    const auto path=fs::canonical(root/"Maps"/"00_Training.dx");const auto size=fs::file_size(path);
    const auto hash=Hash(path);const auto modified=fs::last_write_time(path);
    const auto map=LoadPortablePackageTables(path.string());const auto result=ReadPortableLevel68DeferredState(map,65536u);
    // Independent literal witnesses from static ORIGINAL-DEFERRED-ACTOR-OWNERSHIP.md,
    // NOT expected values produced by a production table/decoder.
    Require(result.firstDeleted==0&&result.reachSpecs==60u&&result.firstDeletedOffset==4370511u&&
        std::bit_cast<std::uint32_t>(result.serializedTime)==0x4324ee0du,"First Training differs from original deferred-state witnesses");
    std::ifstream file(path,std::ios::binary);file.seekg(4370505);std::array<unsigned char,7u> witness{};
    file.read(reinterpret_cast<char*>(witness.data()),witness.size());Require(bool(file)&&witness==std::array<unsigned char,7u>{0x0du,0xeeu,0x24u,0x43u,0u,0u,0u},
        "Independent raw time/preliminary/final FirstDeleted source bytes changed");
    Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Hash(path)==hash,"Original Training bytes/size/mtime changed");
    std::cout<<"PASS ONLY first Training final serialized deferred head: None / 60 ReachSpecs / absolute4370511 (preliminary4370509) / Float4324EE0D; original unchanged. No runtime/startup/cleanup/physics claim.\n";
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
