#pragma once

// Bounded execution of the loader's fixed-width UE1 bytecode. This is not a
// replacement script language: unsupported opcodes/natives fail transactionally.
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace QuestVr { struct StateObject; }

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
struct StateLabel {
    std::string name;
    std::uint32_t offset{};
};
struct ProgramLayout {
    std::vector<std::size_t> statementOffsets;
    // Authored order and duplicates are retained. The None terminator is
    // consumed but omitted, matching pinned FindLabelIndex's label vector.
    std::vector<StateLabel> labels;
    bool terminalLabelTable{};
};
class Execution {
public:
    virtual ~Execution() = default;
    // Synchronous callbacks reuse the caller's interpreter, budgets and host
    // transaction. Never enter public Execute from a host native callback.
    virtual Value CallEvent(const std::string& receiver, const std::string& eventName,
        bool enumDispatch, const std::vector<Evaluation>& arguments = {}) = 0;
};
class Iterator {
public:
    virtual ~Iterator() = default;
    // Next writes its guarded OUT binding, including Object None on exhaustion.
    // It must bound its own native scan work; no asynchronous work is permitted.
    virtual bool Next() = 0;
    virtual std::size_t RetainedBytes() const = 0;
};
class Host {
public:
    virtual ~Host() = default;
    // Begin/Commit/Rollback cover properties, native commands, RNG and effects
    // across the ENTIRE nested call tree. Unknown operations must throw, not no-op.
    virtual void Begin() = 0;
    virtual void Commit() = 0;
    virtual void Rollback() noexcept = 0;
    // Read-only event/function eligibility, before any callee argument/local/
    // native work. The permissive default preserves existing host behavior.
    virtual bool CanCall(const Function&, const std::string&) { return true; }
    // Callable resolution may return only path/source identity. Preparation
    // follows argument evaluation and the fresh eligibility check.
    virtual std::shared_ptr<const Function> PrepareFunction(const Function& identity,
        const std::string&) { return std::make_shared<Function>(identity); }
    virtual std::shared_ptr<const Function> ResolveEvent(const std::string&,
        const std::string&, bool) { throw std::runtime_error("VM event dispatch unavailable"); }
    virtual Property ResolveProperty(const Function&, std::int32_t reference) = 0;
    virtual std::string ResolveName(const Function&, std::int32_t index) = 0;
    virtual std::string ResolveObject(const Function&, std::int32_t reference) = 0;
    // Decoding casts requires an actual UClass, using THIS function's source
    // table, before its child is decoded/evaluated. Structural analysis still
    // uses ResolveObject and does not perform this typed lookup.
    virtual std::string ResolveCastClass(const Function&, std::int32_t) {
        throw std::runtime_error("VM cast class resolution unavailable");
    }
    // Object input is detached and Nothing has already become a null Object.
    // meta=true compares exact UClass/BaseStruct identities; dynamic casts use
    // the target's NameString leaf along the object's actual Class ancestry.
    // Return only a detached Object: its unchanged input identity, or null.
    virtual Value CastObject(const std::string&, const Value&, bool) {
        throw std::runtime_error("VM object cast unavailable");
    }
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
    virtual Evaluation NativeWithExecution(std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration, Execution&) {
        return Native(index, receiver, arguments, declaration);
    }
    // Iterator factories may retain these guarded aliases only within the
    // enclosing synchronous VM frame. Their lifetime never crosses Execute /
    // ResumeState or a commit, and all Next writes share the caller's journal.
    virtual std::unique_ptr<Iterator> CreateIterator(std::uint16_t, const std::string&,
        const std::vector<Evaluation>&, const Function*) {
        throw std::runtime_error("VM native iterator unavailable");
    }
    virtual const QuestVr::StateObject* ReadState(const std::string&) { return nullptr; }
    // Storage identity only, not a transition-generation abort guard. Hosts
    // increment this whenever state locals are recreated, including A->B->A.
    virtual std::uint64_t StateLocalRevision(const std::string&) { return 0; }
    virtual QuestVr::StateObject* MutableState(const std::string&) {
        throw std::runtime_error("VM mutable state unavailable");
    }
    virtual std::shared_ptr<const Function> StateProgram(const std::string&, const std::string&) {
        throw std::runtime_error("VM state program unavailable");
    }
    virtual std::shared_ptr<Reference> StateVariable(const std::string&, const Property&) {
        throw std::runtime_error("VM state local storage unavailable");
    }
    // transition=false means an authored in-code goto: missing labels throw.
    // transition=true is GotoState positioning: None means Begin, miss stops.
    virtual void GotoStateLabel(const std::string&, const std::string&, bool) {
        throw std::runtime_error("VM state label control unavailable");
    }
};
// Structural, read-only inspection of normalized bytecode, not execution
// feasibility or a state continuation. Only a terminal top-level LabelTable
// contributes labels; targets must be top-level statement boundaries.
// May call ResolveName/ResolveObject, never transaction/effect/function lookup.
ProgramLayout AnalyzeProgram(Host& host, const Function& function, const Limits& limits = {});
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
    bool committed{};
    bool passed() const { return status == Status::Returned || (status == Status::Stopped && committed); }
};
Result Execute(Host& host, const Function& function, const std::string& self,
    const std::vector<Evaluation>& arguments = {}, const Limits& limits = {});
// Executes one persistent state slice. Unsupported waits fail explicitly; no
// actor tick, elapsed-time advance or world startup is implied by this call.
Result ResumeState(Host& host, const std::string& self, const Limits& limits = {});
} // namespace QuestVr::Vm
