#pragma once

#include "portable_startup_weapon_fire_types.h"

#include <cmath>
#include <stdexcept>

// Asset-free admission for the bounded first-Training attack diagnostic.
// This validates only the explicit command, not placement, collision, aim,
// available gameplay services or successful original attack/damage behavior.
inline void ValidatePortableStartupWeaponFireInput(const PortableStartupWeaponFireCommand& command) {
    if(!std::isfinite(command.deltaSeconds) || command.deltaSeconds<=0.0f || command.deltaSeconds>0.1f)
        throw std::runtime_error("Startup weapon Fire requires a finite positive step of at most 0.1 seconds");
    if(!command.frames || command.frames>64u)
        throw std::runtime_error("Startup weapon Fire requires one to sixty-four animation/state phases per attack");
    if(!command.attacks || command.attacks>4u)
        throw std::runtime_error("Startup weapon Fire requires one to four original attack cycles");
    if(command.pose) {
        for(const float component:command.pose->location)
            if(!std::isfinite(component))
                throw std::runtime_error("Startup weapon Fire diagnostic pose requires finite location components");
        // Every signed32 rotation word is valid here. No angle normalization,
        // finite-position clamp, chosen target or damage value is supplied.
    }
}
