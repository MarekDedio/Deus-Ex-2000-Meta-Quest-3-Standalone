#include "quest_portable_vm.h"
#include "quest_state_frame.h"
#include "quest_unreal_string.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "Math/coords.h"

namespace QuestVr::Vm {
namespace {
std::string Lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
[[noreturn]] void TypeError() { throw std::runtime_error("VM value type mismatch"); }
std::string NameKey(const std::string& name) { return name.empty() ? "none" : Lower(name); }
bool HasIntArray(const Value& value) {
    if (value.kind == Kind::IntArray) return true;
    for (const auto& [key, field] : value.fields)
        if (HasIntArray(field)) return true;
    return false;
}
bool SnapshotNativeInput(const std::uint16_t index, const Function* declaration = nullptr) {
    // These audited natives copy ordinary inputs during expression evaluation;
    // none has OUT parameters. Unknown/OUT natives retain their guarded aliases.
    switch (index) {
    case 0:
        // Original Attach copies distance/vector; PlayerPawn.ConsoleCommand
        // copies its FString. Native-zero dispatch must use its actual
        // declaration; unknown native-zero/OUT calls retain guarded aliases.
        return declaration && (Lower(declaration->path) == "engine.decal.attachdecal" ||
            Lower(declaration->path) == "engine.playerpawn.consolecommand");
    case 112: case 113: case 124: case 125: case 126: case 127: case 128:
    case 168: case 186: case 187: case 189: case 231: case 234: case 235: case 244: case 245: case 246: case 249: case 250: case 251: case 278:
    case 280: return true; // SetTimer copies rate before evaluating bLoop.
    case 1410: case 1415: case 1416: case 1420: case 1421: case 1422: case 1430: case 1431: case 1432: case 1433: case 1438: case 1454: case 1461: case 1467:
    case 1475: case 1477: case 1510: case 1511: case 1535: case 1538: case 1539: case 1540:
    case 1541: case 1544: case 1545:
        return true; // Retail window wrappers have no OUT parameters.
    default: return false;
    }
}
bool CopyNativeOutTemporary(const std::uint16_t index) {
    // QueryPreferredSize's retail wrapper separately copies each initialized
    // Float result temporary and captures its GProperty alias after Step.
    // Keep both: later argument evaluation may mutate an earlier OUT target,
    // but must not retroactively change that earlier copied input temporary.
    // Other OUT natives retain their existing declaration/handler semantics.
    return index == 1465u;
}
std::int32_t ParseInteger(const std::string& text) {
    // Match atoi's decimal prefix semantics on defined inputs; fail closed
    // rather than invoking its undefined overflow behavior on hostile scripts.
    errno = 0;
    const auto n = std::strtoll(text.c_str(), nullptr, 10);
    if (errno == ERANGE || n < -2147483648LL || n > 2147483647LL)
        throw std::runtime_error("VM string conversion outside int32 range");
    return static_cast<std::int32_t>(n);
}
std::int32_t Truncate(float n) {
    if (!std::isfinite(n) || static_cast<double>(n) < -2147483648.0 ||
        static_cast<double>(n) >= 2147483648.0)
        throw std::runtime_error("VM numeric conversion outside int32 range");
    return static_cast<std::int32_t>(n);
}
} // namespace
Value Value::Integer(std::int32_t n) { Value v; v.kind = Kind::Int; v.integer = n; return v; }
Value Value::Byte(std::uint8_t n) { Value v = Integer(n); v.kind = Kind::Byte; return v; }
Value Value::Float(float n) { Value v; v.kind = Kind::Float; v.floating = n; return v; }
Value Value::Bool(bool n) { Value v; v.kind = Kind::Bool; v.boolean = n; return v; }
Value Value::Text(Kind kind, std::string text) {
    Value v; v.kind = kind; v.text = std::move(text); return v;
}
Value Value::Vector(std::array<float, 3> n) { Value v; v.kind = Kind::Vector; v.vector = n; return v; }
Value Value::Rotator(std::array<std::int32_t, 3> n) {
    Value v; v.kind = Kind::Rotator; v.rotation = n; return v;
}
Value Value::IntArray(std::vector<std::int32_t> values) {
    Value v; v.kind = Kind::IntArray; v.intArray = std::move(values); return v;
}
bool ToBool(const Value& v) {
    if (v.kind == Kind::Nothing) return false;
    if (v.kind != Kind::Bool) TypeError();
    return v.boolean;
}
std::int32_t ToInt(const Value& v) {
    if (v.kind == Kind::Nothing) return 0;
    if (v.kind == Kind::Byte || v.kind == Kind::Int) return v.integer;
    if (v.kind == Kind::Float) return Truncate(v.floating);
    TypeError();
}
float ToFloat(const Value& v) {
    if (v.kind == Kind::Nothing) return 0;
    if (v.kind == Kind::Byte || v.kind == Kind::Int) return static_cast<float>(v.integer);
    if (v.kind == Kind::Float) return v.floating;
    TypeError();
}
Value Coerce(const Value& v, const Value& zero) {
    if (v.kind == Kind::Nothing) return zero;
    switch (zero.kind) {
    case Kind::Int: return Value::Integer(ToInt(v));
    case Kind::Byte: return Value::Byte(static_cast<std::uint8_t>(ToInt(v)));
    case Kind::Float: return Value::Float(ToFloat(v));
    case Kind::Bool: return Value::Bool(ToBool(v));
    case Kind::String:
        if (v.kind == Kind::String || v.kind == Kind::Name) return Value::Text(Kind::String,
            v.kind == Kind::Name && v.text.empty() ? "None" : v.text);
        TypeError();
    case Kind::IntArray:
        if (v.kind != Kind::IntArray) TypeError();
        return v;
    case Kind::Struct: {
        if (v.kind != Kind::Struct || v.fields.size() != zero.fields.size()) TypeError();
        Value result = zero;
        for (const auto& [key, field] : zero.fields) {
            const auto found = v.fields.find(key);
            if (found == v.fields.end()) TypeError();
            result.fields[key] = Coerce(found->second, field);
        }
        return result;
    }
    default:
        if (v.kind != zero.kind) TypeError();
        return v;
    }
}
bool Equal(const Value& a, const Value& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
    case Kind::Nothing: return true;
    case Kind::Int: case Kind::Byte: return a.integer == b.integer;
    case Kind::Float: return a.floating == b.floating;
    case Kind::Bool: return a.boolean == b.boolean;
    case Kind::Name: return NameKey(a.text) == NameKey(b.text);
    case Kind::Object: return Lower(a.text) == Lower(b.text);
    case Kind::String: return a.text == b.text;
    case Kind::Vector: return a.vector == b.vector;
    case Kind::Rotator: return a.rotation == b.rotation;
    case Kind::IntArray: return a.intArray == b.intArray;
    case Kind::Struct:
        if (a.fields.size() != b.fields.size()) return false;
        for (const auto& [key, value] : a.fields) {
            const auto found = b.fields.find(key);
            if (found == b.fields.end() || !Equal(value, found->second)) return false;
        }
        return true;
    }
    return false;
}

