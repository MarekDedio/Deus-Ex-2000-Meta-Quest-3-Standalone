#include "quest_portable_vm.h"
#include "quest_state_frame.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <cstdlib>
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
            case 0x12: case 0x19: child(); n.target = Word(); Byte(); child(); break;
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
                    if (text.size() >= limits_.stringBytes) Fail(Status::Budget, "VM string limit");
                    text.push_back(static_cast<char>(c));
                }
                Retain(text.size()); Retain(text.size());
                n.constant = Value::Text(Kind::String, std::move(text)); break;
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
            case 0x34:
                // Parse even in an untaken branch, but do not guess UTF-16
                // conversion behavior until it has its own fidelity tests.
                for (std::size_t count = 0; Word() != 0; ++count)
                    if (count >= limits_.stringBytes / 2) Fail(Status::Budget, "VM Unicode string limit");
                break;
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
    Value CallEvent(const std::string& receiver, const std::string& eventName,
        const bool enumDispatch, const std::vector<Evaluation>& arguments) override {
        if (eventName.size() > limits_.stringBytes || receiver.size() > limits_.stringBytes)
            Fail(Status::Budget, "VM callback identity/name limit");
        const auto identity = host_.ResolveEvent(receiver, eventName, enumDispatch);
        return identity ? Run(*identity, receiver, arguments) : Value{};
    }
    Value Run(const Function& identity, const std::string& self, const std::vector<Evaluation>& incoming) {
        if (self.empty() || !host_.CanCall(identity, self)) return {};
        if (result_.callStack.size() >= limits_.callDepth) Fail(Status::Budget, "VM call depth limit");
        const auto prepared = host_.PrepareFunction(identity, self);
        if (!prepared || prepared->path != identity.path || prepared->source != identity.source)
            Fail(Status::Invalid, "Prepared VM function changed callable identity");
        const Function& fn = *prepared;
        result_.callStack.push_back(fn.path);
        result_.function = fn.path; result_.offset = 0; result_.opcode = 0;
        if ((fn.flags & 0x8) && !((fn.flags & 0x400) &&
            (fn.nativeIndex == 256u || fn.nativeIndex == 527u)))
            Fail(Status::Unsupported, "Latent VM function requires its runtime handler");
        if (incoming.size() > limits_.arguments) Fail(Status::Budget, "VM argument limit");
        for (const auto& argument : incoming) {
            const auto value = argument.Load(); ValidateValue(value, 0); Retain(ValueBytes(value));
        }
        std::vector<Evaluation> args = incoming;
        std::size_t argCount = 0;
        for (const auto& p : fn.variables) {
            if (!(p.flags & 0x80) || (p.flags & 0x400)) continue;
            if (argCount == args.size()) {
                if (!(p.flags & 0x10)) Fail(Status::Invalid, "Missing required VM parameter " + p.name);
                args.push_back({});
            }
            ++argCount;
        }
        if (args.size() > argCount && !fn.variables.empty()) Fail(Status::Invalid, "Excess VM arguments");
        if (fn.flags & 0x400) {
            Step(fn, 0, 0);
            auto result = Native(fn.nativeIndex, self, args, &fn).Load();
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
            if (n.op == 0x04) { value = Eval(frame, n.children.at(0), self).Load(); returned = true; break; }
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
    Value Resume(const std::string& self) {
        if (self.size() > limits_.stringBytes) Fail(Status::Budget, "VM state receiver identity limit");
        bool active{};
        IteratorStack iterators;
        const auto finish = [&](Value value = {}) {
            if (!iterators.empty())
                Fail(Status::Unsupported, "State continuation with live iterators requires persistent iterator storage");
            if (active) result_.callStack.pop_back();
            return value;
        };
        for (;;) {
            const auto* object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame || object->frame->codePath.empty()) return finish();
            const auto code = object->frame->codePath;
            const auto localOwner = object->frame->localsCodePath;
            const auto statementIndex = object->frame->statementIndex;
            const auto latent = object->frame->latent;
            const auto localRevision = host_.StateLocalRevision(self);
            if (latent == StateLatent::Stop) { result_.status = Status::Stopped; return finish(); }
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
                    if (current && current->frameOverride && current->frame && !current->frame->codePath.empty() &&
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
            if (!object || !object->frameOverride || !object->frame || object->frame->codePath.empty()) return finish(value);
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
                host_.MutableState(self)->frame->latent = StateLatent::Stop;
                result_.status = Status::Stopped; return finish();
            } else if (statement.op == 0x04) {
                ValidateValue(value, 0); return finish(value);
            }
            object = host_.ReadState(self);
            if (!object || !object->frameOverride || !object->frame || object->frame->codePath.empty()) return finish();
            if (object->frame->latent != StateLatent::Continue) {
                if (object->frame->latent == StateLatent::Stop) { result_.status = Status::Stopped; return finish(); }
                if (object->frame->latent == StateLatent::Sleep ||
                    object->frame->latent == StateLatent::WaitForLanding) {
                    result_.status = Status::Waiting; return finish();
                }
                Fail(Status::Unsupported, "State latent action requires its runtime handler");
            }
        }
    }
private:
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
            if (!object || !object->frameOverride || !object->frame || object->frame->codePath.empty()) return;
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
        const auto key = std::make_tuple(self, code, localOwner, host_.StateLocalRevision(self));
        const auto found = stateViews_.find(key);
        if (found != stateViews_.end()) return *found->second;
        auto function = host_.StateProgram(self, code);
        if (!function || function->path != code || function->source.empty() || (function->flags & 0x400u))
            Fail(Status::Invalid, "State program changed code identity or is native");
        const auto locals = localOwner == code ? function : host_.StateProgram(self, localOwner);
        if (!locals || locals->path != localOwner || locals->source.empty() || (locals->flags & 0x400u))
            Fail(Status::Invalid, "State locals owner changed identity or is native");
        Retain(sizeof(StateExecutionView) + self.size() + code.size() + localOwner.size() + function->bytecode.size());
        auto view = std::make_unique<StateExecutionView>(function,
            Parser(host_, *function, limits_, nodes_, result_, false, &retainedBytes_).Parse(), self);
        for (const auto& property : locals->variables) {
            if (property.arrayDimension == 0 || property.arrayDimension > limits_.localElements ||
                localElements_ > limits_.localElements - property.arrayDimension)
                Fail(Status::Budget, "VM state local element limit");
            ValidateValue(property.zero, 0);
            auto reference = host_.StateVariable(self, property);
            if (!reference || !reference->read || !reference->write || reference->dimension != property.arrayDimension)
                Fail(Status::Invalid, "State local storage/schema mismatch");
            localElements_ += property.arrayDimension;
            Retain(sizeof(Reference) + property.key.size());
            for (std::size_t index = 0; index < property.arrayDimension; ++index) {
                const auto element = index ? (reference->element ? reference->element(index) : nullptr) : reference;
                if (!element || !element->read) Fail(Status::Invalid, "State array local storage unavailable");
                const auto stored = element->read(); ValidateValue(stored, 0);
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
    std::size_t nodes_{}, localElements_{}, retainedBytes_{};
    bool accepted_{};
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
    std::vector<std::pair<std::shared_ptr<Reference>, Value>> journal_;
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
    std::size_t ValueBytes(const Value& v) {
        std::size_t bytes = sizeof(Value) + v.text.size();
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
        if (++nodes > limits_.localElements || sizeof(Value) + v.text.size() > limits_.retainedBytes ||
            bytes > limits_.retainedBytes - sizeof(Value) - v.text.size())
            Fail(Status::Budget, "VM aggregate value limit");
        bytes += sizeof(Value) + v.text.size();
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
    void Write(const std::shared_ptr<Reference>& reference, const Value& value) {
        if (!reference || !reference->write) Fail(Status::Invalid, "VM assignment is not writable");
        if (result_.writes >= limits_.writes) Fail(Status::Budget, "VM write limit");
        ++result_.writes;
        auto typed = Coerce(value, reference->zero); ValidateValue(typed, 0);
        auto previous = reference->read(); ValidateValue(previous, 0);
        Retain(ValueBytes(previous));
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
        guarded->write = [this, requireAlive, reference](const Value& v) { requireAlive(); Write(reference, v); };
        if (reference->element)
            guarded->element = [this, requireAlive, reference](std::size_t i) {
                requireAlive(); return GuardReference(reference->element(i));
            };
        return guarded;
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
            std::vector<Evaluation> args;
            for (std::size_t i = 0; i < n.children.size(); ++i) {
                // Pinned VM evaluates arguments using Self, NOT the changed
                // Context receiver. Native &&/|| alone skip their right side.
                if (i == 1 && (index == 130 || index == 132) && (!fn || (fn->flags & 0x400))) {
                    const bool a = ToBool(args[0].Load());
                    if ((index == 130 && !a) || (index == 132 && a)) {
                        result.value = Value::Bool(a); return result;
                    }
                }
                args.push_back(Eval(frame, n.children[i], frame.self));
            }
            Locate(frame.function, n.offset, n.op);
            result = fn ? Evaluation{Run(*fn, context, args), {}} : Native(index, context, args, nullptr);
        } else if (n.op >= 0x39) {
            const auto v = child(0).Load();
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
                } else result.reference = host_.Variable(context, p, n.op == 0x02 ? Scope::Default : Scope::Instance);
                if (!result.reference || !result.reference->read) Fail(Status::Unsupported, "VM property unavailable: " + p.key);
                break;
            }
            case 0x0b: break;
            case 0x0e: child(0); break;
            case 0x0f: case 0x14: {
                auto left = child(0); auto right = child(1);
                Locate(frame.function, n.offset, n.op);
                if (left.reference) { Write(left.reference, right.Load()); result = left; }
                else if (left.Load().kind == Kind::Nothing) result = right;
                else Fail(Status::Invalid, "VM assignment to non-variable");
                break;
            }
            case 0x12: Fail(Status::Unsupported, "ClassContext requires class-default object identity");
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
                    break;
                }
                result = Eval(frame, n.children.at(1), object.text); break;
            }
            case 0x17: result.value = Value::Text(Kind::Object, frame.self); break;
            case 0x18: case 0x2d: result = child(0); break;
            case 0x1a: {
                const auto index = ToInt(child(0).Load()); auto array = child(1);
                Locate(frame.function, n.offset, n.op);
                if (!array.reference || !array.reference->element || array.reference->dimension == 0)
                    Fail(Status::Invalid, "VM fixed array is not a variable");
                const auto clamped = static_cast<std::size_t>(std::clamp<std::int64_t>(index, 0,
                    static_cast<std::int64_t>(array.reference->dimension - 1)));
                result.reference = array.reference->element(clamped); break;
            }
            case 0x1d: case 0x1e: case 0x1f: case 0x22: case 0x23: case 0x24: case 0x2c:
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
                result.value = Value::Bool(Equal(a, b) == (n.op == 0x32)); break;
            }
            case 0x36: {
                const auto p = host_.ResolveProperty(frame.function, n.reference);
                auto parent = child(0); const auto v = parent.Load();
                Locate(frame.function, n.offset, n.op);
                result.value = MemberValue(v, p.name);
                if (parent.reference) {
                    auto ref = std::make_shared<Reference>(); ref->zero = p.zero;
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
        ValidateValue(result.Load(), 0); return result;
    }

    Evaluation Native(std::uint16_t index, const std::string& self,
        const std::vector<Evaluation>& args, const Function* declaration) {
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
        case 112: {
            count(2); Value zero = Value::Text(Kind::String, {});
            auto x = Coerce(a(0), zero).text; const auto y = Coerce(a(1), zero).text;
            if (x.size() + y.size() > limits_.stringBytes) Fail(Status::Budget, "VM concatenation limit");
            return {Value::Text(Kind::String, x + y), {}};
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
            const auto value = host_.NativeWithExecution(index, self, guarded, declaration, *this).Load();
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
    auto program = Parser(host, function, limits, nodes, diagnostics, true).Parse();
    ProgramLayout layout;
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

Result ResumeState(Host& host, const std::string& self, const Limits& limits) {
    Result result;
    bool begun{};
    try {
        host.Begin(); begun = true;
        Machine machine(host, limits, result);
        result.value = machine.Resume(self);
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
        if (host.PollState(self, elapsed, machine)) result.value = machine.Resume(self);
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
