#pragma once

#include "quest_native_work.h"

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
enum class Kind { Nothing, Byte, Int, Bool, Float, Name, Object, String, Vector, Rotator, Struct, IntArray };
struct Value {
    Kind kind{Kind::Nothing};
    std::int32_t integer{};
    float floating{};
    bool boolean{};
    std::string text;
    std::array<float, 3> vector{};
    std::array<std::int32_t, 3> rotation{};
    // Whole native INT-array values only. This does not establish script
    // dynamic indexing or support arrays with any other element type.
    std::vector<std::int32_t> intArray;
    std::map<std::string, Value> fields;
    static Value Integer(std::int32_t n);
    static Value Byte(std::uint8_t n);
    static Value Float(float n);
    static Value Bool(bool n);
    static Value Text(Kind kind, std::string text);
    static Value Vector(std::array<float, 3> n);
    static Value Rotator(std::array<std::int32_t, 3> n);
    static Value IntArray(std::vector<std::int32_t> values);
};
bool ToBool(const Value& value);
std::int32_t ToInt(const Value& value);
float ToFloat(const Value& value);
Value Coerce(const Value& value, const Value& typedZero);
bool Equal(const Value& a, const Value& b);

// Trusted reflected destination capability, never inferred from Value shape.
// CoreColor4 denotes the audited Core.Object.Color four packed Byte channels;
// CoreVector12 denotes the audited Core.Object.Vector three binary32 channels.
enum class ResultLayout { Unknown, CoreColor4, CoreVector12, CoreRotator12 };
struct Property {
    std::string key; // Stable fully-qualified identity, not a package-local index.
    std::string name;
    Value zero;
    std::uint32_t flags{};
    std::size_t arrayDimension{1};
    ResultLayout resultLayout{ResultLayout::Unknown};
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
    ResultLayout resultLayout{ResultLayout::Unknown};
};
// A null Context writes encoded bytes only when an actual result destination
// exists. Keep this separate from reflected lvalue/address state: a missing
// array base does not inherit its Context's zero-fill into the outer result.
enum class ResultDisposition { Written, NoWrite, ZeroFill };
struct Evaluation {
    Value value;
    std::shared_ptr<Reference> reference;
    ResultDisposition disposition{ResultDisposition::Written};
    std::uint8_t zeroFillBytes{};
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
    // Explicit synchronous entry attempts, including absent/ineligible events.
    // Bytecode instructions and all other execution budgets remain shared.
    std::size_t operations{100000};
    // Separate from reflected local/fixed-array elements; includes capacity.
    std::size_t intArrayElements{262144};
    // Shared native/iterator/lifecycle work, separate from VM instructions and
    // fixed collision-table initialization. Zero is valid for pure roots.
    // Larger roots opt in explicitly; NativeWork::MaximumLimit is enforced.
    std::size_t nativeWork{NativeWork::DefaultLimit};
};
struct StateLabel {
    std::string name;
    std::uint32_t offset{};
};
// Detached structural AST metadata, in preorder. Child indices refer into the
// same ProgramLayout::expressions vector. No evaluation or typed field/call
// resolution occurs while producing this snapshot.
struct ExpressionLayout {
    std::size_t offset{};
    std::uint8_t opcode{};
    std::int32_t reference{};
    std::uint16_t target{}, native{};
    std::uint8_t zeroFillSize{};
    std::vector<std::size_t> children;
};
struct ProgramLayout {
    std::vector<std::size_t> statementOffsets;
    std::vector<ExpressionLayout> expressions;
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
    // Calls an actual host-resolved declaration, preserving its own source table
    // and the same eligibility/preparation rules as an ordinary nested call.
    // The declaration must remain alive until this synchronous call returns.
    virtual Value CallFunction(const Function& identity, const std::string& receiver,
        const std::vector<Evaluation>& arguments = {}) = 0;
    // Trusted synchronous host writes to raw host-owned references use the same
    // write/storage budgets and rollback journal as bytecode assignments. A
    // failed write poisons the enclosing transaction even if its caller catches
    // the exception. Do not pass already guarded native OUT references: their
    // write closures already perform this accounting. Closures/backing storage
    // must survive through transaction rollback, just like external OUT aliases.
    virtual void WriteReference(const std::shared_ptr<Reference>& reference,
        const Value& value) = 0;
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
    // Delivered once at the outer execution boundary, before any operation.
    // Legacy hosts retain their original Begin implementation. Hosts owning
    // native services must share this allowance through the whole call tree.
    virtual void BeginRoot(const Limits&) { Begin(); }
    virtual std::optional<NativeWork::Usage> ReadNativeWork() const noexcept { return std::nullopt; }
    virtual void Commit() = 0;
    virtual void Rollback() noexcept = 0;
    // Read-only event/function eligibility, before any callee argument/local/
    // native work. The permissive default preserves existing host behavior.
    virtual bool CanCall(const Function&, const std::string&) { return true; }
    // Script callable resolution may retain only path/source identity. Native
    // declarations also retain immutable flags/index (needed before arguments
    // for lazy operators and entry capture). Preparation follows argument
    // evaluation and the fresh eligibility check; it cannot swap native identity.
    virtual std::shared_ptr<const Function> PrepareFunction(const Function& identity,
        const std::string&) { return std::make_shared<Function>(identity); }
    // Immutable declaration layout for actual argument result destinations.
    // No argument evaluation, eligibility/state mutation or native work occurs.
    virtual std::vector<Property> FunctionParameters(const Function& identity) {
        std::vector<Property> parameters;
        for (const auto& property:identity.variables)
            if ((property.flags&0x80u) && !(property.flags&0x400u)) parameters.push_back(property);
        return parameters;
    }
    // An audited native wrapper may initialize its argument result temporary
    // to something other than the declaration's zero (e.g. cursor hotspots
    // start at -1). Capture detached, read-only seeds for ALL supplied argument
    // slots before evaluating ANY argument. Do not mutate host state, expose
    // references, or resolve a seed from state changed by an earlier argument.
    // No seed preserves the existing declared/builtin result destination.
    virtual std::optional<Value> NativeArgumentSeed(std::uint16_t,
        const std::string&, std::size_t, const Function*) { return std::nullopt; }
    // Audited wrapper temporaries may have NO initialized bytes. Capture this
    // immutable, read-only layout authority for ALL supplied native arguments
    // before ANY argument expression/read. Never infer it from a seed's shape.
    // An engaged layout requires a complete written value or exact full-layout
    // Context zero-fill; Nothing, NoWrite, zero-size/partial writes and seeds
    // cannot supply an initial value. Unknown layouts fail closed. No callbacks
    // or host mutations are permitted. No capability preserves legacy behavior.
    virtual std::optional<ResultLayout> NativeArgumentUninitializedLayout(std::uint16_t,
        const std::string&, std::size_t, const Function*) { return std::nullopt; }
    // Opt-in for wrappers that initialize a later temporary from an already
    // evaluated ordinary input. Queried before argument expressions; only
    // opted-in natives allocate detached prefix snapshots or call the hook.
    // Both hooks are read-only: no host mutation or VM execution/write callback.
    virtual bool HasSequentialNativeArgumentSeeds(std::uint16_t,
        const std::string&, const Function*) { return false; }
    // Narrow opt-in for an audited wrapper whose initialized input temporary
    // consumes EX_Nothing at Step time, rather than deferring an omission to
    // the eventual native handler. Capture this immutable capability for ALL
    // supplied slots before ANY argument is evaluated/read. The initialized
    // value still comes from that slot's sequential/entry/builtin seed; a
    // declaration zero alone is not authority. Hooks are read-only and must
    // not execute VM callbacks or mutate host state. Other natives preserve
    // their explicit Nothing omission markers.
    virtual bool MaterializeNativeArgumentOmission(std::uint16_t,
        const std::string&, std::size_t, const Function*) { return false; }
    // Called immediately BEFORE each supplied argument expression. The prefix
    // contains detached values captured immediately AFTER preceding arguments,
    // never references, and retains explicit Nothing omission markers unless
    // MaterializeNativeArgumentOmission opted that slot into Step-time defaults. A seed
    // overrides the entry seed for this slot only; no seed preserves the entry
    // seed or declared/builtin destination. Do not retain the prefix reference.
    virtual std::optional<Value> SequentialNativeArgumentSeed(std::uint16_t,
        const std::string&, std::size_t, const Function*, const std::vector<Value>&) {
        return std::nullopt;
    }
    virtual std::shared_ptr<const Function> ResolveEvent(const std::string&,
        const std::string&, bool) { throw std::runtime_error("VM event dispatch unavailable"); }
    virtual Property ResolveProperty(const Function&, std::int32_t reference) = 0;
    virtual std::string ResolveName(const Function&, std::int32_t index) = 0;
    virtual std::string ResolveObject(const Function&, std::int32_t reference) = 0;
    // Validate an actual loaded UClass and return its distinct class-owned
    // default-object handle. This is neither the UClass identity nor an actor
    // export/birth. The host owns its lifetime and reflected/default storage.
    virtual std::string ClassDefaultObject(const std::string&) {
        throw std::runtime_error("ClassContext requires class-default object identity");
    }
    // Original ObjectToString uses full UObject::GetPathName, not an operand's
    // unvalidated spelling. Null is handled by the VM before this lookup.
    virtual std::string ObjectString(const std::string&) {
        throw std::runtime_error("VM object path conversion unavailable");
    }
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
    // A detached, read-only native-entry snapshot taken BEFORE argument
    // expressions. Nested calls own independent snapshots; hosts must not
    // materialize/mutate a state frame while capturing this context.
    virtual std::optional<Value> CaptureNativeContext(std::uint16_t, const std::string&, const Function*) {
        return std::nullopt;
    }
    // The executing caller declaration is distinct from the called native's
    // declaration and receiver. Eval supplies its real script/state frame;
    // a top-level direct entry has no caller and passes nullptr. This hook is
    // captured before argument evaluation, and preserves legacy hosts through
    // the default delegation. Never infer a missing caller from the callee.
    virtual std::optional<Value> CaptureNativeCallSiteContext(std::uint16_t index,
        const std::string& receiver, const Function* declaration, const Function*) {
        return CaptureNativeContext(index,receiver,declaration);
    }
    virtual Evaluation NativeWithContext(std::uint16_t index, const std::string& receiver,
        const std::vector<Evaluation>& arguments, const Function* declaration, Execution& execution,
        const std::optional<Value>&) {
        return NativeWithExecution(index, receiver, arguments, declaration, execution);
    }
    // Iterator factories may retain these guarded aliases only within the
    // enclosing synchronous VM frame. Their lifetime never crosses Execute /
    // ResumeState or a commit, and all Next writes share the caller's journal.
    virtual std::unique_ptr<Iterator> CreateIterator(std::uint16_t, const std::string&,
        const std::vector<Evaluation>&, const Function*) {
        throw std::runtime_error("VM native iterator unavailable");
    }
    virtual const QuestVr::StateObject* ReadState(const std::string&) { return nullptr; }
    // ProcessState rechecks deletion between statements. Role/simulated-state
    // eligibility belongs to the initial poll boundary, not this live guard.
    virtual bool IsStateDeleted(const std::string&) { return false; }
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
    // Actor.ProcessState's native poll, not a full Actor.Tick. False means
    // no executable/eligible actor frame; do not manufacture a continuation.
    virtual bool PollState(const std::string&, float, Execution&) {
        throw std::runtime_error("VM native state polling unavailable");
    }
};
// Structural, read-only inspection of normalized bytecode, not execution
// feasibility or a state continuation. Only a terminal top-level LabelTable
// contributes labels; targets must be top-level statement boundaries.
// Non-default Case links target top-level Case labels; a top-level Switch must
// be immediately followed by a Case. Cyclic links are bounded at execution.
// May call ResolveName/ResolveObject, never transaction/effect/function lookup.
ProgramLayout AnalyzeProgram(Host& host, const Function& function, const Limits& limits = {});
enum class Status { Returned, Stopped, Unsupported, Invalid, Budget, Waiting };
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
    // Ephemeral final root accounting, including refused/rolled-back roots;
    // absent for legacy hosts. It is not a persistent/save-state field.
    std::optional<NativeWork::Usage> nativeWork;
    bool passed() const { return status == Status::Returned ||
        ((status == Status::Stopped || status == Status::Waiting) && committed); }
};
Result Execute(Host& host, const Function& function, const std::string& self,
    const std::vector<Evaluation>& arguments = {}, const Limits& limits = {});