namespace {
struct Failure : std::runtime_error {
    Status status;
    Failure(Status s, const std::string& text) : std::runtime_error(text), status(s) {}
};
struct Node {
    std::uint8_t op{};
    std::uint8_t zeroFillSize{};
    std::size_t offset{};
    std::int32_t reference{};
    std::uint16_t target{}, native{};
    Value constant;
    std::vector<Node> children;
    std::vector<StateLabel> labels;
};
struct Program {
    std::vector<Node> statements;
    std::map<std::size_t, std::size_t> boundaries;
};
class Parser {
public:
    Parser(Host& host, const Function& fn, const Limits& limits, std::size_t& totalNodes,
        Result& result, bool analysis = false, std::size_t* executionRetainedBytes = nullptr) :
            host_(host), fn_(fn), limits_(limits), totalNodes_(totalNodes), result_(result),
            analysis_(analysis), executionRetainedBytes_(executionRetainedBytes) {}
    Program Parse() {
        if (fn_.bytecode.size() > limits_.sourceBytes) Fail(Status::Budget, "VM bytecode byte limit");
        Program p;
        while (cursor_ < fn_.bytecode.size()) {
            Retain(2u * sizeof(std::pair<std::size_t, std::size_t>) + 2u * sizeof(std::size_t));
            p.boundaries.emplace(cursor_, p.statements.size());
            p.statements.push_back(Read(0));
        }
        // Only top-level statements are jumpable. Never reinterpret an operand
        // as an opcode or allow a branch to run off the end of a function.
        for (const auto& n : p.statements) ValidateJump(n, p);
        for (std::size_t i = 0; i < p.statements.size(); ++i) {
            const auto& n = p.statements[i];
            if (n.op == 0x05 && (i + 1u >= p.statements.size() || p.statements[i + 1u].op != 0x0a)) {
                result_.offset = n.offset; result_.opcode = n.op;
                Fail(Status::Invalid, "VM Switch is not followed by a Case statement");
            }
        }
        return p;
    }
    std::size_t AnalysisRetainedBytes() const { return retainedBytes_; }
private:
    Host& host_;
    const Function& fn_;
    const Limits& limits_;
    std::size_t& totalNodes_;
    Result& result_;
    std::size_t cursor_{};
    bool analysis_{};
    std::size_t retainedBytes_{}, labels_{};
    std::size_t* executionRetainedBytes_{};
    [[noreturn]] void Fail(Status status, const std::string& text) { throw Failure(status, text); }
    void Retain(const std::size_t bytes) {
        if (!analysis_) return;
        if (bytes > limits_.retainedBytes || retainedBytes_ > limits_.retainedBytes - bytes)
            Fail(Status::Budget, "VM program layout retained byte limit");
        retainedBytes_ += bytes;
    }
    std::string Resolve(const std::int32_t reference, const bool name) {
        std::string spelling;
        try {
            spelling = name ? host_.ResolveName(fn_, reference) : host_.ResolveObject(fn_, reference);
        } catch (const std::exception& error) {
            Fail(Status::Invalid, std::string("Invalid VM program reference: ") + error.what());
        }
        if (spelling.size() > limits_.stringBytes) Fail(Status::Budget, "VM program identity/name byte limit");
        return spelling;
    }
    std::string ResolveCastClass(const std::int32_t reference) {
        std::string identity;
        try { identity = host_.ResolveCastClass(fn_, reference); }
        catch (const std::exception& error) {
            Fail(Status::Invalid, std::string("Invalid VM cast class: ") + error.what());
        }
        if (identity.empty() || identity.find('\0') != std::string::npos)
            Fail(Status::Invalid, "Invalid VM cast class identity");
        if (identity.size() > limits_.stringBytes)
            Fail(Status::Budget, "VM cast class identity byte limit");
        // Parsed identities live alongside all recursive calls/state views.
        // Charge their actual capacity to the same budget as locals/journals,
        // not a fresh per-parser counter or a per-string-only check.
        if (!executionRetainedBytes_ || identity.capacity() > limits_.retainedBytes ||
            *executionRetainedBytes_ > limits_.retainedBytes - identity.capacity())
            Fail(Status::Budget, "VM cast class retained byte limit");
        *executionRetainedBytes_ += identity.capacity();
        return identity;
    }
    std::uint8_t Byte() {
        if (cursor_ >= fn_.bytecode.size()) Fail(Status::Invalid, "Truncated VM expression");
        return fn_.bytecode[cursor_++];
    }
    std::uint16_t Word() { const auto a = Byte(); const auto b = Byte(); return a | (b << 8u); }
    std::uint32_t Dword() { const auto a = Word(); const auto b = Word(); return a | (std::uint32_t(b) << 16u); }
    void ValidateJump(const Node& n, const Program& p) {
        if ((n.op == 0x06 || n.op == 0x07 || n.op == 0x2f) && !p.boundaries.count(n.target)) {
            result_.offset = n.offset; result_.opcode = n.op;
            Fail(Status::Invalid, "VM jump is not a top-level statement boundary");
        }
        if (n.op == 0x2f && p.statements.at(p.boundaries.at(n.target)).op != 0x30)
            Fail(Status::Invalid, "VM iterator end is not an IteratorPop statement");
        if (n.op == 0x0a && n.target != 0xffffu &&
            (!p.boundaries.count(n.target) || p.statements.at(p.boundaries.at(n.target)).op != 0x0a)) {
            result_.offset = n.offset; result_.opcode = n.op;
            Fail(Status::Invalid, "VM Case link is not a top-level Case statement");
        }
        for (const auto& child : n.children) ValidateJump(child, p);
    }
    void Arguments(Node& n, std::size_t depth) {
        while (true) {
            if (cursor_ >= fn_.bytecode.size()) Fail(Status::Invalid, "Unterminated VM arguments");
            if (fn_.bytecode[cursor_] == 0x16) { ++cursor_; break; }
            if (n.children.size() >= limits_.arguments) Fail(Status::Budget, "VM argument limit");
            n.children.push_back(Read(depth + 1));
        }
    }
    Node Read(std::size_t depth) {
        if (depth >= limits_.expressionDepth || ++totalNodes_ > limits_.nodes)
            Fail(Status::Budget, "VM expression/node limit");
        // Conservatively cover internal node-vector capacity. Analysis also
        // accounts for its detached offset/label result while the tree lives.
        Retain(2u * sizeof(Node));
        Node n; n.offset = cursor_; n.op = Byte();
        result_.function = fn_.path; result_.offset = n.offset; result_.opcode = n.op;
        const auto child = [&] { n.children.push_back(Read(depth + 1)); };
        if (n.op >= 0x60) {
            n.native = n.op >= 0x70 ? n.op : static_cast<std::uint16_t>(((n.op - 0x60) << 8u) | Byte());
            Arguments(n, depth);
        } else if (n.op >= 0x39) {
            child();
        } else {
            switch (n.op) {
            case 0x00: case 0x01: case 0x02: case 0x20: case 0x21: case 0x29:
                n.reference = std::bit_cast<std::int32_t>(Dword()); break;
            case 0x04: child(); break; // Supported packages are UE1 version > 61.
            case 0x05: n.reference = static_cast<std::int32_t>(Byte()) + 1; child(); break;
            case 0x06: n.target = Word(); break;
            case 0x07: case 0x09: n.target = Word(); child(); break;
            case 0x08: case 0x0b: case 0x15: case 0x16: case 0x17:
            case 0x25: case 0x26: case 0x27: case 0x28: case 0x2a:
            case 0x30: case 0x31: break;
            case 0x0a: n.target = Word(); if (n.target != 0xffff) child(); break;
            case 0x0c:
                while (true) {
                    const auto name = std::bit_cast<std::int32_t>(Dword());
                    const auto target = Dword();
                    if (analysis_) {
                        if (labels_ >= limits_.nodes) Fail(Status::Budget, "VM program label entry limit");
                        ++labels_;
                        auto spelling = Resolve(name, true);
                        if (NameKey(spelling) == "none") break;
                        Retain(3u * sizeof(StateLabel));
                        Retain(spelling.size()); Retain(spelling.size());
                        n.labels.push_back({std::move(spelling), target});
                    } else if (Lower(host_.ResolveName(fn_, name)) == "none") break;
                }
                break;
            case 0x0d: case 0x0e: case 0x2d: child(); break;
            case 0x0f: case 0x10: case 0x14: case 0x1a: child(); child(); break;
            case 0x11: child(); child(); child(); child(); break;
            case 0x12: case 0x19: {
                child(); n.target = Word(); n.zeroFillSize = Byte();
                const auto childStart = cursor_; child();
                if (cursor_ - childStart != n.target) {
                    result_.offset = n.offset; result_.opcode = n.op;
                    Fail(Status::Invalid, "VM context skip does not match its child expression span");
                }
                break;
            }
            case 0x13: case 0x2e:
                n.reference = std::bit_cast<std::int32_t>(Dword());
                if (!analysis_) n.constant = Value::Text(Kind::Object, ResolveCastClass(n.reference));
                child(); break;
            case 0x36: n.reference = std::bit_cast<std::int32_t>(Dword()); child(); break;
            case 0x18: n.target = Word(); child(); break;
            case 0x1b: case 0x1c: case 0x38:
                n.reference = std::bit_cast<std::int32_t>(Dword()); Arguments(n, depth); break;
            case 0x1d: n.constant = Value::Integer(std::bit_cast<std::int32_t>(Dword())); break;
            case 0x1e: n.constant = Value::Float(std::bit_cast<float>(Dword())); break;
            case 0x1f: {
                std::string text;
                for (auto c = Byte(); c != 0; c = Byte()) {
                    // Code still consumes the entire serialized literal, but
                    // appFromAnsi retains only the first1023 source bytes.
                    if (text.size()<1023u) text.push_back(static_cast<char>(c));
                }
                // Original appFromAnsi has a static 1024-word buffer; Code
                // nevertheless advances through the complete encoded literal.
                n.constant = Value::Text(Kind::String, UnrealString::FromAnsi(text));
                if (n.constant.text.size()>limits_.stringBytes) Fail(Status::Budget,"VM string transport limit");
                Retain(n.constant.text.size()); Retain(n.constant.text.size()); break;
            }
            case 0x22: {
                std::array<std::int32_t, 3> v{};
                for (auto& a : v) a = std::bit_cast<std::int32_t>(Dword());
                n.constant = Value::Rotator(v); break;
            }
            case 0x23: {
                std::array<float, 3> v{};
                for (auto& a : v) a = std::bit_cast<float>(Dword());
                n.constant = Value::Vector(v); break;
            }
            case 0x24: n.constant = Value::Byte(Byte()); break;
            case 0x2b: Byte(); child(); break;
            case 0x2c: n.constant = Value::Byte(Byte()); break;
            case 0x2f: child(); n.target = Word(); break;
            case 0x32: case 0x33: n.reference = std::bit_cast<std::int32_t>(Dword()); child(); child(); break;
            case 0x34: {
                std::u16string units;
                for (auto word=Word(); word!=0u; word=Word()) {
                    // Each unit needs at least one transport byte. This bounds
                    // parsing without treating UTF-16 storage as UTF-8 length.
                    if (units.size()>=limits_.stringBytes) Fail(Status::Budget,"VM Unicode string limit");
                    units.push_back(static_cast<char16_t>(word));
                }
                n.constant=Value::Text(Kind::String,UnrealString::Encode(units));
                if (n.constant.text.size()>limits_.stringBytes) Fail(Status::Budget,"VM Unicode transport limit");
                Retain(n.constant.text.size()); Retain(n.constant.text.size()); break;
            }
            default: Fail(Status::Invalid, "Unknown VM opcode " + std::to_string(n.op));
            }
        }
        if (analysis_) {
            switch (n.op) {
            case 0x21: case 0x1b: case 0x38: Resolve(n.reference, true); break;
            case 0x00: case 0x01: case 0x02: case 0x20: case 0x29:
            case 0x13: case 0x2e: case 0x36: case 0x32: case 0x33: case 0x1c:
                // Structural identities only. Property typed zeros and callable
                // functions need separate host support/graph binding, not here.
                Resolve(n.reference, false); break;
            default: break;
            }
        }
        return n;
    }
};

struct Frame {
    const Function& function;
    std::string self;
    std::map<std::string, std::shared_ptr<Reference>> locals;
};
std::shared_ptr<Reference> LocalReference(const Property& property,
    const std::shared_ptr<std::vector<Value>>& values, std::size_t index = 0) {
    auto ref = std::make_shared<Reference>();
    ref->zero = property.zero; ref->dimension = property.arrayDimension;
    ref->resultLayout = property.resultLayout;
    ref->read = [values, index] { return values->at(index); };
    ref->write = [values, index](const Value& v) { values->at(index) = v; };
    ref->element = [property, values](std::size_t i) { return LocalReference(property, values, i); };
    return ref;
}
Value MemberValue(const Value& v, const std::string& name) {
    const auto key = Lower(name);
    if (v.kind == Kind::Struct) {
        const auto found = v.fields.find(key);
        if (found != v.fields.end()) return found->second;
    } else if (v.kind == Kind::Vector) {
        if (key == "x") return Value::Float(v.vector[0]);
        if (key == "y") return Value::Float(v.vector[1]);
        if (key == "z") return Value::Float(v.vector[2]);
    } else if (v.kind == Kind::Rotator) {
        if (key == "pitch") return Value::Integer(v.rotation[0]);
        if (key == "yaw") return Value::Integer(v.rotation[1]);
        if (key == "roll") return Value::Integer(v.rotation[2]);
    }
    throw std::runtime_error("VM struct member unavailable: " + name);
}
void SetMember(Value& v, const std::string& name, const Value& member) {
    const auto key = Lower(name);
    if (v.kind == Kind::Struct && v.fields.count(key)) v.fields[key] = member;
    else if (v.kind == Kind::Vector && (key == "x" || key == "y" || key == "z"))
        v.vector[key == "x" ? 0 : key == "y" ? 1 : 2] = ToFloat(member);
    else if (v.kind == Kind::Rotator && (key == "pitch" || key == "yaw" || key == "roll"))
        v.rotation[key == "pitch" ? 0 : key == "yaw" ? 1 : 2] = ToInt(member);
    else throw std::runtime_error("VM struct member write unavailable: " + name);
}

class Machine final : public Execution {
public:
    Machine(Host& host, const Limits& limits, Result& result) : host_(host), limits_(limits), result_(result) {}
    ~Machine() {
        *alive_ = false;
        if (accepted_) return;
        // Includes external out-parameter aliases and native writes. Local
        // references keep shared storage alive until rollback is complete.
        for (auto it = journal_.rbegin(); it != journal_.rend(); ++it) {
            try { it->first->write(it->second); } catch (...) {}
        }
    }
    void Accept() { accepted_ = true; }
    void CheckFailure() const {
        if (!failure_) return;
        // A native may catch an inner failure and finish unwinding its own
        // frame. Report the first failing callee, not that later caller cursor.
        result_.function = failureLocation_->function;
        result_.offset = failureLocation_->offset;
        result_.opcode = failureLocation_->opcode;
        result_.callStack = failureLocation_->callStack;
        std::rethrow_exception(failure_);
    }
    Value FinishTransaction(Value value) {
        CheckFailure();
        ValidateValue(value, 0);
        return value;
    }
    Value CallEvent(const std::string& receiver, const std::string& eventName,
        const bool enumDispatch, const std::vector<Evaluation>& arguments) override {
        // Resolution/budget failures at a coordinator boundary have no
        // bytecode caller. Do not blame a previously completed declaration.
        // A nested event must keep its active caller until its callee is known.
        const bool coordinator = result_.callStack.empty();
        if (coordinator && !failure_) {
            result_.function.clear(); result_.offset = 0; result_.opcode = 0;
        }
        return DirectOperation([&] {
            if (eventName.size() > limits_.stringBytes || receiver.size() > limits_.stringBytes)
                Fail(Status::Budget, "VM callback identity/name limit");
            const auto identity = host_.ResolveEvent(receiver, eventName, enumDispatch);
            if (identity && coordinator) Locate(*identity, 0, 0);
            return identity ? Run(*identity, receiver, arguments) : Value{};
        });
    }
    Value CallFunction(const Function& identity, const std::string& receiver,
        const std::vector<Evaluation>& arguments) override {
        // At a coordinator boundary there is no caller bytecode location. A
        // nested callback keeps the current caller until Run enters the callee.
        if (result_.callStack.empty() && !failure_) Locate(identity, 0, 0);
        return DirectOperation([&] {
            if (identity.path.size() > limits_.stringBytes || identity.source.size() > limits_.stringBytes ||
                receiver.size() > limits_.stringBytes)
                Fail(Status::Budget, "VM direct function identity limit");
            return Run(identity, receiver, arguments);
        });
    }
    void WriteReference(const std::shared_ptr<Reference>& reference,const Value& value) override {
        if (result_.callStack.empty() && !failure_) {
            result_.function.clear(); result_.offset=0; result_.opcode=0;
        }
        static_cast<void>(DirectOperation([&] {
            Write(reference,value);
            return Value{};
        }));
    }
    Value Run(const Function& identity, const std::string& self, const std::vector<Evaluation>& incoming,
        std::optional<std::optional<Value>> capturedContext = {}, const bool nativeArgumentsMaterialized = false) {
        if (self.empty() || !host_.CanCall(identity, self)) return {};
        if (result_.callStack.size() >= limits_.callDepth) Fail(Status::Budget, "VM call depth limit");
        // An engaged outer optional records that expression evaluation already
        // captured even a deliberately absent context. Never recapture after
        // argument expressions or after loading caller-provided aliases.
        if (!capturedContext) capturedContext.emplace((identity.flags & 0x400u) ?
            CaptureNativeContext(identity.nativeIndex, self, &identity) : std::nullopt);
        const auto prepared = host_.PrepareFunction(identity, self);
        if (!prepared || prepared->path != identity.path || prepared->source != identity.source ||
            (((prepared->flags | identity.flags) & 0x400u) &&
                (prepared->flags != identity.flags || prepared->nativeIndex != identity.nativeIndex)))
            Fail(Status::Invalid, "Prepared VM function changed callable identity");
        const Function& fn = *prepared;
        result_.callStack.push_back(fn.path);
        result_.function = fn.path; result_.offset = 0; result_.opcode = 0;
        if ((fn.flags & 0x8) && !((fn.flags & 0x400) &&
            (fn.nativeIndex == 256u || fn.nativeIndex == 527u)))
            Fail(Status::Unsupported, "Latent VM function requires its runtime handler");
        if (incoming.size() > limits_.arguments) Fail(Status::Budget, "VM argument limit");
        const bool directNative=(fn.flags&0x400u) && !nativeArgumentsMaterialized;
        const auto uninitializedLayouts=directNative ?
            CaptureUninitializedNativeLayouts(fn.nativeIndex,self,incoming.size(),&fn) :
            std::vector<std::optional<ResultLayout>>{};
        // Expression calls already checked dispositions before any declared
        // zero merge/snapshot. Direct Execution inputs must do so here BEFORE
        // SnapshotNativeInput could turn NoWrite into an apparent written value.
        std::size_t parameterSlot{};
        for (const auto& property:fn.variables) {
            if (!(property.flags&0x80u) || (property.flags&0x400u)) continue;
            if (!uninitializedLayouts.empty() && parameterSlot<uninitializedLayouts.size() &&
                uninitializedLayouts[parameterSlot] && property.resultLayout!=*uninitializedLayouts[parameterSlot])
                Fail(Status::Invalid,"VM uninitialized native argument declaration layout mismatch");
            ++parameterSlot;
        }
        const bool directSequentialSeeds=directNative && ReadOnlyNativeSeedOperation([&] {
            return host_.HasSequentialNativeArgumentSeeds(fn.nativeIndex,self,&fn);
        });
        // Capture ALL immutable defaults before reading ANY incoming alias.
        // Eval already did this before evaluating expressions; never repeat
        // its hooks at the internal Run boundary.
        const auto nativeSeeds=directNative ? CaptureNativeArgumentSeeds(fn.nativeIndex,self,
            incoming.size(),&fn,uninitializedLayouts) : std::vector<std::optional<Value>>{};
        std::vector<Value> directPrefix;
        std::size_t directPrefixBytes{},directPrefixNodes{};
        if (directSequentialSeeds) ReserveNativeInputPrefix(directPrefix,incoming.size());
        std::vector<Evaluation> args;
        args.reserve(incoming.size());
        const bool copiedOutTemporary=(fn.flags&0x400u) && CopyNativeOutTemporary(fn.nativeIndex);
        for (std::size_t slot=0u;slot<incoming.size();++slot) {
            const auto& argument=incoming[slot];
            Evaluation materialized;
            const Evaluation* input=&argument;
            const auto sequentialSeed=directSequentialSeeds ? ReadOnlyNativeSeedOperation([&] {
                return host_.SequentialNativeArgumentSeed(fn.nativeIndex,self,slot,&fn,directPrefix);
            }) : std::optional<Value>{};
            if (sequentialSeed) {
                if (!uninitializedLayouts.empty() && uninitializedLayouts[slot])
                    Fail(Status::Invalid,"VM uninitialized native argument conflicts with an initialized seed");
                ValidateValue(*sequentialSeed,0);Retain(ValueBytes(*sequentialSeed));RetainNativeSeedCapacity(*sequentialSeed);
                ValidateResultLayout(*sequentialSeed,NativeParameterLayout(&fn,slot));
            }
            if (!uninitializedLayouts.empty() && uninitializedLayouts[slot]) {
                materialized=MaterializeUninitializedNativeArgument(argument,*uninitializedLayouts[slot]);input=&materialized;
            } else if (directNative && argument.disposition!=ResultDisposition::Written) {
                const bool entrySeeded=!nativeSeeds.empty() && nativeSeeds[slot].has_value();
                const auto destination=sequentialSeed ? sequentialSeed : entrySeeded ? nativeSeeds[slot] :
                    BuiltinArgumentZero(fn.nativeIndex,slot);
                if (destination) {
                    ValidateValue(*destination,0);Retain(ValueBytes(*destination));
                    materialized={ResultValue(argument,*destination,NativeParameterLayout(&fn,slot),true),{}};
                } else if (argument.disposition==ResultDisposition::ZeroFill && argument.zeroFillBytes!=0u)
                    Fail(Status::Unsupported,"VM context argument result destination layout is unavailable");
                else {
                    // There is no audited initialized temporary. Preserve
                    // absence, not an arbitrary declaration zero or stale
                    // caller payload, and never read an unwritten alias.
                    materialized.disposition=argument.disposition;
                    materialized.zeroFillBytes=argument.zeroFillBytes;
                }
                input=&materialized;
            }
            if (!input->reference) ValidateValue(input->value, 0);
            const auto value = copiedOutTemporary && nativeArgumentsMaterialized ? input->value : input->Load();
            ValidateValue(value, 0); Retain(ValueBytes(value));
            if (directSequentialSeeds) CaptureNativeInput(directPrefix,value,directPrefixBytes,directPrefixNodes);
            // execGotoState consumes each input value before evaluating the
            // next argument. Ordinary native OUT arguments retain aliases.
            if (copiedOutTemporary) args.push_back({value,input->reference,input->disposition,input->zeroFillBytes});
            else if ((fn.flags & 0x400u) && SnapshotNativeInput(fn.nativeIndex, &fn) &&
                input->disposition==ResultDisposition::Written) args.push_back({value,{}});
            else args.push_back(*input);
        }
        std::size_t argCount = 0;
        for (const auto& p : fn.variables) {
            ValidateResultLayout(p.zero,p.resultLayout);
            if (!(p.flags & 0x80) || (p.flags & 0x400)) continue;
            if (argCount == args.size()) {
                if (!(p.flags & 0x10)) Fail(Status::Invalid, "Missing required VM parameter " + p.name);
                args.push_back({});
            }
            if (p.resultLayout!=ResultLayout::Unknown) {
                const auto value=copiedOutTemporary ? args[argCount].value : args[argCount].Load();
                if (value.kind!=Kind::Nothing) ValidateResultLayout(value,p.resultLayout);
            }
            ++argCount;
        }
        if (args.size() > argCount && !fn.variables.empty()) Fail(Status::Invalid, "Excess VM arguments");
        if (fn.flags & 0x400) {
            Step(fn, 0, 0);
            auto result = Native(fn.nativeIndex, self, args, &fn, *capturedContext).Load();
            for (const auto& property:fn.variables)
                if ((property.flags&0x400u) && result.kind!=Kind::Nothing)
                    ValidateResultLayout(result,property.resultLayout);
            ValidateValue(result, 0); result_.callStack.pop_back(); return result;
        }
        auto program = Parser(host_, fn, limits_, nodes_, result_, false, &retainedBytes_).Parse();
        Frame frame{fn, self, {}};
        std::size_t argument = 0;
        std::optional<Property> returnProperty;
        for (const auto& p : fn.variables) {
            if (p.arrayDimension == 0 || p.arrayDimension > limits_.localElements ||
                localElements_ > limits_.localElements - p.arrayDimension)
                Fail(Status::Budget, "VM local element limit");
            localElements_ += p.arrayDimension;
            ValidateValue(p.zero, 0);
            const auto bytes = ValueBytes(p.zero);
            if (bytes > limits_.retainedBytes / p.arrayDimension) Fail(Status::Budget, "VM local byte limit");
            Retain(bytes * p.arrayDimension);
            auto storage = std::make_shared<std::vector<Value>>(p.arrayDimension, p.zero);
            auto reference = LocalReference(p, storage);
            if (!frame.locals.emplace(p.key, reference).second) Fail(Status::Invalid, "Duplicate VM local identity");
            if (p.flags & 0x400) returnProperty = p;
            else if (p.flags & 0x80) {
                if (argument < args.size()) {
                    auto typed = Coerce(args[argument].Load(), p.zero); ValidateValue(typed, 0);
                    const auto typedBytes = ValueBytes(typed);
                    if (typedBytes > bytes) Retain(typedBytes - bytes);
                    reference->write(typed);
                }
                ++argument;
            }
        }
        Value value;
        bool returned = false;
        IteratorStack iterators;
        for (std::size_t pc = 0; pc < program.statements.size();) {
            const Node& n = program.statements[pc];
            Step(fn, n.offset, n.op);
            if (AdvanceIterator(frame, n, program, pc, iterators)) continue;
            if (n.op == 0x04) {
                const auto evaluated = Eval(frame, n.children.at(0), self);
                value = returnProperty ? ResultValue(evaluated,frame.locals.at(returnProperty->key)->read(),returnProperty->resultLayout) : evaluated.Load();
                returned = true; break;
            }
            if (n.op == 0x15) {
                const auto* object = host_.ReadState(self);
                if (object && object->frame && object->frame->control)
                    Fail(Status::Unsupported, "Original opcode 0x15 state semantics are not established");
            }
            if (n.op == 0x08 || n.op == 0x15) Fail(Status::Stopped, "State stop requires a continuation engine");
            if (n.op == 0x06) { pc = program.boundaries.at(n.target); continue; }
            if (n.op == 0x05) {
                auto selector = Eval(frame, n.children.at(0), self);
                RetainSwitchSelector(selector);
                Locate(fn, n.offset, n.op);
                pc = SelectSwitchCase(frame, program, pc + 1u, selector);
                continue;
            }
            if (n.op == 0x07) {
                const auto condition = Eval(frame, n.children.at(0), self).Load(); Locate(fn, n.offset, n.op);
                if (!ToBool(condition)) { pc = program.boundaries.at(n.target); continue; }
            } else if (n.op == 0x09) {
                const auto condition = Eval(frame, n.children.at(0), self).Load(); Locate(fn, n.offset, n.op);
                if (!ToBool(condition))
                    Fail(Status::Invalid, "Script assert failed at line " + std::to_string(n.target));
            } else if (n.op != 0x0a) Eval(frame, n, self);
            ++pc;
        }
        if (!returned) Fail(Status::Invalid, "VM function reached end without Return");
        // Load before copying out or destroying local storage. Native absence
        // stays Nothing; only a scripted return parameter supplies its zero.
        if (value.kind == Kind::Nothing && returnProperty) value = returnProperty->zero;
        ValidateValue(value, 0);
        argument = 0;
        for (const auto& p : fn.variables) {
            if (!(p.flags & 0x80) || (p.flags & 0x400)) continue;
            if ((p.flags & 0x100) && argument < args.size() && args[argument].reference)
                Write(args[argument].reference, frame.locals.at(p.key)->read());
            ++argument;
        }
        for (const auto& p : fn.variables) localElements_ -= p.arrayDimension;
        result_.callStack.pop_back(); return value;
    }
    Value Resume(const std::string& self, std::optional<std::string> priorSelected = {}) {
        if (self.size() > limits_.stringBytes) Fail(Status::Budget, "VM state receiver identity limit");
        bool active{};
        std::size_t selectedChanges{};
        IteratorStack iterators;
        const auto finish = [&](Value value = {}) {
            if (!iterators.empty())
                Fail(Status::Unsupported, "State continuation with live iterators requires persistent iterator storage");
            if (active) result_.callStack.pop_back();
            return value;
        };
        for (;;) {
            const auto* object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame) return finish();
            const auto& control = object->frame->control;
            if (control) {
                if (control->selectedPath.size() > limits_.stringBytes)
                    Fail(Status::Budget, "VM selected state identity limit");
                if (!priorSelected) priorSelected = control->selectedPath;
                if (!control->codePresent || host_.IsStateDeleted(self)) {
                    result_.status = Status::Stopped; return finish();
                }
                if (object->frame->codePath.empty() || control->selectedPath.empty())
                    Fail(Status::Invalid, "Modern state continuation has an empty node identity");
            } else {
                if (priorSelected) Fail(Status::Invalid, "Modern state continuation lost its control metadata");
                if (object->frame->codePath.empty()) return finish();
            }
            const auto code = object->frame->codePath;
            const auto localOwner = object->frame->localsCodePath;
            const auto statementIndex = object->frame->statementIndex;
            const auto latent = object->frame->latent;
            const auto localRevision = host_.StateLocalRevision(self);
            if (latent == StateLatent::Stop) {
                if (control) Fail(Status::Unsupported, "Modern state frame cannot use a synthetic Stop latent action");
                result_.status = Status::Stopped; return finish();
            }
            if (latent == StateLatent::Sleep || latent == StateLatent::WaitForLanding) {
                result_.status = Status::Waiting; return finish();
            }
            if (latent != StateLatent::Continue)
                Fail(Status::Unsupported, "State latent action requires its runtime handler");
            if (!active) {
                if (result_.callStack.size() >= limits_.callDepth) Fail(Status::Budget, "VM state call depth limit");
                result_.callStack.push_back(code); active = true;
            } else result_.callStack.back() = code;
            auto& view = StateView(self, code, localOwner);
            if (statementIndex >= view.program.statements.size())
                Fail(Status::Invalid, "Unexpected end of state code statements");
            const Node& statement = view.program.statements[statementIndex];
            Step(view.function, statement.offset, statement.op);
            auto* writable = host_.MutableState(self);
            if (!writable || !writable->frameOverride || !writable->frame ||
                !HasStateCode(*writable->frame) ||
                writable->frame->codePath != code ||
                writable->frame->statementIndex != statementIndex)
                Fail(Status::Invalid, "State frame changed while advancing its instruction");
            if (statementIndex == std::numeric_limits<std::uint32_t>::max())
                Fail(Status::Invalid, "State statement ordinal overflow");
            writable->frame->statementIndex = statementIndex + 1u;
            // Hold the old statement/view alive while nested calls replace the
            // persistent frame. Its result applies to the new code afterward.
            Value value;
            bool jump{};
            std::size_t iteratorPosition = statementIndex;
            const bool iteratorControl = AdvanceIterator(view.frame, statement, view.program,
                iteratorPosition, iterators);
            const bool switchControl = statement.op == 0x05;
            if (iteratorControl) {}
            else if (switchControl) {
                auto selector = Eval(view.frame, statement.children.at(0), self);
                RetainSwitchSelector(selector);
                Locate(view.function, statement.offset, statement.op);
                // Selector callbacks can replace code/locals before Case
                // search captures its own view. Preserve the enclosing live
                // iterator's original storage identity, not that new view.
                if (!iterators.empty()) {
                    const auto* current = host_.ReadState(self);
                    if (current && current->frameOverride && current->frame && HasStateCode(*current->frame) &&
                        (current->frame->codePath != code || current->frame->localsCodePath != localOwner ||
                            host_.StateLocalRevision(self) != localRevision))
                        Fail(Status::Unsupported, "State code or locals changed with live iterators");
                }
                SelectStateSwitchCase(self, selector, iterators);
            }
            else if (statement.op == 0x04) value = Eval(view.frame, statement.children.at(0), self).Load();
            else if (statement.op == 0x06) jump = true;
            else if (statement.op == 0x07) jump = !ToBool(Eval(view.frame, statement.children.at(0), self).Load());
            else if (statement.op == 0x09) {
                if (!ToBool(Eval(view.frame, statement.children.at(0), self).Load()))
                    Fail(Status::Invalid, "Script assert failed at line " + std::to_string(statement.target));
            } else if (statement.op == 0x0d) {
                value = Eval(view.frame, statement.children.at(0), self).Load();
                if (value.kind != Kind::Name && value.kind != Kind::Nothing) TypeError();
            } else if (statement.op != 0x08 && statement.op != 0x15 && statement.op != 0x0a)
                Eval(view.frame, statement, self);
            Locate(view.function, statement.offset, statement.op);
            object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame) return finish(value);
            if (!HasStateCode(*object->frame)) {
                if (object->frame->control) result_.status = Status::Stopped;
                return finish(value);
            }
            if (!iterators.empty() && (object->frame->codePath != code ||
                object->frame->localsCodePath != localOwner || host_.StateLocalRevision(self) != localRevision))
                Fail(Status::Unsupported, "State code or locals changed with live iterators");
            if (iteratorControl) {
                if (iteratorPosition > std::numeric_limits<std::uint32_t>::max())
                    Fail(Status::Invalid, "State iterator statement ordinal overflow");
                host_.MutableState(self)->frame->statementIndex = static_cast<std::uint32_t>(iteratorPosition);
            } else if (jump) {
                const auto& current = *object->frame;
                auto& currentView = StateView(self, current.codePath, current.localsCodePath);
                const auto target = currentView.program.boundaries.find(statement.target);
                if (target == currentView.program.boundaries.end())
                    Fail(Status::Invalid, "State jump is not a current-code statement boundary");
                auto* changed = host_.MutableState(self);
                changed->frame->statementIndex = static_cast<std::uint32_t>(target->second);
            } else if (statement.op == 0x0d) host_.GotoStateLabel(self, value.text, false);
            else if (statement.op == 0x08 || statement.op == 0x15) {
                auto& frame = *host_.MutableState(self)->frame;
                if (frame.control) {
                    if (statement.op == 0x15)
                        Fail(Status::Unsupported, "Original opcode 0x15 state semantics are not established");
                    // Original EX_Stop clears nullable Code only. The running
                    // Node, selected node, live mask, locals and latent remain.
                    frame.control->codePresent = false;
                } else frame.latent = StateLatent::Stop;
                result_.status = Status::Stopped; return finish();
            } else if (statement.op == 0x04) {
                ValidateValue(value, 0); return finish(value);
            }
            object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame) return finish();
            if (!HasStateCode(*object->frame)) {
                if (object->frame->control) result_.status = Status::Stopped;
                return finish();
            }
            if (object->frame->latent != StateLatent::Continue) {
                if (object->frame->latent == StateLatent::Stop) {
                    if (object->frame->control)
                        Fail(Status::Unsupported, "Modern state frame cannot use a synthetic Stop latent action");
                    result_.status = Status::Stopped; return finish();
                }
                if (object->frame->latent == StateLatent::Sleep ||
                    object->frame->latent == StateLatent::WaitForLanding) {
                    result_.status = Status::Waiting; return finish();
                }
                Fail(Status::Unsupported, "State latent action requires its runtime handler");
            }
            if (priorSelected) {
                if (!object->frame->control)
                    Fail(Status::Invalid, "Modern state continuation lost its control metadata");
                const auto& selected = object->frame->control->selectedPath;
                if (selected.empty()) Fail(Status::Invalid, "Modern state continuation lost its selected node");
                if (selected.size() > limits_.stringBytes)
                    Fail(Status::Budget, "VM selected state identity limit");
                // ProcessState observes the final selected StateNode once per
                // completed statement, not changes inside nested callbacks or
                // running-Node relabels. The fifth observed change ends a slice.
                if (Lower(selected) != Lower(*priorSelected)) {
                    priorSelected = selected;
                    if (++selectedChanges > 4u) return finish();
                }
            }
        }
    }
