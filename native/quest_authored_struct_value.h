#pragma once

#include "quest_portable_vm.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

// Asset-free decoding of UStructProperty's ordered member stream. The caller
// obtains this schema from authored metadata (base Properties, then Children /
// Next), not a host ABI, native struct sizeof, or a guessed property layout.
namespace QuestVr {

struct AuthoredStructSchema;
struct AuthoredStructField {
    std::string key;  // Fully qualified declaration identity; never wire data.
    std::string name;
    Vm::Value zero;   // Scalar kind, or the kind represented by nested.
    std::shared_ptr<const AuthoredStructSchema> nested;
    std::size_t arrayDimension{1u};
    bool classReference{};
    std::string referenceClassPath;
};
struct AuthoredStructSchema {
    std::string path;
    // UObject/export Name, not UStruct::FriendlyName or a path-derived guess.
    std::string name;
    Vm::Kind kind{Vm::Kind::Struct};
    std::vector<AuthoredStructField> fields; // Authored wire order, NOT map order.
};
struct AuthoredStructLimits {
    std::size_t inputBytes{1u << 20u};
    std::size_t valueNodes{32768u};
    std::size_t depth{32u};
    std::size_t stringBytes{8192u};
    std::size_t identityBytes{8192u};
    std::size_t retainedBytes{32u << 20u};
};
struct AuthoredStructResolvers {
    // Both callbacks are bound to the package containing the VALUE bytes, not
    // the struct declaration package. They must reject missing/wrong-type refs.
    std::function<std::string(std::int32_t)> resolveName;
    std::function<std::string(std::int32_t, const AuthoredStructField&)> resolveObject;
    // Optional conversion from bounded original code-page bytes. If absent,
    // preserve them verbatim; this codec does not guess UTF-8 or a code page.
    std::function<std::string(std::string_view)> decodeString;
};

namespace AuthoredStructDetail {
static_assert(sizeof(float)==4u && std::numeric_limits<float>::is_iec559,
    "Authored struct decoder requires IEEE binary32");
[[noreturn]] inline void Fail(const char* message) {
    throw std::runtime_error(std::string("Authored struct: ")+message);
}
inline std::string Fold(std::string_view text) {
    std::string result; result.reserve(text.size());
    for (const unsigned char c : text)
        result.push_back(static_cast<char>(c>='A' && c<='Z' ? c-'A'+'a' : c));
    return result;
}
inline void Text(std::string_view text, const AuthoredStructLimits& limits,
                 bool identity, bool empty=false) {
    if (text.size()>(identity ? limits.identityBytes : limits.stringBytes) || (!empty && text.empty()))
        Fail("invalid string length or string budget exceeded");
    for (const unsigned char c : text)
        if (c==0u || (identity && (c<33u || c>126u)))
            Fail("invalid identity or embedded NUL");
}
inline bool Aggregate(Vm::Kind kind) {
    return kind==Vm::Kind::Struct || kind==Vm::Kind::Vector || kind==Vm::Kind::Rotator;
}
struct Budget {
    const AuthoredStructLimits& limits;
    std::size_t nodes{}, retained{};
    void Retain(std::size_t bytes) {
        if (bytes>limits.retainedBytes || retained>limits.retainedBytes-bytes)
            Fail("retained value budget exceeded");
        retained+=bytes;
    }
    void Node(std::size_t depth) {
        if (depth>=limits.depth || nodes>=limits.valueNodes)
            Fail("value depth or node budget exceeded");
        ++nodes; Retain(sizeof(Vm::Value));
    }
};
// Cumulative accounting for concrete values retained by a batched read. Do not
// reset the budget for each slot: individually bounded structs can otherwise
// amplify into an unbounded result vector. This does not reinterpret values.
inline void RetainedValue(const Vm::Value& value, Budget& budget, std::size_t depth=0u) {
    budget.Node(depth);
    budget.Retain(value.text.capacity());
    if (value.fields.size()>budget.limits.valueNodes-budget.nodes)
        Fail("aggregate retained value field count exceeds node budget");
    for (const auto& [name,member] : value.fields) {
        budget.Retain(sizeof(std::string)+name.capacity()+64u);
        RetainedValue(member,budget,depth+1u);
    }
}
// Validate before constructing output values. The schema may contain cycles or
// shared children; each occurrence is charged because each produces a value.
inline void Validate(const AuthoredStructSchema& schema, Budget& budget,
                     std::size_t depth, std::vector<const AuthoredStructSchema*>& stack) {
    budget.Node(depth); Text(schema.path,budget.limits,true);
    Text(schema.name,budget.limits,true);
    budget.Retain(sizeof(AuthoredStructSchema)+schema.path.size()+schema.name.size());
    if (!Aggregate(schema.kind) || schema.fields.empty()) Fail("invalid or empty struct schema");
    if (schema.fields.size()>budget.limits.valueNodes) Fail("schema field count exceeds node budget");
    for (const auto* parent : stack) if (parent==&schema) Fail("cyclic struct schema");
    stack.push_back(&schema);
    std::set<std::string> names,keys;
    for (const auto& field : schema.fields) {
        Text(field.key,budget.limits,true); Text(field.name,budget.limits,true);
        Text(field.referenceClassPath,budget.limits,true,true);
        // Value.fields has scalar members, not array values. Top-level fixed
        // array slots are the host's responsibility; nested arrays are explicit
        // failures rather than consuming one element and silently discarding it.
        if (field.arrayDimension!=1u) Fail("nested array members are not representable");
        if (!names.insert(Fold(field.name)).second || !keys.insert(Fold(field.key)).second)
            Fail("duplicate canonical struct field identity");
        budget.Retain(sizeof(AuthoredStructField)+field.name.size()+field.key.size()+
            field.referenceClassPath.size()+64u);
        if (field.zero.kind!=Vm::Kind::Object &&
            (field.classReference || !field.referenceClassPath.empty()))
            Fail("object reference constraint on a non-object member");
        if (Aggregate(field.zero.kind)) {
            if (!field.nested || field.nested->kind!=field.zero.kind)
                Fail("missing or mismatched nested schema");
            Validate(*field.nested,budget,depth+1u,stack);
        } else {
            if (field.nested) Fail("nested schema on a scalar member");
            budget.Node(depth+1u);
            switch (field.zero.kind) {
            case Vm::Kind::Byte: case Vm::Kind::Int: case Vm::Kind::Float:
            case Vm::Kind::Bool: case Vm::Kind::Name: case Vm::Kind::Object:
            case Vm::Kind::String: break;
            default: Fail("unsupported struct member kind");
            }
            if (field.zero.kind==Vm::Kind::Name) budget.Retain(4u); // "None" in typed zero.
        }
    }
    if (schema.kind==Vm::Kind::Vector || schema.kind==Vm::Kind::Rotator) {
        if (schema.fields.size()!=3u) Fail("specialized struct requires three scalar members");
        const bool vector=schema.kind==Vm::Kind::Vector;
        const std::set<std::string> expected=vector ? std::set<std::string>{"x","y","z"} :
            std::set<std::string>{"pitch","yaw","roll"};
        if (names!=expected) Fail("specialized struct member names do not match");
        for (const auto& field : schema.fields)
            if (field.zero.kind!=(vector ? Vm::Kind::Float : Vm::Kind::Int))
                Fail("specialized struct member kind does not match");
    }
    stack.pop_back();
}
inline Vm::Value Scalar(Vm::Kind kind) {
    Vm::Value result; result.kind=kind;
    if (kind==Vm::Kind::Name) result.text="None";
    return result;
}
inline Vm::Value Zero(const AuthoredStructSchema& schema) {
    auto result=Scalar(schema.kind);
    if (schema.kind==Vm::Kind::Struct)
        for (const auto& field : schema.fields)
            result.fields.emplace(Fold(field.name),field.nested ? Zero(*field.nested) : Scalar(field.zero.kind));
    return result;
}

class Reader {
public:
    Reader(const std::uint8_t* data,std::size_t size,int version,
           const AuthoredStructResolvers& resolvers,const AuthoredStructLimits& limits)
        : data_(data),size_(size),version_(version),resolvers_(resolvers),limits_(limits) {
        if (size>limits.inputBytes || (size!=0u && data==nullptr)) Fail("invalid input or input budget exceeded");
    }
    Vm::Value Read(const AuthoredStructSchema& schema) {
        auto result=ReadStruct(schema);
        if (pos_!=size_) Fail("trailing bytes after struct value");
        return result;
    }
private:
    const std::uint8_t* data_;
    std::size_t size_,pos_{};
    int version_;
    const AuthoredStructResolvers& resolvers_;
    const AuthoredStructLimits& limits_;
    std::size_t textRetained_{};
    void Require(std::size_t count) {
        if (count>size_-pos_) Fail("truncated struct member stream");
    }
    std::uint8_t U8() { Require(1u); return data_[pos_++]; }
    std::uint32_t U32() {
        Require(4u); std::uint32_t result{};
        for (unsigned i=0; i<4u; ++i) result|=static_cast<std::uint32_t>(data_[pos_++])<<(i*8u);
        return result;
    }
    std::int32_t I32() {
        const auto bits=U32(); std::int32_t result{};
        std::memcpy(&result,&bits,sizeof(result)); return result;
    }
    float F32() {
        const auto bits=U32(); float result{}; std::memcpy(&result,&bits,sizeof(result));
        if (!std::isfinite(result)) Fail("non-finite float member");
        return result;
    }
    std::int32_t Index() {
        const auto first=U8(); const bool negative=(first&0x80u)!=0u;
        std::uint64_t magnitude=first&0x3fu;
        bool more=(first&0x40u)!=0u;
        for (unsigned shift=6u; more; shift+=7u) {
            if (shift>27u) Fail("compact index is overlong");
            const auto next=U8(); magnitude|=static_cast<std::uint64_t>(next&0x7fu)<<shift;
            more=(next&0x80u)!=0u;
        }
        const auto cap=negative ? 0x80000000ull : 0x7fffffffull;
        if (magnitude>cap) Fail("compact index overflow");
        if (negative && magnitude==0x80000000ull) return std::numeric_limits<std::int32_t>::min();
        const auto result=static_cast<std::int32_t>(magnitude);
        return negative ? -result : result;
    }
    std::string CheckedText(std::string text,bool identity,bool empty=false) {
        Text(text,limits_,identity,empty);
        if (text.size()>limits_.retainedBytes || textRetained_>limits_.retainedBytes-text.size())
            Fail("aggregate resolved string budget exceeded");
        textRetained_+=text.size(); return text;
    }
    std::string String() {
        std::size_t count{};
        if (version_>=64) {
            const auto length=Index();
            if (length<0) Fail("negative package string length is unsupported by the pinned loader");
            count=static_cast<std::size_t>(length); Require(count);
            if (count>limits_.stringBytes && count-limits_.stringBytes>1u)
                Fail("encoded string budget exceeded");
        } else {
            const auto begin=pos_;
            while (true) {
                if (pos_-begin>limits_.stringBytes) Fail("encoded string budget exceeded");
                if (U8()==0u) break;
            }
            count=pos_-begin; pos_=begin;
        }
        const auto begin=pos_; pos_+=count;
        // ReadString appends a NUL before constructing std::string, so a stored
        // terminator is not mandatory and an early NUL truncates the text while
        // all declared bytes are consumed. Keep those precise pinned semantics.
        std::size_t length{};
        while (length<count && data_[begin+length]!=0u) ++length;
        const std::string_view raw(length==0u ? "" : reinterpret_cast<const char*>(data_+begin),length);
        if (resolvers_.decodeString) return CheckedText(resolvers_.decodeString(raw),false,true);
        return CheckedText(std::string(raw),false,true);
    }
    Vm::Value ReadField(const AuthoredStructField& field) {
        if (field.nested) return ReadStruct(*field.nested);
        auto result=Scalar(field.zero.kind);
        switch (field.zero.kind) {
        case Vm::Kind::Byte: result.integer=U8(); break;
        case Vm::Kind::Int: result.integer=I32(); break;
        case Vm::Kind::Float: result.floating=F32(); break;
        case Vm::Kind::Bool: result.boolean=U8()==1u; break;
        case Vm::Kind::Name: {
            const auto index=Index();
            if (index<0 || !resolvers_.resolveName) Fail("invalid name index or missing name resolver");
            // Name-table values need not be property/path identifiers: authored
            // tags can contain spaces or original code-page characters.
            result.text=CheckedText(resolvers_.resolveName(index),false); break;
        }
        case Vm::Kind::Object: {
            const auto index=Index();
            if (index!=0) {
                if (!resolvers_.resolveObject) Fail("missing object resolver");
                result.text=CheckedText(resolvers_.resolveObject(index,field),true);
            }
            break;
        }
        case Vm::Kind::String: result.text=String(); break;
        default: Fail("unsupported struct member kind");
        }
        return result;
    }
    Vm::Value ReadStruct(const AuthoredStructSchema& schema) {
        auto result=Scalar(schema.kind);
        for (const auto& field : schema.fields) {
            auto member=ReadField(field); const auto name=Fold(field.name);
            if (schema.kind==Vm::Kind::Struct) result.fields.emplace(name,std::move(member));
            else if (schema.kind==Vm::Kind::Vector) {
                const auto index=name=="x" ? 0u : name=="y" ? 1u : 2u;
                result.vector[index]=member.floating;
            } else {
                const auto index=name=="pitch" ? 0u : name=="yaw" ? 1u : 2u;
                result.rotation[index]=member.integer;
            }
        }
        return result;
    }
};
} // namespace AuthoredStructDetail

inline void ValidateAuthoredStructTag(const AuthoredStructSchema& schema,
                                     std::string_view tagName,
                                     const AuthoredStructLimits& limits={}) {
    AuthoredStructDetail::Text(schema.name,limits,true);
    AuthoredStructDetail::Text(tagName,limits,true);
    // Pinned UStructProperty::LoadValue compares Struct->Name with NameString
    // case-insensitive semantics. FriendlyName is independent metadata.
    if (AuthoredStructDetail::Fold(schema.name)!=AuthoredStructDetail::Fold(tagName))
        AuthoredStructDetail::Fail("tag does not match the declaration export Name");
}

inline Vm::Value MakeAuthoredStructZero(const AuthoredStructSchema& schema,
                                       const AuthoredStructLimits& limits={}) {
    AuthoredStructDetail::Budget budget{limits};
    std::vector<const AuthoredStructSchema*> stack;
    AuthoredStructDetail::Validate(schema,budget,0u,stack);
    return AuthoredStructDetail::Zero(schema);
}
inline Vm::Value DecodeAuthoredStructValue(const AuthoredStructSchema& schema,
    const std::uint8_t* data,std::size_t size,int packageVersion,
    const AuthoredStructResolvers& resolvers={},const AuthoredStructLimits& limits={}) {
    AuthoredStructDetail::Budget budget{limits};
    std::vector<const AuthoredStructSchema*> stack;
    AuthoredStructDetail::Validate(schema,budget,0u,stack);
    // Structural memory and resolved value strings share a single total bound.
    auto remaining=limits; remaining.retainedBytes-=budget.retained;
    return AuthoredStructDetail::Reader(data,size,packageVersion,resolvers,remaining).Read(schema);
}
inline Vm::Value DecodeAuthoredStructValue(const AuthoredStructSchema& schema,
    const std::vector<std::uint8_t>& bytes,int packageVersion,
    const AuthoredStructResolvers& resolvers={},const AuthoredStructLimits& limits={}) {
    return DecodeAuthoredStructValue(schema,bytes.data(),bytes.size(),packageVersion,resolvers,limits);
}
} // namespace QuestVr
