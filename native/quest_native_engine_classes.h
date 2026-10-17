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
};

inline constexpr std::array<NativeEngineClassRegistration,5u> NativeEngineClasses{{
    {"Core.Package","Core.Object","Core.Object",0x34u,0x12u,0x04084004u},
    {"Engine.Primitive","Core.Object","Core.Object",0x54u,0x12u,0x04084004u},
    {"Engine.GameEngine","Engine.Engine","Core.Object",0xd0u,0x1eu,0x04084004u},
    {"Extension.GameEngineExt","Engine.GameEngine","Core.Object",0xd0u,0x1eu,0x04084004u},
    {"DeusEx.DeusExGameEngine","Extension.GameEngineExt","Core.Object",0xd0u,0x1eu,0x04084004u}
}};

// Separate closed metadata registration phase. These are genuine original
// native UClasses, not default blocks, fabricated serialized descriptors or
// permission to replay Cpp-property constructors after package loading.
// See ORIGINAL-NATIVE-METADATA-REGISTRATION.md for each audited native header.
inline constexpr std::array<NativeEngineClassRegistration,11u> NativeMetadataClasses{{
    {"Core.Field","Core.Object","Core.Object",0x34u,0x13u,0x04084004u},
    {"Core.Struct","Core.Field","Core.Object",0x6cu,0x12u,0x04084004u},
    {"Core.State","Core.Struct","Core.Object",0x484u,0x12u,0x04084004u},
    {"Core.Class","Core.State","Core.Package",0x4f4u,0x12u,0x04084004u},
    {"Core.Property","Core.Field","Core.Field",0x5cu,0x13u,0x04084004u},
    {"Core.ObjectProperty","Core.Property","Core.Field",0x64u,0x12u,0x04084004u},
    {"Core.ClassProperty","Core.ObjectProperty","Core.Field",0x68u,0x12u,0x04084004u},
    {"Core.IntProperty","Core.Property","Core.Field",0x5cu,0x12u,0x04084004u},
    {"Core.BoolProperty","Core.Property","Core.Field",0x60u,0x12u,0x04084004u},
    {"Core.StrProperty","Core.Property","Core.Field",0x5cu,0x12u,0x04084004u},
    {"Core.ArrayProperty","Core.Property","Core.Field",0x60u,0x12u,0x04084004u}
}};

} // namespace QuestVr
