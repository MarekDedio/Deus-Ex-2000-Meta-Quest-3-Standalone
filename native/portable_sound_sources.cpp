#include "Precomp.h"
#include "portable_sound_sources.h"
#include "quest_native_config.h"
#include "quest_native_config_import.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string_view>

namespace {
namespace fs=std::filesystem;
[[noreturn]] void Fail(const char* reason){throw std::runtime_error(std::string("Sound source admission: ")+reason);}
std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
struct Budget {
    const PortableSoundSourceLimits& limits;QuestVr::NativeWork::Budget work;
    std::size_t retained{},scratch{},serialized{},directories{};
    explicit Budget(const PortableSoundSourceLimits& value):limits(value),work(value.nativeWork){
        if(!value.retainedBytes||value.retainedBytes>(64u<<20u)||!value.serializedBytes||value.serializedBytes>(32u<<20u)||
            value.packages>128u||value.bindings>4096u||value.directoryEntries>65536u)Fail("unsupported bounded policy");
    }
    std::size_t Add(std::size_t a,std::size_t b)const{if(b>std::numeric_limits<std::size_t>::max()-a)Fail("byte arithmetic overflow");return a+b;}
    std::size_t Mul(std::size_t a,std::size_t b)const{if(a&&b>std::numeric_limits<std::size_t>::max()/a)Fail("byte arithmetic overflow");return a*b;}
    void Peak(std::size_t bytes)const{
        if(retained>limits.retainedBytes||scratch>limits.retainedBytes-retained||bytes>limits.retainedBytes-retained-scratch)
            Fail("retained working-byte limit");
    }
    void Retain(std::size_t bytes){Peak(bytes);retained+=bytes;}
    void Text(std::string_view value){if(value.size()>65536u)Fail("identity text limit");work.Step(value.size()+1u);}
    struct Scope {
        Budget& owner;std::size_t before;
        explicit Scope(Budget& value):owner(value),before(value.scratch){}
        Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
        void Hold(std::size_t bytes){owner.Peak(bytes);owner.scratch+=bytes;}
        ~Scope(){owner.scratch=before;}
    };
};
void Segment(std::string_view value){
    if(value.empty()||value.size()>65536u||value.find_first_of("./\\:")!=std::string_view::npos||
        value.find('\0')!=std::string_view::npos||Fold(std::string(value))=="none")Fail("invalid Package/object identity segment");
}
const std::string& Name(const PortablePackageTables& source,std::int32_t index,Budget& budget){
    if(index<0||static_cast<std::size_t>(index)>=source.names.size())Fail("name index outside authored source");
    const auto& value=source.names[static_cast<std::size_t>(index)].Name.ToString();budget.Text(value);return value;
}
const ImportTableEntry& Import(const PortablePackageTables& source,std::int32_t reference){
    if(reference>=0||static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference))>source.imports.size())
        Fail("invalid authored import reference");
    return source.imports[static_cast<std::size_t>(-static_cast<std::int64_t>(reference)-1)];
}
const ExportTableEntry& Export(const PortablePackageTables& source,std::size_t index){
    if(index>=source.exports.size()||index>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        Fail("invalid authored export reference");
    return source.exports[index];
}
std::string Identity(const PortablePackageTables& source,std::int32_t reference,Budget& budget,
    const std::function<bool(const std::string&,const std::string&)>& constraint={}){
    Budget::Scope scratch(budget);scratch.Hold(budget.Add(4u*65536u,budget.Mul(source.sourcePath.size()+1u,128u)));
    auto value=ResolvePortableValueObjectReference(source,reference,constraint);budget.Text(value);return value;
}
std::string RootPackage(const PortablePackageTables& source,std::int32_t reference,Budget& budget){
    std::array<std::int32_t,32u> seen{};std::size_t count{};bool first=true;
    while(reference){
        budget.work.Step();
        if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,reference)!=seen.begin()+count)
            Fail("cyclic/deep import outer chain");
        seen[count++]=reference;const auto& entry=Import(source,reference);
        const auto& segment=Name(source,entry.ObjName,budget);Segment(segment);
        if(!first||!entry.ObjOuter)
            if(Fold(Name(source,entry.ClassName,budget))!="package"||Fold(Name(source,entry.ClassPackage,budget))!="core")
                Fail("authored import outer is not Core.Package");
        if(!entry.ObjOuter)return segment;
        first=false;reference=entry.ObjOuter;
    }
    Fail("authored import has no root Package");
}
void ExactNativeClass(const PortablePackageTables& source,std::int32_t reference,const char* expected,Budget& budget){
    // The supported serialized resource profile names native asset classes by
    // their genuine Core.Class import, not a same-named instance or surrogate.
    const auto& cls=Import(source,reference);
    if(Fold(Name(source,cls.ClassPackage,budget))!="core"||Fold(Name(source,cls.ClassName,budget))!="class")
        Fail("resource Class import has no Core.Class provenance");
    static_cast<void>(RootPackage(source,reference,budget));
    if(Fold(Identity(source,reference,budget,[](const std::string&,const std::string& type){return Fold(type)=="class";}))!=Fold(expected))
        Fail("selected source export has a different actual native Class");
}
void Context(const ExportTableEntry& entry,QuestVr::OriginalLinkerLoadProfile profile){
    if(QuestVr::OriginalLinkerCreateExportDecision(profile,static_cast<std::uint32_t>(entry.ObjFlags),false)!=
        QuestVr::OriginalLinkerCreateExportAction::Construct)Fail("required Sound/outer export is excluded by actual source context");
}
void OuterPackages(const PortablePackageTables& source,std::size_t index,QuestVr::OriginalLinkerLoadProfile profile,Budget& budget){
    std::array<std::int32_t,32u> seen{};std::size_t count{};auto reference=Export(source,index).ObjOuter;
    while(reference){
        budget.work.Step();
        if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,reference)!=seen.begin()+count)
            Fail("cyclic/deep resource outer chain");
        seen[count++]=reference;
        if(reference>0){
            const auto& entry=Export(source,static_cast<std::size_t>(reference-1));Segment(Name(source,entry.ObjName,budget));
            ExactNativeClass(source,entry.ObjClass,"Core.Package",budget);Context(entry,profile);reference=entry.ObjOuter;
        }else{
            const auto& entry=Import(source,reference);Segment(Name(source,entry.ObjName,budget));
            if(Fold(Name(source,entry.ClassPackage,budget))!="core"||Fold(Name(source,entry.ClassName,budget))!="package")
                Fail("resource import outer is not Core.Package");
            reference=entry.ObjOuter;
        }
    }
}
struct Stamp {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;
    explicit Stamp(fs::path value):path(std::move(value)){
        std::error_code error;if(!fs::is_regular_file(path,error)||error)Fail("source is not an accessible regular file");
        size=fs::file_size(path);modified=fs::last_write_time(path);
    }
    void Check()const{if(fs::file_size(path)!=size||fs::last_write_time(path)!=modified)Fail("source changed during admission");}
};
fs::path Resolve(const std::vector<std::string>& paths,const fs::path& system,const std::string& package,Budget& budget){
    for(const auto& raw:paths){
        budget.Text(raw);Budget::Scope scratch(budget);scratch.Hold(budget.Add(65536u,budget.Mul(raw.size()+1u,128u)));
        std::u16string imported;
        if(!QuestVr::NativeConfigImport::String(QuestVr::UnrealString::Decode(raw),0u,imported).returned)
            Fail("Core.System.Paths String import failed");
        auto text=QuestVr::UnrealString::Encode(imported);std::replace(text.begin(),text.end(),'\\','/');if(text.empty())continue;
        const fs::path mask(text);const auto extension=mask.extension();
        if(extension.empty()||mask.filename().string()!="*"+extension.string()||mask.parent_path().string().find_first_of("*?")!=std::string::npos)
            Fail("unsupported Core.System.Paths filename mask");
        const auto directory=(system/mask).lexically_normal().parent_path();
        std::error_code error;const auto status=fs::status(directory,error);
        if(error==std::errc::no_such_file_or_directory||(!error&&status.type()==fs::file_type::not_found))continue;
        if(error||!fs::is_directory(status))Fail("configured source directory is inaccessible");
        std::optional<fs::path> selected;
        for(const auto& entry:fs::directory_iterator(directory)){
            budget.work.Step();if(budget.directories==budget.limits.directoryEntries)Fail("directory entry scan limit");++budget.directories;
            const auto filename=entry.path().filename().string();budget.Text(filename);budget.Peak(budget.Mul(filename.size()+1u,128u));
            if(entry.is_regular_file()&&Fold(entry.path().extension().string())==Fold(extension.string())&&Fold(entry.path().stem().string())==Fold(package)){
                if(selected&&*selected!=entry.path())Fail("ambiguous case-folded source Package in configured directory");
                selected=entry.path();
            }
        }
        if(selected)return fs::canonical(*selected);
    }
    Fail("authored Sound package is absent from ordered Core.System.Paths");
}
struct Scan {
    std::ifstream input;std::size_t size{},position{};Budget& budget;
    Scan(const fs::path& path,std::size_t bytes,Budget& value):input(path,std::ios::binary),size(bytes),budget(value){if(!input)Fail("cannot open source preflight");}
    void Seek(std::size_t value){if(value>size)Fail("preflight seek outside source");input.seekg(static_cast<std::streamoff>(value));if(!input)Fail("preflight seek failed");position=value;}
    std::uint8_t Byte(){char value{};if(position==size||!input.get(value))Fail("truncated source preflight");++position;budget.work.Step();return static_cast<std::uint8_t>(value);}
    std::uint32_t Word(){std::uint32_t value{};for(unsigned i=0u;i<4u;++i)value|=std::uint32_t(Byte())<<(8u*i);return value;}
    std::int32_t Index(){
        const auto first=Byte();std::uint32_t value=first&63u;bool more=(first&64u)!=0u;unsigned shift=6u;
        for(unsigned slot=1u;more;++slot){
            if(slot>4u)Fail("overlong compact source index");
            const auto next=Byte();
            if(slot==4u&&(next&0xe0u))Fail("compact source index overflow");
            value|=std::uint32_t(next&(slot==4u?31u:127u))<<shift;more=(next&128u)!=0u;shift+=7u;
        }
        if(first&128u){if(value>0x80000000u)Fail("negative compact source index overflow");return value==0x80000000u?std::numeric_limits<std::int32_t>::min():-static_cast<std::int32_t>(value);}
        if(value>0x7fffffffu)Fail("positive compact source index overflow");
        return static_cast<std::int32_t>(value);
    }
};
PortablePackageTables Load(const fs::path& path,Budget& budget){
    if(path.native().size()>65536u)Fail("source filesystem path limit");
    Budget::Scope preflight(budget);preflight.Hold(budget.Add(1u<<20u,budget.Mul(path.native().size()+1u,128u)));
    const Stamp stamp(path);
    if(stamp.size<36u||stamp.size>budget.limits.serializedBytes||budget.serialized>budget.limits.serializedBytes-stamp.size)
        Fail("serialized source byte limit");
    Scan scan(path,static_cast<std::size_t>(stamp.size),budget);
    if(scan.Word()!=0x9e2a83c1u||scan.Word()!=68u)Fail("sound source requires exact v68/licensee0 header");
    static_cast<void>(scan.Word());const auto names=scan.Word(),nameOffset=scan.Word(),exports=scan.Word(),exportOffset=scan.Word(),imports=scan.Word(),importOffset=scan.Word();
    if(names>2'000'000u||exports>2'000'000u||imports>2'000'000u||names>stamp.size/5u||exports>stamp.size/12u||imports>stamp.size/7u||
        nameOffset>stamp.size||exportOffset>stamp.size||importOffset>stamp.size)Fail("impossible source table counts/offsets");
    scan.Seek(nameOffset);std::size_t encodedNames{};
    for(std::size_t i=0u;i<names;++i){
        const auto length=scan.Index();const auto units=length<0?-static_cast<std::int64_t>(length):length;
        if(units<0||units>65536)Fail("source name unit limit");
        const auto bytes=budget.Mul(static_cast<std::size_t>(units),length<0?2u:1u);encodedNames=budget.Add(encodedNames,bytes);
        if(bytes>scan.size-scan.position)Fail("truncated serialized source name");
        std::uint8_t previous{},last{};for(std::size_t j=0u;j<bytes;++j){previous=last;last=scan.Byte();}
        if(bytes&&(last||(length<0&&previous)))Fail("unterminated serialized source name");
        static_cast<void>(scan.Word());
    }
    const auto tables=budget.Add(budget.Mul(names,sizeof(NameTableEntry)+96u),budget.Add(budget.Mul(exports,sizeof(ExportTableEntry)),budget.Mul(imports,sizeof(ImportTableEntry))));
    // Account for actual metadata/name bytes before table-loader allocation,
    // not the much larger opaque audio payload. Process NameString arena
    // growth has its pre-existing process lifetime, not detached rollback.
    budget.Retain(budget.Add(1u<<20u,budget.Add(tables,budget.Mul(encodedNames,64u))));
    budget.work.Step(budget.Add(names,budget.Add(exports,imports)));
    auto result=LoadPortablePackageTables(path.string());stamp.Check();budget.serialized+=static_cast<std::size_t>(stamp.size);return result;
}
using ExportIndex=std::map<std::string,std::size_t>;
ExportIndex MakeIndex(const PortablePackageTables& source,Budget& budget){
    ExportIndex result;
    for(std::size_t index=0u;index<source.exports.size();++index){
        budget.work.Step();static_cast<void>(Export(source,index));
        auto identity=Identity(source,static_cast<std::int32_t>(index+1u),budget);budget.Retain(budget.Add(128u,budget.Mul(identity.size()+1u,2u)));
        if(!result.emplace(Fold(std::move(identity)),index).second)Fail("duplicate qualified source export identity");
    }
    return result;
}
std::size_t Match(const PortablePackageTables& source,const ExportIndex& exports,const std::string& path,
    QuestVr::OriginalLinkerLoadProfile profile,Budget& budget){
    budget.work.Step();const auto found=exports.find(Fold(path));if(found==exports.end())Fail("authored Sound import has no matching actual export");
    const auto& entry=Export(source,found->second);Segment(Name(source,entry.ObjName,budget));
    ExactNativeClass(source,entry.ObjClass,"Engine.Sound",budget);Context(entry,profile);OuterPackages(source,found->second,profile,budget);
    const Stamp stamp(source.sourcePath);
    if(entry.ObjSize<=0||entry.ObjOffset<0||static_cast<std::uint64_t>(entry.ObjOffset)+static_cast<std::uint64_t>(entry.ObjSize)>stamp.size)
        Fail("Sound payload extent is outside its actual source");
    return found->second; // Metadata/provenance admission, not PCM decode.
}
} // namespace

