#include "quest_native_struct_import.h"

#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
namespace Import=QuestVr::NativeConfigImport;
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F action){bool failed{};try{action();}catch(const std::runtime_error&){failed=true;}Require(failed,"Expected bounded import refusal");++refusals;}
struct Operations {
    using Name=unsigned;using StructureType=unsigned;using Field=unsigned;
    struct Property {Name name{};int dimension{1},element{4};bool property{true},port{true},string{};Field next{};};
    std::map<Name,std::u16string> names{{0u,u"None"},{1u,u"Command"},{2u,u"Alias"},{3u,u"Unknown"}};
    std::array<Property,4u> fields{{{}, {1u,1,12,true,true,true,2u},{2u,1,4,true,true,false,0u},{2u,1,4,true,true,false,0u}}};
    std::array<std::u16string,4u> strings;std::array<Name,4u> values{};
    std::vector<std::uint32_t> importFlags;std::vector<Import::StructImportWarning> warnings;
    bool inherited{};std::size_t work{},limit{10000u},finds{},adds{},imports{};
    void Step(){if(work>=limit)throw std::runtime_error("Mock import shared work exhausted");++work;}
    void Warning(Import::StructImportWarning value){warnings.push_back(value);}
    Name FindName(std::u16string_view text){++finds;for(const auto& [id,name]:names)if(QuestVr::UnrealString::EqualInsensitive(text,name))return id;return 0u;}
    Name AddName(std::u16string_view text){++adds;if(text.empty())return 0u;const auto old=FindName(text);if(old)return old;
        const auto id=static_cast<Name>(names.size());names.emplace(id,text);return id;}
    StructureType Structure(){return 1u;}StructureType Super(StructureType value){return value==1u&&inherited?2u:0u;}
    Field Child(StructureType value){return value==1u?1u:3u;}Field Next(Field value){return fields.at(value).next;}
    bool IsProperty(Field value){return fields.at(value).property;}Name FieldName(Field value){return fields.at(value).name;}
    int Dimension(Field value){return fields.at(value).dimension;}int ElementSize(Field value){return fields.at(value).element;}
    bool Port(Field value){return fields.at(value).port;}
    Import::Result Import(Field field,unsigned index,std::u16string_view text,std::uint32_t flags){
        Require(index==0u,"Struct invented a repaired inner array index");++imports;importFlags.push_back(flags);
        if(fields.at(field).string)return Import::String(text,flags,strings.at(field));
        return Import::Name(text,values.at(field),[&](std::u16string_view name){return AddName(name);});
    }
};
void Scalars(){
    Operations operations;unsigned name=99u;
    auto result=Import::Name(u"Fire,rest",name,[&](auto value){return operations.AddName(value);});
    Require(result.returned&&result.nextUnit==4u&&operations.names.at(name)==u"Fire"&&operations.adds==1u,"Name import did not use actual Add after token");
    name=99u;result=Import::Name(u"\"unterminated",name,[&](auto value){return operations.AddName(value);});
    Require(!result.returned&&name==99u&&operations.adds==1u,"Failed Name token wrote or interned a name");
    std::u16string text=u"before";result=Import::String(u"full string, not token",0u,text);
    Require(result.returned&&result.nextUnit==0u&&text==u"full string, not token","flags0 String import did not preserve original cursor/full CString");
    result=Import::String(u"\"a,b) c\",tail",2u,text);
    Require(result.returned&&result.nextUnit==8u&&text==u"a,b) c","String delimiter mode did not use original token parser");
    result=Import::String(u"\"unterminated",2u,text);Require(!result.returned&&text==u"a,b) c","Failed delimited String overwrote prior storage");
    result=Import::String(u"\"A\\00B\"",2u,text);Require(result.returned&&text==u"A","String copied past decoded CString NUL");
    for(const auto length:{1022u,1023u,4094u,4095u}) {
        const std::u16string token(length,u'X');text=u"unchanged";
        const auto parsed=Import::String(u'"'+token+u'"',2u,text);
        Require(parsed.returned==(length<4095u)&&text==(length<4095u?token:u"unchanged"),"String token did not retain separate4096-unit capacity");
    }
    for(auto capacity:{0u,1u,4097u})Refuse([&]{Import::ReadToken(u"x",false,0u,capacity);});
}
void Structures(){
    {Operations op;const auto text=u"(Command=\"Button bFire | Fire\",Alias=Fire)tail";
        const auto result=Import::Struct(text,0u,op);
        Require(result.returned&&result.nextUnit==std::u16string_view(text).find(u"tail")&&op.strings[1u]==u"Button bFire | Fire"&&
            op.names.at(op.values[2u])==u"Fire"&&op.imports==2u&&op.importFlags==std::vector<std::uint32_t>{2u,2u},"Structured aliases did not import typed metadata/flags/cursors");}
    for(const auto& text:{u"()",u"(Unknown=(A=(B=1)))",u"(notInterned=17)",u"(Command=\"x\",)"}) {
        Operations op;const auto before=op.names.size();const auto result=Import::Struct(text,0u,op);
        Require(result.returned&&op.names.size()==before,"Unknown/empty struct import interned field names or failed valid separators");}
    {Operations op;const auto result=Import::Struct(u"(Command=\"ok\",Alias=\"unterminated)",0u,op);
        Require(!result.returned&&op.strings[1u]==u"ok"&&op.values[2u]==0u,"Failed Struct erased original partial field writes");}
    for(const auto& [text,warning]:std::vector<std::pair<std::u16string,Import::StructImportWarning>>{
        {u" (Alias=Fire)",Import::StructImportWarning::ExpectedOpen},
        {u"(Alias[0]=Fire)",Import::StructImportWarning::ExpectedArrayClose},
        {u"(Alias Fire)",Import::StructImportWarning::ExpectedEquals},
        {u"(Unknown=(abc",Import::StructImportWarning::TooManyOpen},
        {u"(Alias=Fire x)",Import::StructImportWarning::ExpectedSeparator}}) {
        Operations op;Require(!Import::Struct(text,0u,op).returned&&op.warnings==std::vector{warning},"Original struct warning/refusal control changed");}
    {Operations op;op.inherited=true;const auto result=Import::Struct(u"(Alias=Fire)",4u,op);
        Require(result.returned&&op.names.at(op.values[2u])==u"Fire"&&op.names.at(op.values[3u])==u")"&&
            op.importFlags==std::vector<std::uint32_t>{6u,6u},"Struct stopped at first duplicate instead of preserving live inherited traversal");}
    for(const unsigned mode:{0u,1u,2u,3u}) {
        Operations op;
        if(mode==0)op.fields[2].port=false;
        if(mode==1)op.fields[2].dimension=0;
        if(mode==2)op.fields[2].element=0;
        if(mode==3)op.fields[2].property=false;
        Require(Import::Struct(u"(Alias=Fire)",0u,op).returned&&op.imports==0u&&op.values[2u]==0u,"Nonportable/zero/nonproperty field was imported");}
    {Operations warm;Import::Struct(u"(Command=\"x\",Alias=Fire)",0u,warm);
        for(std::size_t cut=0;cut<warm.work;++cut){Operations op;op.limit=cut;
            Refuse([&]{Import::Struct(u"(Command=\"x\",Alias=Fire)",0u,op);});Require(op.work==cut,"Struct reset shared work budget");}}
    {Operations op;Refuse([&]{Import::Struct(std::u16string(16385u,u'('),0u,op);});}
}
}
int main(){try{Scalars();Structures();std::cout<<"PASS native Name/String/Struct imports: "<<checks<<" checks / "<<refusals
    <<" refusals; explicit mock Struct/Property/FName authority, NOT initialized native Input\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL native struct import: "<<error.what()<<'\n';return 1;}}
