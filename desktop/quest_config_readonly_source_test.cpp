#include "quest_config_readonly_source.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {bool denyAllocations{};}
#if defined(__GNUC__)
#define READONLY_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define READONLY_NOINLINE __declspec(noinline)
#else
#define READONLY_NOINLINE
#endif
READONLY_NOINLINE void* operator new(std::size_t size) {
    if(denyAllocations)throw std::bad_alloc();
    if(auto* value=std::malloc(size?size:1u))return value;
    throw std::bad_alloc();
}
READONLY_NOINLINE void* operator new[](std::size_t size){return ::operator new(size);}
READONLY_NOINLINE void operator delete(void* value) noexcept{std::free(value);}
READONLY_NOINLINE void operator delete[](void* value) noexcept{std::free(value);}
READONLY_NOINLINE void operator delete(void* value,std::size_t) noexcept{std::free(value);}
READONLY_NOINLINE void operator delete[](void* value,std::size_t) noexcept{std::free(value);}
#undef READONLY_NOINLINE

namespace {
using namespace QuestVr::ConfigFile;
unsigned checks{},refusals{};
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class Action>void Refuse(Action&& action,const char* message) {
    bool refused{};try{action();}catch(const std::exception&){refused=true;++refusals;}Require(refused,message);
}
Bytes Text(std::string_view value){return {value.begin(),value.end()};}
bool TryWrite(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)return false;
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));out.close();return bool(out);
}
void Write(const std::filesystem::path& path,const Bytes& bytes){Require(TryWrite(path,bytes),"Cannot write generated source fixture");}
Bytes RawRead(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);Require(bool(in),"Cannot inspect generated source fixture");
    const auto size=in.tellg();Require(size>=0&&static_cast<std::uint64_t>(size)<=MaximumBytes+1u,"Generated source read exceeds bound");
    Bytes bytes(static_cast<std::size_t>(size));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),size);
    Require(bool(in),"Generated source read truncated");return bytes;
}
std::uint64_t Hash(const Bytes& bytes) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto byte:bytes){hash^=byte;hash*=1099511628211ull;}return hash;
}
struct Fixture {
    std::filesystem::path root,parent,path,moved;
    Bytes original;
    std::filesystem::file_time_type time;
    std::uint64_t hash{};
    explicit Fixture(Bytes bytes=Text("[Engine.Input]\r\nAliases[0]=(Command=Fire,Alias=Fire)\r\n")):original(std::move(bytes)) {
        const auto base=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned attempt=0u;attempt<20u;++attempt) {
            const auto proposed=base/("deusex-readonly-config-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if(std::filesystem::create_directory(proposed)){root=std::filesystem::canonical(proposed);break;}
        }
        Require(!root.empty()&&root.parent_path()==base&&root.filename().string().starts_with("deusex-readonly-config-"),
            "Cannot create bounded isolated read-only fixture");
        parent=root/"source";path=parent/"DefUser.ini";moved=root/"moved";
        Require(std::filesystem::create_directory(parent),"Cannot create generated source parent");
        Write(path,original);time=std::filesystem::last_write_time(path);hash=Hash(original);
    }
    void Unchanged() const {
        const auto bytes=RawRead(path);
        Require(bytes==original&&Hash(bytes)==hash&&std::filesystem::file_size(path)==original.size()&&
            std::filesystem::last_write_time(path)==time,"Read-only wrapper changed source bytes/hash/length/mtime");
        Require(!std::filesystem::exists(root/"Config")&&!std::filesystem::exists(parent/"Config")&&
            !std::filesystem::exists(parent/"User.ini")&&!std::filesystem::exists(parent/"User.ini.lock")&&
            !std::filesystem::exists(parent/"User.ini.stage"),"Read-only wrapper created writable backend artifacts");
    }
    ~Fixture() {
        std::error_code ignored;
        // Known generated leaves only; never a recursive or original-source removal.
        for(const auto& dir:{parent,moved}) {
            for(const auto* leaf:{"DefUser.ini","alias.ini"}) {
#ifdef _WIN32
                SetFileAttributesW((dir/leaf).c_str(),FILE_ATTRIBUTE_NORMAL);
#endif
                std::filesystem::remove(dir/leaf,ignored);
            }
            std::filesystem::remove(dir,ignored);
        }
        for(const auto* leaf:{"leaf-link","parent-link","other.ini"})std::filesystem::remove(root/leaf,ignored);
        std::filesystem::remove(root,ignored);
    }
};
constexpr auto Allow=[](std::size_t){};

