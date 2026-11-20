#include "Precomp.h"
#include "GC/GC.h"
#include "portable_sound_sources.h"
#include "portable_object_lookup.h"
#include "quest_runtime_script_packages.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string_view>

namespace {
namespace fs=std::filesystem;using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{},ownedBindings{};
constexpr QuestVr::OriginalLinkerLoadProfile profile{false,true,true};
void Require(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
template<class F>void Refuse(F&& operation,const char* why){bool failed{};try{operation();}catch(const std::exception&){failed=true;}
    Require(failed,why);++refusals;}
std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;}
void Word(Bytes& bytes,std::size_t offset,std::uint32_t value){for(unsigned i=0u;i<4u;++i)bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8u*i));}
void U32(Bytes& bytes,std::uint32_t value){const auto at=bytes.size();bytes.resize(at+4u);Word(bytes,at,value);}
void Index(Bytes& bytes,std::int32_t value){
    const bool negative=value<0;auto magnitude=static_cast<std::uint32_t>(negative?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(negative?128u:0u));magnitude>>=6u;
    if(magnitude)first|=64u;
    bytes.push_back(first);
    while(magnitude){auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude)next|=128u;bytes.push_back(next);}
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(fs::path p):path(std::move(p)),size(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash()){}
    std::uint64_t Hash()const{std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint Sound source read-only");
        std::array<char,65536u> block{};std::uint64_t result=14695981039346656037ull;
        while(input){input.read(block.data(),block.size());for(std::streamsize i=0;i<input.gcount();++i){result^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);result*=1099511628211ull;}}
        Require(input.eof()&&!input.bad(),"Cannot finish Sound source fingerprint");return result;}
    void Unchanged()const{Require(size==fs::file_size(path)&&modified==fs::last_write_time(path)&&hash==Hash(),"Sound/map/script/INI source changed");}
};
struct Files {
    fs::path parent=fs::canonical(fs::temp_directory_path()),directory;
    Files(){directory=parent/("quest-sound-sources-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(fs::create_directory(directory),"Cannot create isolated Sound fixture directory");directory=fs::canonical(directory);}
    ~Files(){std::error_code error;const auto actual=fs::weakly_canonical(directory,error);
        if(!error&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-sound-sources-",0u)==0u)fs::remove_all(actual,error);}
    fs::path Write(const fs::path& name,const Bytes& bytes)const{const auto path=directory/name;fs::create_directories(path.parent_path());
        std::ofstream output(path,std::ios::binary);Require(bool(output),"Cannot create isolated Sound fixture");
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));output.close();
        Require(bool(output),"Cannot finish isolated Sound fixture");return path;}
    fs::path Text(const fs::path& name,const std::string& bytes)const{return Write(name,Bytes(bytes.begin(),bytes.end()));}
};
std::int32_t Name(PortablePackageTables& source,const char* value){
    const auto found=std::find_if(source.names.begin(),source.names.end(),[&](const auto& entry){return entry.Name==value;});
    if(found!=source.names.end())return static_cast<std::int32_t>(found-source.names.begin());
    source.names.push_back({NameString(value),0u});return static_cast<std::int32_t>(source.names.size()-1u);
}
std::int32_t Import(PortablePackageTables& source,const char* classPackage,const char* type,std::int32_t outer,const char* object){
    source.imports.push_back({Name(source,classPackage),Name(source,type),outer,Name(source,object)});
    return -static_cast<std::int32_t>(source.imports.size());
}
Bytes Package(const PortablePackageTables& source){
    // Actual v68 table encoding with opaque generated four-byte payload. This
    // tests metadata admission, not successful Sound format/PCM preparation.
    Bytes bytes(36u);Word(bytes,0u,0x9e2a83c1u);Word(bytes,4u,source.version|(std::uint32_t(source.licenseeMode)<<16u));Word(bytes,8u,source.flags);
    Word(bytes,12u,static_cast<std::uint32_t>(source.names.size()));Word(bytes,16u,static_cast<std::uint32_t>(bytes.size()));
    for(const auto& entry:source.names){const auto& name=entry.Name.ToString();Index(bytes,static_cast<std::int32_t>(name.size()+1u));
        bytes.insert(bytes.end(),name.begin(),name.end());bytes.push_back(0u);U32(bytes,entry.Flags);}
    Word(bytes,20u,static_cast<std::uint32_t>(source.exports.size()));Word(bytes,24u,static_cast<std::uint32_t>(bytes.size()));
    for(const auto& entry:source.exports){Index(bytes,entry.ObjClass);Index(bytes,entry.ObjBase);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));
        Index(bytes,entry.ObjName);U32(bytes,static_cast<std::uint32_t>(entry.ObjFlags));Index(bytes,entry.ObjSize);if(entry.ObjSize>0)Index(bytes,entry.ObjOffset);}
    Word(bytes,28u,static_cast<std::uint32_t>(source.imports.size()));Word(bytes,32u,static_cast<std::uint32_t>(bytes.size()));
    for(const auto& entry:source.imports){Index(bytes,entry.ClassPackage);Index(bytes,entry.ClassName);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));Index(bytes,entry.ObjName);}
    Require(bytes.size()<1024u,"Generated Sound table encoding exceeded fixture payload boundary");
    bytes.resize(1028u);bytes[1024u]=0x51u;bytes[1025u]=0x75u;bytes[1026u]=0x65u;bytes[1027u]=0x73u;return bytes;
}
PortablePackageTables SoundFixture(){
    PortablePackageTables source;source.version=68u;Name(source,"None");
    const auto engine=Import(source,"Core","Package",0,"Engine");const auto sound=Import(source,"Core","Class",engine,"Sound");
    const auto core=Import(source,"Core","Package",0,"Core");const auto package=Import(source,"Core","Class",core,"Package");
    source.exports.push_back({package,0,0,Name(source,"Group"),static_cast<ObjectFlags>(0x00030004u),0,-1});
    source.exports.push_back({sound,0,1,Name(source,"Ping"),static_cast<ObjectFlags>(0x00030004u),4,1024});return source;
}
PortablePackageTables MapFixture(){
    PortablePackageTables map;map.version=68u;Name(map,"None");
    const auto root=Import(map,"Core","Package",0,"TestSounds");const auto group=Import(map,"Core","Package",root,"Group");
    Import(map,"Engine","Sound",group,"Ping");
    const auto unrelated=Import(map,"Core","Package",0,"UnreferencedMissing");Import(map,"Engine","Texture",unrelated,"NoTextureSource");return map;
}
void NoPublication(const GCStats& before){const auto after=GC::GetStats();
    Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage&&!ReadPortableRuntimeObjectLookup().prepared,
        "Detached Sound source admission published UObjects/lookup or a hidden native root");}
