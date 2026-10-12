#include "Precomp.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string Lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
void Print(const PortablePackageTables& table,const PortableTaggedProperty& value) {
    std::cout << "  " << value.name.ToString() << '[' << value.arrayIndex << "] type=" << unsigned(value.type);
    if (value.type == 2u && value.value.size() == 4u) {
        std::int32_t number{}; std::memcpy(&number,value.value.data(),4u); std::cout << " value=" << number;
    } else if (value.type == 3u) std::cout << " value=" << value.boolValue;
    else if (value.type == 4u && value.value.size() == 4u) {
        float number{}; std::memcpy(&number,value.value.data(),4u); std::cout << " value=" << number;
    } else if (value.type == 6u) std::cout << " name=" << DecodePortableNameProperty(table,value);
    else if (value.type == 5u || value.type == 8u) {
        auto path = GetPortableObjectPath(table,DecodePortableObjectReference(value));
        if (DecodePortableObjectReference(value) > 0) path=std::filesystem::path(table.sourcePath).stem().string()+'.'+path;
        std::cout << " object=" << path;
    } else {
        std::cout << " bytes=" << value.value.size() << " data=";
        for (const auto byte:value.value) std::cout << std::hex << unsigned(byte) << ',';
        std::cout << std::dec;
    }
    std::cout << '\n';
}
}
int main(int argc,char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: dialogue_ui_audit PACKAGE FILTER");
        const auto package=LoadPortablePackageTables(argv[1]);
        for (std::size_t i=0;i<package.exports.size();++i) {
            const auto name=GetPortableObjectPath(package,static_cast<std::int32_t>(i+1u));
            if (Lower(name).find(Lower(argv[2]))==std::string::npos) continue;
            const auto& entry=package.exports[i];
            const auto cls=GetPortableObjectPath(package,entry.ObjClass);
            std::cout << name << " class=" << cls << " base=" << GetPortableObjectPath(package,entry.ObjBase) << '\n';
            if (entry.ObjClass==0) for (const auto& value:LoadPortableClassDescriptor(package,i).defaults) Print(package,value);
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
