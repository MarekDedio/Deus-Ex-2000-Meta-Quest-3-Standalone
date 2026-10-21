#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace QuestVr {

// Original Core1014FDB0..1014FE9F, native UObject virtual Destroy(+24),
// NOT UnrealScript Destroyed, a destructor, an object-index release or unhash.
// Providers bind actual live storage/services. The optional work callback is
// borrowed from the enclosing execution root; these helpers own no transaction
// or private allowance. Each provider is required only if its path is reached.
struct OriginalConditionalDestroyQueries {
    std::function<std::int32_t(std::uintptr_t)> objectIndex;
    std::function<std::uint32_t(std::uintptr_t)> objectFlags;
    std::function<void(std::uintptr_t,std::uint32_t)> writeObjectFlags;
    std::function<void(std::uintptr_t)> nativeDestroy;
    std::function<std::string(std::uintptr_t)> fullName;
    std::function<void(const std::string&)> logMissingDebugDestroy;
    std::function<void()> work;
};

// Engine10368F60..10368FBC. Auxiliary is the authenticated native Actor+314
// storage, not a reflected None/rendering inference. The selected auxiliary
// ConditionalDestroy result is ignored; its provider failure is NOT ignored.
struct OriginalActorDestroyQueries {
    std::function<std::uintptr_t(std::uintptr_t)> auxiliary;
    std::function<std::int32_t(std::uintptr_t)> conditionalDestroy;
    std::function<void(std::uintptr_t)> clearAuxiliary;
    std::function<void(std::uintptr_t)> baseObjectDestroy;
    std::function<void()> work;
};

// Core1014FA50..1014FAD3. ExitProperties must traverse actual ConstructorLink
// and selected native DestroyValue; SetLinker must detach the live export slot.
// Neither service may erase authored provenance or generic script overlays.
struct OriginalObjectDestroyQueries {
    std::function<std::uint32_t(std::uintptr_t)> objectFlags;
    std::function<void(std::uintptr_t,std::uint32_t)> writeObjectFlags;
    std::function<std::uintptr_t(std::uintptr_t)> actualClass;
    std::function<void(std::uintptr_t,std::uintptr_t)> exitProperties;
    std::function<void(std::uintptr_t,std::uintptr_t,std::int32_t)> setLinker;
    std::function<std::uintptr_t(std::uintptr_t)> outer;
    std::function<std::int32_t(std::uintptr_t)> objectIndex;
    // The same native Object+14 storage first holds a live linker index, then
    // the current Outer's object index for later destructor hash removal.
    std::function<void(std::uintptr_t,std::int32_t)> writeLinkerIndex;
    std::function<void()> work;
};

// Complete supported normal SetLinker body, Core1014FB20..1014FBC0. The old
// Linker.ExportMap[index].Object is real live attachment storage. exportObject
// is read twice, matching the nonnull assertion then the equality assertion;
// clearExportObject addresses the retained slot, not a later source lookup.
// New binding writes do not attach an export slot (the original does not).
struct OriginalSetLinkerQueries {
    std::function<std::uintptr_t(std::uintptr_t)> linker;
    std::function<std::int32_t(std::uintptr_t)> linkerIndex;
    std::function<std::uintptr_t(std::uintptr_t,std::int32_t)> exportObject;
    std::function<void(std::uintptr_t,std::int32_t)> clearExportObject;
    std::function<void(std::uintptr_t,std::uintptr_t)> writeLinker;
    std::function<void(std::uintptr_t,std::int32_t)> writeLinkerIndex;
    std::function<void()> work;
};

namespace ObjectDestroyDetail {
template<class Queries,class Function,class... Arguments>
inline auto Call(const Queries& query,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original object destruction lacks a reached native provider");
    if(query.work)query.work();
    return function(std::forward<Arguments>(arguments)...);
}
inline void RequireObject(std::uintptr_t object) {
    if(!object)throw std::invalid_argument("Original object destruction requires a nonnull native UObject");
}
} // namespace ObjectDestroyDetail

inline std::int32_t ConditionalDestroyOriginalObject(std::uintptr_t object,
    const OriginalConditionalDestroyQueries& query) {
    using ObjectDestroyDetail::Call;
    ObjectDestroyDetail::RequireObject(object);
    if(Call(query,query.objectIndex,object)==-1)return 0;
    const auto flags=Call(query,query.objectFlags,object);
    if(flags&0x00800000u)return 0;
    // In particular clear a PREEXISTING DebugDestroy marker before dispatch.
    Call(query,query.writeObjectFlags,object,(flags&0x7f7fffffu)|0x00800000u);
    Call(query,query.nativeDestroy,object);
    if(!(Call(query,query.objectFlags,object)&0x80000000u)) {
        const auto name=Call(query,query.fullName,object);
        Call(query,query.logMissingDebugDestroy,name);
    }
    return 1; // Missing-Super diagnostic is not an assertion/false return.
}

inline void DestroyOriginalActor(std::uintptr_t object,const OriginalActorDestroyQueries& query) {
    using ObjectDestroyDetail::Call;
    ObjectDestroyDetail::RequireObject(object);
    const auto auxiliary=Call(query,query.auxiliary,object);
    if(auxiliary) {
        static_cast<void>(Call(query,query.conditionalDestroy,auxiliary));
        Call(query,query.clearAuxiliary,object);
    }
    Call(query,query.baseObjectDestroy,object);
}

inline void SetOriginalObjectLinker(std::uintptr_t object,std::uintptr_t newLinker,
    std::int32_t newIndex,const OriginalSetLinkerQueries& query) {
    using ObjectDestroyDetail::Call;
    ObjectDestroyDetail::RequireObject(object);
    const auto oldLinker=Call(query,query.linker,object);
    if(oldLinker) {
        const auto oldIndex=Call(query,query.linkerIndex,object);
        if(!Call(query,query.exportObject,oldLinker,oldIndex))
            throw std::runtime_error("Original SetLinker old export attachment is null");
        if(Call(query,query.exportObject,oldLinker,oldIndex)!=object)
            throw std::runtime_error("Original SetLinker old export attachment differs from object");
        Call(query,query.clearExportObject,oldLinker,oldIndex);
    }
    Call(query,query.writeLinker,object,newLinker);
    Call(query,query.writeLinkerIndex,object,newIndex);
}

inline void DestroyOriginalObject(std::uintptr_t object,const OriginalObjectDestroyQueries& query) {
    using ObjectDestroyDetail::Call;
    ObjectDestroyDetail::RequireObject(object);
    const auto flags=Call(query,query.objectFlags,object);
    Call(query,query.writeObjectFlags,object,flags|0x80000000u);
    const auto actualClass=Call(query,query.actualClass,object);
    Call(query,query.exitProperties,object,actualClass);
    Call(query,query.setLinker,object,std::uintptr_t{},-1);
    const auto currentOuter=Call(query,query.outer,object);
    const auto cachedIndex=currentOuter?Call(query,query.objectIndex,currentOuter):0;
    Call(query,query.writeLinkerIndex,object,cachedIndex);
}

} // namespace QuestVr
