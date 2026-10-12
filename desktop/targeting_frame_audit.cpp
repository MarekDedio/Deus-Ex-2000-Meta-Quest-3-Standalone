#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include <cstring>
#include <iostream>

// Read-only original class-default evidence. This never runs commercial code.
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: targeting_frame_audit GAME_ROOT");
        const auto package = LoadPortablePackageTables(std::string(argv[1])+"/System/DeusEx.u");
        for (const auto* name : {"FrobDisplayWindow", "ColorThemeHUD", "ColorThemeHUD_Default"}) {
            const auto cls = LoadPortableClassDescriptor(package,FindPortableExport(package,name));
            std::cout << "CLASS " << name << '\n';
            for (const auto& p : cls.defaults) {
                std::cout << p.name.ToString() << '[' << p.arrayIndex << "] type=" << unsigned(p.type) << " value=";
                if (p.type == 13u) std::cout << DecodePortableStringProperty(p);
                else if (p.type == 6u) std::cout << DecodePortableNameProperty(package,p);
                else if (p.type == 4u && p.value.size() == 4u) {
                    float number{}; std::memcpy(&number,p.value.data(),4u); std::cout << number;
                } else for (auto byte : p.value) std::cout << unsigned(byte) << ',';
                std::cout << '\n';
            }
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