PortableSoundSourceBundle LoadPortableRuntimeSoundSources(const std::vector<PortablePackageTables>& baseSources,
    const PortablePackageTables& map,const std::string& engineIniPath,QuestVr::OriginalLinkerLoadProfile profile,
    const PortableSoundSourceLimits& limits){
    Budget budget(limits);PortableSoundSourceBundle result;budget.Retain(sizeof(result)+(1u<<20u));
    if(map.version!=68u||map.licenseeMode)Fail("map requires exact v68/licensee0 profile");
    budget.Text(engineIniPath);budget.Text(map.sourcePath);
    const Stamp ini(engineIniPath);budget.Peak(20u<<20u);
    const auto document=QuestVr::NativeConfig::ReadOrderedIni(engineIniPath);if(!document)Fail("missing explicit engine INI source");
    budget.Retain(20u<<20u);const auto paths=document->StringArray("Core.System","Paths");if(!paths)Fail("missing actual Core.System.Paths");
    std::map<std::string,const PortablePackageTables*> available;
    std::map<const PortablePackageTables*,ExportIndex> indexes;
    std::vector<Stamp> stamps;stamps.reserve(baseSources.size()+limits.packages+1u);
    budget.Retain(budget.Add(budget.Mul(baseSources.size()+limits.packages+1u,sizeof(Stamp)),
        budget.Add(budget.Mul(limits.packages,sizeof(PortablePackageTables)),budget.Mul(limits.bindings,sizeof(PortableSoundSourceBinding)))));
    result.packages.reserve(limits.packages);result.bindings.reserve(limits.bindings);
    const auto add=[&](const PortablePackageTables& source){
        budget.Text(source.sourcePath);auto key=Fold(fs::path(source.sourcePath).stem().string());Segment(key);
        budget.Retain(budget.Add(256u,budget.Mul(source.sourcePath.size()+key.size()+2u,4u)));
        if(!available.emplace(key,&source).second)Fail("duplicate/conflicting baseline/source Package identity");
        stamps.emplace_back(source.sourcePath);
    };
    for(const auto& source:baseSources)add(source);
    add(map);
    const auto discover=[&](const PortablePackageTables& importer){
        for(std::size_t index=0u;index<importer.imports.size();++index){
            budget.work.Step();const auto& entry=importer.imports[index];
            if(Fold(Name(importer,entry.ClassName,budget))!="sound")continue;
            if(index>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())||result.bindings.size()==limits.bindings)
                Fail("Sound import binding/reference limit");
            const auto reference=-static_cast<std::int32_t>(index+1u);const auto package=RootPackage(importer,reference,budget);
            // Match according to actual value-import ClassName and then the
            // selected export's genuine Engine.Sound Class. The original value
            // resolver does not gate ClassPackage spelling independently.
            const auto path=Identity(importer,reference,budget,[](const std::string&,const std::string& type){return Fold(type)=="sound";});
            const auto found=available.find(Fold(package));const PortablePackageTables* source{};
            if(found!=available.end())source=found->second;
            else{
                if(result.packages.size()==limits.packages)Fail("additional Sound source Package limit");
                const auto resolved=Resolve(*paths,fs::path(engineIniPath).parent_path(),package,budget);
                result.packages.push_back(Load(resolved,budget));source=&result.packages.back();add(*source);
            }
            auto cache=indexes.find(source);
            if(cache==indexes.end()){
                budget.Retain(sizeof(ExportIndex)+128u);cache=indexes.emplace(source,MakeIndex(*source,budget)).first;
            }
            const auto selected=Match(*source,cache->second,path,profile,budget);
            const auto actual=Identity(importer,reference,budget,[&](const std::string& value,const std::string& type){
                return Fold(type)=="sound"&&Fold(value)==Fold(Identity(*source,static_cast<std::int32_t>(selected+1u),budget));
            });
            budget.Retain(budget.Mul(importer.sourcePath.size()+source->sourcePath.size()+actual.size()+3u,2u));
            result.bindings.push_back({importer.sourcePath,reference,source->sourcePath,actual,selected,static_cast<std::uint32_t>(source->exports[selected].ObjFlags)});
        }
    };
    for(const auto& source:baseSources)discover(source);
    discover(map);
    for(std::size_t index=0u;index<result.packages.size();++index)discover(result.packages[index]);
    ini.Check();for(const auto& stamp:stamps)stamp.Check();
    result.nativeWork=budget.work.Read();result.retainedBytes=budget.retained;result.serializedBytes=budget.serialized;return result;
}
