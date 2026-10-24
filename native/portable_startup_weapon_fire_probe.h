#pragma once

#include "portable_retail_collision.h"
#include "portable_startup_weapon_fire_types.h"

// Separate diagnostic interface: normal startup options/results remain
// unchanged. Both outputs belong to ONE synchronous rollback-only execution.
// The actual weapon must be selected by options.weapon and genuinely equipped
// by original script before the additional selected Fire is attempted.
struct PortableTrainingWeaponFireProbeResult {
    PortableRetailCollisionStartupResult startup;
    std::vector<PortableStartupWeaponFireObservation> observations;
};
PortableTrainingWeaponFireProbeResult ProbePortableTrainingWeaponFire(
    const PortableWorldStartupProbeOptions& options,
    const PortableStartupWeaponFireCommand& command,
    const QuestVr::Vm::Limits& limits = {});
