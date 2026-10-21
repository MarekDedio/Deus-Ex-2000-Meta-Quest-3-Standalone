#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace QuestVr {

// Explicit original native UClass registrations, not serialized UClass/CDO
// records. See ORIGINAL-ENGINE-CYLINDER.md for owned PE instruction evidence.
// Flags record the native UClass constructor baseline, not serialized post-load
// state, instance-allocation flags or proof of full Register/defaults/config or
// an Engine/Primitive instance constructor/Init.
struct NativeEngineClassRegistration {
    std::string_view path, base, within;
    std::uint32_t nativeSize, registrationClassFlags, registrationObjectFlags;
    // Audited UObject.Register package literal, not a split display path.
    std::string_view package;
};

inline constexpr std::array<NativeEngineClassRegistration,5u> NativeEngineClasses{{
    {"Core.Package","Core.Object","Core.Object",0x34u,0x12u,0x04084004u,"Core"},
    {"Engine.Primitive","Core.Object","Core.Object",0x54u,0x12u,0x04084004u,"Engine"},
    {"Engine.GameEngine","Engine.Engine","Core.Object",0xd0u,0x1eu,0x04084004u,"Engine"},
    {"Extension.GameEngineExt","Engine.GameEngine","Core.Object",0xd0u,0x1eu,0x04084004u,"Extension"},
    {"DeusEx.DeusExGameEngine","Extension.GameEngineExt","Core.Object",0xd0u,0x1eu,0x04084004u,"DeusEx"}
}};

// Separate closed metadata registration phase. These are genuine original
// native UClasses, not default blocks, fabricated serialized descriptors or
// permission to replay Cpp-property constructors after package loading.
// See ORIGINAL-NATIVE-METADATA-REGISTRATION.md for each audited native header.
inline constexpr std::array<NativeEngineClassRegistration,12u> NativeMetadataClasses{{
    {"Core.Field","Core.Object","Core.Object",0x34u,0x13u,0x04084004u,"Core"},
    {"Core.Struct","Core.Field","Core.Object",0x6cu,0x12u,0x04084004u,"Core"},
    {"Core.State","Core.Struct","Core.Object",0x484u,0x12u,0x04084004u,"Core"},
    {"Core.Class","Core.State","Core.Package",0x4f4u,0x12u,0x04084004u,"Core"},
    {"Core.Property","Core.Field","Core.Field",0x5cu,0x13u,0x04084004u,"Core"},
    {"Core.ObjectProperty","Core.Property","Core.Field",0x64u,0x12u,0x04084004u,"Core"},
    {"Core.ClassProperty","Core.ObjectProperty","Core.Field",0x68u,0x12u,0x04084004u,"Core"},
    {"Core.IntProperty","Core.Property","Core.Field",0x5cu,0x12u,0x04084004u,"Core"},
    {"Core.BoolProperty","Core.Property","Core.Field",0x60u,0x12u,0x04084004u,"Core"},
    {"Core.StrProperty","Core.Property","Core.Field",0x5cu,0x12u,0x04084004u,"Core"},
    {"Core.ArrayProperty","Core.Property","Core.Field",0x60u,0x12u,0x04084004u,"Core"},
    // Required by first Training's original nested NanoKeyData destruction.
    // Actual native size64, not the Struct pointer's offset5C plus4 guess.
    // See ORIGINAL-STRUCT-PROPERTY-REGISTRATION.md. No instance/CDO is implied.
    {"Core.StructProperty","Core.Property","Core.Field",0x64u,0x12u,0x04084004u,"Core"}
}};

// The actual native bases of Engine's five config ClassProperties. Console
// is serialized in Engine.u and MUST NOT be replaced with a native wrapper.
inline constexpr std::array<NativeEngineClassRegistration,4u> NativeConfigClasses{{
    {"Core.Language","Core.Object","Core.Object",0x2cu,0x1bu,0x04084004u,"Core"},
    {"Engine.AudioSubsystem","Core.Subsystem","Core.Object",0x2cu,0x17u,0x04084004u,"Engine"},
    {"Engine.RenderDevice","Core.Subsystem","Core.Object",0xbcu,0x17u,0x04084004u,"Engine"},
    {"Engine.NetDriver","Core.Subsystem","Core.Object",0x90u,0x1fu,0x04084004u,"Engine"}
}};

} // namespace QuestVr
