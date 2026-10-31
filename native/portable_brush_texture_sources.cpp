#include "Precomp.h"
#include "portable_brush_texture_sources.h"
#include "portable_brush_polys.h"
#include "portable_model_geometry.h"
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
#include <type_traits>

namespace {
namespace fs=std::filesystem;
[[noreturn]] void Fail(const char* why){throw std::runtime_error(std::string("Brush texture source admission: ")+why);}
std::string Fold(std::string value){
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
struct Budget {
    const PortableBrushTextureSourceLimits& limits;
    QuestVr::NativeWork::Budget work;
    std::size_t retained{},scratch{},serialized{},directories{};
    explicit Budget(const PortableBrushTextureSourceLimits& v):limits(v),work(v.nativeWork){
        if(!v.retainedBytes||v.retainedBytes>(64u<<20u)||!v.serializedBytes||v.serializedBytes>(32u<<20u)||
            v.packages>128u||v.bindings>4096u||v.directoryEntries>65536u)Fail("unsupported bounded policy");
    }
    std::size_t Add(std::size_t a,std::size_t b)const{
        if(b>std::numeric_limits<std::size_t>::max()-a)Fail("byte arithmetic overflow");
        return a+b;
    }
    std::size_t Mul(std::size_t a,std::size_t b)const{
        if(a&&b>std::numeric_limits<std::size_t>::max()/a)Fail("byte arithmetic overflow");
        return a*b;
    }
    std::size_t Remaining()const{
        if(retained>limits.retainedBytes||scratch>limits.retainedBytes-retained)Fail("retained working-byte limit");
        return limits.retainedBytes-retained-scratch;
    }
    void Peak(std::size_t bytes)const{if(bytes>Remaining())Fail("retained working-byte limit");}
    void Retain(std::size_t bytes){Peak(bytes);retained+=bytes;}
    void Text(std::string_view value){
        if(value.size()>65536u)Fail("identity text bound");
        work.Step(value.size()+1u);
    }
    struct Scope {
        Budget& owner;std::size_t before;
        explicit Scope(Budget& b):owner(b),before(b.scratch){}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
        void Hold(std::size_t bytes){owner.Peak(bytes);owner.scratch+=bytes;}
        ~Scope(){owner.scratch=before;}
    };
};
const std::string& Name(const PortablePackageTables& p,std::int32_t i,Budget& b){
    if(i<0||static_cast<std::size_t>(i)>=p.names.size())Fail("source name index outside table");
    const auto& value=p.names[static_cast<std::size_t>(i)].Name.ToString();b.Text(value);return value;
}
void Segment(std::string_view value){
    const auto none=value.size()==4u&&(value[0]=='N'||value[0]=='n')&&
        (value[1]=='O'||value[1]=='o')&&(value[2]=='N'||value[2]=='n')&&(value[3]=='E'||value[3]=='e');
    if(value.empty()||value.size()>65536u||none||
        value.find_first_of("./\\:")!=std::string_view::npos||value.find('\0')!=std::string_view::npos)
        Fail("invalid package/object identity segment");
}
const ImportTableEntry& Import(const PortablePackageTables& p,std::int32_t ref){
    if(ref>=0||static_cast<std::uint64_t>(-static_cast<std::int64_t>(ref))>p.imports.size())Fail("invalid source import reference");
    return p.imports[static_cast<std::size_t>(-static_cast<std::int64_t>(ref)-1)];
}
const ExportTableEntry& Export(const PortablePackageTables& p,std::size_t i){
    if(i>=p.exports.size()||i>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))Fail("source export index outside table");
    return p.exports[i];
}
std::string Stem(const PortablePackageTables& p,Budget& b){
    b.Text(p.sourcePath);b.Peak(b.Mul(p.sourcePath.size()+1u,128u));
    auto value=fs::path(p.sourcePath).stem().string();b.Text(value);Segment(value);return value;
}
std::string Identity(const PortablePackageTables& p,std::int32_t ref,Budget& b,
    const std::function<bool(const std::string&,const std::string&)>& constraint={}){
    b.Text(p.sourcePath);
    b.Peak(b.Add(4u*65536u,b.Mul(p.sourcePath.size()+1u,128u)));
    auto value=ResolvePortableValueObjectReference(p,ref,constraint);b.Text(value);return value;
}
std::string RootPackage(const PortablePackageTables& p,std::int32_t ref,Budget& b){
    std::array<std::int32_t,32u> seen{};std::size_t count{};bool first=true;
    while(ref){
        b.work.Step();if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,ref)!=seen.begin()+count)Fail("cyclic/deep import outer chain");
        seen[count++]=ref;const auto& e=Import(p,ref);const auto& name=Name(p,e.ObjName,b);Segment(name);
        // The leaf may be a Texture/Class. Every actual outer, including the
        // root, must itself be imported as Core.Package, not a lookalike path.
        if(!first||!e.ObjOuter){
            if(Fold(Name(p,e.ClassPackage,b))!="core"||Fold(Name(p,e.ClassName,b))!="package")Fail("import root is not Core.Package");
        }
        if(!e.ObjOuter)return name;
        first=false;
        ref=e.ObjOuter;
    }
    Fail("source import has no root Package");
}
std::string ClassIdentity(const PortablePackageTables& p,std::int32_t ref,Budget& b){
    if(ref>0){if(Export(p,static_cast<std::size_t>(ref-1)).ObjClass)Fail("class reference names an instance export");}
    else {
        const auto& e=Import(p,ref);
        if(Fold(Name(p,e.ClassPackage,b))!="core"||Fold(Name(p,e.ClassName,b))!="class")Fail("class import lacks Core.Class provenance");
        static_cast<void>(RootPackage(p,ref,b));
    }
    return Identity(p,ref,b,[](const std::string&,const std::string& type){return Fold(type)=="class";});
}
void ExactNativeClass(const PortablePackageTables& p,std::int32_t ref,const char* expected,Budget& b){
    if(Fold(ClassIdentity(p,ref,b))!=Fold(expected))Fail("asset source has a different actual native Class");
    if(ref<0){
        const auto& cls=Import(p,ref);const auto& root=Import(p,cls.ObjOuter);
        const auto expectedPath=std::string(expected);const auto dot=expectedPath.find('.');
        if(dot==std::string::npos||root.ObjOuter||Fold(Name(p,root.ClassPackage,b))!="core"||
            Fold(Name(p,root.ClassName,b))!="package"||Fold(Name(p,root.ObjName,b))!=Fold(expectedPath.substr(0u,dot)))
            Fail("native Class has an invalid actual package outer");
    }
}
void Context(const ExportTableEntry& e,QuestVr::OriginalLinkerLoadProfile profile){
    if(QuestVr::OriginalLinkerCreateExportDecision(profile,static_cast<std::uint32_t>(e.ObjFlags),false)!=
        QuestVr::OriginalLinkerCreateExportAction::Construct)Fail("required asset excluded by original load context");
}
void OuterPackages(const PortablePackageTables& p,std::size_t index,QuestVr::OriginalLinkerLoadProfile profile,Budget& b){
    std::array<std::int32_t,32u> seen{};std::size_t count{};auto ref=Export(p,index).ObjOuter;
    while(ref){
        b.work.Step();if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,ref)!=seen.begin()+count)Fail("cyclic/deep asset outer chain");
        seen[count++]=ref;
        if(ref>0){const auto& e=Export(p,static_cast<std::size_t>(ref-1));Segment(Name(p,e.ObjName,b));
            ExactNativeClass(p,e.ObjClass,"Core.Package",b);Context(e,profile);ref=e.ObjOuter;}
        else {const auto& e=Import(p,ref);Segment(Name(p,e.ObjName,b));
            if(Fold(Name(p,e.ClassPackage,b))!="core"||Fold(Name(p,e.ClassName,b))!="package")Fail("asset import outer is not Core.Package");
            ref=e.ObjOuter;}
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
fs::path Resolve(const std::vector<std::string>& paths,const fs::path& system,const std::string& package,Budget& b){
    for(const auto& raw:paths){
        b.Text(raw);b.Peak(b.Add(65536u,b.Mul(raw.size()+1u,128u)));
        const auto units=QuestVr::UnrealString::Decode(raw);std::u16string imported;
        if(!QuestVr::NativeConfigImport::String(units,0u,imported).returned)Fail("Core.System.Paths CString import failed");
        auto text=QuestVr::UnrealString::Encode(imported);std::replace(text.begin(),text.end(),'\\','/');if(text.empty())continue;
        const fs::path mask(text);const auto filenameMask=mask.filename().string();const auto extension=mask.extension();
        // This bounded source-admission profile authenticates the actual
        // shipped '*.extension' Paths form. Do not silently ignore a literal
        // filename, prefixed wildcard or directory wildcard and broaden it.
        if(extension.empty()||filenameMask!="*"+extension.string()||
            mask.parent_path().string().find_first_of("*?")!=std::string::npos)
            Fail("unsupported Core.System.Paths filename mask");
        const auto directory=(system/mask).lexically_normal().parent_path();
        std::error_code error;const auto status=fs::status(directory,error);
        if(error==std::errc::no_such_file_or_directory||(!error&&status.type()==fs::file_type::not_found))continue;
        if(error||!fs::is_directory(status))Fail("configured package directory inaccessible");
        std::optional<fs::path> selected;
        for(const auto& entry:fs::directory_iterator(directory)){
            b.work.Step();if(b.directories==b.limits.directoryEntries)Fail("directory scan entry bound");
            ++b.directories;
            const auto filename=entry.path().filename().string();b.Text(filename);b.Peak(b.Mul(filename.size()+1u,128u));
            if(entry.is_regular_file()&&Fold(entry.path().extension().string())==Fold(extension.string())&&Fold(entry.path().stem().string())==Fold(package)){
                if(selected&&*selected!=entry.path())Fail("ambiguous case-folded source Package");
                selected=entry.path();
            }
        }
        if(selected)return fs::canonical(*selected);
    }
    Fail("required Texture package absent from ordered Core.System.Paths");
}
struct Scan {
    std::ifstream input;std::size_t size{},position{};Budget& budget;
    Scan(const fs::path& p,std::size_t n,Budget& b):input(p,std::ios::binary),size(n),budget(b){if(!input)Fail("cannot open source preflight");}
    void Seek(std::size_t at){
        if(at>size)Fail("preflight seek outside source");
        input.seekg(static_cast<std::streamoff>(at));
        if(!input)Fail("preflight seek failed");
        position=at;
    }
    std::uint8_t Byte(){
        if(position==size)Fail("truncated source preflight");
        char c{};
        if(!input.get(c))Fail("truncated source preflight");
        ++position;budget.work.Step();return static_cast<std::uint8_t>(c);
    }
    std::uint32_t Word(){std::uint32_t value{};for(unsigned i=0;i<4u;++i)value|=std::uint32_t(Byte())<<(8u*i);return value;}
    std::int32_t Index(){const auto first=Byte();std::uint32_t value=first&63u;bool more=(first&64u)!=0u;unsigned shift=6u;
        for(unsigned slot=1u;more;++slot){
            if(slot>4u)Fail("overlong compact source index");
            const auto next=Byte();
            if(slot==4u&&(next&0xe0u))Fail("source compact index overflow");
            value|=std::uint32_t(next&(slot==4u?31u:127u))<<shift;more=(next&128u)!=0u;shift+=7u;}
        if(first&128u){
            if(value>0x80000000u)Fail("source compact negative overflow");
            return value==0x80000000u?std::numeric_limits<std::int32_t>::min():-static_cast<std::int32_t>(value);}
        if(value>0x7fffffffu)Fail("source compact positive overflow");
        return static_cast<std::int32_t>(value);}
};
PortablePackageTables Load(const fs::path& path,Budget& b){
    // Fixed stream/scan/Stamp/path handles must be admitted before opening the
    // constant-storage preflight, not after it has already consumed buffers.
    if(path.native().size()>65536u)Fail("source filesystem path bound");
    Budget::Scope preflight(b);
    preflight.Hold(b.Add(1u<<20u,b.Mul(path.native().size()+1u,128u)));
    const Stamp stamp(path);
    if(stamp.size<36u||stamp.size>b.limits.serializedBytes||b.serialized>b.limits.serializedBytes-stamp.size)Fail("serialized source byte bound");
    Scan scan(path,static_cast<std::size_t>(stamp.size),b);
    if(scan.Word()!=0x9e2a83c1u||scan.Word()!=68u)Fail("source requires exact v68/licensee0 header");
    static_cast<void>(scan.Word());const auto names=scan.Word(),nameOffset=scan.Word(),exports=scan.Word(),exportOffset=scan.Word(),imports=scan.Word(),importOffset=scan.Word();
    if(names>2'000'000u||exports>2'000'000u||imports>2'000'000u||names>stamp.size/5u||exports>stamp.size/12u||imports>stamp.size/7u||
        nameOffset>stamp.size||exportOffset>stamp.size||importOffset>stamp.size)Fail("impossible source table counts/offsets");
    // Constant-storage scan of actual serialized names, not whole mip bytes.
    // The normal loader performs Unicode/surrogate/table reference validation.
    scan.Seek(nameOffset);std::size_t encodedNames{};
    for(std::size_t i=0;i<names;++i){const auto length=scan.Index();const auto units=length<0?-static_cast<std::int64_t>(length):length;
        if(units<0||units>65536)Fail("source name unit bound");
        const auto bytes=b.Mul(static_cast<std::size_t>(units),length<0?2u:1u);
        encodedNames=b.Add(encodedNames,bytes);
        if(bytes>scan.size-scan.position)Fail("truncated source name");
        std::uint8_t previous{},last{};for(std::size_t j=0;j<bytes;++j){previous=last;last=scan.Byte();}
        if(bytes&&(last||(length<0&&previous)))Fail("unterminated serialized source name");
        static_cast<void>(scan.Word());}
    const auto tableBytes=b.Add(b.Mul(names,sizeof(NameTableEntry)+96u),b.Add(b.Mul(exports,sizeof(ExportTableEntry)),b.Mul(imports,sizeof(ImportTableEntry))));
    // Covers detached vector capacities, incremental decoded/name payload and
    // buffered reads BEFORE loader allocation. Shared NameString interning is
    // process-owned: reallocating an already-live global arena is outside this
    // detached ledger (and is not rollback-safe). This is NOT a process-memory
    // cap or a replacement arena/transaction. Serialized mip payload is not a
    // retained table allocation and is charged separately, without decoding.
    const auto admitted=b.Add(1u<<20u,b.Add(tableBytes,b.Mul(encodedNames,64u)));
    b.Retain(admitted);b.work.Step(b.Add(names,b.Add(exports,imports)));
    auto result=LoadPortablePackageTables(path.string());stamp.Check();b.serialized+=static_cast<std::size_t>(stamp.size);return result;
}
std::size_t ModelBytes(const PortableModelGeometry& g,Budget& b){
    auto bytes=b.Add(sizeof(g),g.objectPath.capacity()+1u);const auto array=[&](const auto& v){bytes=b.Add(bytes,b.Mul(v.capacity(),sizeof(typename std::decay_t<decltype(v)>::value_type)));};
    array(g.vectors);array(g.points);array(g.nodes);array(g.surfaces);array(g.vertices);array(g.zones);array(g.lightMaps);
    array(g.lightBits);array(g.renderBounds);array(g.leafHulls);array(g.leaves);array(g.lights);return bytes;
}
struct ClassSlot {const PortablePackageTables* source{};std::size_t index{};};
using Classes=std::map<std::string,ClassSlot>;
void AddClasses(const PortablePackageTables& source,Classes& classes,Budget& b){
    for(std::size_t i=0;i<source.exports.size();++i){b.work.Step();if(source.exports[i].ObjClass)continue;
        if(i>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))Fail("Class export reference bound");
        const auto path=Identity(source,static_cast<std::int32_t>(i+1u),b);b.Retain(b.Add(128u,path.size()+1u));
        if(!classes.emplace(Fold(path),ClassSlot{&source,i}).second)Fail("duplicate baseline/map Class identity");}
}
bool Mover(const PortablePackageTables& importer,std::int32_t ref,const Classes& classes,Budget& b){
    // The current qualified ancestry string survives the next source resolver.
    Budget::Scope identityScope(b);identityScope.Hold(2u*65536u);
    auto path=ClassIdentity(importer,ref,b);std::array<const ExportTableEntry*,128u> seen{};std::size_t depth{};bool mover{};
    while(!path.empty()){
        b.work.Step();const auto found=classes.find(Fold(path));if(found==classes.end())Fail("actor ancestry requires an actual baseline Class export");
        const auto& slot=found->second;const auto& e=Export(*slot.source,slot.index);
        if(depth==seen.size()||std::find(seen.begin(),seen.begin()+depth,&e)!=seen.begin()+depth)Fail("cyclic/deep actor Class ancestry");
        seen[depth++]=&e;
        if(Fold(path)=="engine.mover")mover=true;
        if(e.ObjBase)path=ClassIdentity(*slot.source,e.ObjBase,b);
        else {
            if(Fold(path)!="core.object")Fail("actor Class ancestry has an unsupported zero base");
            path.clear();}
    }
    return mover;
}
void LevelSource(const PortablePackageTables& map,QuestVr::OriginalLinkerLoadProfile profile,Budget& b){
    std::size_t count{};
    for(const auto& e:map.exports){
        b.work.Step();if(!e.ObjClass)continue;
        const auto nameIndex=e.ObjClass<0?Import(map,e.ObjClass).ObjName:
            Export(map,static_cast<std::size_t>(e.ObjClass-1)).ObjName;
        if(Fold(Name(map,nameIndex,b))!="level")continue;
        ExactNativeClass(map,e.ObjClass,"Engine.Level",b);Context(e,profile);
        if(++count!=1u)Fail("ambiguous actual Level source");
        if(e.ObjSize<=0)Fail("actual Level has no source payload");
        b.work.Step(static_cast<std::size_t>(e.ObjSize));
    }
    if(!count)Fail("map has no actual Engine.Level source");
}
std::size_t MatchTexture(const PortablePackageTables& source,const std::string& path,QuestVr::OriginalLinkerLoadProfile profile,Budget& b){
    b.Text(source.sourcePath);Budget::Scope sourceScope(b);
    sourceScope.Hold(b.Add(2u*65536u,b.Mul(source.sourcePath.size()+1u,128u)));
    std::optional<std::size_t> match;
    for(std::size_t i=0;i<source.exports.size();++i){b.work.Step();
        if(i>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))Fail("Texture export reference bound");
        const auto identity=Identity(source,static_cast<std::int32_t>(i+1u),b);
        if(Fold(identity)!=Fold(path))continue;
        if(match)Fail("ambiguous required Texture export");
        match=i;}
    if(!match)Fail("required Texture import has no actual matching export");
    const auto& e=Export(source,*match);Segment(Name(source,e.ObjName,b));
    if(e.ObjClass>=0)Fail("required Texture lacks its actual Engine native Class import");
    ExactNativeClass(source,e.ObjClass,"Engine.Texture",b);Context(e,profile);OuterPackages(source,*match,profile,b);
    if(e.ObjOffset<0||e.ObjSize<=0)Fail("required Texture has no original source payload");
    const Stamp stamp(source.sourcePath);
    if(static_cast<std::uint64_t>(e.ObjOffset)+static_cast<std::uint64_t>(e.ObjSize)>stamp.size)Fail("Texture payload outside source");
    return *match; // Identity admission ONLY: no image/mip decode.
}
} // namespace