void Fixtures(){
    Files files;auto fixture=SoundFixture();const auto soundPath=files.Write("Sounds/TestSounds.uax",Package(fixture));
    const auto mapPath=files.Write("Maps/00_Training.dx",Package(MapFixture()));const auto map=LoadPortablePackageTables(mapPath.string());
    const auto ini=files.Text("System/DeusEx.ini","[Core.System]\nPaths=../Absent/*.uax\nPaths=../Sounds/*.uax\nPaths=../LastAbsent/*.uax\n");
    const auto gc=GC::GetStats();const Source originalSound(soundPath),originalMap(mapPath),originalIni(ini);
    const auto bundle=LoadPortableRuntimeSoundSources({},map,ini.string(),profile);
    Require(bundle.packages.size()==1u&&bundle.bindings.size()==1u,"Generated Sound admission lost actual imported source or selected unrelated Texture");
    const auto& b=bundle.bindings.front();Require(b.importerSourcePath==map.sourcePath&&b.importReference==-3&&
        fs::equivalent(b.sourcePath,soundPath)&&b.objectPath=="TestSounds.Group.Ping"&&b.exportIndex==1u&&b.objectFlags==0x00030004u,
        "Generated Sound import/export/outer/source provenance differs");
    Require(bundle.serializedBytes==1028u&&bundle.nativeWork.used&&bundle.retainedBytes<=PortableSoundSourceLimits{}.retainedBytes,
        "Generated Sound byte/work accounting differs");NoPublication(gc);
    const auto baseline=LoadPortableRuntimeSoundSources(bundle.packages,map,ini.string(),profile);
    Require(baseline.packages.empty()&&baseline.bindings.size()==1u&&baseline.serializedBytes==0u&&baseline.bindings[0].objectPath==b.objectPath,
        "Existing actual baseline Sound source was reloaded or lost");
    auto noSound=map;noSound.imports[2u].ClassName=Name(noSound,"Texture");
    const auto unrelated=LoadPortableRuntimeSoundSources({},noSound,ini.string(),profile);
    Require(unrelated.packages.empty()&&unrelated.bindings.empty()&&unrelated.serializedBytes==0u,"Unreferenced source enumeration fabricated a Sound dependency");
    auto ignoredClassPackage=map;ignoredClassPackage.imports[2u].ClassPackage=Name(ignoredClassPackage,"Other");
    Require(LoadPortableRuntimeSoundSources({},ignoredClassPackage,ini.string(),profile).bindings.size()==1u,
        "Sound import invented a ClassPackage spelling gate absent from original value resolution");
    Refuse([&]{LoadPortableRuntimeSoundSources({},map,ini.string(),{});},"Fresh Sound export ignored all-false original context mask");
    auto bad=map;bad.imports[2u].ObjOuter=-3;
    Refuse([&]{LoadPortableRuntimeSoundSources({},bad,ini.string(),profile);},"Cyclic Sound import outer accepted");
    bad=map;bad.imports[2u].ObjOuter=1;
    Refuse([&]{LoadPortableRuntimeSoundSources({},bad,ini.string(),profile);},"Sound import used export outer");
    bad=map;bad.imports[2u].ClassName=static_cast<std::int32_t>(bad.names.size());
    Refuse([&]{LoadPortableRuntimeSoundSources({},bad,ini.string(),profile);},"Invalid authored Sound ClassName index accepted");
    bad=map;bad.imports[1u].ClassName=Name(bad,"Class");
    Refuse([&]{LoadPortableRuntimeSoundSources({},bad,ini.string(),profile);},"Sound import grouping outer had no Core.Package provenance");
    bad=map;bad.names[static_cast<std::size_t>(bad.imports[0u].ObjName)].Name="../TestSounds";
    Refuse([&]{LoadPortableRuntimeSoundSources({},bad,ini.string(),profile);},"Unsafe imported Sound package segment accepted");
    bad=map;bad.version=67u;
    Refuse([&]{LoadPortableRuntimeSoundSources({},bad,ini.string(),profile);},"Non-v68 map accepted by exact source profile");
    auto duplicate=bundle.packages;duplicate.push_back(bundle.packages.front());
    Refuse([&]{LoadPortableRuntimeSoundSources(duplicate,map,ini.string(),profile);},"Duplicate baseline Package alias accepted");
    const auto baselineRefusal=[&](const auto& mutate,const char* why){auto sources=bundle.packages;mutate(sources.front());
        Refuse([&]{LoadPortableRuntimeSoundSources(sources,map,ini.string(),profile);},why);};
    baselineRefusal([](auto& source){source.exports[1u].ObjClass=0;},"Same-named export without actual Engine.Sound Class accepted");
    baselineRefusal([](auto& source){source.exports[1u].ObjFlags=static_cast<ObjectFlags>(0x00740004u);},"Editor-only Sound export ignored actual source context");
    baselineRefusal([](auto& source){source.exports[0u].ObjFlags=ObjectFlags::Public;},"Excluded actual grouping Package context accepted");
    baselineRefusal([](auto& source){source.exports[0u].ObjClass=-2;},"Resource outer had wrong actual native Class");
    baselineRefusal([](auto& source){source.exports[1u].ObjSize=0;},"Sound without actual serialized payload accepted");
    baselineRefusal([](auto& source){source.exports[1u].ObjOffset=1027;},"Sound payload crossed actual source boundary");
    baselineRefusal([](auto& source){source.exports.push_back(source.exports[1u]);},"Duplicate qualified Sound export identity accepted");
    const auto missing=files.Text("Missing.ini","[Core.System]\nPaths=Absent/*.uax\n");
    Refuse([&]{LoadPortableRuntimeSoundSources({},map,missing.string(),profile);},"Missing actual Sound source fabricated None");
    const auto unsupported=files.Text("Unsupported.ini","[Core.System]\nPaths=Sounds/Test*.uax\n");
    Refuse([&]{LoadPortableRuntimeSoundSources({},map,unsupported.string(),profile);},"Unsupported Paths mask guessed a source");
    Refuse([&]{LoadPortableRuntimeSoundSources({},map,(files.directory/"NoIni.ini").string(),profile);},"Absent INI became a default search policy");
    for(unsigned control=0u;control<7u;++control){PortableSoundSourceLimits limits;
        if(control==0u)limits.nativeWork=0u;
        if(control==1u)limits.retainedBytes=1u;
        if(control==2u)limits.serializedBytes=1027u;
        if(control==3u)limits.packages=0u;
        if(control==4u)limits.bindings=0u;
        if(control==5u)limits.directoryEntries=0u;
        if(control==6u)limits.nativeWork=QuestVr::NativeWork::MaximumLimit+1u;
        Refuse([&]{LoadPortableRuntimeSoundSources({},map,ini.string(),profile,limits);},"Sound admission ignored explicit finite limit");}
    // Earlier configured package wins. A correctly named invalid package must
    // refuse, not silently fall through to another directory's valid twin.
    auto corrupt=Package(fixture);Word(corrupt,12u,2'000'000u);files.Write("First/TestSounds.uax",corrupt);
    const auto ordered=files.Text("Ordered.ini","[Core.System]\nPaths=First/*.uax\nPaths=Sounds/*.uax\n");
    Refuse([&]{LoadPortableRuntimeSoundSources({},map,ordered.string(),profile);},"Malformed first ordered Package fell through or allocated huge table reserve");
    corrupt=Package(fixture);corrupt[4u]=67u;files.Write("First/TestSounds.uax",corrupt);
    Refuse([&]{LoadPortableRuntimeSoundSources({},map,ordered.string(),profile);},"Non-v68 Sound package accepted");
    NoPublication(gc);originalSound.Unchanged();originalMap.Unchanged();originalIni.Unchanged();
}
void Owned(const fs::path& root){
    std::vector<Source> originals;std::vector<PortablePackageTables> scripts;
    for(const auto* package:QuestVr::RuntimeScriptPackages){originals.emplace_back(root/"System"/(std::string(package)+".u"));scripts.push_back(LoadPortablePackageTables(originals.back().path.string()));}
    originals.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(originals.back().path.string());
    originals.emplace_back(root/"System"/"DeusEx.ini");const auto ini=originals.back().path.string();
    originals.emplace_back(root/"Sounds"/"Ambient.uax");originals.emplace_back(root/"Sounds"/"MoverSFX.uax");
    const auto gc=GC::GetStats();const auto bundle=LoadPortableRuntimeSoundSources(scripts,map,ini,profile);
    Require(bundle.packages.size()==2u&&bundle.serializedBytes==13'736'291u&&!bundle.bindings.empty(),
        "First Training Sound admission differs from its actual two .uax source bytes");
    std::map<std::string,const PortablePackageTables*> sources;for(const auto& source:scripts)sources.emplace(source.sourcePath,&source);
    sources.emplace(map.sourcePath,&map);std::set<std::string> additional;
    for(const auto& source:bundle.packages){sources.emplace(source.sourcePath,&source);additional.insert(Fold(fs::path(source.sourcePath).stem().string()));}
    Require(additional==std::set<std::string>{"ambient","moversfx"},"First Training admitted unrelated Sound source packages");
    std::map<std::string,std::size_t> importedCounts;
    for(const auto& [path,source]:sources)for(const auto& entry:source->imports)
        if(entry.ClassName>=0&&static_cast<std::size_t>(entry.ClassName)<source->names.size()&&source->names[static_cast<std::size_t>(entry.ClassName)].Name=="Sound")++importedCounts[path];
    std::set<std::pair<std::string,std::int32_t>> visited;bool baselineSound{},mapSound{};
    for(const auto& binding:bundle.bindings){const auto importer=sources.find(binding.importerSourcePath),selected=sources.find(binding.sourcePath);
        Require(importer!=sources.end()&&selected!=sources.end()&&binding.importReference<0&&binding.exportIndex<selected->second->exports.size(),
            "Owned Sound binding lost actual importer/source/export provenance");
        Require(visited.emplace(binding.importerSourcePath,binding.importReference).second,"Owned Sound binding duplicated an authored import");
        const auto& imported=importer->second->imports.at(static_cast<std::size_t>(-static_cast<std::int64_t>(binding.importReference)-1));
        const auto& entry=selected->second->exports[binding.exportIndex];
        const auto importedPath=ResolvePortableValueObjectReference(*importer->second,binding.importReference,[](const auto&,const auto& type){return Fold(type)=="sound";});
        const auto actualPath=ResolvePortableValueObjectReference(*selected->second,static_cast<std::int32_t>(binding.exportIndex+1u));
        const auto actualClass=ResolvePortableValueObjectReference(*selected->second,entry.ObjClass,[](const auto&,const auto& type){return Fold(type)=="class";});
        Require(importer->second->names.at(static_cast<std::size_t>(imported.ClassName)).Name=="Sound"&&importedPath==binding.objectPath&&
            Fold(importedPath)==Fold(actualPath)&&actualClass=="Engine.Sound"&&binding.objectFlags==static_cast<std::uint32_t>(entry.ObjFlags)&&
            QuestVr::OriginalLinkerCreateExportDecision(profile,binding.objectFlags,false)==QuestVr::OriginalLinkerCreateExportAction::Construct,
            "Owned Sound binding is not exact actual import/native Class/export/context identity");
        if(Fold(fs::path(binding.sourcePath).stem().string())=="deusexsounds")baselineSound=true;
        if(binding.importerSourcePath==map.sourcePath)mapSound=true;
        ++ownedBindings;
    }
    std::size_t actualCount{};for(const auto& [path,count]:importedCounts){actualCount+=count;const auto found=std::count_if(visited.begin(),visited.end(),[&](const auto& item){return item.first==path;});
        Require(static_cast<std::size_t>(found)==count,"Owned Sound import was silently omitted from source admission");}
    Require(actualCount==bundle.bindings.size()&&baselineSound&&mapSound&&bundle.nativeWork.used&&bundle.retainedBytes<=PortableSoundSourceLimits{}.retainedBytes,
        "Owned source admission omitted baseline/map Sound or bounded accounting");
    NoPublication(gc);for(const auto& source:originals)source.Unchanged();
    std::cout<<"OWNED first Training Sound packages="<<bundle.packages.size()<<" bindings="<<bundle.bindings.size()
        <<" serialized="<<bundle.serializedBytes<<" retained="<<bundle.retainedBytes<<" work="<<bundle.nativeWork.used<<'\n';
}
} // namespace
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    bool owned{};fs::path root;
    for(int argument=1;argument<argc;++argument){const std::string_view value=argv[argument];
        if(value=="--owned")owned=true;else if(value=="--game-root"&&argument+1<argc){owned=true;root=argv[++argument];}
        else throw std::runtime_error("Usage: portable_sound_sources_test [--owned | --game-root OWNED_GAME_ROOT]");}
    if(owned&&root.empty()){if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT"))root=configured;}
    if(owned&&root.empty()){std::cout<<"SKIP owned first Training Sound source admission: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
    if(owned)Owned(fs::canonical(root));else Fixtures();
    std::cout<<"PASS detached Sound source admission checks="<<checks<<" refusals="<<refusals<<" ownedBindings="<<ownedBindings
        <<"; actual v68 import/export/Class/context/ordered-Paths and source bytes only. NOT PCM decode, runtime audio publication, audible Quest output or playable Training.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL detached Sound source admission: "<<error.what()<<'\n';return 1;}}
