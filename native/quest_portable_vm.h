#pragma once

// Bounded execution of the loader's fixed-width UE1 bytecode. This is not a
// replacement script language: unsupported opcodes/natives fail transactionally.
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace QuestVr::Vm {
enum class Kind { Nothing, Byte, Int, Bool, Float, Name, Object, String, Vector, Rotator, Struct };
struct Value {
    Kind kind{Kind::Nothing};
    std::int32_t integer{};
    float floating{};
    bool boolean{};
    std::string text;
    std::array<float, 3> vector{};
    std::array<std::int32_t, 3> rotation{};
    std::map<std::string, Value> fields;
    static Value Integer(std::int32_t n);
    static Value Byte(std::uint8_t n);
    static Value Float(float n);
    static Value Bool(bool n);
    static Value Text(Kind kind, std::string text);
    static Value Vector(std::array<float, 3> n);
    static Value Rotator(std::array<std::int32_t, 3> n);
};
bool ToBool(const Value& value);
std::int32_t ToInt(const Value& value);
float ToFloat(const Value& value);
Value Coerce(const Value& value, const Value& typedZero);
bool Equal(const Value& a, const Value& b);

struct Property {
    std::string key; // Stable fully-qualified identity, not a package-local index.
    std::string name;
    Value zero;
    std::uint32_t flags{};
    std::size_t arrayDimension{1};
};
struct Function {
    std::string path;
    std::string source; // Reference/name table of THIS function, including nested calls.
    std::vector<std::uint8_t> bytecode;
    std::vector<Property> variables; // UStruct.Children -> UProperty.Next order.
    std::uint16_t nativeIndex{};
    std::uint32_t flags{};
};
struct Reference {
    Value zero;
    std::size_t dimension{1};
    std::function<Value()> read;
    std::function<void(const Value&)> write;
    std::function<std::shared_ptr<Reference>(std::size_t)> element;
};
struct Evaluation {
    Value value;
    std::shared_ptr<Reference> reference;
    Value Load() const { return reference ? reference->read() : value; }
};
enum class Scope { Instance, Default };
enum class CallKind { Virtual, Global, Final };
struct Invocation {
    CallKind kind{CallKind::Virtual};
    std::int32_t reference{};
    std::string name;
};
struct Limits {
    std::size_t sourceBytes{1u << 20u};
    std::size_t nodes{32768};
    std::size_t expressionDepth{64};
    std::size_t callDepth{32};
    std::size_t instructions{100000};
    std::size_t localElements{16384};
    std::size_t writes{16384};
    std::size_t stringBytes{8192};
    std::size_t arguments{256};
    std::size_t retainedBytes{32u << 20u};
};
class Host {
public:
    virtual ~Host() = default;
    // Begin/Commit/Rollback cover properties, native commands, RNG and effects
    // across the ENTIRE nested call tree. Unknown operations must throw, not no-op.
    virtual void Begin() = 0;
    virtual void Commit() = 0;
    virtual void Rollback() noexcept = 0;
    virtual Property ResolveProperty(const Function&, std::int32_t reference) = 0;
    virtual std::string ResolveName(const Function&, std::int32_t index) = 0;
    virtual std::string ResolveObject(const Function&, std::int32_t reference) = 0;
    virtual std::shared_ptr<const Function> ResolveFunction(const Function& caller,
        const std::string& receiver, const Invocation&) = 0;
    virtual std::shared_ptr<Reference> Variable(const std::string& receiver,
        const Property&, Scope) = 0;
    // Built-in numeric/vector operators are handled by the interpreter. These
    // arguments retain aliases for native out parameters. Native index 0 also
    // receives its declaration, since it is NOT a globally unique registration.
    // Native execution is synchronous; do not retain references for later work.
    virtual Evaluation Native(std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration) = 0;
};
enum class Status { Returned, Stopped, Unsupported, Invalid, Budget };
struct Result {
    Status status{Status::Invalid};
    Value value;
    std::size_t instructions{}, writes{};
    std::string function;
    std::size_t offset{};
    std::uint8_t opcode{};
    std::vector<std::string> callStack;
    std::string error;
    bool passed() const { return status == Status::Returned; }
};
Result Execute(Host& host, const Function& function, const std::string& self,
    const std::vector<Evaluation>& arguments = {}, const Limits& limits = {});
} // namespace QuestVr::Vm
