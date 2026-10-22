#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include <iostream>

int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("Usage: dump_location_inspect OWNED_DeusEx.u");
        const auto table=LoadPortablePackageTables(argv[1]);
        const auto index=FindPortableExport(table,"DumpLocation");
        const auto cls=LoadPortableClassDescriptor(table,index);
        std::cout<<"DumpLocation flags="<<std::hex<<static_cast<std::uint32_t>(table.exports[index].ObjFlags)
            <<" classFlags="<<cls.classFlags<<std::dec<<" base="<<GetPortableObjectPath(table,table.exports[index].ObjBase)
            <<" within="<<GetPortableObjectPath(table,cls.classWithinReference.value_or(0))<<'\n';
        for(std::size_t i=0u;i<table.exports.size();++i)if(
            GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)).rfind("DumpLocation.",0u)==0u) {
            const auto type=GetPortableObjectPath(table,table.exports[i].ObjClass);
            if(type.find("Property")==std::string::npos)continue;
            const auto field=LoadPortablePropertyDescriptor(table,i);
            std::cout<<GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u))<<" type="<<field.type
                <<" flags="<<std::hex<<field.flags<<std::dec<<" dimension="<<field.arrayDimension
                <<" ref="<<GetPortableObjectPath(table,field.referencedType)<<'\n';
        }
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
