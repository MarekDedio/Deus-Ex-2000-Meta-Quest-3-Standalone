#include "quest_native_input_classes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

// Registration-only literal contracts. No commercial binary is loaded or
// executed; no UObject, native defaults, property layout or Input instance is
// fabricated here. Runtime allocation/lifetime/config tests are separate.
//
// Independent owned source proof:
// Engine.dll SHA256
// 0C3A8B18239CA98AE132506A1BD966CB5D91C6852E650E179CB15860832C20B4:
// initializer10392FE0..10393047, PrivateStaticClass10588550 (EAT1415),
// ctor10393430; its StaticConstructor10393570 is an empty RET, NOT the
// separate StaticInitInput103930C0..10393344 property/default construction.
// Extension.dll SHA256
// 93C35502D3C6FE61ECBECC7DBA526D09281A9FBCE16EE045061B1A4D8B916D9C:
// initializer1002CF10, UClass ctor call1002CF64,
// PrivateStaticClass100A2E00 (EAT1016/ordinal1017), size push1002CF56,
// supplied flags push1002CF51, UObject flags push1002CF20,
// Within IAT10059004->Core.Object, Super IAT10059328->Engine.Input,
// InputExt name100986F6 (not native prefix X at100986F4),
// package10097C9C, config User10098708. Instance callback1001C670 and
// ctor1002CFB0..1002D073 are NOT executed by class registration.
// Core.dll SHA256
// 8BF3039E14F849942EE70A95BDF26CF1F636245926E30106BBE58B7097EBA7E4:
// UClass native ctor1012866F OR12; Register101278F2..1012790C ORs
// Super.ClassFlags&4EC. The loaded Core.Subsystem flags are112.
namespace {

constexpr std::array<std::string_view,2u> Paths{
    "Engine.Input","Extension.InputExt"};
constexpr std::array<std::string_view,2u> Bases{
    "Core.Subsystem","Engine.Input"};
constexpr std::array<std::string_view,2u> Packages{
    "Engine","Extension"};

constexpr bool AuditedRegistrations() {
    if(QuestVr::NativeInputClasses.size()!=2u)return false;
    for(std::size_t i=0u;i<2u;++i) {
        const auto& row=QuestVr::NativeInputClasses[i];
        if(row.path!=Paths[i]||row.base!=Bases[i]||row.within!="Core.Object"||
            row.nativeSize!=0xfb0u||row.registrationClassFlags!=0x1eu||
            row.registrationObjectFlags!=0x04084004u||row.package!=Packages[i])return false;
    }
    return true;
}

template<std::size_t N>
constexpr bool DisjointFrom(const std::array<QuestVr::NativeEngineClassRegistration,N>& other) {
    for(const auto& input:QuestVr::NativeInputClasses)
        for(const auto& row:other)if(input.path==row.path)return false;
    return true;
}

static_assert(std::is_same_v<decltype(QuestVr::NativeInputClasses),
    const std::array<QuestVr::NativeEngineClassRegistration,2u>>,
    "Input class metadata must use the existing native registration row contract");
static_assert(AuditedRegistrations(),
    "Input native class relationships/literals differ from the owned registration audit");
static_assert(QuestVr::NativeInputClasses[0u].path!=QuestVr::NativeInputClasses[1u].path,
    "InputExt is a separate original UClass, not an alias of Input");
static_assert(QuestVr::NativeInputClasses[1u].base==QuestVr::NativeInputClasses[0u].path,
    "InputExt must derive from the actual Input class identity");
static_assert(DisjointFrom(QuestVr::NativeEngineClasses)&&
    DisjointFrom(QuestVr::NativeMetadataClasses)&&DisjointFrom(QuestVr::NativeConfigClasses),
    "Input registrations belong to their own closed class phase");
static_assert(QuestVr::NativeInputConfigName=="User",
    "Both original input classes supply the User config name, not System");
static_assert(QuestVr::NativeInputSuppliedClassFlags==0x0cu,
    "The initializer argument is distinct from the UClass constructor baseline");
static_assert((0x0cu|0x12u)==0x1eu&&
    QuestVr::NativeInputClasses[0u].registrationClassFlags==(QuestVr::NativeInputSuppliedClassFlags|0x12u)&&
    QuestVr::NativeInputClasses[1u].registrationClassFlags==(QuestVr::NativeInputSuppliedClassFlags|0x12u),
    "Native UClass constructor must preserve its own OR12 baseline");
static_assert((0x112u&0x4ecu)==0u&&(0x1eu&0x4ecu)==0x0cu&&
    (0x1eu|(0x112u&0x4ecu))==0x1eu&&(0x1eu|(0x1eu&0x4ecu))==0x1eu,
    "The audited superclass inheritance step changes neither Input baseline");
static_assert((0x100u&0x4ecu)==0u,
    "A superclass serialized 100 bit is not an inherited registration flag");

} // namespace

int main() {
    std::cout<<"Native Input class registration literal contracts passed (2 classes; no defaults/instances)\n";
}