// Coordinates several real script/native/event calls in one interpreter and
// one host transaction. There is no per-call commit; later failure also restores
// earlier external OUT writes. The callback is synchronous trusted host code,
// must not retain Execution, and must bound its own non-script scan work. Entry
// attempts share limits.operations; all nested script budgets/journals are shared.
// External Reference closures and their backing storage must survive through
// this function's return, including rollback after the callback has unwound.
// Callback-local OUT storage must be owned by its closures (for example via
// shared_ptr), not captured by reference to a callback-local stack variable.
// A failed Execution call poisons the transaction even if the callback catches
// its exception. The original failure is reported and cannot become a commit.
Result ExecuteTransaction(Host& host, const std::function<Value(Execution&)>& operation,
    const Limits& limits = {});
// Executes one persistent state slice. Sleep yields Waiting without advancing
// elapsed time; other unsupported waits still fail explicitly. No
// actor tick, elapsed-time advance or world startup is implied by this call.
Result ResumeState(Host& host, const std::string& self, const Limits& limits = {});
// Poll + resume share one transaction/interpreter budget. This is the explicit
// Actor.ProcessState phase, not animation, Tick events, physics or timers.
Result AdvanceState(Host& host, const std::string& self, float elapsed, const Limits& limits = {});
} // namespace QuestVr::Vm