private:
    static bool HasStateCode(const StateFrame& frame) {
        return frame.control ? frame.control->codePresent : !frame.codePath.empty();
    }
    struct ActiveIterator {
        std::unique_ptr<Iterator> iterator;
        std::size_t start{}, end{}, retained{};
    };
    using IteratorStack = std::vector<ActiveIterator>;
    void CheckIteratorStorage(ActiveIterator& iterator) {
        const auto bytes = iterator.iterator->RetainedBytes();
        if (bytes > iterator.retained) {
            Retain(bytes - iterator.retained);
            iterator.retained = bytes;
        }
    }
    bool AdvanceIterator(Frame& frame, const Node& statement, const Program& program,
        std::size_t& pc, IteratorStack& iterators) {
        if (statement.op == 0x30) {
            if (iterators.empty()) Fail(Status::Invalid, "VM IteratorPop without an iterator");
            iterators.pop_back(); ++pc; return true;
        }
        if (statement.op != 0x2f && statement.op != 0x31) return false;
        if (statement.op == 0x2f) {
            if (iterators.size() >= limits_.expressionDepth)
                Fail(Status::Budget, "VM iterator stack limit");
            auto iterator = IteratorFactory(frame, statement.children.at(0), frame.self);
            Locate(frame.function, statement.offset, statement.op);
            if (!iterator) Fail(Status::Invalid, "VM Iterator statement without an iterator");
            Retain(2u * sizeof(ActiveIterator));
            iterators.push_back({std::move(iterator), pc + 1u, program.boundaries.at(statement.target), 0u});
            CheckIteratorStorage(iterators.back());
        } else if (iterators.empty()) Fail(Status::Invalid, "VM IteratorNext without an iterator");
        auto& iterator = iterators.back();
        const bool next = iterator.iterator->Next();
        CheckIteratorStorage(iterator);
        pc = next ? iterator.start : iterator.end;
        return true;
    }
    Value LoadSwitchSelector(const Evaluation& selector) {
        Value value;
        try { value = selector.Load(); }
        catch (const Failure&) { throw; }
        catch (const std::exception& error) {
            Fail(Status::Invalid, std::string("VM Switch selector alias is unavailable: ") + error.what());
        }
        ValidateValue(value, 0);
        return value;
    }
    void RetainSwitchSelector(const Evaluation& selector) {
        // Keep the Evaluation, not just this value snapshot: the pinned
        // Switch result retains a variable alias and reloads it after each
        // visited Case expression. Native returns are already detached.
        Retain(ValueBytes(LoadSwitchSelector(selector)));
        if (selector.reference) Retain(2u * sizeof(Reference));
    }
    bool SwitchMatches(const Evaluation& selector, const Value& label) {
        const auto value = LoadSwitchSelector(selector);
        // ExpressionValue::IsEqual dispatches on its LEFT/selector type. It
        // is not generic Equal (which requires identical portable kinds),
        // nor Byte assignment coercion (which wraps integers to uint8).
        switch (value.kind) {
        case Kind::Nothing:
            return label.kind == Kind::Nothing || (label.kind == Kind::Object && label.text.empty());
        case Kind::Byte: case Kind::Int: return ToInt(value) == ToInt(label);
        case Kind::Float: return ToFloat(value) == ToFloat(label);
        case Kind::Bool: return value.boolean == ToBool(label);
        case Kind::Name:
            if (label.kind != Kind::Name && label.kind != Kind::Nothing) TypeError();
            return NameKey(value.text) == NameKey(label.text);
        case Kind::String:
            if (label.kind != Kind::String && label.kind != Kind::Name && label.kind != Kind::Nothing) TypeError();
            return value.text == (label.kind == Kind::Name && label.text.empty() ? "None" : label.text);
        case Kind::Object:
            if (label.kind != Kind::Object && label.kind != Kind::Nothing) TypeError();
            return Lower(value.text) == Lower(label.text);
        case Kind::Vector:
            if (label.kind != Kind::Vector && label.kind != Kind::Nothing) TypeError();
            return value.vector == (label.kind == Kind::Nothing ? std::array<float, 3>{} : label.vector);
        case Kind::Rotator:
            if (label.kind != Kind::Rotator && label.kind != Kind::Nothing) TypeError();
            return value.rotation == (label.kind == Kind::Nothing ? std::array<std::int32_t, 3>{} : label.rotation);
        case Kind::Struct:
            // The portable value has no UStruct layout identity. Pinned
            // memory-punning ToVector/ToRotator and UStruct::IsEqual cannot
            // safely be inferred from an arbitrary field-name map.
            Fail(Status::Unsupported, "VM Switch generic Struct comparison requires its layout identity");
        case Kind::IntArray:
            Fail(Status::Unsupported, "VM Switch dynamic INT-array comparison is not established");
        }
        Fail(Status::Unsupported, "VM Switch selector kind is unavailable");
    }
    std::size_t SelectSwitchCase(Frame& frame, const Program& program,
        std::size_t pc, const Evaluation& selector) {
        for (;;) {
            if (pc >= program.statements.size() || program.statements[pc].op != 0x0a)
                Fail(Status::Invalid, "VM Switch search is not at a Case statement");
            const auto& label = program.statements[pc];
            Step(frame.function, label.offset, label.op);
            ++pc;
            if (label.target == 0xffffu) return pc;
            const auto value = Eval(frame, label.children.at(0), frame.self).Load();
            Locate(frame.function, label.offset, label.op);
            if (SwitchMatches(selector, value)) return pc;
            pc = program.boundaries.at(label.target);
        }
    }
    void SelectStateSwitchCase(const std::string& self, const Evaluation& selector,
        const IteratorStack& iterators) {
        // ProcessSwitch reads the LIVE Func/StatementIndex, including any
        // changes made by the selector. Each Case advances that ordinal before
        // evaluating its expression. A match preserves callback-selected PC;
        // a miss resolves this old Case's offset in the new live program.
        for (;;) {
            const auto* object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame || !HasStateCode(*object->frame)) return;
            const auto code = object->frame->codePath;
            const auto localOwner = object->frame->localsCodePath;
            const auto revision = host_.StateLocalRevision(self);
            const auto pc = object->frame->statementIndex;
            auto& view = StateView(self, code, localOwner);
            if (pc >= view.program.statements.size() || view.program.statements[pc].op != 0x0a)
                Fail(Status::Invalid, "State Switch search is not at a current-code Case statement");
            const auto& label = view.program.statements[pc];
            Step(view.function, label.offset, label.op);
            auto* writable = host_.MutableState(self);
            if (!writable || !writable->frameOverride || !writable->frame ||
                !HasStateCode(*writable->frame) ||
                writable->frame->codePath != code || writable->frame->localsCodePath != localOwner ||
                writable->frame->statementIndex != pc || host_.StateLocalRevision(self) != revision)
                Fail(Status::Invalid, "State frame changed while advancing its Case");
            if (pc == std::numeric_limits<std::uint32_t>::max())
                Fail(Status::Invalid, "State Case statement ordinal overflow");
            writable->frame->statementIndex = pc + 1u;
            if (label.target == 0xffffu) return;
            const auto value = Eval(view.frame, label.children.at(0), self).Load();
            Locate(view.function, label.offset, label.op);
            object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame || object->frame->codePath.empty())
                Fail(Status::Invalid, "State code was cleared while evaluating a Case");
            if (!HasStateCode(*object->frame)) return;
            if (!iterators.empty() && (object->frame->codePath != code ||
                object->frame->localsCodePath != localOwner || host_.StateLocalRevision(self) != revision))
                Fail(Status::Unsupported, "State code or locals changed with live iterators");
            if (SwitchMatches(selector, value)) return;
            const auto currentCode = object->frame->codePath;
            const auto currentOwner = object->frame->localsCodePath;
            const auto currentRevision = host_.StateLocalRevision(self);
            auto& current = StateView(self, currentCode, currentOwner);
            Locate(view.function, label.offset, label.op);
            const auto target = current.program.boundaries.find(label.target);
            if (target == current.program.boundaries.end() || current.program.statements[target->second].op != 0x0a)
                Fail(Status::Invalid, "State Case link is not a current-code Case statement");
            if (target->second > std::numeric_limits<std::uint32_t>::max())
                Fail(Status::Invalid, "State Case target ordinal overflow");
            writable = host_.MutableState(self);
            if (!writable || !writable->frameOverride || !writable->frame ||
                !HasStateCode(*writable->frame) ||
                writable->frame->codePath != currentCode || writable->frame->localsCodePath != currentOwner ||
                host_.StateLocalRevision(self) != currentRevision)
                Fail(Status::Invalid, "State frame changed while following its Case link");
            writable->frame->statementIndex = static_cast<std::uint32_t>(target->second);
        }
    }
    std::unique_ptr<Iterator> IteratorFactory(Frame& frame, const Node& expression,
        const std::string& context) {
        Step(frame.function, expression.offset, expression.op);
        if (expression.op == 0x19) {
            const auto object = Eval(frame, expression.children.at(0), context).Load();
            Locate(frame.function, expression.offset, expression.op);
            if (object.kind != Kind::Object && object.kind != Kind::Nothing) TypeError();
            return object.text.empty() ? nullptr : IteratorFactory(frame, expression.children.at(1), object.text);
        }
        if (expression.op < 0x60 && expression.op != 0x1b &&
            expression.op != 0x1c && expression.op != 0x38)
            Fail(Status::Unsupported, "VM iterator factory is not a native function call");
        std::uint16_t index = expression.native;
        std::shared_ptr<const Function> identity;
        if (expression.op < 0x60) {
            Invocation call;
            call.kind = expression.op == 0x1c ? CallKind::Final :
                expression.op == 0x38 ? CallKind::Global : CallKind::Virtual;
            call.reference = expression.reference;
            if (expression.op != 0x1c) call.name = host_.ResolveName(frame.function, expression.reference);
            identity = host_.ResolveFunction(frame.function, context, call);
            if (!identity) Fail(Status::Unsupported, "Unresolved VM iterator function call " + call.name);
        }
        std::vector<Evaluation> arguments;
        for (const auto& argument : expression.children)
            arguments.push_back(Eval(frame, argument, frame.self));
        Locate(frame.function, expression.offset, expression.op);
        std::shared_ptr<const Function> declaration;
        if (identity) {
            if (context.empty() || !host_.CanCall(*identity, context)) return {};
            if (result_.callStack.size() >= limits_.callDepth) Fail(Status::Budget, "VM iterator call depth limit");
            declaration = host_.PrepareFunction(*identity, context);
            if (!declaration || declaration->path != identity->path || declaration->source != identity->source)
                Fail(Status::Invalid, "Prepared VM iterator function changed callable identity");
            if ((declaration->flags & 0x404u) != 0x404u || (declaration->flags & 0x8u) != 0u)
                Fail(Status::Unsupported, "VM iterator factory is not a synchronous native iterator");
            index = declaration->nativeIndex;
            std::size_t count{};
            for (const auto& property : declaration->variables) {
                if (!(property.flags & 0x80u) || (property.flags & 0x400u)) continue;
                if (count == arguments.size()) {
                    if (!(property.flags & 0x10u)) Fail(Status::Invalid, "Missing required VM iterator parameter " + property.name);
                    arguments.push_back({});
                }
                ++count;
            }
            if (arguments.size() > count && !declaration->variables.empty())
                Fail(Status::Invalid, "Excess VM iterator arguments");
        }
        for (auto& argument : arguments) {
            const auto value = argument.Load(); ValidateValue(value, 0); Retain(ValueBytes(value));
            if (argument.reference) Retain(sizeof(Reference) * 2u);
            argument.reference = GuardReference(argument.reference);
        }
        if (declaration) {
            result_.callStack.push_back(declaration->path);
            Step(*declaration, 0u, 0u);
        }
        auto iterator = host_.CreateIterator(index, context, arguments, declaration.get());
        if (declaration) result_.callStack.pop_back();
        return iterator;
    }
    struct StateExecutionView {
        std::shared_ptr<const Function> owner;
        const Function& function;
        Program program;
        Frame frame;
        StateExecutionView(std::shared_ptr<const Function> f, Program p, std::string self)
            : owner(std::move(f)), function(*owner), program(std::move(p)), frame{function, std::move(self), {}} {}
    };
    std::map<std::tuple<std::string, std::string, std::string, std::uint64_t>, std::unique_ptr<StateExecutionView>> stateViews_;
    StateExecutionView& StateView(const std::string& self, const std::string& code, const std::string& localOwner) {
        if (code.size() > limits_.stringBytes || localOwner.size() > limits_.stringBytes)
            Fail(Status::Budget, "VM state code identity limit");
        if (localOwner.empty()) {
            const auto* object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame || !object->frame->control ||
                !object->frame->locals.empty())
                Fail(Status::Invalid, "Null state locals owner cannot provide retained locals");
        }
        const auto key = std::make_tuple(self, code, localOwner, host_.StateLocalRevision(self));
        const auto found = stateViews_.find(key);
        if (found != stateViews_.end()) return *found->second;
        auto function = host_.StateProgram(self, code);
        if (!function || function->path != code || function->source.empty() || (function->flags & 0x400u))
            Fail(Status::Invalid, "State program changed code identity or is native");
        if (localOwner.empty()) {
            if (!function->variables.empty())
                Fail(Status::Invalid, "Null state locals owner cannot provide declared locals");
        }
        const auto locals = localOwner.empty() || localOwner == code ? function : host_.StateProgram(self, localOwner);
        if (!locals || (!localOwner.empty() && locals->path != localOwner) || locals->source.empty() || (locals->flags & 0x400u))
            Fail(Status::Invalid, "State locals owner changed identity or is native");
        Retain(sizeof(StateExecutionView) + self.size() + code.size() + localOwner.size() + function->bytecode.size());
        auto view = std::make_unique<StateExecutionView>(function,
            Parser(host_, *function, limits_, nodes_, result_, false, &retainedBytes_).Parse(), self);
        for (const auto& property : locals->variables) {
            if (property.arrayDimension == 0 || property.arrayDimension > limits_.localElements ||
                localElements_ > limits_.localElements - property.arrayDimension)
                Fail(Status::Budget, "VM state local element limit");
            ValidateValue(property.zero, 0);
            auto reference = BindResultLayout(host_.StateVariable(self, property),property.resultLayout);
            if (!reference || !reference->read || !reference->write || reference->dimension != property.arrayDimension)
                Fail(Status::Invalid, "State local storage/schema mismatch");
            localElements_ += property.arrayDimension;
            Retain(sizeof(Reference) + property.key.size());
            for (std::size_t index = 0; index < property.arrayDimension; ++index) {
                const auto element = index ? BindResultLayout(reference->element ? reference->element(index) : nullptr,
                    property.resultLayout) : reference;
                if (!element || !element->read) Fail(Status::Invalid, "State array local storage unavailable");
                const auto stored = element->read(); ValidateValue(stored, 0);
                ValidateResultLayout(property.zero,property.resultLayout);ValidateResultLayout(stored,property.resultLayout);
                if (!Equal(Coerce(stored, property.zero), stored)) Fail(Status::Invalid, "State local has incorrect stored type");
                Retain(ValueBytes(stored));
            }
            if (!view->frame.locals.emplace(property.key, std::move(reference)).second)
                Fail(Status::Invalid, "Duplicate state local identity");
        }
        auto* result = view.get(); stateViews_.emplace(key, std::move(view)); return *result;
    }
    Host& host_;
    const Limits& limits_;
    Result& result_;
    std::size_t nodes_{}, localElements_{}, retainedBytes_{}, operations_{};
    bool accepted_{};
    bool nativeSeedHookActive_{};
    std::exception_ptr failure_;
    struct FailureLocation {
        std::string function;
        std::size_t offset{};
        std::uint8_t opcode{};
        std::vector<std::string> callStack;
    };
    std::optional<FailureLocation> failureLocation_;
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
    std::vector<std::pair<std::shared_ptr<Reference>, Value>> journal_;
    void RememberFailure() {
        if (!failure_) {
            failureLocation_ = FailureLocation{result_.function, result_.offset, result_.opcode, result_.callStack};
            failure_ = std::current_exception();
        }
    }
    template<class Operation> Value DirectOperation(Operation&& operation) {
        CheckFailure();
        try {
            if (nativeSeedHookActive_)
                Fail(Status::Invalid, "VM callback during read-only native seed capture");
            if (operations_ >= limits_.operations) Fail(Status::Budget, "VM operation entry limit");
            ++operations_;
            auto result = operation();
            CheckFailure();
            return result;
        } catch (...) {
            // A host coordinator/native cannot suppress one failed callback
            // and commit an earlier prefix using the damaged interpreter.
            RememberFailure();
            throw;
        }
    }
    template<class Operation> auto ReadOnlyNativeSeedOperation(Operation&& operation) -> decltype(operation()) {
        const auto previous = std::exchange(nativeSeedHookActive_, true);
        try {
            auto value = operation();
            nativeSeedHookActive_ = previous;
            CheckFailure();
            return value;
        } catch (...) {
            nativeSeedHookActive_ = previous;
            RememberFailure();
            throw;
        }
    }
    [[noreturn]] void Fail(Status status, const std::string& text) { throw Failure(status, text); }
    void Locate(const Function& fn, std::size_t offset, std::uint8_t op) {
        result_.function = fn.path; result_.offset = offset; result_.opcode = op;
    }
    void Step(const Function& fn, std::size_t offset, std::uint8_t op) {
        Locate(fn, offset, op);
        if (result_.instructions >= limits_.instructions) Fail(Status::Budget, "VM instruction limit");
        ++result_.instructions;
    }
    void Retain(std::size_t bytes) {
        if (bytes > limits_.retainedBytes || retainedBytes_ > limits_.retainedBytes - bytes)
            Fail(Status::Budget, "VM retained byte limit");
        retainedBytes_ += bytes;
    }
    std::size_t IntArrayBytes(const Value& v) {
        if (v.intArray.size() > limits_.intArrayElements || v.intArray.capacity() > limits_.intArrayElements ||
            v.intArray.capacity() > limits_.retainedBytes / sizeof(std::int32_t))
            Fail(Status::Budget, "VM dynamic INT-array storage limit");
        return v.intArray.capacity() * sizeof(std::int32_t);
    }
    std::size_t ValueOwnBytes(const Value& v) {
        const auto arrayBytes = IntArrayBytes(v);
        if (sizeof(Value) > limits_.retainedBytes || v.text.size() > limits_.retainedBytes - sizeof(Value) ||
            arrayBytes > limits_.retainedBytes - sizeof(Value) - v.text.size())
            Fail(Status::Budget, "VM value byte limit");
        return sizeof(Value) + v.text.size() + arrayBytes;
    }
    std::size_t ValueIntArrayBytes(const Value& v) {
        auto bytes = IntArrayBytes(v);
        for (const auto& [key, field] : v.fields) {
            const auto childBytes = ValueIntArrayBytes(field);
            if (bytes > limits_.retainedBytes - childBytes)
                Fail(Status::Budget, "VM aggregate dynamic INT-array byte limit");
            bytes += childBytes;
        }
        return bytes;
    }
    std::size_t ValueBytes(const Value& v) {
        std::size_t bytes = ValueOwnBytes(v);
        for (const auto& [key, field] : v.fields) {
            const auto childBytes = ValueBytes(field);
            if (bytes > limits_.retainedBytes || childBytes > limits_.retainedBytes ||
                key.size() > limits_.retainedBytes - childBytes ||
                bytes > limits_.retainedBytes - childBytes - key.size())
                Fail(Status::Budget, "VM value byte limit");
            bytes += childBytes + key.size();
        }
        return bytes;
    }
    void ValidateValue(const Value& v, std::size_t depth) {
        std::size_t bytes = 0, nodes = 0;
        ValidateValueTree(v, depth, bytes, nodes);
    }
    void ValidateValueTree(const Value& v, std::size_t depth, std::size_t& bytes, std::size_t& nodes) {
        if (depth >= limits_.expressionDepth || v.text.size() > limits_.stringBytes || v.fields.size() > limits_.localElements)
            Fail(Status::Budget, "VM value storage limit");
        const auto ownBytes = ValueOwnBytes(v);
        if (++nodes > limits_.localElements || bytes > limits_.retainedBytes - ownBytes)
            Fail(Status::Budget, "VM aggregate value limit");
        bytes += ownBytes;
        if (v.kind == Kind::Float && !std::isfinite(v.floating)) Fail(Status::Invalid, "Non-finite VM float");
        if (v.kind == Kind::Vector)
            for (float f : v.vector) if (!std::isfinite(f)) Fail(Status::Invalid, "Non-finite VM vector");
        for (const auto& [key, field] : v.fields) {
            if (key.size() > limits_.stringBytes) Fail(Status::Budget, "VM member name limit");
            if (key.size() > limits_.retainedBytes || bytes > limits_.retainedBytes - key.size())
                Fail(Status::Budget, "VM aggregate member byte limit");
            bytes += key.size();
            ValidateValueTree(field, depth + 1, bytes, nodes);
        }
    }
    void ValidateResultLayout(const Value& value, const ResultLayout layout) {
        if (layout==ResultLayout::Unknown) return;
        if (layout!=ResultLayout::CoreColor4)
            Fail(Status::Invalid,"VM unknown trusted result layout");
        if (value.kind!=Kind::Struct || value.fields.size()!=4u || !value.text.empty() || !value.intArray.empty())
            Fail(Status::Invalid,"VM CoreColor4 destination is not canonical Color storage");
        for (const auto* channel:{"r","g","b","a"}) {
            const auto found=value.fields.find(channel);
            if (found==value.fields.end() || found->second.kind!=Kind::Byte ||
                found->second.integer<0 || found->second.integer>255 || !found->second.text.empty() ||
                !found->second.fields.empty() || !found->second.intArray.empty())
                Fail(Status::Invalid,"VM CoreColor4 destination has malformed Byte channels");
        }
    }
    std::shared_ptr<Reference> BindResultLayout(const std::shared_ptr<Reference>& reference, const ResultLayout layout) {
        if (!reference || reference->resultLayout==layout) return reference;
        // The resolved Property supplies authority. Never grant a capability
        // based on a same-shaped value or retain a host reference's conflicting
        // capability for an Unknown property. Do not mutate host-owned aliases.
        Retain(sizeof(Reference));
        auto bound=std::make_shared<Reference>(*reference);bound->resultLayout=layout;
        return bound;
    }
    void Write(const std::shared_ptr<Reference>& reference, const Value& value) {
        if (nativeSeedHookActive_) {
            try { Fail(Status::Invalid, "VM write during read-only native seed capture"); }
            catch (...) { RememberFailure(); throw; }
        }
        if (!reference || !reference->write) Fail(Status::Invalid, "VM assignment is not writable");
        if (result_.writes >= limits_.writes) Fail(Status::Budget, "VM write limit");
        ++result_.writes;
        // Check caller-owned capacity before Coerce's detached copy can shrink
        // it, and account native replacement payloads as well as rollback data.
        ValidateValue(value, 0); ValidateValue(reference->zero, 0);
        ValidateResultLayout(reference->zero,reference->resultLayout);
        if (value.kind!=Kind::Nothing) ValidateResultLayout(value,reference->resultLayout);
        auto typed = Coerce(value, reference->zero); ValidateValue(typed, 0);
        ValidateResultLayout(typed,reference->resultLayout);
        auto previous = reference->read(); ValidateValue(previous, 0);
        ValidateResultLayout(previous,reference->resultLayout);
        Retain(ValueBytes(previous));
        Retain(ValueIntArrayBytes(typed));
        journal_.emplace_back(reference, std::move(previous));
        reference->write(typed);
    }
    std::shared_ptr<Reference> GuardReference(const std::shared_ptr<Reference>& reference) {
        if (!reference) return {};
        auto guarded = std::make_shared<Reference>(*reference);
        const std::weak_ptr<bool> lifetime = alive_;
        const auto requireAlive = [lifetime] {
            const auto alive = lifetime.lock();
            if (!alive || !*alive) throw std::runtime_error("Expired VM native reference");
        };
        guarded->read = [requireAlive, reference] { requireAlive(); return reference->read(); };
        guarded->write = [this, requireAlive, reference](const Value& v) {
            requireAlive();
            try { Write(reference, v); }
            catch (...) { RememberFailure(); throw; }
        };
        if (reference->element)
            guarded->element = [this, requireAlive, reference](std::size_t i) {
                requireAlive(); return GuardReference(reference->element(i));
            };
        return guarded;
    }
    Value ResultValue(const Evaluation& evaluation, const Value& destination,
        const ResultLayout layout = ResultLayout::Unknown, const bool nativeSeed = false) {
        ValidateResultLayout(destination,layout);
        if (evaluation.disposition == ResultDisposition::Written) {
            auto value=evaluation.Load();
            if (value.kind!=Kind::Nothing) ValidateResultLayout(value,layout);
            return value;
        }
        if (evaluation.disposition == ResultDisposition::NoWrite || evaluation.zeroFillBytes == 0u)
            return destination;
        std::size_t bytes{};
        Value zero;
        switch (destination.kind) {
        case Kind::Byte: bytes=1u; zero=Value::Byte(0u); break;
        case Kind::Int: bytes=4u; zero=Value::Integer(0); break;
        case Kind::Float: bytes=4u; zero=Value::Float(0.0f); break;
        case Kind::Bool: bytes=4u; zero=Value::Bool(false); break;
        case Kind::Name: bytes=4u; zero=Value::Text(Kind::Name,"None"); break;
        case Kind::Object: bytes=4u; zero=Value::Text(Kind::Object,{}); break;
        case Kind::String: bytes=12u; zero=Value::Text(Kind::String,{}); break;
        case Kind::Vector: bytes=12u; zero=Value::Vector({}); break;
        case Kind::Rotator: bytes=12u; zero=Value::Rotator({}); break;
        case Kind::Struct:
            if (layout!=ResultLayout::CoreColor4)
                Fail(Status::Unsupported,"VM context zero-fill Struct destination layout is not established");
            bytes=4u;zero=destination;
            for (auto& [name,channel]:zero.fields) channel=Value::Byte(0u);
            break;
        default: Fail(Status::Unsupported,"VM context zero-fill destination layout is not established");
        }
        // Retail writes low-address bytes in its little-endian native result
        // temporary. Only these three scalar native-seed layouts have an
        // audited portable partial-write representation. Ordinary script
        // destinations, packed bools, identities and pointer/aggregate layouts
        // retain the existing explicit partial-write refusal.
        if (nativeSeed && evaluation.zeroFillBytes < bytes &&
            (destination.kind == Kind::Byte || destination.kind == Kind::Int || destination.kind == Kind::Float)) {
            const unsigned bits = static_cast<unsigned>(evaluation.zeroFillBytes) * 8u;
            const auto mask = ~((std::uint32_t{1u} << bits) - 1u);
            return destination.kind == Kind::Float ?
                Value::Float(std::bit_cast<float>(std::bit_cast<std::uint32_t>(destination.floating) & mask)) :
                Value::Integer(std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(destination.integer) & mask));
        }
        // Never silently widen a partial or overflowing write into typed zero.
        if (evaluation.zeroFillBytes != bytes)
            Fail(Status::Unsupported,"VM partial context zero-fill destination is not established");
        return zero;
    }
    void RetainNativeSeedCapacity(const Value& seed) {
        // ValueBytes already counts logical text/member bytes and audited
        // INT-array capacity. A host-returned detached seed may nevertheless
        // reserve much more string storage; bound that retained allocation too.
        Retain(seed.text.capacity() - seed.text.size());
        for (const auto& [key, field] : seed.fields) {
            Retain(key.capacity() - key.size());
            RetainNativeSeedCapacity(field);
        }
    }
    void ReserveNativeInputPrefix(std::vector<Value>& prefix, const std::size_t count) {
        if (count>limits_.arguments || count>limits_.retainedBytes/sizeof(Value))
            Fail(Status::Budget,"VM native input prefix storage limit");
        Retain(count*sizeof(Value));prefix.reserve(count);
        if (prefix.capacity()>limits_.retainedBytes/sizeof(Value))
            Fail(Status::Budget,"VM native input prefix capacity limit");
        Retain((prefix.capacity()-count)*sizeof(Value));
    }
    void CaptureNativeInput(std::vector<Value>& prefix,const Value& value,std::size_t& bytes,std::size_t& nodes) {
        prefix.push_back(value);
        ValidateValueTree(prefix.back(),0,bytes,nodes);
        Retain(ValueBytes(prefix.back())-sizeof(Value));RetainNativeSeedCapacity(prefix.back());
    }
    ResultLayout NativeParameterLayout(const Function* declaration,const std::size_t slot) {
        if (!declaration) return ResultLayout::Unknown;
        std::size_t index{};
        for (const auto& property:declaration->variables) {
            if (!(property.flags&0x80u) || (property.flags&0x400u)) continue;
            if (index++==slot) return property.resultLayout;
        }
        return ResultLayout::Unknown;
    }
    std::vector<std::optional<Value>> CaptureNativeArgumentSeeds(const std::uint16_t index,
        const std::string& receiver,const std::size_t count,const Function* declaration,
        const std::vector<std::optional<ResultLayout>>& uninitializedLayouts,
        const std::vector<Property>* parameters=nullptr) {
        if (count>limits_.arguments) Fail(Status::Budget,"VM native argument seed count limit");
        std::vector<std::optional<Value>> seeds;
        for (std::size_t slot=0u;slot<count;++slot) {
            auto seed=ReadOnlyNativeSeedOperation([&] {
                return host_.NativeArgumentSeed(index,receiver,slot,declaration);
            });
            if (!seed) continue;
            if (!uninitializedLayouts.empty() && uninitializedLayouts[slot])
                Fail(Status::Invalid,"VM uninitialized native argument conflicts with an initialized seed");
            ValidateValue(*seed,0);Retain(ValueBytes(*seed));RetainNativeSeedCapacity(*seed);
            ValidateResultLayout(*seed,parameters && slot<parameters->size() ?
                (*parameters)[slot].resultLayout : NativeParameterLayout(declaration,slot));
            // No allocation on the ordinary no-default hot path.
            if (seeds.empty()) {
                if (count>limits_.retainedBytes/sizeof(std::optional<Value>))
                    Fail(Status::Budget,"VM native argument seed storage limit");
                Retain(count*sizeof(std::optional<Value>));seeds.resize(count);
                if (seeds.capacity()>limits_.retainedBytes/sizeof(std::optional<Value>))
                    Fail(Status::Budget,"VM native argument seed capacity limit");
                Retain((seeds.capacity()-count)*sizeof(std::optional<Value>));
            }
            seeds[slot]=std::move(seed);
        }
        return seeds;
    }
    std::vector<std::optional<ResultLayout>> CaptureUninitializedNativeLayouts(const std::uint16_t index,
        const std::string& receiver,const std::size_t count,const Function* declaration,
        const std::vector<Property>* parameters=nullptr) {
        if (count>limits_.arguments) Fail(Status::Budget,"VM uninitialized native argument count limit");
        std::vector<std::optional<ResultLayout>> layouts;
        for (std::size_t slot=0u;slot<count;++slot) {
            const auto layout=ReadOnlyNativeSeedOperation([&] {
                return host_.NativeArgumentUninitializedLayout(index,receiver,slot,declaration);
            });
            if (!layout) continue;
            if (*layout==ResultLayout::Unknown)
                Fail(Status::Unsupported,"VM uninitialized native argument layout is not established");
            if (*layout!=ResultLayout::CoreColor4)
                Fail(Status::Invalid,"VM unknown uninitialized native argument layout");
            if (parameters && slot<parameters->size() && (*parameters)[slot].resultLayout!=*layout)
                Fail(Status::Invalid,"VM uninitialized native argument declaration layout mismatch");
            if (layouts.empty()) {
                if (count>limits_.retainedBytes/sizeof(std::optional<ResultLayout>))
                    Fail(Status::Budget,"VM uninitialized native argument layout storage limit");
                Retain(count*sizeof(std::optional<ResultLayout>));layouts.resize(count);
                if (layouts.capacity()>limits_.retainedBytes/sizeof(std::optional<ResultLayout>))
                    Fail(Status::Budget,"VM uninitialized native argument layout capacity limit");
                Retain((layouts.capacity()-count)*sizeof(std::optional<ResultLayout>));
            }
            layouts[slot]=layout;
        }
        return layouts;
    }
    Evaluation MaterializeUninitializedNativeArgument(const Evaluation& argument,const ResultLayout layout) {
        if (layout!=ResultLayout::CoreColor4)
            Fail(Status::Unsupported,"VM uninitialized native argument layout is not established");
        Value value;
        if (argument.disposition==ResultDisposition::Written) {
            if (!argument.reference) {
                ValidateValue(argument.value,0);RetainNativeSeedCapacity(argument.value);
            }
            value=argument.Load();ValidateValue(value,0);ValidateResultLayout(value,layout);
        } else if (argument.disposition==ResultDisposition::ZeroFill && argument.zeroFillBytes==4u) {
            // Construct storage only AFTER a complete four-byte write is known.
            // No declared zero or arbitrary seed initializes an unwritten temp.
            value.kind=Kind::Struct;
            for (const auto* channel:{"r","g","b","a"}) value.fields.emplace(channel,Value::Byte(0u));
            ValidateValue(value,0);ValidateResultLayout(value,layout);
        } else Fail(Status::Unsupported,"VM uninitialized native argument requires a complete result write");
        Retain(ValueBytes(value));RetainNativeSeedCapacity(value);
        return {std::move(value),{}};
    }
    std::optional<Value> BuiltinArgumentZero(const std::uint16_t index,const std::size_t argument) {
        const auto zero=[](const Kind kind) {
            Value value;value.kind=kind;if(kind==Kind::Name)value.text="None";return value;
        };
        switch(index) {
        case 129: case 130: case 131: case 132: case 242: case 243:return zero(Kind::Bool);
        case 114: case 119:return zero(Kind::Object);
        case 254: case 255:return zero(Kind::Name);
        case 112: case 168: case 122: case 123: case 124: case 125: case 126: case 235:return zero(Kind::String);
        case 127: case 128: case 234:return zero(argument==0u ? Kind::String : Kind::Int);
        case 143: case 144: case 145: case 146: case 147: case 150: case 151: case 152: case 153:
        case 154: case 155: case 156: case 157: case 158: case 161: case 162: case 163: case 164:
        case 165: case 166: case 249: case 250: case 251:return zero(Kind::Int);
        case 169: case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177:
        case 178: case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187: case 189:
        case 244: case 245: case 246:return zero(Kind::Float);
        case 212: case 214: case 221:return zero(argument==0u ? Kind::Vector : Kind::Float);
        case 213:return zero(argument==0u ? Kind::Float : Kind::Vector);
        case 215: case 216: case 217: case 218: case 219: case 220: case 223: case 224: case 225:
        case 296:return zero(Kind::Vector);
        default:return std::nullopt;
        }
    }
    Evaluation Eval(Frame& frame, const Node& n, const std::string& context) {
        Step(frame.function, n.offset, n.op);
        Evaluation result;
        const auto child = [&](std::size_t i) {
            auto value = Eval(frame, n.children.at(i), context);
            Locate(frame.function, n.offset, n.op);
            return value;
        };
        if (n.op >= 0x60 || n.op == 0x1b || n.op == 0x1c || n.op == 0x38) {
            std::uint16_t index = n.native;
            std::shared_ptr<const Function> fn;
            if (n.op < 0x60) {
                Invocation call;
                call.kind = n.op == 0x1c ? CallKind::Final : n.op == 0x38 ? CallKind::Global : CallKind::Virtual;
                call.reference = n.reference;
                if (n.op != 0x1c) call.name = host_.ResolveName(frame.function, n.reference);
                fn = host_.ResolveFunction(frame.function, context, call);
                if (!fn) Fail(Status::Unsupported, "Unresolved VM function call " + call.name);
                if (fn->flags & 0x400) index = fn->nativeIndex;
            }
            std::optional<std::optional<Value>> capturedContext;
            capturedContext.emplace(!fn || (fn->flags & 0x400u) ?
                CaptureNativeContext(index, context, fn.get()) : std::nullopt);
            const auto parameters=fn ? host_.FunctionParameters(*fn) : std::vector<Property>{};
            if (parameters.size()>limits_.arguments) Fail(Status::Budget,"VM argument schema count limit");
            Retain(parameters.capacity()*sizeof(Property));
            for (const auto& parameter:parameters) {
                ValidateValue(parameter.zero,0);
                ValidateResultLayout(parameter.zero,parameter.resultLayout);
                Retain(parameter.key.capacity());Retain(parameter.name.capacity());Retain(ValueBytes(parameter.zero));
            }
            const bool nativeCall = !fn || (fn->flags & 0x400u);
            const auto uninitializedLayouts=nativeCall ?
                CaptureUninitializedNativeLayouts(index,context,n.children.size(),fn.get(),&parameters) :
                std::vector<std::optional<ResultLayout>>{};
            const bool sequentialSeeds = nativeCall && ReadOnlyNativeSeedOperation([&] {
                return host_.HasSequentialNativeArgumentSeeds(index, context, fn.get());
            });
            const auto nativeSeeds=nativeCall ? CaptureNativeArgumentSeeds(index,context,n.children.size(),
                fn.get(),uninitializedLayouts,&parameters) : std::vector<std::optional<Value>>{};
            std::vector<Value> capturedInputs;
            std::size_t capturedInputBytes{}, capturedInputNodes{};
            if (sequentialSeeds) ReserveNativeInputPrefix(capturedInputs,n.children.size());
            std::vector<Evaluation> args;
            for (std::size_t i = 0; i < n.children.size(); ++i) {
                // Retail CallFunction evaluates arguments with FFrame.Object,
                // not the incoming opcode receiver. &&/|| alone skip RHS.
                if (i == 1 && (index == 130 || index == 132) && (!fn || (fn->flags & 0x400))) {
                    const bool a = ToBool(args[0].Load());
                    if ((index == 130 && !a) || (index == 132 && a)) {
                        result.value = Value::Bool(a); return result;
                    }
                }
                if (sequentialSeeds) Locate(frame.function, n.offset, n.op);
                auto sequentialSeed = sequentialSeeds ? ReadOnlyNativeSeedOperation([&] {
                    return host_.SequentialNativeArgumentSeed(index, context, i, fn.get(), capturedInputs);
                }) : std::optional<Value>{};
                if (sequentialSeed) {
                    if (!uninitializedLayouts.empty() && uninitializedLayouts[i])
                        Fail(Status::Invalid,"VM uninitialized native argument conflicts with an initialized seed");
                    ValidateValue(*sequentialSeed,0); Retain(ValueBytes(*sequentialSeed));
                    RetainNativeSeedCapacity(*sequentialSeed);
                    if (i<parameters.size()) ValidateResultLayout(*sequentialSeed,parameters[i].resultLayout);
                }
                auto argument = Eval(frame, n.children[i], frame.self);
                // Explicit EX_Nothing is the native optional-argument marker,
                // not a null Context writing into a typed argument slot. Keep
                // it intact for the declaration-aware native reader/default.
                const bool nativeOmission=nativeCall && n.children[i].op==0x0bu;
                if (!uninitializedLayouts.empty() && uninitializedLayouts[i])
                    argument=MaterializeUninitializedNativeArgument(argument,*uninitializedLayouts[i]);
                else if (argument.disposition!=ResultDisposition::Written && !nativeOmission) {
                    const bool entrySeeded = nativeCall && !nativeSeeds.empty() && nativeSeeds[i].has_value();
                    const auto builtinZero=nativeCall ? BuiltinArgumentZero(index,i) : std::nullopt;
                    const bool seeded = sequentialSeed.has_value() || entrySeeded || builtinZero.has_value();
                    const auto typedZero=sequentialSeed ? sequentialSeed : entrySeeded ? nativeSeeds[i] :
                        builtinZero ? builtinZero : i<parameters.size() ? std::optional<Value>{parameters[i].zero} : std::nullopt;
                    if (typedZero) {
                        ValidateValue(*typedZero,0);Retain(ValueBytes(*typedZero));
                        const auto layout=i<parameters.size()?parameters[i].resultLayout:ResultLayout::Unknown;
                        argument={ResultValue(argument,*typedZero,layout,seeded),{}};
                    } else if (argument.disposition==ResultDisposition::ZeroFill && argument.zeroFillBytes!=0u)
                        Fail(Status::Unsupported,"VM context argument result destination layout is unavailable");
                }
                const bool snapshotInput = nativeCall && SnapshotNativeInput(index, fn.get());
                const bool copiedOutTemporary = nativeCall && CopyNativeOutTemporary(index);
                if (snapshotInput || sequentialSeeds || copiedOutTemporary) {
                    auto value = argument.Load(); ValidateValue(value, 0); Retain(ValueBytes(value));
                    if (sequentialSeeds) CaptureNativeInput(capturedInputs,value,capturedInputBytes,capturedInputNodes);
                    if (snapshotInput) argument = {std::move(value),{}};
                    else if (copiedOutTemporary) argument.value=std::move(value);
                }
                args.push_back(std::move(argument));
            }
            Locate(frame.function, n.offset, n.op);
            result = fn ? Evaluation{Run(*fn, context, args, capturedContext,nativeCall), {}} :
                Native(index, context, args, nullptr, *capturedContext);
        } else if (n.op == 0x53) {
            // Retail Core.execIntToString10131600 seeds an INT0 temporary and
            // steps exactly one operand using FFrame.Object, even inside a
            // different opcode Context. NoWrite/partial null-context fills
            // affect that temporary, never a stale returned property alias.
            const auto operand=Eval(frame,n.children.at(0),frame.self);
            Locate(frame.function,n.offset,n.op);
            const auto value=ResultValue(operand,Value::Integer(0),ResultLayout::Unknown,true);
            // The defined pinned evaluator also accepts Byte/Float/Nothing via
            // ToInt. This bounded portable policy is not retail raw-memory
            // reinterpretation of malformed typed Float operands.
            result.value=Value::Text(Kind::String,std::to_string(ToInt(value)));
        } else if (n.op >= 0x39) {
            // Retail ObjectToString explicitly evaluates its operand using
            // FFrame.Object even when the enclosing opcode receiver changed.
            const auto v = (n.op == 0x56 ?
                Eval(frame, n.children.at(0), frame.self) : child(0)).Load();
            Locate(frame.function, n.offset, n.op);
            switch (n.op) {
            case 0x39: {
                // Pinned ExpressionEvaluator::RotatorToVector uses exactly
                // Coords::Rotation(...).XAxis, with Nothing as rot(0,0,0).
                // Never reinterpret an arbitrary generic Struct as a Rotator.
                if (v.kind != Kind::Rotator && v.kind != Kind::Nothing) TypeError();
                const auto rotation = v.kind == Kind::Nothing
                    ? std::array<std::int32_t, 3>{} : v.rotation;
                const auto direction = Coords::Rotation(
                    Rotator(rotation[0], rotation[1], rotation[2])).XAxis;
                result.value = Value::Vector({direction.x, direction.y, direction.z});
                break;
            }
            case 0x3a: result.value = Value::Integer(static_cast<std::uint8_t>(ToInt(v))); break;
            case 0x3b: result.value = Value::Bool(static_cast<std::uint8_t>(ToInt(v)) != 0); break;
            case 0x3c: result.value = Value::Float(static_cast<std::uint8_t>(ToInt(v))); break;
            case 0x3d: result.value = Value::Byte(static_cast<std::uint8_t>(ToInt(v))); break;
            case 0x3e: result.value = Value::Bool(ToInt(v) != 0); break;
            case 0x3f: result.value = Value::Float(static_cast<float>(ToInt(v))); break;
            case 0x43: result.value = Value::Byte(static_cast<std::uint8_t>(Truncate(ToFloat(v)))); break;
            case 0x44: result.value = Value::Integer(Truncate(ToFloat(v))); break;
            case 0x45: result.value = Value::Bool(ToFloat(v) != 0); break;
            case 0x40: result.value = Value::Byte(ToBool(v) ? 1 : 0); break;
            case 0x41: result.value = Value::Integer(ToBool(v) ? 1 : 0); break;
            case 0x42: result.value = Value::Float(ToBool(v) ? 1.0f : 0.0f); break;
            case 0x47:
                if (v.kind != Kind::Object && v.kind != Kind::Nothing) TypeError();
                result.value = Value::Bool(!v.text.empty()); break;
            case 0x48:
                if (v.kind != Kind::Name && v.kind != Kind::Nothing) TypeError();
                result.value = Value::Bool(!v.text.empty() && Lower(v.text) != "none"); break;
            case 0x57:
                if (v.kind != Kind::Name && v.kind != Kind::Nothing) TypeError();
                result.value = Value::Text(Kind::String, v.text.empty() ? "None" : v.text); break;
            case 0x49: case 0x4a: case 0x4b: case 0x4c: {
                if (v.kind != Kind::String && v.kind != Kind::Name && v.kind != Kind::Nothing) TypeError();
                if (n.op == 0x4c) result.value = Value::Float(static_cast<float>(std::strtod(v.text.c_str(), nullptr)));
                else {
                    const auto parsed = ParseInteger(v.text);
                    result.value = n.op == 0x49 ? Value::Byte(static_cast<std::uint8_t>(parsed)) :
                        n.op == 0x4a ? Value::Integer(parsed) : Value::Bool(parsed != 0);
                }
                break;
            }
            case 0x5a:
                if (v.kind != Kind::String && v.kind != Kind::Name && v.kind != Kind::Nothing) TypeError();
                result.value = Value::Text(Kind::Name, v.text.empty() ? "None" : v.text); break;
            case 0x56:
                if (v.kind!=Kind::Object && v.kind!=Kind::Nothing) TypeError();
                result.value=Value::Text(Kind::String,v.text.empty() ? "None" : host_.ObjectString(v.text)); break;
            default: Fail(Status::Unsupported, "Unsupported VM conversion " + std::to_string(n.op));
            }
        } else {
            switch (n.op) {
            case 0x00: case 0x01: case 0x02: {
                const auto p = host_.ResolveProperty(frame.function, n.reference);
                if (n.op == 0x00) {
                    const auto found = frame.locals.find(p.key);
                    if (found == frame.locals.end()) Fail(Status::Invalid, "VM local not declared: " + p.key);
                    result.reference = found->second;
                } else result.reference = BindResultLayout(host_.Variable(context, p,
                    n.op == 0x02 ? Scope::Default : Scope::Instance),p.resultLayout);
                if (!result.reference || !result.reference->read) Fail(Status::Unsupported, "VM property unavailable: " + p.key);
                break;
            }
            case 0x0b: result.disposition=ResultDisposition::NoWrite; break;
            case 0x0e: child(0); break;
            case 0x0f: case 0x14: {
                // Both assignment operands use FFrame.Object, not the receiver
                // supplied by an enclosing ordinary/class Context.
                auto left = Eval(frame, n.children.at(0), frame.self);
                auto right = Eval(frame, n.children.at(1), frame.self);
                Locate(frame.function, n.offset, n.op);
                if (left.reference) {
                    // Let forwards the existing destination to the RHS. LetBool
                    // instead prezeros its own integer temporary, then masks the
                    // target bit even when the RHS performs no result write.
                    const auto destination=n.op==0x14u ? Value::Bool(false) : left.Load();
                    const auto value=ResultValue(right,destination,n.op==0x14u?ResultLayout::Unknown:left.reference->resultLayout);
                    if (n.op==0x14u || right.disposition==ResultDisposition::Written ||
                        (right.disposition==ResultDisposition::ZeroFill && right.zeroFillBytes!=0u))
                        Write(left.reference,value);
                    // execLet/execLetBool never use their own incoming Result.
                    // Assignment is a reflected side effect, not a value alias.
                    result.disposition=ResultDisposition::NoWrite;
                }
                else if (left.Load().kind == Kind::Nothing) {
                    // A null LHS still evaluates RHS effects using scratch
                    // storage; it never manufactures a writable reflected alias.
                    result.disposition=ResultDisposition::NoWrite;
                }
                else Fail(Status::Invalid, "VM assignment to non-variable");
                break;
            }
            case 0x10:
                // GOTY Core.dll execDynArrayElement (1012F190) unconditionally
                // asserts "Array" before its unusable placeholder access. No
                // original element-address/clamp/grow semantics are established.
                Fail(Status::Unsupported, "Original dynamic-array indexing opcode 0x10 is not implemented");
            case 0x12: {
                // The selector uses FFrame.Object, exactly once; the child uses
                // a distinct default-block receiver while retaining the frame.
                const auto selected=Eval(frame,n.children.at(0),frame.self).Load();
                Locate(frame.function,n.offset,n.op);
                if (selected.kind!=Kind::Object && selected.kind!=Kind::Nothing) TypeError();
                if (selected.text.empty()) {
                    result.disposition=ResultDisposition::ZeroFill;
                    result.zeroFillBytes=n.zeroFillSize;
                    break;
                }
                const auto receiver=host_.ClassDefaultObject(selected.text);
                if (receiver.empty() || receiver.find('\0')!=std::string::npos ||
                    Lower(receiver)==Lower(selected.text))
                    Fail(Status::Invalid,"VM ClassContext host returned an invalid default-object identity");
                if (receiver.size()>limits_.stringBytes) Fail(Status::Budget,"VM class-default identity byte limit");
                result=Eval(frame,n.children.at(1),receiver);
                break;
            }
            case 0x13: case 0x2e: {
                auto object = child(0).Load();
                Locate(frame.function, n.offset, n.op);
                if (object.kind != Kind::Object && object.kind != Kind::Nothing) TypeError();
                if (object.kind == Kind::Nothing) object = Value::Text(Kind::Object, {});
                result.value = host_.CastObject(n.constant.text, object, n.op == 0x13);
                if (result.value.kind != Kind::Object ||
                    (!result.value.text.empty() && !Equal(result.value, object)))
                    Fail(Status::Invalid, "VM object cast changed value kind or source identity");
                // Deliberately no reference: casts cannot assign through their
                // operand, and later writes cannot alter this value snapshot.
                break;
            }
            case 0x19: {
                const auto object = child(0).Load();
                Locate(frame.function, n.offset, n.op);
                if (object.kind != Kind::Object && object.kind != Kind::Nothing) TypeError();
                if (object.text.empty()) {
                    result.disposition=ResultDisposition::ZeroFill;
                    result.zeroFillBytes=n.zeroFillSize;
                    break;
                }
                result = Eval(frame, n.children.at(1), object.text); break;
            }
            case 0x17: result.value = Value::Text(Kind::Object, context); break;
            case 0x18: result = child(0); break;
            case 0x2d: {
                const auto variable=child(0);
                // BoolVariable evaluates its child with nil Result, using only
                // the reflected address. A null address produces false.
                result.value=Value::Bool(variable.reference ? ToBool(variable.Load()) : false);
                result.reference=variable.reference;
                break;
            }
            case 0x1a: {
                // Retail uses FFrame.Object for the index but the incoming
                // opcode receiver for the array's reflected address.
                const auto index = ToInt(Eval(frame,n.children.at(0),frame.self).Load());
                auto array = child(1);
                Locate(frame.function, n.offset, n.op);
                if (!array.reference && array.disposition!=ResultDisposition::Written) {
                    result.disposition=ResultDisposition::NoWrite;
                    break;
                }
                if (!array.reference || !array.reference->element || array.reference->dimension == 0)
                    Fail(Status::Invalid, "VM fixed array is not a variable");
                const auto clamped = static_cast<std::size_t>(std::clamp<std::int64_t>(index, 0,
                    static_cast<std::int64_t>(array.reference->dimension - 1)));
                result.reference = BindResultLayout(array.reference->element(clamped),array.reference->resultLayout); break;
            }
            case 0x1d: case 0x1e: case 0x1f: case 0x22: case 0x23: case 0x24: case 0x2c: case 0x34:
                result.value = n.constant; break;
            case 0x20: result.value = Value::Text(Kind::Object, host_.ResolveObject(frame.function, n.reference)); break;
            case 0x21: result.value = Value::Text(Kind::Name, host_.ResolveName(frame.function, n.reference)); break;
            case 0x25: result.value = Value::Integer(0); break;
            case 0x26: result.value = Value::Integer(1); break;
            case 0x27: result.value = Value::Bool(true); break;
            case 0x28: result.value = Value::Bool(false); break;
            case 0x2a: result.value = Value::Text(Kind::Object, {}); break;
            case 0x32: case 0x33: {
                const auto a = child(0).Load(); const auto b = child(1).Load();
                Locate(frame.function, n.offset, n.op);
                if (HasIntArray(a) || HasIntArray(b))
                    Fail(Status::Unsupported, "VM script dynamic INT-array comparison is not established");
                result.value = Value::Bool(Equal(a, b) == (n.op == 0x32)); break;
            }
            case 0x36: {
                const auto p = host_.ResolveProperty(frame.function, n.reference);
                auto parent = child(0); const auto v = parent.Load();
                Locate(frame.function, n.offset, n.op);
                result.value = MemberValue(v, p.name);
                if (parent.reference) {
                    auto ref = std::make_shared<Reference>(); ref->zero = p.zero;
                    ref->resultLayout = p.resultLayout;
                    ref->read = [base = parent.reference, name = p.name] { return MemberValue(base->read(), name); };
                    ref->write = [base = parent.reference, name = p.name](const Value& member) {
                        auto value = base->read(); SetMember(value, name, member); base->write(value);
                    };
                    result.reference = std::move(ref);
                }
                break;
            }
            default: Fail(Status::Unsupported, "Unsupported VM opcode " + std::to_string(n.op));
            }
        }
        // Nothing denotes absence of a VM return/result write, not a typed
        // scalar zero. Concrete None is an Object value with an empty handle.
        // Preserve explicit null-context ZeroFill separately from this case.
        if (!result.reference && result.value.kind==Kind::Nothing &&
            result.disposition==ResultDisposition::Written)
            result.disposition=ResultDisposition::NoWrite;
        if (!result.reference) ValidateValue(result.value, 0);
        else {
            const auto value=result.Load();ValidateValue(value, 0);
            ValidateResultLayout(result.reference->zero,result.reference->resultLayout);
            ValidateResultLayout(value,result.reference->resultLayout);
        }
        return result;
    }

    std::optional<Value> CaptureNativeContext(std::uint16_t index, const std::string& self,
        const Function* declaration) {
        auto context = host_.CaptureNativeContext(index, self, declaration);
        if (context) { ValidateValue(*context, 0); Retain(ValueBytes(*context)); }
        return context;
    }
    Evaluation Native(std::uint16_t index, const std::string& self,
        const std::vector<Evaluation>& args, const Function* declaration,
        const std::optional<Value>& capturedContext = {}) {
        const auto count = [&](std::size_t n) {
            if (args.size() != n) Fail(Status::Invalid, "VM native argument count for " + std::to_string(index));
        };
        const auto a = [&](std::size_t i) { return args.at(i).Load(); };
        const auto binaryInt = [&](auto op) { count(2); return Evaluation{Value::Integer(op(ToInt(a(0)), ToInt(a(1)))), {}}; };
        const auto binaryFloat = [&](auto op) { count(2); return Evaluation{Value::Float(op(ToFloat(a(0)), ToFloat(a(1)))), {}}; };
        const auto compareInt = [&](auto op) { count(2); return Evaluation{Value::Bool(op(ToInt(a(0)), ToInt(a(1)))), {}}; };
        const auto compareFloat = [&](auto op) { count(2); return Evaluation{Value::Bool(op(ToFloat(a(0)), ToFloat(a(1)))), {}}; };
        const auto vector = [&](std::size_t i) {
            auto v = a(i); if (v.kind == Kind::Nothing) return std::array<float, 3>{};
            if (v.kind != Kind::Vector) TypeError();
            return v.vector;
        };
        switch (index) {
        case 129: count(1); return {Value::Bool(!ToBool(a(0))), {}};
        case 130: count(2); return {Value::Bool(ToBool(a(0)) && ToBool(a(1))), {}};
        case 131: count(2); return {Value::Bool(ToBool(a(0)) != ToBool(a(1))), {}};
        case 132: count(2); return {Value::Bool(ToBool(a(0)) || ToBool(a(1))), {}};
        case 114: case 119: case 254: case 255: case 242: case 243:
        case 217: case 218: case 122: case 123: {
            count(2);
            const Kind kind = index == 114 || index == 119 ? Kind::Object :
                index == 254 || index == 255 ? Kind::Name : index == 242 || index == 243 ? Kind::Bool :
                index == 217 || index == 218 ? Kind::Vector : Kind::String;
            Value zero; zero.kind = kind; if (kind == Kind::Name) zero.text = "None";
            const bool equal = Equal(Coerce(a(0), zero), Coerce(a(1), zero));
            return {Value::Bool(equal == (index == 114 || index == 254 || index == 242 || index == 217 || index == 122)), {}};
        }
        case 143: count(1); return {Value::Integer(std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(ToInt(a(0))))), {}};
        case 144: return binaryInt([](auto x, auto y) { return std::bit_cast<std::int32_t>(std::uint32_t(x) * std::uint32_t(y)); });
        case 145: return binaryInt([&](auto x, auto y) {
            if (y == 0 || (x == std::numeric_limits<std::int32_t>::min() && y == -1)) Fail(Status::Invalid, "Unsafe VM integer division");
            return x / y;
        });
        case 146: return binaryInt([](auto x, auto y) { return std::bit_cast<std::int32_t>(std::uint32_t(x) + std::uint32_t(y)); });
        case 147: return binaryInt([](auto x, auto y) { return std::bit_cast<std::int32_t>(std::uint32_t(x) - std::uint32_t(y)); });
        case 150: return compareInt([](auto x, auto y) { return x < y; });
        case 151: return compareInt([](auto x, auto y) { return x > y; });
        case 152: return compareInt([](auto x, auto y) { return x <= y; });
        case 153: return compareInt([](auto x, auto y) { return x >= y; });
        case 154: return compareInt([](auto x, auto y) { return x == y; });
        case 155: return compareInt([](auto x, auto y) { return x != y; });
        case 156: return binaryInt([](auto x, auto y) { return x & y; });
        case 157: return binaryInt([](auto x, auto y) { return x ^ y; });
        case 158: return binaryInt([](auto x, auto y) { return x | y; });
        case 163: case 164: case 165: case 166: {
            count(1);
            const auto& reference = args[0].reference;
            if (!reference || !reference->write || reference->zero.kind != Kind::Int)
                Fail(Status::Invalid, "VM integer increment/decrement needs a writable Int reference");
            const auto previous = a(0);
            if (previous.kind != Kind::Int)
                Fail(Status::Invalid, "VM integer increment/decrement reference has the wrong concrete kind");
            const auto bits = static_cast<std::uint32_t>(previous.integer);
            const auto next = Value::Integer(std::bit_cast<std::int32_t>(
                index == 163 || index == 165 ? bits + 1u : bits - 1u));
            Write(reference, next);
            return {index == 163 || index == 164 ? next : previous, {}};
        }
        case 169: count(1); return {Value::Float(-ToFloat(a(0))), {}};
        case 170: return binaryFloat([](auto x, auto y) { return std::pow(x, y); });
        case 171: return binaryFloat([](auto x, auto y) { return x * y; });
        case 172: return binaryFloat([](auto x, auto y) { return x / y; });
        case 173: return binaryFloat([](auto x, auto y) { return std::fmod(x, y); });
        case 174: return binaryFloat([](auto x, auto y) { return x + y; });
        case 175: return binaryFloat([](auto x, auto y) { return x - y; });
        case 176: return compareFloat([](auto x, auto y) { return x < y; });
        case 177: return compareFloat([](auto x, auto y) { return x > y; });
        case 178: return compareFloat([](auto x, auto y) { return x <= y; });
        case 179: return compareFloat([](auto x, auto y) { return x >= y; });
        case 180: return compareFloat([](auto x, auto y) { return x == y; });
        case 181: return compareFloat([](auto x, auto y) { return x != y; });
        case 212: case 213: case 214: {
            count(2); const auto v = vector(index == 213 ? 1 : 0);
            const float f = ToFloat(a(index == 213 ? 0 : 1)); std::array<float, 3> out{};
            for (std::size_t i = 0; i < 3; ++i) out[i] = index == 214 ? v[i] / f : v[i] * f;
            return {Value::Vector(out), {}};
        }
        case 215: case 216: case 296: {
            count(2); const auto x = vector(0); const auto y = vector(1); std::array<float, 3> out{};
            for (std::size_t i = 0; i < 3; ++i) out[i] = index == 215 ? x[i] + y[i] : index == 216 ? x[i] - y[i] : x[i] * y[i];
            return {Value::Vector(out), {}};
        }
        case 219: {
            count(2); const auto x = vector(0); const auto y = vector(1);
            return {Value::Float(x[0]*y[0] + x[1]*y[1] + x[2]*y[2]), {}};
        }
        case 220: {
            count(2); const auto x = vector(0); const auto y = vector(1);
            return {Value::Vector({x[1]*y[2]-x[2]*y[1], x[2]*y[0]-x[0]*y[2], x[0]*y[1]-x[1]*y[0]}), {}};
        }
        case 221: {
            count(2);
            const auto& reference = args[0].reference;
            if (!reference || !reference->write || reference->zero.kind != Kind::Vector ||
                a(0).kind != Kind::Vector)
                Fail(Status::Invalid, "VM vector multiplication assignment needs a writable Vector reference");
            const auto value = Native(212u, self, args, nullptr).Load();
            Write(reference, value);
            return {args[0].Load(), {}};
        }
        case 225: {
            count(1); const auto v = vector(0u);
            // NObject::VSize uses the pinned binary32 dot/length operations,
            // not a double hypot that would silently change overflow behavior.
            return {Value::Float(length(vec3(v[0], v[1], v[2]))), {}};
        }
        case 186: {
            // Retail execAbs10135c60 compares with Float+0 and conditionally
            // changes sign. Unlike fabs, it preserves negative zero unchanged.
            count(1);const float value=ToFloat(a(0));
            return {Value::Float(value<0.0f?-value:value),{}};
        }
        case 187:
            // Retail Core.dll execSin10135d20: initialize Float0, evaluate
            // once, promote to double appSin, then store a binary32 result.
            // appSin10123240 uses x87 FSIN. Out-of-range |x|>=2^63 leaves
            // its operand unchanged. For the supported radians, use portable
            // double libm like Tan; do not claim identical x87 final rounding.
            count(1); {
                const float angle=ToFloat(a(0));
                return {Value::Float(std::abs(angle)>=0x1p63f?angle:
                    static_cast<float>(std::sin(static_cast<double>(angle)))),{}};
            }
        case 189: count(1); return {Value::Float(static_cast<float>(
            std::tan(static_cast<double>(ToFloat(a(0)))))),{}};
        case 244:
            // Retail execFMin (Core.dll 10136320) copies A then B into Float
            // locals and selects A on equality, including either signed zero.
            // Existing VM ingress refuses non-finite inputs before this call.
            return binaryFloat([](float x,float y) {return y<x ? y : x;});
        case 245:
            // Retail execFMax (Core.dll thunk 10101d02 -> 10136400) likewise
            // copies A then B. Its finite equality path selects A, preserving
            // the first signed zero; non-finite ingress remains refused.
            return binaryFloat([](float x,float y) {return x<y ? y : x;});
        case 246: {
            count(3); const auto v=ToFloat(a(0)), low=ToFloat(a(1)), high=ToFloat(a(2));
            // Original x87 tests C0, so unordered comparisons take the branch.
            return {Value::Float(std::isnan(v)||std::isnan(low)||v<low ? low :
                std::isnan(high)||high<v ? high : v),{}};
        }
        case 249: return binaryInt([](auto x,auto y) { return x<y ? x : y; });
        case 250: {
            count(2);
            // Retail thunk101015ff -> execMax10134b00 independently zeros
            // and copies A then B; signed JL selects B only when A<B.
            // Equality selects A, and no arithmetic can overflow.
            const auto integer=[&](std::size_t slot) {
                const auto value=a(slot);
                if (value.kind==Kind::Nothing) return std::int32_t{0};
                if (value.kind!=Kind::Int && value.kind!=Kind::Byte) TypeError();
                return value.integer;
            };
            const auto first=integer(0u),second=integer(1u);
            return {Value::Integer(first<second ? second : first),{}};
        }
        case 251: {
            count(3);
            // Retail Core.dll thunk 10101c5d -> execClamp10134be0 copies
            // three zero-initialized Int temporaries, then uses signed V<A,
            // V>=B tests in this order. Do not sort bounds or use std::clamp
            // (whose reversed-bound precondition would not match retail).
            const auto integer=[&](std::size_t slot) {
                const auto value=a(slot);
                if (value.kind==Kind::Nothing) return std::int32_t{0};
                if (value.kind!=Kind::Int && value.kind!=Kind::Byte) TypeError();
                // A compiled Float input needs an actual FloatToInt opcode;
                // literal Object None is not the optional/no-write marker.
                return value.integer;
            };
            const auto value=integer(0u),low=integer(1u),high=integer(2u);
            return {Value::Integer(value<low ? low : value>=high ? high : value),{}};
        }
        case 124: case 125: case 126: case 127: case 128: case 234: case 235: {
            const auto text=[&](std::size_t i) { return UnrealString::Decode(Coerce(a(i),Value::Text(Kind::String,{})).text); };
            if (index==127u) { if (args.size()<2u || args.size()>3u) Fail(Status::Invalid,"VM Mid argument count"); }
            else count(index==125u || index==235u ? 1u : 2u);
            auto x=text(0);
            if (index==124u) return {Value::Bool(UnrealString::EqualInsensitive(x,text(1))),{}};
            if (index==125u) return {Value::Integer(static_cast<std::int32_t>(x.size())),{}};
            if (index==126u) {
                const auto y=text(1); const auto found=UnrealString::CString(x).find(UnrealString::CString(y));
                return {Value::Integer(found==std::u16string_view::npos ? -1 : static_cast<std::int32_t>(found)),{}};
            }
            if (index==235u) {
                if (x.find(u'\0')!=std::u16string::npos) Fail(Status::Invalid,"Original Caps has unsafe embedded-NUL storage");
                for (auto& c:x) if (c>=u'a' && c<=u'z') c-=u'a'-u'A';
                return {Value::Text(Kind::String,UnrealString::Encode(x)),{}};
            }
            const auto len=static_cast<std::uint32_t>(x.size());
            std::uint32_t begin=0u,end=len;
            if (index==127u) {
                const auto start=static_cast<std::uint32_t>(ToInt(a(1)));
                const auto amount=args.size()<3u || a(2).kind==Kind::Nothing ? 65535u : static_cast<std::uint32_t>(ToInt(a(2)));
                begin=std::min(start,len); end=std::clamp(start+amount,begin,len);
            } else {
                const auto amount=static_cast<std::uint32_t>(std::clamp<std::int64_t>(ToInt(a(1)),0,len));
                if (index==128u) end=amount; else begin=len-amount;
            }
            auto slice=std::u16string(std::u16string_view(x).substr(begin,end-begin));
            // FString::Right constructs from the C-string suffix, unlike the
            // predetermined-length appStrncpy copies used by Left and Mid.
            if (index==234u) return {Value::Text(Kind::String,
                UnrealString::Encode(UnrealString::CString(slice))),{}};
            // appStrncpy/wcsncpy pads through the requested count after NUL.
            if (const auto nul=slice.find(u'\0'); nul!=std::u16string::npos)
                std::fill(slice.begin()+static_cast<std::ptrdiff_t>(nul),slice.end(),u'\0');
            return {Value::Text(Kind::String,UnrealString::Encode(slice)),{}};
        }
        case 112: case 168: {
            count(2); Value zero = Value::Text(Kind::String, {});
            const auto first=Coerce(a(0),zero).text, second=Coerce(a(1),zero).text;
            if (first.size()>limits_.stringBytes || second.size()>limits_.stringBytes-first.size() ||
                (index==168u && first.size()+second.size()==limits_.stringBytes)) Fail(Status::Budget,"VM concatenation limit");
            auto x=UnrealString::Decode(first); const auto y=UnrealString::Decode(second);
            if (index==168u) x.push_back(u' ');
            x+=UnrealString::CString(y);
            return {Value::Text(Kind::String, UnrealString::Encode(x)), {}};
        }
        case 161: case 162: case 182: case 183: case 184: case 185: case 223: case 224: {
            count(2); const auto op = index == 161 ? 146 : index == 162 ? 147 :
                index == 182 ? 171 : index == 183 ? 172 : index == 184 ? 174 : index == 185 ? 175 : index == 223 ? 215 : 216;
            auto value = Native(static_cast<std::uint16_t>(op), self, args, nullptr).Load();
            Write(args[0].reference, value); return {args[0].Load(), {}};
        }
        default: {
            // No implicit success for missing campaign behavior or latent calls.
            auto guarded = args;
            for (auto& argument : guarded) argument.reference = GuardReference(argument.reference);
            const auto value = [&] {
                try {
                    auto evaluated = host_.NativeWithContext(index, self, guarded, declaration, *this, capturedContext);
                    // Validate a detached native allocation before copying can
                    // discard spare capacity. Native aliases are snapshotted.
                    if (!evaluated.reference) { ValidateValue(evaluated.value, 0); return std::move(evaluated.value); }
                    return evaluated.Load();
                }
                catch (...) { CheckFailure(); throw; }
            }();
            CheckFailure();
            // Native return slots are values in the pinned VM, never aliases
            // to an out argument or to a guard owned by this Machine.
            ValidateValue(value, 0); return {value, {}};
        }
        }
    }
};
} // namespace

