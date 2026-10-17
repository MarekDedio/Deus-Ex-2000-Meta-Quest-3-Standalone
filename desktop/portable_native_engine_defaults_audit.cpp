#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_native_config.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

// Read-only owned metadata inventory for native Engine default preparation.
// This never initializes a runtime, registers a class or executes original code.
namespace {
namespace fs=std::filesystem;
void Require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
std::string Path(const PortablePackageTables& table,std::int32_t reference) {
    auto result=GetPortableObjectPath(table,reference);
    if(reference>0&&!result.empty())result=fs::path(table.sourcePath).stem().string()+'.'+result;
    return result;
}
std::uint64_t Hash(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot read owned default source");
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t bytes{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
        bytes+=static_cast<std::uintmax_t>(count);Require(bytes<=512u*1024u*1024u,"Owned default source exceeds inspection bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(input.eof(),"Could not finish reading owned default source");return hash;
}
void Inventory(const fs::path& root,const char* package,const std::vector<std::string>& classes) {
    const auto file=fs::canonical(root/"System"/(std::string(package)+".u"));
    const auto bytes=fs::file_size(file);const auto modified=fs::last_write_time(file);const auto fingerprint=Hash(file);
    const auto table=LoadPortablePackageTables(file.string());Require(table.version==68u&&table.licenseeMode==0u,"Unsupported default source layout");
    for(const auto& name:classes) {
        const auto index=FindPortableExport(table,name);const auto& entry=table.exports.at(index);
        Require(entry.ObjClass==0&&(static_cast<std::uint32_t>(entry.ObjFlags)&0x04000000u)!=0u,
            "Default source is not an actual serialized native UClass");
        const auto cls=LoadPortableClassDescriptor(table,index);
        std::cout<<"CLASS "<<Path(table,static_cast<std::int32_t>(index+1u))<<" base="<<Path(table,entry.ObjBase)
            <<" flags=0x"<<std::hex<<cls.classFlags<<std::dec<<" defaults="<<cls.defaults.size()<<'\n';
        std::set<std::int32_t> visited;auto child=cls.state.children;
        while(child) {
            Require(child>0&&static_cast<std::size_t>(child)<=table.exports.size()&&visited.size()<65536u&&visited.insert(child).second,
                "Default source has an invalid/cyclic/over-budget Class.Children chain");
            const auto slot=static_cast<std::size_t>(child-1);const auto& field=table.exports.at(slot);
            Require(field.ObjOuter==static_cast<std::int32_t>(index+1u),"Default source field has wrong declaring Outer");
            const auto type=Path(table,field.ObjClass);
            std::int32_t next{};
            if(type.size()>=8u&&type.substr(type.size()-8u)=="Property") {
                const auto property=LoadPortablePropertyDescriptor(table,slot);
                std::cout<<"FIELD "<<Path(table,child)<<" type="<<type<<" dim="<<property.arrayDimension
                    <<" flags=0x"<<std::hex<<property.flags<<std::dec<<" base="<<property.baseField
                    <<" typeRef="<<Path(table,property.referencedType)<<" secondaryRef="<<Path(table,property.secondaryType)<<'\n';
                next=property.nextField;
            }else if(type=="Core.Function")next=LoadPortableFunctionScript(table,slot).nextField;
            else if(type=="Core.State")next=LoadPortableStateDescriptor(table,slot).nextField;
            else next=LoadPortableFieldLinks(table,slot).nextField;
            child=next;
        }
        for(const auto& tag:cls.defaults) {
            std::cout<<"TAG "<<tag.name.ToString()<<'['<<tag.arrayIndex<<"] type="<<static_cast<unsigned>(tag.type)
                <<" bytes="<<tag.value.size()<<" bool="<<tag.boolValue;
            if(tag.value.size()==4u) {
                const auto value=static_cast<std::uint32_t>(tag.value[0u])|static_cast<std::uint32_t>(tag.value[1u])<<8u|
                    static_cast<std::uint32_t>(tag.value[2u])<<16u|static_cast<std::uint32_t>(tag.value[3u])<<24u;
                std::cout<<" raw32=0x"<<std::hex<<value<<std::dec;
            }
            std::cout<<'\n';
        }
    }
    Require(fs::file_size(file)==bytes&&fs::last_write_time(file)==modified&&Hash(file)==fingerprint,
        "Native default metadata inspection changed source bytes/mtime");
}
void ConfigInventory(const fs::path& root) {
    // Explicit port binding to owned System/DeusEx.ini, not a claim that the
    // packed launcher's GConfig factory/System.ini alias has been recovered.
    const auto file=fs::canonical(root/"System"/"DeusEx.ini");const auto before=Hash(file);
    const auto size=fs::file_size(file);const auto modified=fs::last_write_time(file);
    const auto doc=QuestVr::NativeConfig::ReadOrderedIni(file);Require(bool(doc),"Owned engine INI is missing");
    for(const auto* cls:{"Engine.Engine","Engine.GameEngine","Extension.GameEngineExt","DeusEx.DeusExGameEngine"}) {
        for(const auto* field:{"GameRenderDevice","AudioDevice","Console","NetworkDevice","Language","CacheSizeMegs","UseSound"}) {
            const auto value=doc->PropertyScalar(cls,field);
            std::cout<<"CONFIG-SCALAR "<<cls<<'.'<<field<<" action="<<(value?"import":"preserve-inherited")
                <<" units="<<(value?QuestVr::UnrealString::Decode(*value).size():0u)<<'\n';
        }
        if(std::string_view(cls)=="Engine.Engine")continue; // Arrays are native GameEngine declarations.
        for(const auto* field:{"ServerActors","ServerPackages"}) {
            const auto values=doc->StringArray(cls,field);
            std::cout<<"CONFIG-ARRAY "<<cls<<'.'<<field<<" action="<<(values?"replace":"preserve-inherited")
                <<" entries="<<(values?values->size():0u)<<'\n';
        }
    }
    Require(fs::file_size(file)==size&&fs::last_write_time(file)==modified&&Hash(file)==before,
        "Native default config inspection changed source bytes/mtime");
}
}
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Usage: portable_native_engine_defaults_audit OWNED_GAME_ROOT");
        const auto root=fs::canonical(argv[1]);Inventory(root,"Core",{"Object","Subsystem"});Inventory(root,"Engine",{"Engine"});ConfigInventory(root);
        std::cout<<"PASS read-only native engine default metadata inventory; no class/default/instance construction or original execution\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"Native engine default inventory failed: "<<error.what()<<'\n';return 1;}
}
