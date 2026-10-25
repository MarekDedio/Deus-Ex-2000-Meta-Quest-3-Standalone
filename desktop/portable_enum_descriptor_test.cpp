#include "Precomp.h"
#include "surreal_portable_package_tables.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F action){bool failed{};try{action();}catch(const std::exception&){failed=true;}Require(failed,"Malformed Enum was accepted");++refusals;}
void Index(Bytes& bytes,std::int32_t value){
    auto magnitude=static_cast<std::uint32_t>(value<0?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((value<0?128u:0u)|(magnitude&63u));magnitude>>=6u;
    if(magnitude)first|=64u;
    bytes.push_back(first);
    while(magnitude){auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude)next|=128u;bytes.push_back(next);}
}
struct Fixture {
    std::filesystem::path parent,directory,path;PortablePackageTables package;
    Fixture(){parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned index=0;index<20u;++index){const auto candidate=parent/("quest-enum-"+std::to_string(stamp)+'-'+std::to_string(index));
            if(std::filesystem::create_directory(candidate)){directory=std::filesystem::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated Enum fixture");
        path=directory/"GeneratedEnum.bin";package.sourcePath=path.string();package.version=68u;
        for(const auto* name:{"None","Core","Enum","Owner","TestEnum","IK_A","IK_F1","Foreign"})package.names.push_back({NameString(name),0u});
        package.imports={{0,0,0,1},{1,2,-1,2},{0,0,0,7},{7,2,-3,2}};
        package.exports={{-2,0,2,4,ObjectFlags{},0,37},{0,0,0,3,ObjectFlags{},0,0}};
    }
    ~Fixture(){std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("quest-enum-",0u)==0u)
            std::filesystem::remove_all(actual,error);}
    void Write(const Bytes& bytes){std::ofstream output(path,std::ios::binary|std::ios::trunc);std::array<char,37u> prefix{};
        output.write(prefix.data(),prefix.size());if(!bytes.empty())output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        Require(bool(output),"Cannot write generated Enum fixture");package.exports[0].ObjSize=static_cast<std::int32_t>(bytes.size());}
};
Bytes Body(std::int32_t count,std::initializer_list<std::int32_t> names){Bytes bytes{0u};Index(bytes,-2);Index(bytes,2);Index(bytes,count);for(auto name:names)Index(bytes,name);return bytes;}
}
int main(){try{
    Fixture fixture;const auto valid=Body(4,{5,0,6,5});fixture.Write(valid);
    const auto enumeration=LoadPortableEnumDescriptor(fixture.package,0u);
    Require(enumeration.objectPath=="Owner.TestEnum"&&enumeration.baseField==-2&&enumeration.nextField==2&&
        enumeration.names==std::vector<NameString>{NameString("IK_A"),NameString("None"),NameString("IK_F1"),NameString("IK_A")},
        "Enum decoder lost original field links/name identities/order/None/duplicates");
    const auto links=LoadPortableFieldLinks(fixture.package,0u);Require(links.baseField==-2&&links.nextField==2,"Production Enum field link path differs");
    fixture.Write(Body(0,{}));Require(LoadPortableEnumDescriptor(fixture.package,0u).names.empty(),"Empty Enum rejected");
    for(std::size_t length=0;length<valid.size();++length){fixture.Write(Bytes(valid.begin(),valid.begin()+length));Refuse([&]{LoadPortableEnumDescriptor(fixture.package,0u);});}
    for(const auto& bytes:std::vector<Bytes>{Body(-1,{}),Body(65537,{}),Body(5,{5,0,6,5}),Body(1,{-1}),Body(1,{8}),Body(0,{5})}){
        fixture.Write(bytes);Refuse([&]{LoadPortableEnumDescriptor(fixture.package,0u);});Refuse([&]{LoadPortableFieldLinks(fixture.package,0u);});}
    fixture.Write(valid);Refuse([&]{LoadPortableEnumDescriptor(fixture.package,2u);});
    for(auto budget:{std::size_t{0},std::size_t{1},valid.size(),std::size_t{128}})
        Refuse([&]{LoadPortableEnumDescriptor(fixture.package,0u,budget);});
    fixture.package.exports[0].ObjClass=-4;Refuse([&]{LoadPortableEnumDescriptor(fixture.package,0u);});
    fixture.package.exports[0].ObjClass=0;Refuse([&]{LoadPortableEnumDescriptor(fixture.package,0u);});
    std::cout<<"PASS complete Enum descriptor: "<<checks<<" checks / "<<refusals<<" malformed/budget refusals; generated bounded payloads only\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL Enum descriptor: "<<error.what()<<'\n';return 1;}}
