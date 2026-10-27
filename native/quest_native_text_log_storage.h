#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace QuestVr {

// Portable ownership of the retail native Lines records, not a reflected
// DynamicArray or a container of UObject references.
struct NativeTextLogLine {
    float expiry{};
    std::string text;
    std::array<std::uint8_t,4u> color{};
};

struct NativeTextLogStorage {
    bool initialized{};
    std::vector<NativeTextLogLine> lines;

    NativeTextLogStorage()=default;
    NativeTextLogStorage(const NativeTextLogStorage& other):initialized(other.initialized) {
        lines.reserve(other.lines.capacity());
        if(lines.capacity()!=other.lines.capacity())
            throw std::runtime_error("Native TextLog copy cannot preserve vector capacity on this library");
        for(const auto& source:other.lines) {
            auto& line=lines.emplace_back();
            // Preserve Float storage, including every nonfinite IEEE word.
            std::memcpy(&line.expiry,&source.expiry,sizeof(line.expiry));
            line.color=source.color;
            // Construct at the old capacity before assigning the shorter
            // logical contents: reserve-after-copy may choose a larger growth
            // allocation and would not restore the actual native before-image.
            line.text=std::string(source.text.capacity(),'\0');
            line.text.assign(source.text.data(),source.text.size());
            if(line.text.capacity()!=source.text.capacity())
                throw std::runtime_error("Native TextLog copy cannot preserve string capacity on this library");
        }
    }
    NativeTextLogStorage(NativeTextLogStorage&&) noexcept=default;
    NativeTextLogStorage& operator=(const NativeTextLogStorage& other) {
        if(this!=&other) {NativeTextLogStorage copied(other);Swap(copied);}
        return *this;
    }
    NativeTextLogStorage& operator=(NativeTextLogStorage&&) noexcept=default;

    void Swap(NativeTextLogStorage& other) noexcept {
        std::swap(initialized,other.initialized);lines.swap(other.lines);
    }
    void Construct() noexcept {
        std::vector<NativeTextLogLine>{}.swap(lines);initialized=true;
    }
    void Clear() {
        if(!initialized)throw std::runtime_error("Native TextLog Lines is not constructed");
        std::vector<NativeTextLogLine>{}.swap(lines);
    }
    std::size_t RetainedBytes() const {
        auto bytes=Multiply(lines.capacity(),sizeof(NativeTextLogLine));
        for(const auto& line:lines)bytes=Add(bytes,Add(line.text.capacity(),1u));
        return bytes;
    }

private:
    static std::size_t Add(const std::size_t a,const std::size_t b) {
        if(b>std::numeric_limits<std::size_t>::max()-a)
            throw std::runtime_error("Native TextLog retained byte addition overflow");
        return a+b;
    }
    static std::size_t Multiply(const std::size_t count,const std::size_t size) {
        if(size&&count>std::numeric_limits<std::size_t>::max()/size)
            throw std::runtime_error("Native TextLog retained byte multiplication overflow");
        return count*size;
    }
};

} // namespace QuestVr