void BasicAndBinding() {
    ReadOnlySource empty;
    Require(!empty.Bound()&&empty.RetainedBytes()==sizeof(ReadOnlySource),"Empty read-only binding fabricated retained authority");
    Refuse([&]{static_cast<void>(empty.Path());},"Unbound source exposed a path");
    Refuse([&]{static_cast<void>(empty.ReadAdmitted(Allow));},"Unbound source returned cached bytes");
    Fixture fixture;
    {
        std::weak_ptr<int> borrowed;
        auto token=std::make_shared<int>(1);borrowed=token;
        auto source=ReadOnlySource::BindAdmitted(fixture.path,[capture=token](std::size_t){static_cast<void>(capture);});
        token.reset();Require(borrowed.expired(),"Source retained/copied the caller's admission callback");
        Require(source.Bound()&&Detail::SamePath(source.Path(),fixture.path)&&source.RetainedBytes()>sizeof(source)+fixture.original.size(),
            "Read-only source omitted pinned/cached retained storage");
        const auto& first=source.ReadAdmitted(Allow);auto copy=source;
        Require(first==fixture.original && &copy.ReadAdmitted(Allow)==&first,"Binding copies duplicated or replaced immutable cache authority");
        source.RevalidateAdmitted(fixture.parent/"."/"DefUser.ini",Allow);
        Write(fixture.root/"other.ini",fixture.original);
        Refuse([&]{source.RevalidateAdmitted(fixture.root/"other.ini",Allow);},"Idempotent preparation accepted source rebinding");
        Require(source.ReadAdmitted(Allow)==fixture.original,"Rejected source rebinding damaged original authority");
        bool nestedRefused{};std::size_t nestedAdmissions{};
        static_cast<void>(source.ReadAdmitted([copy,&nestedRefused,&nestedAdmissions](std::size_t){
            try{static_cast<void>(copy.ReadAdmitted([&](std::size_t){++nestedAdmissions;}));}
            catch(const std::exception&){nestedRefused=true;}
        }));
        Require(nestedRefused&&nestedAdmissions==0u,"Shared-binding callback reentrancy acquired/read the same held source");
        fixture.Unchanged();
    }
    fixture.Unchanged();
}
void AdmissionAndBounds() {
    for(const auto size:{std::size_t{0u},std::size_t{7u},MaximumBytes}) {
        Fixture fixture(Bytes(size,0xa5u));std::array<std::size_t,8u> peaks{};std::size_t calls{};bool reached{};
        ReadOnlySource source;denyAllocations=true;
        try {
            source=ReadOnlySource::BindAdmitted(fixture.path,[&](std::size_t peak){
                denyAllocations=false;reached=true;Require(calls<peaks.size(),"Unbounded bind admission sequence");peaks[calls++]=peak;
            });
        }catch(...){denyAllocations=false;throw;}
        Require(reached&&calls>=2u&&peaks[calls-1u]>=source.RetainedBytes(),"Bind allocated or omitted cached binding admission");
        calls=0u;reached=false;denyAllocations=true;
        try {
            const auto& bytes=source.ReadAdmitted([&](std::size_t peak){
                denyAllocations=false;reached=true;Require(calls<peaks.size(),"Unbounded source-read admission sequence");peaks[calls++]=peak;
            });
            Require(bytes==fixture.original,"Admitted source read changed raw bytes");
        }catch(...){denyAllocations=false;throw;}
        Require(reached&&calls==2u&&peaks[1u]==peaks[0u]+size,
            "Read admission did not use actual held payload size or charged retained cached bytes twice");
        const auto exact=peaks[1u];
        Require(source.ReadAdmitted([&](std::size_t peak){if(peak>exact)throw std::runtime_error("exact quota");})==fixture.original,
            "Exact read scratch quota refused");
        Refuse([&]{static_cast<void>(source.ReadAdmitted([&](std::size_t peak){if(peak>=exact)throw std::runtime_error("short quota");}));},
            "Read allocation occurred despite one-byte-short admission");
        ReadOnlySource refused;
        Refuse([&]{refused=ReadOnlySource::BindAdmitted(fixture.path,[](std::size_t){throw std::runtime_error("deny binding");});},
            "Source ignored initial binding refusal");
        Require(!refused.Bound()&&source.ReadAdmitted(Allow)==fixture.original,"Admission refusal leaked binding or damaged original source");
        fixture.Unchanged();
    }
    Fixture fixture;
    for(const auto bound:{std::size_t{0u},MaximumBytes+1u})
        Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.path,Allow,bound));},"Invalid source cap accepted");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.path,Allow,fixture.original.size()-1u));},"Tightened source cap ignored");
    Fixture oversized(Bytes(MaximumBytes+1u,0x5au));
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(oversized.path,Allow));},"Oversized immutable source accepted");
    fixture.Unchanged();oversized.Unchanged();
}
void InvalidAndMissingPaths() {
    Fixture fixture;
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(std::filesystem::path{},Allow));},"Empty source path accepted");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted("DefUser.ini",Allow));},"Implicit relative source path accepted");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.root/"missing"/"DefUser.ini",Allow));},"Missing parent accepted");
    Require(!std::filesystem::exists(fixture.root/"missing"),"Source bind created a missing parent");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.parent/"missing.ini",Allow));},"Missing leaf accepted");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.parent,Allow));},"Directory accepted as source file");
    auto withNul=fixture.path.native();withNul.push_back(0);withNul+=fixture.path.filename().native();
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(std::filesystem::path(withNul),Allow));},"Embedded NUL source path accepted");
    fixture.Unchanged();
}
void LinkRefusals() {
    Fixture fixture;std::error_code error;
    std::filesystem::create_hard_link(fixture.path,fixture.parent/"alias.ini",error);
    Require(!error,"Cannot create generated hardlink control");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.path,Allow));},"Multiply linked original source accepted");
    Require(std::filesystem::remove(fixture.parent/"alias.ini"),"Cannot remove generated hardlink control");
    std::filesystem::create_symlink(fixture.path,fixture.root/"leaf-link",error);
    if(!error)Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.root/"leaf-link",Allow));},"Source leaf symlink accepted");
