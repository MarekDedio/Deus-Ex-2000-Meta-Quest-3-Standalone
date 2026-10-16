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

} // namespace QuestVr