ProgramLayout AnalyzeProgram(Host& host, const Function& function, const Limits& limits) {
    Result diagnostics;
    std::size_t nodes{};
    Parser parser(host, function, limits, nodes, diagnostics, true);
    auto program = parser.Parse();
    auto retainedBytes = parser.AnalysisRetainedBytes();
    const auto retain = [&](const std::size_t bytes) {
        if (bytes > limits.retainedBytes || retainedBytes > limits.retainedBytes - bytes)
            throw std::runtime_error("VM program layout retained byte limit");
        retainedBytes += bytes;
    };
    ProgramLayout layout;
    if (nodes > limits.retainedBytes / sizeof(ExpressionLayout))
        throw std::runtime_error("VM program layout retained byte limit");
    retain(nodes * sizeof(ExpressionLayout));
    layout.expressions.reserve(nodes);
    const auto append = [&](const auto& recurse, const Node& node) -> std::size_t {
        const auto index = layout.expressions.size();
        layout.expressions.push_back({node.offset,node.op,node.reference,node.target,
            node.native,node.zeroFillSize,{}});
        retain(node.children.size() * sizeof(std::size_t));
        layout.expressions[index].children.reserve(node.children.size());
        for (const auto& child : node.children) {
            const auto childIndex = recurse(recurse,child);
            layout.expressions[index].children.push_back(childIndex);
        }
        return index;
    };
    for (const auto& statement : program.statements) append(append,statement);
    layout.statementOffsets.reserve(program.statements.size());
    for (const auto& statement : program.statements) layout.statementOffsets.push_back(statement.offset);
    if (!program.statements.empty() && program.statements.back().op == 0x0cu) {
        layout.terminalLabelTable = true;
        layout.labels = std::move(program.statements.back().labels);
        for (const auto& label : layout.labels)
            if (!program.boundaries.count(label.offset))
                throw std::runtime_error("VM state label is not a top-level statement boundary");
    }
    return layout;
}