#ifndef _WIN32
    else Require(false,"Cannot create POSIX leaf symlink fixture");
#endif
    error.clear();std::filesystem::create_directory_symlink(fixture.parent,fixture.root/"parent-link",error);
    if(!error)Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fixture.root/"parent-link"/"DefUser.ini",Allow));},"Source ancestor symlink accepted");
#ifndef _WIN32
    else Require(false,"Cannot create POSIX parent symlink fixture");
#endif
    fixture.Unchanged();
    Fixture afterBinding;
    auto source=ReadOnlySource::BindAdmitted(afterBinding.path,Allow);
    error.clear();std::filesystem::create_hard_link(afterBinding.path,afterBinding.parent/"alias.ini",error);
    if(!error)Refuse([&]{static_cast<void>(source.ReadAdmitted(Allow));},"Read accepted a hardlink introduced after binding");
#ifndef _WIN32
    else Require(false,"Cannot create POSIX post-binding hardlink fixture");
#endif
    afterBinding.Unchanged();
}
void ReadOnlyPermissions() {
    Fixture fixture;
#ifdef _WIN32
    Require(SetFileAttributesW(fixture.path.c_str(),FILE_ATTRIBUTE_READONLY)!=0,"Cannot mark generated source read-only");
#else
    std::filesystem::permissions(fixture.path,std::filesystem::perms::owner_read,std::filesystem::perm_options::replace);
#endif
    {
        auto source=ReadOnlySource::BindAdmitted(fixture.path,Allow);
        Require(source.ReadAdmitted(Allow)==fixture.original,"Read-only source incorrectly required write permissions");
        fixture.Unchanged();
    }
#ifndef _WIN32
    const auto fifo=fixture.root/"other.ini";
    Require(mkfifo(fifo.c_str(),0600)==0,"Cannot create generated FIFO refusal control");
    Refuse([&]{static_cast<void>(ReadOnlySource::BindAdmitted(fifo,Allow));},"Nonregular FIFO source accepted");
#endif
}
void SourceAndParentSubstitution() {
    for(const bool parent:{false,true}) {
        Fixture fixture;auto source=ReadOnlySource::BindAdmitted(fixture.path,Allow);std::error_code error;
        const auto moved=parent?fixture.moved:fixture.parent/"alias.ini";
        std::filesystem::rename(parent?fixture.parent:fixture.path,moved,error);
#ifdef _WIN32
        Require(bool(error),"Windows source/ancestor pin unexpectedly allowed deletion/rename");
        Require(source.ReadAdmitted(Allow)==fixture.original,"Denied Windows substitution damaged source authority");fixture.Unchanged();
#else
        Require(!error,"Cannot rename generated source substitution fixture");
        if(parent)Require(std::filesystem::create_directory(fixture.parent),"Cannot create replacement source parent");
        Write(fixture.path,fixture.original);std::filesystem::last_write_time(fixture.path,fixture.time);
        Refuse([&]{static_cast<void>(source.ReadAdmitted(Allow));},"Same-content/stamp source or parent substitution accepted");
        Require(RawRead(parent?moved/"DefUser.ini":moved)==fixture.original,
            "Original pinned source changed during parent substitution");
#endif
    }
}
void ContentAndReadRaces() {
    {
        Fixture fixture;auto source=ReadOnlySource::BindAdmitted(fixture.path,Allow);
        Bytes changed(fixture.original.size(),0x3fu);const auto wrote=TryWrite(fixture.path,changed);
#ifdef _WIN32
        Require(!wrote,"Windows read-only pin allowed source writes");fixture.Unchanged();
        Require(source.ReadAdmitted(Allow)==fixture.original,"Denied in-place source write changed cached authority");
#else
        Require(wrote,"Cannot mutate generated POSIX source control");std::filesystem::last_write_time(fixture.path,fixture.time);
        Refuse([&]{static_cast<void>(source.ReadAdmitted(Allow));},"In-place source mutation with restored mtime accepted");
#endif
    }
    for(const bool grow:{false,true}) {
        Fixture fixture;auto source=ReadOnlySource::BindAdmitted(fixture.path,Allow);std::size_t callbacks{};bool wrote{};
        const auto changed=grow?Bytes(fixture.original.size()+5u,0x5au):Text("short");
        const auto read=[&]{static_cast<void>(source.ReadAdmitted([&](std::size_t){if(++callbacks==2u)wrote=TryWrite(fixture.path,changed);}));};
#ifdef _WIN32
        read();Require(!wrote&&callbacks==2u,"Windows admitted read allowed mutation or skipped actual-size callback");fixture.Unchanged();
#else
        Refuse(read,"Source growth/truncation during admitted held read accepted");
        Require(wrote&&callbacks==2u,"Held read did not reach the admitted mutation boundary");
#endif
    }
}
}
int main() {
    try {
        BasicAndBinding();AdmissionAndBounds();InvalidAndMissingPaths();LinkRefusals();ReadOnlyPermissions();SourceAndParentSubstitution();ContentAndReadRaces();
        std::cout<<"PASS "<<checks<<" checks / "<<refusals<<" refusals: separately pinned read-only original source, exact cached bytes, "
            "actual-size admission, no backend artifacts, rebind/reentrancy/security/identity/parent/content/race checks.\n";return 0;
    }catch(const std::exception& error){denyAllocations=false;std::cerr<<error.what()<<'\n';return 1;}
}
