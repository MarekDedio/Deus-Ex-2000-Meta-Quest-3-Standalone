#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_input_key_queries.h"
#include "quest_native_config.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Owned read-only data test. Explicit supplied User.ini scalar inventory, NOT
// native Input class/default/property/config preparation or a live instance.
namespace {
namespace Query=QuestVr::NativeInputKeyQueries;
std::size_t checks{},enumCount{},commands{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Fingerprint {std::filesystem::path path;std::uintmax_t size{};std::filesystem::file_time_type modified;std::uint64_t hash{14695981039346656037ull};};
Fingerprint Source(const std::filesystem::path& path){Fingerprint result;result.path=path;result.size=std::filesystem::file_size(path);result.modified=std::filesystem::last_write_time(path);
    std::ifstream input(path,std::ios::binary);std::array<char,65536u> block{};Require(bool(input),"Cannot fingerprint owned Input source");
    while(input){input.read(block.data(),block.size());for(std::streamsize i=0;i<input.gcount();++i){result.hash^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);result.hash*=1099511628211ull;}}
    Require(input.eof()&&!input.bad(),"Cannot finish owned Input fingerprint");return result;}
struct Operations {
    using Name=int;
    const PortableEnumDescriptor& enumeration;
    std::array<std::u16string,255u> bindings{};
    std::u16string output;std::size_t work{};
    void Step(){if(++work>200000u)throw std::runtime_error("Owned Input query shared work bound");}
    std::size_t EnumSize(){return enumeration.names.size();}
    Name EnumName(std::size_t index){return enumeration.names.at(index).GetCompareIndex();}
    std::u16string NameText(Name name){for(const auto& value:enumeration.names)if(value.GetCompareIndex()==name)return QuestVr::UnrealString::Decode(value.ToString());
        throw std::runtime_error("Owned key query requested absent Enum FName identity");}
    Name FindName(std::u16string_view text){return NameString::FindCompareIndex(QuestVr::UnrealString::Encode(text)).value_or(0);}
    std::u16string Binding(std::size_t index){return bindings.at(index);}
    void Log(std::u16string_view text){output.append(text);}
};
}
int main(int argc,char** argv){try{
    if(argc==1){std::cout<<"SKIP owned Input query data: supply GOTY root\n";return 77;}
    if(argc!=2)throw std::runtime_error("Usage: portable_input_key_queries_test GAME_ROOT");
    const auto root=std::filesystem::canonical(argv[1]);std::vector<Fingerprint> sources;
    PortableEnumDescriptor actor,console;
    for(const auto* packageName:{"Core","Engine","Extension","DeusEx","DeusExUI","DeusExItems","DeusExCharacters","DeusExDeco"}){
        const auto path=root/"System"/(std::string(packageName)+".u");sources.push_back(Source(path));
        const auto package=LoadPortablePackageTables(path.string());
        for(std::size_t i=0;i<package.exports.size();++i){
            if(package.exports[i].ObjClass>=0||GetPortableObjectPath(package,package.exports[i].ObjClass)!="Core.Enum")continue;
            const auto enumeration=LoadPortableEnumDescriptor(package,i);++enumCount;
            const auto links=LoadPortableFieldLinks(package,i);
            Require(links.baseField==enumeration.baseField&&links.nextField==enumeration.nextField,"Owned Enum production links differ");
            if(std::string(packageName)=="Engine"&&enumeration.objectPath=="Actor.EInputKey")actor=enumeration;
            if(std::string(packageName)=="Engine"&&enumeration.objectPath=="Console.EInputKey")console=enumeration;
        }
    }
    Require(actor.names.size()==255u&&console.names.size()==255u&&actor.names!=console.names,"Actual Actor/Console input enums were confused");
    Require(actor.names[0]=="IK_None"&&actor.names[1]=="IK_LeftMouse"&&actor.names[65]=="IK_A"&&
        actor.names[112]=="IK_F1"&&actor.names[254]=="IK_OEMClear","Owned Actor.EInputKey source identity differs");
    const auto userPath=root/"System"/"User.ini";sources.push_back(Source(userPath));
    const auto ini=QuestVr::NativeConfig::ReadOrderedIni(userPath);Require(bool(ini),"Owned User.ini is missing");
    Operations operations{actor,{}, {},0u};
    // Inventory both concrete inheritance sections explicitly; this does NOT
    // claim native CDO preparation, alias import, defaults or LoadConfig itself.
    for(const auto* section:{"Engine.Input","Extension.InputExt"})for(std::size_t i=0;i<actor.names.size();++i){
        const auto units=QuestVr::UnrealString::Decode(actor.names[i].ToString());
        if(actor.names[i].IsNone()||units.size()<=3u)continue;
        const auto value=ini->PropertyScalar(section,QuestVr::UnrealString::Encode(std::u16string_view(units).substr(3u)));
        if(value)operations.bindings[i]=QuestVr::UnrealString::Decode(*value);
    }
    for(std::size_t index=0;index<255u;++index){
        const auto original=QuestVr::UnrealString::Decode(actor.names[index].ToString());
        const auto expected=original.size()>3u?original.substr(3u):std::u16string{};
        operations.output.clear();const auto keyCommand=u"KEYNAME "+QuestVr::UnrealString::Decode(std::to_string(index));
        Require(Query::Exec(operations,keyCommand)&&operations.output==expected,"Owned original KEYNAME differs from literal Enum suffix");++commands;
        operations.output.clear();Require(Query::Exec(operations,u"KEYBINDING "+expected),"Owned key binding not handled");++commands;
        std::u16string binding;
        const auto find=NameString::FindCompareIndex(QuestVr::UnrealString::Encode(u"IK_"+expected));
        if(find&&*find)for(std::size_t candidate=0;candidate<actor.names.size();++candidate)
            if(actor.names[candidate].GetCompareIndex()==*find){binding=operations.bindings[candidate];break;}
        Require(operations.output==binding,"Owned query result differs from explicit supplied-file scalar inventory");
    }
    const auto unknown=u"IK_QuestInputNameMustNotBeInterned_ZZ782";
    Require(!NameString::FindCompareIndex(QuestVr::UnrealString::Encode(unknown)),"Unknown-key fixture already interned");
    operations.output.clear();Require(Query::Exec(operations,u"KEYBINDING QuestInputNameMustNotBeInterned_ZZ782")&&operations.output.empty(),"Unknown key fabricated output");
    Require(!NameString::FindCompareIndex(QuestVr::UnrealString::Encode(unknown)),"FNAME_Find lookup interned unknown key");
    for(const auto& before:sources){const auto after=Source(before.path);Require(before.size==after.size&&before.modified==after.modified&&before.hash==after.hash,"Input data checks changed owned source");}
    std::cout<<"PASS owned Input key data: "<<checks<<" checks / "<<enumCount<<" complete Enums / "<<commands
        <<" key queries; actual Actor.EInputKey and explicit User.ini inventory unchanged. NOT native Input instance/config initialization or live startup\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL owned Input key data: "<<error.what()<<'\n';return 1;}}
