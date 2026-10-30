#include "Precomp.h"
#include "portable_music_sources.h"
#include "quest_native_config.h"
#include "quest_native_config_import.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include <set>

namespace {
namespace fs=std::filesystem;
[[noreturn]] void Fail(const char* why){throw std::runtime_error(std::string("Music source admission: ")+why);}
std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
struct Budget {
    const PortableMusicSourceLimits& limits;QuestVr::NativeWork::Budget work;
    std::size_t retained{},serialized{},directories{};
    explicit Budget(const PortableMusicSourceLimits& value):limits(value),work(value.nativeWork){
        if(!value.retainedBytes||value.retainedBytes>(256u<<20u)||!value.serializedBytes||value.serializedBytes>(64u<<20u)||
            value.packages>128u||value.bindings>4096u||value.directoryEntries>65536u)Fail("unsupported bounded policy");
    }
    std::size_t Add(std::size_t a,std::size_t b)const{if(b>std::numeric_limits<std::size_t>::max()-a)Fail("byte arithmetic overflow");return a+b;}
    std::size_t Mul(std::size_t a,std::size_t b)const{if(a&&b>std::numeric_limits<std::size_t>::max()/a)Fail("byte arithmetic overflow");return a*b;}
    void Peak(std::size_t extra)const{if(extra>limits.retainedBytes||retained>limits.retainedBytes-extra)Fail("retained working-byte limit");}
    void Retain(std::size_t bytes){Peak(bytes);retained+=bytes;}
    void Text(std::string_view text){work.Step(text.size()+1u);if(text.size()>65536u)Fail("identity text limit");}
};
const std::string& Name(const PortablePackageTables& source,std::int32_t index,Budget& budget){
    if(index<0||static_cast<std::size_t>(index)>=source.names.size())Fail("name index outside authored table");
    const auto& value=source.names[static_cast<std::size_t>(index)].Name.ToString();budget.Text(value);return value;
}
const ImportTableEntry& Import(const PortablePackageTables& source,std::int32_t ref){
    if(ref>=0||static_cast<std::uint64_t>(-static_cast<std::int64_t>(ref))>source.imports.size())Fail("invalid import chain reference");
    return source.imports[static_cast<std::size_t>(-static_cast<std::int64_t>(ref)-1)];
}
std::string RootPackage(const PortablePackageTables& source,std::int32_t ref,Budget& budget){
    std::array<std::int32_t,32u> seen{};std::size_t count{};
    while(ref){
        budget.work.Step();if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,ref)!=seen.begin()+count)Fail("cyclic/deep import outer chain");
        seen[count++]=ref;const auto& entry=Import(source,ref);const auto& segment=Name(source,entry.ObjName,budget);
        if(segment.empty()||segment.find_first_of("./\\:\0",0u,5u)!=std::string::npos||Fold(segment)=="none")Fail("invalid package/object import segment");
        if(!entry.ObjOuter){
            if(Fold(Name(source,entry.ClassName,budget))!="package"||Fold(Name(source,entry.ClassPackage,budget))!="core")
                Fail("outermost import is not an actual Core.Package");
            return segment;
        }
        ref=entry.ObjOuter;
    }
    Fail("missing root package import");
}
struct Stamp {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;
    explicit Stamp(fs::path value):path(std::move(value)){
        std::error_code error;if(!fs::is_regular_file(path,error)||error)Fail("source is not an accessible regular file");
        size=fs::file_size(path);modified=fs::last_write_time(path);
    }
    void Check()const{if(fs::file_size(path)!=size||fs::last_write_time(path)!=modified)Fail("source changed during admission");}
};
fs::path Resolve(const std::vector<std::string>& paths,const fs::path& system,std::string_view package,Budget& budget){
    for(const auto& raw:paths){
        budget.Text(raw);budget.Peak(budget.Mul(raw.size()+1u,12u));
        std::u16string imported;const auto units=QuestVr::UnrealString::Decode(raw);
        if(!QuestVr::NativeConfigImport::String(units,0u,imported).returned)Fail("configured Paths String import failed");
        auto text=QuestVr::UnrealString::Encode(imported);std::replace(text.begin(),text.end(),'\\','/');
        if(text.empty())continue;
        const fs::path mask(text);const auto directory=(system/mask).lexically_normal().parent_path();const auto extension=mask.extension();
        std::error_code error;const auto status=fs::status(directory,error);
        if(error==std::errc::no_such_file_or_directory||(!error&&status.type()==fs::file_type::not_found))continue;
        if(error||!fs::is_directory(status))Fail("configured package directory is inaccessible");
        std::optional<fs::path> selected;
        for(const auto& entry:fs::directory_iterator(directory)){
            budget.work.Step();if(budget.directories==budget.limits.directoryEntries)Fail("directory scan entry limit");++budget.directories;
            const auto filename=entry.path().filename().string();budget.Text(filename);budget.Peak(budget.Mul(filename.size()+1u,8u));
            if(entry.is_regular_file()&&entry.path().extension()==extension&&Fold(entry.path().stem().string())==Fold(std::string(package))){
                if(selected&&*selected!=entry.path())Fail("ambiguous case-folded package identity in one directory");
                selected=entry.path();
            }
        }
        if(selected)return fs::canonical(*selected);
    }
    Fail("authored Music package is absent from configured Paths");
}
std::uint32_t Word(const std::array<unsigned char,36u>& header,std::size_t offset){
    std::uint32_t value{};for(unsigned i=0u;i<4u;++i)value|=std::uint32_t(header[offset+i])<<(8u*i);return value;
}
PortablePackageTables Load(const fs::path& path,Budget& budget){
    const Stamp stamp(path);
    if(stamp.size<36u||stamp.size>budget.limits.serializedBytes||budget.serialized>budget.limits.serializedBytes-stamp.size)
        Fail("serialized source byte limit");
    std::array<unsigned char,36u> header{};std::ifstream input(path,std::ios::binary);
    if(!input.read(reinterpret_cast<char*>(header.data()),header.size()))Fail("truncated package header");
    const auto version=Word(header,4u)&0xffffu;
    if(Word(header,0u)!=0x9e2a83c1u||version<60u||version>=100u)Fail("unsupported source package header");
    const auto names=Word(header,12u),exports=Word(header,20u),imports=Word(header,28u);
    for(const auto offset:{16u,24u,32u})if(Word(header,offset)>stamp.size)Fail("table offset outside source");
    if(names>stamp.size/5u||exports>stamp.size/12u||imports>stamp.size/7u)Fail("impossible table counts before allocation");
    // Conservative BEFORE-loader admission: vectors + decoded/interned names,
    // buffered I/O and the one possibly truncated max65536-unit name scratch.
    // Name transport/UTF16 decoding is bounded by serialized bytes plus that
    // last-name scratch, not an unchecked count times maximum-name allocation.
    const auto bytes=budget.Add(1u<<20u,budget.Add(budget.Mul(static_cast<std::size_t>(stamp.size),16u),
        budget.Add(budget.Mul(names,sizeof(NameTableEntry)+64u),budget.Add(budget.Mul(exports,sizeof(ExportTableEntry)),budget.Mul(imports,sizeof(ImportTableEntry))))));
    budget.Retain(bytes);budget.work.Step(budget.Add(names,budget.Add(exports,imports)));
    auto result=LoadPortablePackageTables(path.string());stamp.Check();budget.serialized+=static_cast<std::size_t>(stamp.size);return result;
}
std::size_t Match(const PortablePackageTables& source,const std::string& path,QuestVr::OriginalLinkerLoadProfile profile,Budget& budget){
    std::optional<std::size_t> selected;std::set<std::string> identities;
    for(std::size_t i=0u;i<source.exports.size();++i){
        budget.work.Step();budget.Peak(128u<<10u);
        const auto identity=ResolvePortableValueObjectReference(source,static_cast<std::int32_t>(i+1u));budget.Text(identity);
        budget.Retain(budget.Add(64u,budget.Mul(identity.size()+1u,2u)));
        if(!identities.insert(Fold(identity)).second)Fail("duplicate qualified asset export identity");
        if(Fold(identity)!=Fold(path))continue;
        if(selected)Fail("ambiguous authored Music export");
        selected=i;
    }
    if(!selected)Fail("authored Music import has no actual matching export");
    const auto& entry=source.exports[*selected];const auto& cls=Import(source,entry.ObjClass);
    const auto& parent=Import(source,cls.ObjOuter);
    if(Fold(Name(source,cls.ClassPackage,budget))!="core"||Fold(Name(source,cls.ClassName,budget))!="class"||
        Fold(Name(source,cls.ObjName,budget))!="music"||parent.ObjOuter||Fold(Name(source,parent.ObjName,budget))!="engine"||
        Fold(Name(source,parent.ClassPackage,budget))!="core"||Fold(Name(source,parent.ClassName,budget))!="package")
        Fail("selected export lacks its actual Engine.Music native class import");
    if(QuestVr::OriginalLinkerCreateExportDecision(profile,static_cast<std::uint32_t>(entry.ObjFlags),false)!=
        QuestVr::OriginalLinkerCreateExportAction::Construct)Fail("required Music export is excluded by the actual source load context");
    if(entry.ObjSize<=0||static_cast<std::size_t>(entry.ObjSize)>budget.limits.serializedBytes)Fail("invalid Music payload bound");
    budget.Peak(budget.Add(65536u,budget.Mul(static_cast<std::size_t>(entry.ObjSize),3u)));
    const Stamp stamp(source.sourcePath);const auto decoded=LoadPortableMusic(source,*selected,budget.limits.serializedBytes);
    if(Fold(decoded.objectPath)!=Fold(path))Fail("Music payload/export identity mismatch");
    stamp.Check();return *selected;
}
}