PortableBrushTextureSourceBundle LoadPortableRuntimeBrushTextureSources(const std::vector<PortablePackageTables>& baseSources,
    const PortablePackageTables& map,const std::string& engineIniPath,QuestVr::OriginalLinkerLoadProfile profile,
    const PortableBrushTextureSourceLimits& limits){
    Budget b(limits);PortableBrushTextureSourceBundle result;
    // Fixed owner structs, call stacks and small live identity copies remain
    // charged while helper-specific peak reservations are checked underneath.
    b.Retain(b.Add(sizeof(result),1u<<20u));
    if(map.version!=68u||map.licenseeMode)Fail("map requires exact v68/licensee0 profile");
    b.Text(engineIniPath);b.Text(map.sourcePath);
    b.Retain(b.Mul(b.Add(engineIniPath.size()+1u,map.sourcePath.size()+1u),128u));
    const Stamp ini(engineIniPath),mapStamp(map.sourcePath);
    b.Peak(20u<<20u);const auto document=QuestVr::NativeConfig::ReadOrderedIni(engineIniPath);if(!document)Fail("missing explicit engine INI");
    b.Retain(20u<<20u);const auto paths=document->StringArray("Core.System","Paths");if(!paths)Fail("missing Core.System.Paths");
    std::map<std::string,const PortablePackageTables*> available;Classes classes;
    const auto addSource=[&](const PortablePackageTables& source){const auto key=Fold(Stem(source,b));b.Retain(b.Add(128u,key.size()+1u));
        if(!available.emplace(key,&source).second)Fail("duplicate/conflicting source Package identity");};
    for(const auto& source:baseSources){addSource(source);AddClasses(source,classes,b);}addSource(map);AddClasses(map,classes,b);
    b.Retain(b.Add(b.Mul(limits.packages,sizeof(PortablePackageTables)+sizeof(Stamp)),b.Mul(limits.bindings,sizeof(PortableBrushTextureSourceBinding))));
    result.packages.reserve(limits.packages);result.bindings.reserve(limits.bindings);
    std::vector<Stamp> admittedStamps;admittedStamps.reserve(limits.packages);
    LevelSource(map,profile,b);
    const auto order=ReadPortableLevel68ActorOrder(map,b.Remaining());Budget::Scope orderScope(b);orderScope.Hold(b.Mul(order.capacity(),sizeof(std::int32_t)));
    std::set<std::int32_t> textures; // Map-relative ref dedup; first Model/Polys provenance is retained.
    for(const auto actorRef:order){
        b.work.Step();if(!actorRef)continue;
        if(actorRef<0)Fail("authored Level Actor is not a local export");
        const auto& actor=Export(map,static_cast<std::size_t>(actorRef-1));if(!Mover(map,actor.ObjClass,classes,b))continue;
        if(QuestVr::OriginalLinkerCreateExportDecision(profile,static_cast<std::uint32_t>(actor.ObjFlags),false)==QuestVr::OriginalLinkerCreateExportAction::Exclude)continue;
        std::optional<std::int32_t> brush;
        if(actor.ObjSize<=0)Fail("authored Mover has no source payload");
        b.work.Step(static_cast<std::size_t>(actor.ObjSize));
        {const auto properties=LoadPortableExportProperties(map,static_cast<std::size_t>(actorRef-1),b.Remaining());
            for(const auto& tag:properties.properties){b.work.Step();const auto& name=tag.name.ToString();b.Text(name);
                if(Fold(name)!="brush")continue;
                if(brush||tag.arrayIndex||tag.type!=5u)Fail("Mover serialized Brush hint is duplicate or wrong schema");
                brush=DecodePortableObjectReference(tag);}}
        // Absent authored hint is not a fabricated None runtime provider. This
        // prefetch deliberately does not run scripts or infer inherited writes.
        if(!brush||!*brush)continue;
        if(*brush<0)Fail("Mover serialized Brush hint is not a local Model");
        const auto modelIndex=static_cast<std::size_t>(*brush-1);const auto& modelEntry=Export(map,modelIndex);
        ExactNativeClass(map,modelEntry.ObjClass,"Engine.Model",b);Context(modelEntry,profile);
        Budget::Scope modelScope(b);
        if(modelEntry.ObjSize<=0)Fail("Mover source Model has no payload");
        b.work.Step(static_cast<std::size_t>(modelEntry.ObjSize));
        const auto raw=b.Add(65536u,b.Mul(static_cast<std::size_t>(modelEntry.ObjSize),2u));b.Peak(raw);
        const auto decoded=std::min<std::size_t>(16u<<20u,b.Remaining()-raw);
        auto model=LoadPortableCollisionModel68(map,modelIndex,b.Remaining(),decoded);modelScope.Hold(ModelBytes(model,b));
        if(model.polysReference<=0)Fail("Mover Model does not reference local authored Polys");
        const auto polysIndex=static_cast<std::size_t>(model.polysReference-1);const auto& polysEntry=Export(map,polysIndex);
        ExactNativeClass(map,polysEntry.ObjClass,"Engine.Polys",b);Context(polysEntry,profile);
        if(polysEntry.ObjSize<=0)Fail("Mover source Polys has no payload");
        b.work.Step(static_cast<std::size_t>(polysEntry.ObjSize));
        const auto polys=LoadPortableBrushPolys68(map,polysIndex,std::min<std::size_t>(32u<<20u,b.Remaining()));
        modelScope.Hold(b.Add(sizeof(polys),b.Add(b.Mul(polys.polygons.capacity(),sizeof(PortableBrushPoly)),
            polys.sourcePath.capacity()+polys.objectPath.capacity()+2u)));
        for(const auto& poly:polys.polygons){
            b.work.Step();const auto ref=poly.textureReference;if(!ref||textures.count(ref))continue;
            if(result.bindings.size()==limits.bindings)Fail("Texture binding count bound");
            b.Retain(64u);textures.insert(ref);
            Budget::Scope bindingScope(b);bindingScope.Hold(4u*65536u);
            const PortablePackageTables* target=&map;std::string path;
            if(ref<0){const auto& imported=Import(map,ref);
                if(Fold(Name(map,imported.ClassName,b))!="texture"||Fold(Name(map,imported.ClassPackage,b))!="engine")Fail("polygon Texture import lacks actual Engine.Texture provenance");
                const auto package=RootPackage(map,ref,b);path=Identity(map,ref,b,[](const std::string&,const std::string& type){return Fold(type)=="texture";});
                const auto found=available.find(Fold(package));
                if(found!=available.end())target=found->second;
                else {if(result.packages.size()==limits.packages)Fail("additional Texture package count bound");
                    const auto resolved=Resolve(*paths,fs::path(engineIniPath).parent_path(),package,b);
                    if(resolved.native().size()>65536u)Fail("resolved source filesystem path bound");
                    b.Retain(b.Mul(resolved.native().size()+1u,128u));
                    admittedStamps.emplace_back(resolved);
                    result.packages.push_back(Load(resolved,b));target=&result.packages.back();
                    if(Fold(Stem(*target,b))!=Fold(package))Fail("resolved file disagrees with actual Package name");
                    addSource(*target);}
            }else path=Identity(map,ref,b);
            const auto selected=MatchTexture(*target,path,profile,b);
            b.Retain(b.Mul(map.sourcePath.size()+target->sourcePath.size()+path.size()+3u,2u));
            result.bindings.push_back({map.sourcePath,modelIndex,polysIndex,ref,target->sourcePath,path,selected,
                static_cast<std::uint32_t>(target->exports[selected].ObjFlags)});
        }
    }
    for(const auto& stamp:admittedStamps){b.work.Step();stamp.Check();}
    ini.Check();mapStamp.Check();b.work.Check();result.nativeWork=b.work.Read();result.retainedBytes=b.retained;
    result.serializedBytes=b.serialized;return result;
}