Result Execute(Host& host, const Function& function, const std::string& self,
    const std::vector<Evaluation>& arguments, const Limits& limits) {
    Result result;
    bool begun = false;
    try {
        host.Begin(); begun = true;
        Machine machine(host, limits, result);
        result.value = machine.Run(function, self, arguments);
        machine.CheckFailure();
        host.Commit(); begun = false;
        machine.Accept();
        result.committed = true;
        result.status = Status::Returned;
        result.function = function.path;
    } catch (const Failure& error) {
        result.status = error.status; result.error = error.what();
    } catch (const std::exception& error) {
        result.status = Status::Unsupported; result.error = error.what();
    } catch (...) {
        result.status = Status::Invalid; result.error = "Unknown VM execution failure";
    }
    if (begun) host.Rollback();
    if (!result.passed()) result.value = {};
    return result;
}

Result ExecuteTransaction(Host& host, const std::function<Value(Execution&)>& operation,
    const Limits& limits) {
    Result result;
    bool begun{};
    try {
        host.Begin(); begun = true;
        Machine machine(host, limits, result);
        if (!operation) throw Failure(Status::Invalid, "Missing VM transaction callback");
        try {
            result.value = machine.FinishTransaction(operation(machine));
        } catch (...) {
            // Prefer the first failed script entry even if a callback catches
            // it and subsequently throws an unrelated host exception.
            machine.CheckFailure();
            // With no poisoned entry, this is coordinator work (or callback
            // return validation), not the last successfully returned script.
            // Do not attribute a host-side failure to that stale bytecode PC.
            result.function.clear(); result.offset = 0; result.opcode = 0;
            result.callStack.clear();
            throw;
        }
        host.Commit(); begun = false;
        machine.Accept(); result.committed = true; result.status = Status::Returned;
    } catch (const Failure& error) {
        result.status = error.status; result.error = error.what();
    } catch (const std::exception& error) {
        result.status = Status::Unsupported; result.error = error.what();
    } catch (...) {
        result.status = Status::Invalid; result.error = "Unknown VM transaction failure";
    }
    if (begun) host.Rollback();
    if (!result.passed()) result.value = {};
    return result;
}

