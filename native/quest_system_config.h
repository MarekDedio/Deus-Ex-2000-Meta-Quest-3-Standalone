#pragma once

#include "quest_native_config.h"
#include "quest_native_config_import.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace QuestVr::SystemConfig {

// Typed payload only: Name is the caller's actual opaque FName identity. Its
// default value must be NAME_None; this helper does not create a name arena,
// UObject, native class/default, source-file binding or runtime service.
template<class Name>
struct Values {
    std::int32_t purgeCacheDays{};
    std::string savePath,cachePath,cacheExt;
    std::vector<std::string> paths;
    std::vector<Name> suppress;
};

namespace Detail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Native System config: ")+reason);
}
inline std::size_t Add(std::size_t left,std::size_t right) {
    if(right>std::numeric_limits<std::size_t>::max()-left)Fail("byte count overflow");
    return left+right;
}
inline std::size_t Multiply(std::size_t count,std::size_t bytes) {
    if(bytes&&count>std::numeric_limits<std::size_t>::max()/bytes)Fail("byte count overflow");
    return count*bytes;
}
}

// Includes vector/string capacities (conservatively including SSO), but not
// the independently owned FName arena. Ops::AddName must account for that arena.
template<class Name>
std::size_t RetainedBytes(const Values<Name>& values) {
    auto bytes=Detail::Add(sizeof(values),Detail::Multiply(values.paths.capacity(),sizeof(std::string)));
    bytes=Detail::Add(bytes,Detail::Multiply(values.suppress.capacity(),sizeof(Name)));
    for(const auto* text:{&values.savePath,&values.cachePath,&values.cacheExt})
        bytes=Detail::Add(bytes,Detail::Add(text->capacity(),1u));
    for(const auto& text:values.paths)bytes=Detail::Add(bytes,Detail::Add(text.capacity(),1u));
    return bytes;
}

namespace Detail {
template<class Ops>
struct Operation {
    using Name=typename Ops::Name;
    const NativeConfig::OrderedIni& document;
    Ops& ops;
    Values<Name>& values;
    std::size_t queryBytes{},peak{};

    void Step(std::size_t count=1u) {for(std::size_t i=0u;i<count;++i)ops.Step();}
    void Admit(std::size_t scratch=0u) {
        const auto bytes=Add(Add(RetainedBytes(values),queryBytes),scratch);
        if(bytes>peak) {ops.Admit(bytes);peak=bytes;}
    }
    void Query() {
        // OrderedIni owns its own bounded case-comparison guard. This joins
        // its record traversal to the enclosing runtime transaction's work.
        Step(Add(Add(document.EntryCount(),document.SectionCount()),1u));
        Admit();
    }
    std::size_t ImportScratch(std::size_t transportBytes) const {
        // Simultaneous Decode, ImportText UTF16 storage/token and Encode,
        // including the audited libraries' geometric capacities/SSO. Array
        // query copies remain separately covered by queryBytes throughout.
        return Add(8192u,Multiply(Add(transportBytes,1u),16u));
    }
    void String(std::string_view source,std::string& destination) {
        Step(Add(source.size(),1u));Admit(ImportScratch(source.size()));
        const auto units=UnrealString::Decode(source);std::u16string imported;
        const auto result=NativeConfigImport::String(units,0u,imported);
        if(result.returned)destination=UnrealString::Encode(imported);
        Admit();
    }
    std::optional<std::string> Scalar(std::string_view key) {
        Query();const auto raw=document.Scalar("Core.System",key);
        if(!raw)return std::nullopt;
        // PropertyScalar's GetString1024 truncation happens BEFORE ImportText.
        // Admit its full raw decode before asking it to allocate that decode.
        Admit(ImportScratch(raw->size()));Step(Add(raw->size(),1u));
        Query();return document.PropertyScalar("Core.System",key);
    }
    void ScalarString(std::string_view key,std::string& destination) {
        const auto source=Scalar(key);if(source)String(*source,destination);
    }
    void Suppress() {
        Query();const auto source=document.StringArray("Core.System","Suppress");
        if(!source)return; // Missing section retains the old array.
        Admit(Multiply(source->size(),Multiply(2u,sizeof(Name))));
        // Native destroys the old array, zeroes ALL replacement cells, then
        // imports in source order. An ordinary failed Name token leaves a
        // zero/None cell; it does not drop the entry or stop later imports.
        values.suppress=std::vector<Name>{};values.suppress.resize(source->size());
        for(std::size_t i=0u;i<source->size();++i) {
            const auto& text=(*source)[i];Step(Add(text.size(),1u));Admit(ImportScratch(text.size()));
            const auto units=UnrealString::Decode(text);
            static_cast<void>(NativeConfigImport::Name(units,values.suppress[i],
                [&](std::u16string_view spelling){return ops.AddName(spelling);}));
        }
        Admit();
    }
    void Paths() {
        Query();const auto source=document.StringArray("Core.System","Paths");
        if(!source)return;
        Admit(Multiply(source->size(),2u*sizeof(std::string)));
        values.paths=std::vector<std::string>{};values.paths.resize(source->size());
        for(std::size_t i=0u;i<source->size();++i)String((*source)[i],values.paths[i]);
        Admit();
    }
};
}

// One Register-time LoadConfig(Propagate0, ClassNone, FilenameNone) pass for
// the actual Core.System CDO. Its native six-field ConfigLink order is the
// reverse of Cpp creation: Suppress, Paths, CacheExt, CachePath, SavePath,
// PurgeCacheDays. No parent recursion, universal filename alias, fallback game
// defaults, instance constructor reload or later appInit name-flag OR occurs.
//
// Ops supplies Name, Step(), Admit(absolute additional peak bytes excluding the
// already retained document), and AddName(UTF16 CString)->actual Name. The
// caller owns source admission and any baseline argument copy before entry;
// pass a moved staged baseline when necessary. A refusal may follow real
// AddName prefix effects: an enclosing runtime journal owns rollback, not this
// pure returned-value helper. Ordinary ImportText failure is NOT a refusal.
template<class Ops>
Values<typename Ops::Name> Load(const NativeConfig::OrderedIni& document,Ops& ops,
    Values<typename Ops::Name> baseline={}) {
    // Covers OrderedIni's copied array/vector/name query storage before query
    // allocation. Per-value import scratch is admitted separately below.
    const auto queryBytes=Detail::Add(4096u,Detail::Multiply(document.RetainedBytes(),2u));
    Detail::Operation<Ops> operation{document,ops,baseline,queryBytes};
    operation.Admit();operation.Step();
    operation.Suppress();operation.Paths();
    operation.ScalarString("CacheExt",baseline.cacheExt);
    operation.ScalarString("CachePath",baseline.cachePath);
    operation.ScalarString("SavePath",baseline.savePath);
    if(const auto source=operation.Scalar("PurgeCacheDays")) {
        operation.Step(Detail::Add(source->size(),1u));operation.Admit(operation.ImportScratch(source->size()));
        const auto units=UnrealString::Decode(*source);
        static_cast<void>(NativeConfigImport::Integer(units,baseline.purgeCacheDays));
    }
    operation.Admit();return baseline;
}

} // namespace QuestVr::SystemConfig
