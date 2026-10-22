#pragma once

#include "quest_native_config.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace QuestVr::DumpLocation {

// Owned DeusEx.dll100160C0..10016157: DLL-session state, not an instance
// default or a count of DMP files. Words preserve the original bitwise copies.
struct Session {
    bool saved{};
    std::array<std::uint32_t,3u> location{},view{};
    bool operator==(const Session&) const = default;
    void Save(const std::array<std::uint32_t,3u>& position,const std::array<std::uint32_t,3u>& rotation) noexcept {
        saved=true;location=position;view=rotation;
    }
    bool Load(std::array<std::uint32_t,3u>& position,std::array<std::uint32_t,3u>& rotation) noexcept {
        if(!saved)return false;
        saved=false;position=location;rotation=view;return true;
    }
};

inline constexpr std::size_t MaximumFiles=4096u,MaximumNameBytes=8192u,MaximumDirectoryBytes=1u<<20u;
inline constexpr std::size_t OriginalHelperBytes=0x1220u;
struct ConstructorData {
    std::string currentUser;
    std::vector<std::string> directory;
    std::int32_t fileIndex{-1},locationIndex{-1};
    std::int32_t locationCount{};
};

// Explicit embedding bindings, not recovered original launcher factories/CWD.
// Both providers are read-only. FindFiles must retain provider order and return
// filenames only, matching *.DMP / files=true / directories=false. Missing or
// malformed dependencies must throw, not manufacture an empty successful list.
using FindFiles=std::function<std::vector<std::string>(const std::string&,bool,bool)>;
inline ConstructorData Construct(const NativeConfig::OrderedIni& config,const FindFiles& files) {
    if(!files)throw std::runtime_error("DumpLocation constructor has no bound FindFiles provider");
    ConstructorData result;
    // Audited Editor GetString1021B651 initializes destination[0] before a
    // missing key; appStrncpy count64 retains at most63 UTF16 code units.
    const auto value=config.Scalar("DeusEx.DumpLocation","CurrentUser");
    auto units=UnrealString::Decode(value?*value:std::string{});
    if(units.size()>63u)units.resize(63u);
    result.currentUser=UnrealString::Encode(units);
    result.directory=files("*.DMP",true,false);
    if(result.directory.size()>MaximumFiles)throw std::runtime_error("DumpLocation directory count exceeds portable bound");
    std::size_t bytes=result.currentUser.capacity()+1u+result.directory.capacity()*sizeof(std::string);
    for(const auto& name:result.directory) {
        if(name.empty()||name=="."||name==".."||name.size()>MaximumNameBytes||name.find('\0')!=std::string::npos||
            name.find_first_of("/\\")!=std::string::npos)
            throw std::runtime_error("DumpLocation FindFiles returned an invalid filename");
        if(bytes>MaximumDirectoryBytes||name.capacity()+1u>MaximumDirectoryBytes-bytes)
            throw std::runtime_error("DumpLocation directory bytes exceed portable bound");
        bytes+=name.capacity()+1u;
    }
    if(bytes>MaximumDirectoryBytes)throw std::runtime_error("DumpLocation directory bytes exceed portable bound");
    return result;
}

} // namespace QuestVr::DumpLocation
