#pragma once
#include "quest_native_scalar.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {
using InputReadId=std::uintptr_t;
struct OriginalInputReadQueries {
    std::function<void()> validate,work;
    std::function<void(std::size_t)> enterFrame,leaveFrame;
    std::function<bool()> running;
    std::function<InputReadId()> acquireProperties;
    std::function<void(InputReadId)> releaseProperties; // noexcept, unmetered release.
    std::function<std::size_t(InputReadId)> propertyCount;
    std::function<InputReadId(InputReadId,std::size_t)> property;
    std::function<bool(InputReadId)> isFloat;
    std::function<std::uint8_t(std::int32_t)> key;
    // Original global output device is freshly selected for EACH Hold call.
    std::function<InputReadId()> globalOutput;
    std::function<void(InputReadId,std::int32_t,std::int32_t,float)> process;
    std::function<float(InputReadId)> readFloat;
    std::function<void(InputReadId,float)> writeFloat;
};
struct OriginalInputReadResult {bool running{},cacheAcquired{};std::size_t holds{},scaled{};};
namespace InputReadDetail {
template<class Q> struct Frame {
    const Q& q;static constexpr std::size_t Bytes=512u;
    explicit Frame(const Q& value):q(value){
        if(!q.validate||!q.work||!q.enterFrame||!q.leaveFrame)throw std::runtime_error("Original ReadInput requires actual root/work/frame authority");
        q.enterFrame(Bytes);
    }
    ~Frame()noexcept{q.leaveFrame(Bytes);}
};
template<class Q,class F,class... Args> auto Call(const Q& q,const F& f,Args&&... args){
    if(!f)throw std::runtime_error("Original ReadInput reached a missing provider");
    q.work();q.validate();return f(std::forward<Args>(args)...);
}
inline bool Polls(float elapsed)noexcept{return !std::isnan(elapsed)&&elapsed!=-1.0f;}
} // namespace InputReadDetail

// Engine10394580..10394847. Running false precedes cache/Actor acquisition.
// No neutral-value sanitization: source IEEE multiplication/scaling is retained.
inline OriginalInputReadResult ReadOriginalInput(float elapsed,const OriginalInputReadQueries& q){
    using namespace InputReadDetail;Frame frame(q);OriginalInputReadResult result;
    result.running=Call(q,q.running);if(!result.running)return result;
    if(!q.releaseProperties)throw std::runtime_error("Original ReadInput lacks its cache-release provider");
    const auto cache=Call(q,q.acquireProperties);
    if(!cache)throw std::runtime_error("Original ReadInput cache has no actual Class authority");
    struct Release {const OriginalInputReadQueries& q;InputReadId id;~Release()noexcept{q.releaseProperties(id);}} release{q,cache};
    result.cacheAcquired=true;
    if(Polls(elapsed))for(std::int32_t key=0;key<255;++key){
        if(!Call(q,q.key,key))continue;
        const auto output=Call(q,q.globalOutput);
        if(!output)throw std::runtime_error("Original ReadInput has no actual selected global output device");
        Call(q,q.process,output,key,2,elapsed);++result.holds;
    }
    const auto factor=Polls(elapsed)?NativeScalar::Divide(NativeScalar::Load(20.0),NativeScalar::Load(elapsed)):
        NativeScalar::Load(0.0);
    const auto count=Call(q,q.propertyCount,cache);
    for(std::size_t i=0u;i<count;++i){
        const auto field=Call(q,q.property,cache,i);
        if(!field)continue; // Actual cached null property is skipped by source.
        if(!Call(q,q.isFloat,field))continue;
        const auto current=Call(q,q.readFloat,field);
        Call(q,q.writeFloat,field,NativeScalar::StoreFloat(NativeScalar::Multiply(factor,NativeScalar::Memory(current))));
        ++result.scaled;
    }
    q.work();q.validate();return result;
}
struct OriginalViewportReadQueries {
    std::function<void()> validate,work;
    std::function<void(std::size_t)> enterFrame,leaveFrame;
    std::function<InputReadId()> input;
    std::function<void(std::int32_t)> updateInput;
    std::function<InputReadId()> globalOutput;
    std::function<void(InputReadId,float,InputReadId)> readInput;
};
inline void ReadOriginalViewportInput(float elapsed,const OriginalViewportReadQueries& q){
    using namespace InputReadDetail;Frame frame(q);
    if(!Call(q,q.input))throw std::runtime_error("Original viewport ReadInput requires its actual configured Input");
    if(Polls(elapsed))Call(q,q.updateInput,0);
    // Reload AFTER UpdateInput. The source separately reloads global output.
    const auto input=Call(q,q.input);const auto output=Call(q,q.globalOutput);
    if(!input||!output)throw std::runtime_error("Original viewport ReadInput lost Input/output after polling");
    Call(q,q.readInput,input,elapsed,output);q.work();q.validate();
}
} // namespace QuestVr
