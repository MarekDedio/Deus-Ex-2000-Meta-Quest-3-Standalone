#include "portable_startup_weapon_fire_input.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{},accepted{},refusals{};
void Require(bool condition,const char* why) {
    ++checks;if(!condition)throw std::runtime_error(why);
}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(const PortableStartupWeaponFireCommand& a,const PortableStartupWeaponFireCommand& b) {
    if(Bits(a.deltaSeconds)!=Bits(b.deltaSeconds)||a.frames!=b.frames||a.attacks!=b.attacks||bool(a.pose)!=bool(b.pose))
        return false;
    if(a.pose) {
        if(a.pose->viewRotation!=b.pose->viewRotation)return false;
        for(std::size_t i=0u;i<3u;++i)
            if(Bits(a.pose->location[i])!=Bits(b.pose->location[i]))return false;
    }
    return true;
}
void Accept(const PortableStartupWeaponFireCommand& command) {
    const auto before=command;
    ValidatePortableStartupWeaponFireInput(command);
    Require(Same(command,before),"Input validation modified a valid command");++accepted;
}
void Refuse(const PortableStartupWeaponFireCommand& command) {
    const auto before=command;bool rejected{};
    try{ValidatePortableStartupWeaponFireInput(command);}catch(const std::runtime_error&){rejected=true;}
    Require(rejected,"Invalid Training Fire input was admitted");
    Require(Same(command,before),"Input refusal modified the command");++refusals;
}
std::size_t ObservationCapacity(const PortableStartupWeaponFireCommand& command) {
    ValidatePortableStartupWeaponFireInput(command);
    return static_cast<std::size_t>(command.attacks)*(static_cast<std::size_t>(command.frames)+1u)+
        (command.pose?2u:0u);
}
void ValidBoundaries() {
    PortableStartupWeaponFireCommand command;Accept(command);
    Require(command.deltaSeconds==0.025f&&command.frames==64u&&command.attacks==1u&&!command.pose,
        "Default diagnostic inputs changed");
    for(float delta:{std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),
        std::nextafter(0.1f,0.0f),0.1f}) {
        command.deltaSeconds=delta;Accept(command);
    }
    command={};command.pose.emplace();
    const auto minimum=std::numeric_limits<std::int32_t>::min();
    const auto maximum=std::numeric_limits<std::int32_t>::max();
    const std::array<float,10u> finite{0.0f,-0.0f,-1.0f,1.0f,
        std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),-std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()};
    for(std::size_t component=0u;component<3u;++component)for(float value:finite) {
        command.pose->location={0.0f,-0.0f,-123.5f};
        command.pose->location[component]=value;
        command.pose->viewRotation={minimum,maximum,-65537};Accept(command);
    }
    for(std::size_t component=0u;component<3u;++component)
        for(std::int32_t angle:{minimum,maximum,0,-1,1,-65537,65536}) {
            command.pose->viewRotation={0,0,0};command.pose->viewRotation[component]=angle;Accept(command);
        }
    for(unsigned mask=0u;mask<8u;++mask) {
        for(unsigned component=0u;component<3u;++component)
            command.pose->location[component]=(mask&(1u<<component))?-0.0f:0.0f;
        Accept(command);
        for(unsigned component=0u;component<3u;++component)
            Require(Bits(command.pose->location[component])==((mask&(1u<<component))?0x80000000u:0u),
                "Validation normalized a finite pose signed zero");
    }
    // These are input-admission controls only. Extreme finite positions and
    // raw signed32 rotations are NOT proof of a valid world placement/query.
}
void InvalidInputs() {
    PortableStartupWeaponFireCommand command;
    for(float delta:{0.0f,-0.0f,-std::numeric_limits<float>::denorm_min(),-1.0f,
        std::nextafter(0.1f,std::numeric_limits<float>::infinity()),std::numeric_limits<float>::max()}) {
        command.deltaSeconds=delta;Refuse(command);
    }
    const std::array<float,4u> nonfinite{std::bit_cast<float>(std::uint32_t{0x7fc12345u}),
        std::bit_cast<float>(std::uint32_t{0xffc54321u}),std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()};
    for(float value:nonfinite) {
        command={};command.deltaSeconds=value;Refuse(command);
        for(std::size_t component=0u;component<3u;++component) {
            command={};command.pose.emplace();command.pose->location={1,-2,3};
            command.pose->location[component]=value;Refuse(command);
        }
    }
    for(std::uint32_t frames:{0u,65u,256u,std::numeric_limits<std::uint32_t>::max()}) {
        command={};command.frames=frames;Refuse(command);
    }
    for(std::uint32_t attacks:{0u,5u,256u,std::numeric_limits<std::uint32_t>::max()}) {
        command={};command.attacks=attacks;Refuse(command);
    }
    command={};command.pose.emplace();command.pose->location={nonfinite[0],nonfinite[1],nonfinite[2]};Refuse(command);
    command={};command.frames=0u;command.attacks=0u;command.deltaSeconds=-0.0f;Refuse(command);
}
void CapacityBounds() {
    static_assert(std::numeric_limits<std::size_t>::max()>=262u);
    PortableStartupWeaponFireCommand command;std::size_t largest{};
    for(std::uint32_t attacks=1u;attacks<=4u;++attacks)
        for(std::uint32_t frames=1u;frames<=64u;++frames)for(bool pose:{false,true}) {
            command.attacks=attacks;command.frames=frames;
            if(pose)command.pose.emplace();else command.pose.reset();
            Accept(command);
            const auto capacity=ObservationCapacity(command);
            const auto expected=static_cast<std::uint64_t>(attacks)*(static_cast<std::uint64_t>(frames)+1u)+
                (pose?2u:0u);
            Require(capacity==expected&&capacity<=262u,"Validated observation count overflowed its262-row bound");
            Require(capacity>=2u,"Validated observation count lost a required Fire/frame row");
            if(capacity>largest)largest=capacity;
        }
    Require(largest==262u,"Maximum four-attack/pose observation capacity changed");
    command={};command.attacks=4u;command.pose.emplace();
    Require(ObservationCapacity(command)==262u,"Full command did not admit exactly262 possible observations");
    command.pose.reset();Require(ObservationCapacity(command)==260u,"Pose-free capacity reserved placement rows");
}
} // namespace
int main() {
    try {
        ValidBoundaries();InvalidInputs();CapacityBounds();
        std::cout<<"PASS Training Fire asset-free input checks="<<checks<<" accepted="<<accepted<<" refusals="<<refusals
            <<"; input validation/capacity only, not placement, original attacks, damage, loot, gravity or headset evidence\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL Training Fire input: "<<error.what()<<'\n';return 1;
    }
}