Result ResumeState(Host& host, const std::string& self, const Limits& limits) {
    Result result;
    bool begun{};
    try {
        host.Begin(); begun = true;
        Machine machine(host, limits, result);
        result.value = machine.Resume(self);
        machine.CheckFailure();
        host.Commit(); begun = false;
        machine.Accept(); result.committed = true;
        if (result.status != Status::Stopped && result.status != Status::Waiting) result.status = Status::Returned;
    } catch (const Failure& error) {
        result.status = error.status; result.error = error.what();
    } catch (const std::exception& error) {
        result.status = Status::Unsupported; result.error = error.what();
    } catch (...) {
        result.status = Status::Invalid; result.error = "Unknown state execution failure";
    }
    if (begun) host.Rollback();
    if (!result.passed()) result.value = {};
    return result;
}
Result AdvanceState(Host& host, const std::string& self, const float elapsed, const Limits& limits) {
    Result result;
    if (!std::isfinite(elapsed) || elapsed < 0.0f) {
        result.status = Status::Invalid; result.error = "State elapsed time must be finite and nonnegative";
        return result;
    }
    bool begun{};
    try {
        host.Begin(); begun = true;
        Machine machine(host, limits, result);
        std::optional<std::string> selectedBeforePoll;
        const auto* object = host.ReadState(self);
        const auto* control = object && object->frameOverride && object->frame && object->frame->control
            ? &*object->frame->control : nullptr;
        if (control) selectedBeforePoll = control->selectedPath;
        if (control && (!control->codePresent || host.IsStateDeleted(self))) result.status = Status::Stopped;
        else {
            const bool eligible = host.PollState(self, elapsed, machine);
            machine.CheckFailure();
            if (eligible) result.value = machine.Resume(self, selectedBeforePoll);
        }
        machine.CheckFailure();
        host.Commit(); begun = false;
        machine.Accept(); result.committed = true;
        if (result.status != Status::Stopped && result.status != Status::Waiting) result.status = Status::Returned;
    } catch (const Failure& error) {
        result.status = error.status; result.error = error.what();
    } catch (const std::exception& error) {
        result.status = Status::Unsupported; result.error = error.what();
    } catch (...) {
        result.status = Status::Invalid; result.error = "Unknown state polling failure";
    }
    if (begun) host.Rollback();
    if (!result.passed()) result.value = {};
    return result;
}
} // namespace QuestVr::Vm