PortableMusicSourceBundle LoadPortableRuntimeMusicSources(const std::vector<PortablePackageTables>& baseSources,
    const PortablePackageTables& map,const std::string& engineIniPath,QuestVr::OriginalLinkerLoadProfile profile,
    const PortableMusicSourceLimits& limits){
    Budget budget(limits);PortableMusicSourceBundle result;
    budget.Retain(sizeof(result));const Stamp ini(engineIniPath);
    budget.Peak(20u<<20u);const auto document=QuestVr::NativeConfig::ReadOrderedIni(engineIniPath);
    if(!document)Fail("missing explicit engine INI source");
    const auto paths=document->StringArray("Core.System","Paths");if(!paths)Fail("missing actual Core.System Paths section");
    budget.Retain(20u<<20u); // Conservative INI document + ordered Paths/import scratch lifetime.
    std::map<std::string,const PortablePackageTables*> available;
    const auto source=[&](const PortablePackageTables& table){
        budget.Text(table.sourcePath);const auto key=Fold(fs::path(table.sourcePath).stem().string());
        if(key.empty()||key.find('.')!=std::string::npos||key.find('\0')!=std::string::npos)Fail("invalid source Package identity");
        budget.Retain(key.size()+sizeof(void*)+96u);
        if(!available.emplace(key,&table).second)Fail("duplicate/conflicting baseline source Package identity");
    };
    for(const auto& table:baseSources)source(table);
    if(available.count(Fold(fs::path(map.sourcePath).stem().string())))Fail("map source aliases baseline Package identity");
    // Reserve the explicit finite packet before pointers into additional tables
    // are published in the local dictionary; no reallocation/stale table view.
    budget.Retain(budget.Add(budget.Mul(limits.packages,sizeof(PortablePackageTables)),budget.Mul(limits.bindings,sizeof(PortableMusicSourceBinding))));
    result.packages.reserve(limits.packages);result.bindings.reserve(limits.bindings);
    const auto discover=[&](const PortablePackageTables& importer){
        for(std::size_t i=0u;i<importer.imports.size();++i){
            budget.work.Step();const auto& entry=importer.imports[i];
            if(Fold(Name(importer,entry.ClassName,budget))!="music")continue;
            if(result.bindings.size()==limits.bindings)Fail("Music import binding count limit");
            const auto ref=-static_cast<std::int32_t>(i+1u);const auto package=RootPackage(importer,ref,budget);
            budget.Peak(128u<<10u);const auto path=ResolvePortableValueObjectReference(importer,ref,
                [](const std::string&,const std::string& importedClass){return Fold(importedClass)=="music";});
            const auto found=available.find(Fold(package));const PortablePackageTables* table{};
            if(found!=available.end())table=found->second;
            else{
                if(result.packages.size()==limits.packages)Fail("additional Music source package limit");
                const auto resolved=Resolve(*paths,fs::path(engineIniPath).parent_path(),package,budget);
                result.packages.push_back(Load(resolved,budget));table=&result.packages.back();source(*table);
            }
            const auto index=Match(*table,path,profile,budget);
            // Second, fully authenticated value resolution: class matching now
            // uses the actual selected export/type, not a display path alone.
            const auto actual=ResolvePortableValueObjectReference(importer,ref,[&](const std::string& value,const std::string& type){
                return Fold(type)=="music"&&Fold(value)==Fold(ResolvePortableValueObjectReference(*table,static_cast<std::int32_t>(index+1u)));
            });
            budget.Retain(budget.Mul(importer.sourcePath.size()+table->sourcePath.size()+actual.size()+3u,2u));
            result.bindings.push_back({importer.sourcePath,ref,table->sourcePath,actual,index,static_cast<std::uint32_t>(table->exports[index].ObjFlags)});
        }
    };
    for(const auto& table:baseSources)discover(table);
    discover(map);
    for(std::size_t i=0u;i<result.packages.size();++i)discover(result.packages[i]);
    ini.Check();result.nativeWork=budget.work.Read();result.retainedBytes=budget.retained;result.serializedBytes=budget.serialized;return result;
}
