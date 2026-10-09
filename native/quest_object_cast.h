#pragma once

#include "quest_portable_vm.h"

#include <set>
#include <string_view>

// Asset-free evaluation of the pinned MetaCast/DynamicCast membership rules.
// Package-local target resolution happens in the caller before operand
// evaluation. These callbacks must resolve original identities, not fabricate
// classes from a path or a coincidentally matching FriendlyName.
namespace QuestVr {

struct ObjectCastClass {
    std::string path;     // Canonical identity of an actual UClass.
    std::string name;     // Literal UObject/export Name, not a path-derived leaf.
    std::string basePath; // Authored/native BaseStruct identity; empty at a root.
};
struct ObjectCastObject {
    std::string path;      // Canonical identity of the operand object.
    std::string classPath; // Actual Class/metaclass, including for class objects.
    bool classObject{};   // Actual UClass kind, not script IsA("Class").
};
struct ObjectCastResolvers {
    // Must reject missing/wrong-kind classes and supply exact native/serialized
    // base semantics. Returning an empty base for an unresolved base is invalid.
    std::function<ObjectCastClass(const std::string&)> resolveClass;
    std::function<ObjectCastObject(const std::string&)> resolveObject;
};

namespace ObjectCastDetail {
[[noreturn]] inline void Fail(const char* message) {
    throw std::runtime_error(std::string("Authored object cast: ")+message);
}
inline void Text(std::string_view text, bool empty=false) {
    if (text.size()>8192u || (!empty && text.empty()) || text.find('\0')!=std::string_view::npos)
        Fail("identity/name is empty, contains NUL or exceeds 8192 bytes");
}
// NameString::CompareIndex folds ASCII letters only; non-ASCII bytes remain
// distinct. Do not replace this with locale-sensitive or Unicode case folding.
inline std::string Fold(std::string_view text) {
    std::string result; result.reserve(text.size());
    for (const unsigned char c : text)
        result.push_back(static_cast<char>(c>='A' && c<='Z' ? c-'A'+'a' : c));
    return result;
}
inline ObjectCastClass Class(const std::string& path, const ObjectCastResolvers& resolvers) {
    Text(path);
    if (!resolvers.resolveClass) Fail("class resolver unavailable");
    auto cls=resolvers.resolveClass(path);
    Text(cls.path); Text(cls.name); Text(cls.basePath,true);
    if (Fold(cls.path)!=Fold(path)) Fail("class resolver changed requested identity");
    return cls;
}
} // namespace ObjectCastDetail

inline Vm::Value CastAuthoredObject(const std::string& target, const Vm::Value& source,
    const bool meta, const ObjectCastResolvers& resolvers, const std::size_t hierarchyLimit=128u) {
    using namespace ObjectCastDetail;
    // Resolve/validate the class even for null operands or a wrong value kind.
    // Null targets are rejected fail-closed: the pin dereferences them for a
    // nonnull DynamicCast operand, rather than defining a graceful result.
    const auto targetClass=Class(target,resolvers);
    if (hierarchyLimit==0u || hierarchyLimit>128u) Fail("hierarchy limit is outside 1..128");
    if (source.kind!=Vm::Kind::Object && source.kind!=Vm::Kind::Nothing)
        Fail("operand is not Object/Nothing");
    if (source.kind==Vm::Kind::Nothing || source.text.empty())
        return Vm::Value::Text(Vm::Kind::Object,{});
    Text(source.text);
    if (!resolvers.resolveObject) Fail("object resolver unavailable");
    const auto object=resolvers.resolveObject(source.text);
    Text(object.path); Text(object.classPath);
    if (Fold(object.path)!=Fold(source.text)) Fail("object resolver changed requested identity");
    if (meta && !object.classObject) return Vm::Value::Text(Vm::Kind::Object,{});

    // MetaCast walks the class represented BY the operand. DynamicCast instead
    // walks the operand's actual Class (Core.Class for an ordinary UClass).
    auto path=meta ? object.path : object.classPath;
    std::set<std::string> visited;
    for (std::size_t depth=0u; !path.empty(); ++depth) {
        if (depth>=hierarchyLimit) Fail("class hierarchy exceeds limit");
        const auto cls=Class(path,resolvers);
        if (!visited.insert(Fold(cls.path)).second) Fail("class hierarchy cycles");
        const bool matched=meta ? cls.path==targetClass.path : Fold(cls.name)==Fold(targetClass.name);
        // Preserve the pin's early match: a later malformed/cyclic base is not
        // traversed when the current class already satisfies the cast.
        if (matched) return Vm::Value::Text(Vm::Kind::Object,object.path);
        path=cls.basePath;
    }
    return Vm::Value::Text(Vm::Kind::Object,{});
}
} // namespace QuestVr
