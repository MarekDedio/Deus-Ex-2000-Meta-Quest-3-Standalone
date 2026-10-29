#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: flag_base_inspect OWNED_Extension.u");
    const auto table=LoadPortablePackageTables(argv[1]);
    for(const auto* name:{"ExtensionObject","FlagBase","Flag","FlagInt","PlayerPawnExt"}){
        const auto slot=FindPortableExport(table,name);const auto cls=LoadPortableClassDescriptor(table,slot);
        std::cout<<"CLASS "<<name<<" flags="<<std::hex<<cls.classFlags<<std::dec<<" base="<<GetPortableObjectPath(table,table.exports[slot].ObjBase)
            <<" within="<<GetPortableObjectPath(table,cls.classWithinReference.value_or(0))<<" defaults="<<cls.defaults.size()
            <<" logical="<<cls.state.logicalSize<<" children="<<GetPortableObjectPath(table,cls.state.children)
            <<" deps="<<cls.dependencyCount<<" imports="<<cls.packageImportCount<<'\n';
    }
    for(std::size_t i=0;i<table.exports.size();++i){const auto path=GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u));
        if(path.rfind("Flag",0u)!=0u&&path!="PlayerPawnExt.FlagBase")continue;
        const auto type=GetPortableObjectPath(table,table.exports[i].ObjClass);
        if(type.find("Property")!=std::string::npos){const auto f=LoadPortablePropertyDescriptor(table,i);
            std::cout<<"FIELD "<<path<<" type="<<f.type<<" dim="<<f.arrayDimension<<" flags="<<std::hex<<f.flags<<std::dec
                <<" ref="<<GetPortableObjectPath(table,f.referencedType)<<" next="<<GetPortableObjectPath(table,f.nextField)<<'\n';}
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
