#pragma once

#include "quest_native_engine_classes.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace QuestVr {

// Both native initializers supply these values independently. ConfigName is
// registration metadata, not evidence that User.ini or defaults were loaded.
inline constexpr std::uint32_t NativeInputSuppliedClassFlags=0x0cu;
inline constexpr std::string_view NativeInputConfigName="User";

// Genuine native-only UClasses absent from the owned Engine.u/Extension.u
// export tables. Engine10392FE0..10393047 and Extension1002CF10..1002CF64
// supply sizeFB0, WithinObject, their own package/name and flags0C/04084004.
// UClass's native constructor ORs12 (Core1012866F), so the constructor baseline
// below is1E, NOT the supplied0C. Register's Super.ClassFlags&4EC step
// (Core101278F2..1012790C) adds no further bits: Subsystem112&4EC=0 and
// Input1E&4EC=0C. These are not independently serialized post-load flags.
//
// This closed phase registers class identities/relationships ONLY. It does
// not construct native property metadata, defaults, aliases, key bindings,
// EInputKey, an Input instance, viewport ownership or Init/ResetInput state.
// In particular, InputExt's instance LoadConfig/name-bootstrap behavior is a
// separate prerequisite, not implied by its registered class or config name.
// See ORIGINAL-PLAYER-INPUT-CONSOLE.md, ORIGINAL-INPUT-ENUM-IDENTITY.md and
// ORIGINAL-NATIVE-METADATA-REGISTRATION.md for owned instruction evidence.
inline constexpr std::array<NativeEngineClassRegistration,2u> NativeInputClasses{{
    {"Engine.Input","Core.Subsystem","Core.Object",0xfb0u,0x1eu,0x04084004u,"Engine"},
    {"Extension.InputExt","Engine.Input","Core.Object",0xfb0u,0x1eu,0x04084004u,"Extension"}
}};

} // namespace QuestVr
