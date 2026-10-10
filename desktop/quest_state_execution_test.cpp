#include "quest_portable_vm.h"
#include "quest_state_frame.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace Vm = QuestVr::Vm;
using QuestVr::StateFrame;
using QuestVr::StateLatent;
using QuestVr::StateObject;
using Vm::Value;
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Check(const bool condition, const std::string& text) {
    if (!condition) throw std::runtime_error(text);
    ++checks;
}
void Dword(Bytes& bytes, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8u)));
}
Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes result;
    for (const auto& part : parts) result.insert(result.end(), part.begin(), part.end());
    return result;
}
Bytes Ref(std::uint8_t op, std::uint32_t index) { Bytes result{op}; Dword(result, index); return result; }
Bytes Int(std::int32_t value) { return Ref(0x1d, static_cast<std::uint32_t>(value)); }
Bytes Return(Bytes expression = {0x0b}) { return Join({{0x04}, expression}); }
Bytes Let(Bytes lhs, Bytes rhs) { return Join({{0x0f}, lhs, rhs}); }
Bytes Call(std::uint32_t name, std::initializer_list<Bytes> args = {}) {
    auto result = Ref(0x1b, name);
    for (const auto& arg : args) result.insert(result.end(), arg.begin(), arg.end());
    result.push_back(0x16); return result;
}
Bytes Native(std::uint16_t index, std::initializer_list<Bytes> args = {}) {
    Bytes result;
    if (index >= 0x70 && index <= 0xff) result.push_back(static_cast<std::uint8_t>(index));
    else { result.push_back(static_cast<std::uint8_t>(0x60u + (index >> 8u))); result.push_back(static_cast<std::uint8_t>(index)); }
    for (const auto& arg : args) result.insert(result.end(), arg.begin(), arg.end());
    result.push_back(0x16); return result;
}
Bytes Jump(std::uint16_t offset) { return {0x06, static_cast<std::uint8_t>(offset), static_cast<std::uint8_t>(offset >> 8u)}; }
Bytes Switch(Bytes selector) { return Join({{0x05,4},selector}); }
Bytes Case(Bytes value) { return Join({{0x0a,0,0},value}); }
void Append(Bytes& code, const Bytes& value) { code.insert(code.end(),value.begin(),value.end()); }
void Target(Bytes& code, const std::size_t position, const std::size_t offset) {
    if (offset > 0xfffeu) throw std::runtime_error("synthetic branch offset overflow");
    code.at(position+1u) = static_cast<std::uint8_t>(offset);
    code.at(position+2u) = static_cast<std::uint8_t>(offset >> 8u);
}
struct Choices {
    Bytes body;
    std::uint32_t first{}, second{}, fallback{}, end{};
};
Choices Choice(Bytes selector, Bytes firstValue, Bytes firstBody, Bytes secondValue, Bytes secondBody,
    Bytes fallbackBody, bool breakFirst = true, Bytes continuation = {0x08}) {
    Choices result; result.body = Switch(std::move(selector));
    result.first = static_cast<std::uint32_t>(result.body.size()); Append(result.body,Case(std::move(firstValue)));
    Append(result.body,firstBody);
    const auto firstBreak = result.body.size(); if (breakFirst) Append(result.body,Jump(0));
    result.second = static_cast<std::uint32_t>(result.body.size()); Append(result.body,Case(std::move(secondValue)));
    Append(result.body,secondBody); const auto secondBreak = result.body.size(); Append(result.body,Jump(0));
    result.fallback = static_cast<std::uint32_t>(result.body.size()); Append(result.body,{0x0a,0xff,0xff});
    Append(result.body,fallbackBody); result.end = static_cast<std::uint32_t>(result.body.size()); Append(result.body,continuation);
    Target(result.body,result.first,result.second); Target(result.body,result.second,result.fallback);
    if (breakFirst) Target(result.body,firstBreak,result.end);
    Target(result.body,secondBreak,result.end); return result;
}
void Relocate(Choices& choice, const std::uint32_t base) {
    Target(choice.body,choice.first,base+choice.second); Target(choice.body,choice.second,base+choice.fallback);
    Target(choice.body,choice.second-3u,base+choice.end); Target(choice.body,choice.fallback-3u,base+choice.end);
}
std::string Leaf(const std::string& text) { return text.substr(text.find_last_of('.') + 1u); }

// This host implements only synthetic authored programs. Observation traces
// survive rollback so callback ordering and skipped preparation remain visible.
struct Host final : Vm::Host {
    std::map<std::string, StateObject> states, savedStates;
    std::map<std::string, std::uint64_t> localRevisions, savedLocalRevisions;
    std::map<std::string, Vm::Function> programs, functions;
    std::map<std::uint32_t, Vm::Property> properties;
    std::map<std::uint32_t, std::string> names{{0,"None"},{1,"Begin"},{2,"A"},{3,"B"},{4,"C"},{5,"D"},
        {6,"Worker"},{7,"AnimEnd"},{8,"ParentOnly"},{9,"Missing"},{10,"ContinueHere"},{12,"Probe"}};
    std::map<std::string, std::vector<std::string>> labelChain;
    std::map<std::string, Value> values, savedValues;
    std::vector<std::string> trace;
    bool active{}, begun{true}, deleted{}, badPreparedIdentity{};
    std::size_t begins{}, commits{}, rollbacks{}, preparations{}, mutableReads{}, effects{}, savedEffects{};
    std::size_t stateReads{};
    std::size_t iteratorCreates{}, iteratorDestroys{}, iteratorSteps{};
    void Begin() override {
        if (active) throw std::runtime_error("nested transaction");
        savedStates = states; savedLocalRevisions = localRevisions; savedValues = values; savedEffects = effects; active = true; ++begins;
    }
    void Commit() override { Check(active,"commit outside transaction"); active = false; ++commits; }
    void Rollback() noexcept override {
        states.swap(savedStates); localRevisions.swap(savedLocalRevisions); values.swap(savedValues); effects = savedEffects; active = false; ++rollbacks;
    }
    std::string StateName(const std::string& self) const {
        const auto found = states.find(self);
        return found != states.end() && found->second.frame && !found->second.frame->codePath.empty()
            ? Leaf(found->second.frame->codePath) : "None";
    }
    bool Enabled(const std::string& self, const std::string& name) const {
        const auto found = states.find(self);
        if (found == states.end()) return true;
        const auto disabled = found->second.disabled.find(StateName(self));
        return disabled == found->second.disabled.end() || !disabled->second.contains(name);
    }
    bool CanCall(const Vm::Function& identity, const std::string& receiver) override {
        trace.push_back("eligible:" + identity.path);
        return Enabled(receiver, Leaf(identity.path));
    }
    std::shared_ptr<const Vm::Function> Identity(const std::string& path) {
        const auto& function = functions.at(path);
        auto identity = std::make_shared<Vm::Function>();
        identity->path = path; identity->source = function.source;
        identity->nativeIndex = function.nativeIndex; identity->flags = function.flags;
        return identity;
    }
    std::shared_ptr<const Vm::Function> PrepareFunction(const Vm::Function& identity, const std::string&) override {
        ++preparations; trace.push_back("prepare:" + identity.path);
        auto prepared = std::make_shared<Vm::Function>(functions.at(identity.path));
        if (badPreparedIdentity) prepared->path += ".Wrong";
        return prepared;
    }
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function&, const std::string&,
        const Vm::Invocation& invocation) override { return Identity("Class." + names.at(invocation.reference)); }
    std::shared_ptr<const Vm::Function> ResolveEvent(const std::string& receiver, const std::string& name, bool enumDispatch) override {
        trace.push_back("event:" + StateName(receiver) + "." + name);
        if (!Enabled(receiver,name) || !begun || (deleted && (!enumDispatch || name != "Destroyed"))) return {};
        const auto path = "State." + StateName(receiver) + "." + name;
        return functions.contains(path) ? Identity(path) : nullptr;
    }
    Vm::Property ResolveProperty(const Vm::Function&, std::int32_t reference) override { return properties.at(reference); }
    std::string ResolveName(const Vm::Function&, std::int32_t index) override { return names.at(index); }
    std::string ResolveObject(const Vm::Function&, std::int32_t reference) override {
        return reference ? properties.at(reference).key : std::string{};
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string&, const Vm::Property& property, Vm::Scope) override {
        auto ref = std::make_shared<Vm::Reference>(); ref->zero = property.zero; ref->dimension = 1;
        ref->read = [this, key = property.key] { return values.at(key); };
        ref->write = [this, key = property.key](const Value& value) { values.at(key) = value; };
        return ref;
    }
    const StateObject* ReadState(const std::string& self) override {
        ++stateReads; const auto found = states.find(self); return found == states.end() ? nullptr : &found->second;
    }
    StateObject* MutableState(const std::string& self) override {
        if (!active) throw std::runtime_error("state mutation outside transaction");
        ++mutableReads; return &states.at(self);
    }
    std::uint64_t StateLocalRevision(const std::string& self) override { return localRevisions[self]; }
    std::shared_ptr<const Vm::Function> StateProgram(const std::string&, const std::string& path) override {
        return std::make_shared<Vm::Function>(programs.at(path));
    }
    std::shared_ptr<Vm::Reference> StateReference(const std::string& self, const Vm::Property& property, std::size_t index = 0) {
        const auto revision = localRevisions.at(self);
        auto find = [this, self, key = property.key, index, revision]() -> Value& {
            if (localRevisions.at(self) != revision) throw std::runtime_error("stale state local alias");
            auto& locals = states.at(self).frame.value().locals;
            const auto found = std::find_if(locals.begin(), locals.end(), [&](const auto& local) { return local.key == key; });
            if (found == locals.end()) throw std::runtime_error("state local is absent from retained owner");
            return found->values.at(index);
        };
        auto ref = std::make_shared<Vm::Reference>(); ref->zero = property.zero; ref->dimension = property.arrayDimension;
        ref->read = [find] { return find(); };
        ref->write = [find](const Value& value) { find() = value; };
        ref->element = [this, self, property](std::size_t i) { return StateReference(self, property, i); };
        return ref;
    }
    std::shared_ptr<Vm::Reference> StateVariable(const std::string& self, const Vm::Property& property) override {
        return StateReference(self,property);
    }
    void GotoStateLabel(const std::string& self, const std::string& label, bool transition) override {
        auto& frame = MutableState(self)->frame.value();
        const auto requested = transition && (label.empty() || label == "None") ? "Begin" : label;
        std::vector<std::string> candidates;
        if (!transition) candidates.push_back(frame.codePath);
        const auto found = labelChain.find(Leaf(frame.codePath));
        if (found != labelChain.end()) candidates.insert(candidates.end(),found->second.begin(),found->second.end());
        else candidates.push_back(frame.codePath);
        for (const auto& code : candidates) {
            const auto layout = Vm::AnalyzeProgram(*this,programs.at(code));
            for (const auto& entry : layout.labels) if (entry.name == requested) {
                const auto pc = std::find(layout.statementOffsets.begin(),layout.statementOffsets.end(),entry.offset);
                if (pc == layout.statementOffsets.end()) throw std::runtime_error("label outside statements");
                frame.codePath = code; frame.statementIndex = static_cast<std::uint32_t>(pc-layout.statementOffsets.begin());
                if (transition) frame.latent = StateLatent::Continue;
                return;
            }
        }
        if (transition) frame.latent = StateLatent::Stop;
        else throw std::runtime_error("missing authored goto label");
    }
    std::unique_ptr<Vm::Iterator> CreateIterator(std::uint16_t index, const std::string&,
        const std::vector<Vm::Evaluation>&, const Vm::Function*) override {
        if (index != 819) throw std::runtime_error("unknown synthetic iterator");
        struct Cursor final : Vm::Iterator {
            Host& host; bool yielded{};
            explicit Cursor(Host& owner) : host(owner) {}
            ~Cursor() override { ++host.iteratorDestroys; }
            bool Next() override { ++host.iteratorSteps; const auto result = !yielded; yielded = true; return result; }
            std::size_t RetainedBytes() const override { return sizeof(Cursor); }
        };
        ++iteratorCreates; return std::make_unique<Cursor>(*this);
    }
    Vm::Evaluation Native(std::uint16_t, const std::string&, const std::vector<Vm::Evaluation>&, const Vm::Function*) override {
        throw std::runtime_error("native requires execution bridge");
    }
    Vm::Evaluation NativeWithExecution(std::uint16_t index, const std::string& self,
        const std::vector<Vm::Evaluation>& args, const Vm::Function*, Vm::Execution& execution) override {
        if (index == 113) {
            const auto name = args.empty() || args[0].Load().kind == Vm::Kind::Nothing ? StateName(self) : args[0].Load().text;
            const auto target = programs.contains("State." + name) ? "State." + name : std::string{};
            auto& object = *MutableState(self);
            if (!object.frame) object.frame = StateFrame{};
            object.frameOverride = true;
            const auto old = object.frame->codePath;
            if (!old.empty() && old != target) execution.CallEvent(self,"EndState",true);
            if (old != target) {
                ++localRevisions[self];
                auto& frame = object.frame.value(); frame.codePath = target; frame.localsCodePath = target; frame.locals.clear();
                if (!target.empty()) for (const auto& property : programs.at(target).variables)
                    frame.locals.push_back({property.key,std::vector<Value>(property.arrayDimension,property.zero)});
            }
            if (!target.empty()) {
                object.hasStack = true;
                GotoStateLabel(self,args.size() < 2 ? "" : args[1].Load().text,true);
            }
            if (!target.empty() && old != target) execution.CallEvent(self,"BeginState",true);
            return {};
        }
        if (index == 117 || index == 118) {
            auto& disabled = MutableState(self)->disabled[StateName(self)];
            if (index == 117) disabled.erase(args.at(0).Load().text);
            else disabled.insert(args.at(0).Load().text);
            return {};
        }
        if (index == 800) { ++effects; trace.push_back("effect:" + std::to_string(args.empty() ? 0 : Vm::ToInt(args[0].Load()))); return {}; }
        if (index == 801) throw std::runtime_error("missing required native");
        if (index == 802) return {Value::Integer(static_cast<std::int32_t>(states.at(self).frame->statementIndex)),{}};
        if (index == 803) { execution.CallEvent(self,"Notify",true); return {}; }
        if (index == 804) { MutableState(self)->frame->latent = StateLatent::Sleep; return {}; }
        if (index == 805 || index == 806) { execution.CallEvent(self,"Destroyed",index == 805); return {}; }
        if (index == 807 || index == 808 || index == 809) {
            NativeWithExecution(113,self,{{Value::Text(Vm::Kind::Name,"B"),{}}},nullptr,execution);
            if (index == 807) return {Value::Bool(false),{}};
            if (index == 808) return {Value::Text(Vm::Kind::Name,"ParentOnly"),{}};
            return {Value::Integer(42),{}};
        }
        if (index == 810) {
            MutableState(self)->frame->latent = StateLatent::Sleep;
            return {Value::Text(Vm::Kind::Name,"ParentOnly"),{}};
        }
        if (index == 811) {
            trace.push_back("probe:" + StateName(self));
            execution.CallEvent(self,args.at(0).Load().text,true);
            return args.size() > 1 ? args[1] : Vm::Evaluation{};
        }
        if (index == 814) {
            args.at(0).reference->write(args.at(1).Load());
            return {args.at(2).Load(),{}};
        }
        if (index == 815) {
            MutableState(self)->frame->latent = StateLatent::Stop;
            return {args.at(0).Load(),{}};
        }
        if (index == 816) {
            GotoStateLabel(self,args.at(0).Load().text,false);
            return {};
        }
        if (index == 817 || index == 818) {
            auto& object = *MutableState(self);
            ++localRevisions[self];
            if (index == 818) object.frame.reset();
            else {
                object.frame->codePath.clear(); object.frame->localsCodePath.clear(); object.frame->locals.clear();
            }
            return {args.at(0).Load(),{}};
        }
        throw std::runtime_error("unknown synthetic native");
    }
    Vm::Function Function(const std::string& path, Bytes body, std::vector<Vm::Property> variables = {}) {
        Vm::Function fn{path,"Synthetic",std::move(body),std::move(variables),0,0};
        functions[path] = fn; return fn;
    }
    void Program(const std::string& path, Bytes body, std::vector<Vm::Property> variables = {},
        std::vector<std::pair<std::uint32_t,std::uint32_t>> labels = {{1,0}}) {
        body.push_back(0x0c);
        for (const auto& [name,offset] : labels) { Dword(body,name); Dword(body,offset); }
        Dword(body,0); Dword(body,0xffff);
        programs[path] = Vm::Function{path,"Synthetic",std::move(body),std::move(variables),0,0};
    }
    void Install(const std::string& code, std::uint32_t pc = 0, StateLatent latent = StateLatent::Continue) {
        ++localRevisions["Actor"];
        auto& object = states["Actor"]; object.frameOverride = true; object.hasStack = true;
        object.frame = StateFrame{code,code,pc,latent,{}};
        if (!code.empty()) for (const auto& property : programs.at(code).variables)
            object.frame->locals.push_back({property.key,std::vector<Value>(property.arrayDimension,property.zero)});
    }
};

void SuccessStopAndReturn() {
    Host host;
    host.properties[20] = {"Class.Count","Count",Value::Integer(0),0,1}; host.values["Class.Count"] = Value::Integer(0);
    host.Program("State.A",Join({Let(Ref(0x01,20),Native(802)),{0x08}})); host.Install("State.A");
    const auto result = Vm::ResumeState(host,"Actor");
    Check(result.passed() && result.committed && result.status == Vm::Status::Stopped,"state Stop did not commit");
    Check(host.values.at("Class.Count").integer == 1,"state PC did not advance before native evaluation");
    Check(host.states.at("Actor").frame->statementIndex == 2 && host.states.at("Actor").frame->latent == StateLatent::Stop,
        "state Stop did not preserve advanced ordinal");
    const auto reads = host.mutableReads;
    const auto dormant = Vm::ResumeState(host,"Actor");
    Check(dormant.passed() && dormant.status == Vm::Status::Stopped && dormant.instructions == 0 && host.mutableReads == reads,
        "stopped state mutated or executed");
    host.Program("State.B",Join({Return(Int(42)),{0x08}})); host.Install("State.B");
    const auto returned = Vm::ResumeState(host,"Actor");
    Check(returned.passed() && returned.status == Vm::Status::Returned && returned.value.integer == 42,
        "state Return lost returned value");
    Check(host.states.at("Actor").frame->latent == StateLatent::Continue && host.states.at("Actor").frame->statementIndex == 1,
        "state Return incorrectly stopped or reset its ordinal");
    Check(Vm::ResumeState(host,"Actor").status == Vm::Status::Stopped,"next state slice did not resume after Return");
    const auto ordinary = host.Function("Class.Stop",{0x08});
    const auto failed = Vm::Execute(host,ordinary,"Actor");
    Check(!failed.passed() && !failed.committed && failed.status == Vm::Status::Stopped,"ordinary Stop was treated as committed state stop");
    ++rejections;
}

void ReentryAndSameState() {
    Host host;
    for (const auto& name : {"A","B","C","D"}) host.Program(std::string("State.")+name,{0x08});
    host.Function("State.A.EndState",Join({Native(800,{Int(1)}),Native(113,{Ref(0x21,4)}),Return()}));
    // Suppress only A.EndState inside its own reentrant C transition.
    host.functions["State.A.EndState"].bytecode = Join({Native(118,{Ref(0x21,11)}),Native(800,{Int(1)}),Native(113,{Ref(0x21,4)}),Return()});
    host.names[11] = "EndState";
    host.Function("State.C.BeginState",Join({Native(800,{Int(2)}),Return()}));
    host.Function("State.B.BeginState",Join({Native(800,{Int(3)}),Native(113,{Ref(0x21,5)}),Return()}));
    host.Function("State.D.BeginState",Join({Native(800,{Int(4)}),Return()}));
    host.Install("State.A");
    const auto root = host.Function("Class.Root",Join({Native(113,{Ref(0x21,3)}),Return()}));
    const auto result = Vm::Execute(host,root,"Actor");
    Check(result.passed() && host.begins == 1 && host.commits == 1 && host.rollbacks == 0,"reentrant transitions used nested transactions");
    Check(host.states.at("Actor").frame->codePath == "State.D" && host.effects == 4,"EndState/BeginState reentrant transition order changed");
    std::vector<std::string> effectsTrace;
    for (const auto& event : host.trace) if (event.starts_with("effect:")) effectsTrace.push_back(event);
    Check(effectsTrace == std::vector<std::string>{"effect:1","effect:2","effect:3","effect:4"},
        "synchronous entry/exit effect ordering changed");
    Check(host.states.at("Actor").disabled.at("A").contains("EndState"),"state transition cleared old disabled set");
    const auto effects = host.effects;
    const auto same = host.Function("Class.Same",Join({Native(113,{Ref(0x21,5)}),Return()}));
    Check(Vm::Execute(host,same,"Actor").passed() && host.effects == effects,"same-state transition called entry/exit events");
    const auto clear = host.Function("Class.Clear",Join({Native(113,{Ref(0x21,0)}),Return()}));
    host.states.at("Actor").frame->statementIndex = 91; host.states.at("Actor").frame->latent = StateLatent::Sleep;
    Check(Vm::Execute(host,clear,"Actor").passed(),"clear-state control failed");
    const auto& frame = host.states.at("Actor").frame.value();
    Check(frame.codePath.empty() && frame.localsCodePath.empty() && frame.locals.empty() && frame.statementIndex == 91 && frame.latent == StateLatent::Sleep,
        "clear-state discarded frame position/latent or retained locals");
    const auto mutations = host.mutableReads;
    Check(Vm::ResumeState(host,"Actor").passed() && host.mutableReads == mutations,"null-code frame was executed or mutated");
}

void InheritedLabelsAndLocals() {
    Host host;
    const Vm::Property local{"State.Parent.Counter","Counter",Value::Integer(0),0,1}; host.properties[30] = local;
    const Vm::Property derivedOnly{"Child.A.DerivedOnly","DerivedOnly",Value::Integer(0),0,1}; host.properties[32] = derivedOnly;
    host.Program("Child.A",Join({Let(Ref(0x00,30),Int(17)),Let(Ref(0x00,32),Int(99)),{0x0d},Ref(0x21,8),Native(801),{0x08}}),{local,derivedOnly});
    host.Program("Parent.A",Join({Let(Ref(0x00,30),Native(146,{Ref(0x00,30),Int(1)})),{0x08}}),{local},{{8,0}});
    host.labelChain["A"] = {"Child.A","Parent.A"}; host.Install("Child.A");
    const auto result = Vm::ResumeState(host,"Actor");
    const auto& frame = host.states.at("Actor").frame.value();
    Check(result.passed() && result.status == Vm::Status::Stopped,"inherited-label state slice failed");
    Check(frame.codePath == "Parent.A" && frame.localsCodePath == "Child.A" && frame.locals[0].values[0].integer == 18,
        "inherited label reset locals or changed their owner");
    Check(frame.locals.size() == 2 && frame.locals[1].values[0].integer == 99,"parent label discarded derived-owner local schema/storage");
    host.Program("State.B",Join({{0x0d},Ref(0x21,9),{0x08}})); host.Install("State.B");
    const auto before = host.states.at("Actor").frame->statementIndex;
    const auto missing = Vm::ResumeState(host,"Actor");
    Check(!missing.passed() && !missing.committed && host.states.at("Actor").frame->statementIndex == before,
        "missing in-code label succeeded or retained preadvanced PC"); ++rejections;
    const auto transition = host.Function("Class.MissingLabel",Join({Native(113,{Ref(0x21,3),Ref(0x21,9)}),Return()}));
    Check(Vm::Execute(host,transition,"Actor").passed() && host.states.at("Actor").frame->latent == StateLatent::Stop,
        "transition-label miss threw instead of stopping");
}

void CallbackRollbackAndBudgets() {
    Host host;
    host.Program("State.A",Join({Native(803),{0x08}})); host.Install("State.A");
    host.Function("State.A.Notify",Join({Native(800,{Int(1)}),Native(118,{Ref(0x21,7)}),Native(801),Return()}));
    const auto failed = Vm::ResumeState(host,"Actor");
    Check(!failed.passed() && !failed.committed && host.begins == 1 && host.rollbacks == 1,"callback failure did not roll back one root slice");
    Check(host.effects == 0 && host.states.at("Actor").frame->statementIndex == 0 && host.states.at("Actor").disabled.empty(),
        "callback rollback lost PC/effects/disabled set atomicity"); ++rejections;
    host.functions["State.A.Notify"].bytecode = Join({Native(800,{Int(1)}),Return()});
    Vm::Limits instructions; instructions.instructions = 4;
    const auto budget = Vm::ResumeState(host,"Actor",instructions);
    Check(!budget.passed() && budget.status == Vm::Status::Budget && host.effects == 0 && host.states.at("Actor").frame->statementIndex == 0,
        "callback did not share state instruction budget"); ++rejections;
    Vm::Limits depth; depth.callDepth = 1;
    Check(Vm::ResumeState(host,"Actor",depth).status == Vm::Status::Budget,"callback omitted persistent frame from shared depth budget"); ++rejections;
    host.begun = false;
    Check(Vm::ResumeState(host,"Actor").passed() && host.effects == 0,"pre-startup callback was not suppressed");
    host.begun = true; host.deleted = true;
    host.Function("State.A.Destroyed",Join({Native(800),Return()}));
    const auto enumDestroyed = host.Function("Class.EnumDestroyed",Join({Native(805),Return()}));
    Check(Vm::Execute(host,enumDestroyed,"Actor").passed() && host.effects == 1,"enum Destroyed exception did not dispatch after deletion");
    const auto namedDestroyed = host.Function("Class.NamedDestroyed",Join({Native(806),Return()}));
    Check(Vm::Execute(host,namedDestroyed,"Actor").passed() && host.effects == 1,"named Destroyed incorrectly received enum exception");
}

void PreparationAfterArguments() {
    Host host;
    host.Program("State.A",{0x08}); host.Install("State.A");
    host.functions["Class.Worker"] = {"Class.Worker","Synthetic",Return(),{},0,0};
    const auto disable = host.Function("Class.DisableArgument",Return(Call(6,{Native(118,{Ref(0x21,6)})})));
    const auto before = host.preparations;
    Check(Vm::Execute(host,disable,"Actor").passed(),"argument disable invocation failed");
    Check(host.preparations == before + 1,"callee was prepared before argument disabled it");
    host.functions["Class.Worker"].bytecode = Return(Int(73));
    const auto enable = host.Function("Class.EnableArgument",Return(Call(6,{Native(117,{Ref(0x21,6)})})));
    const auto enabled = Vm::Execute(host,enable,"Actor");
    Check(enabled.passed() && enabled.value.integer == 73,"argument enable used stale suppressed callable metadata");
    host.badPreparedIdentity = true;
    const auto malformed = Vm::Execute(host,enable,"Actor");
    Check(!malformed.passed() && malformed.status == Vm::Status::Invalid && !malformed.committed,"prepared function changed identity without rejection"); ++rejections;
}

void StateRejectionControls() {
    Host host;
    host.Program("State.A",{0x08}); host.Install("State.A");
    for (const auto latent : {StateLatent::Sleep,StateLatent::FinishAnim,StateLatent::MoveTo}) {
        host.states.at("Actor").frame->latent = latent;
        const auto before = host.mutableReads;
        const auto result = Vm::ResumeState(host,"Actor");
        Check(!result.passed() && result.status == Vm::Status::Unsupported && host.mutableReads == before,"unhandled latent action executed or mutated"); ++rejections;
    }
    host.states.at("Actor").frame->latent = StateLatent::Continue;
    host.states.at("Actor").frame->statementIndex = 500;
    Check(Vm::ResumeState(host,"Actor").status == Vm::Status::Invalid,"out-of-range state ordinal was accepted"); ++rejections;
    host.states.at("Actor").frame->statementIndex = 1;
    const auto table = Vm::ResumeState(host,"Actor");
    Check(!table.passed() && !table.committed,"terminal LabelTable was treated as executable success"); ++rejections;
    host.Program("State.B",Join({Native(804),{0x08}})); host.Install("State.B");
    Check(!Vm::ResumeState(host,"Actor").passed() && host.states.at("Actor").frame->latent == StateLatent::Continue && host.states.at("Actor").frame->statementIndex == 0,
        "unhandled newly entered latent action committed a partial slice"); ++rejections;
    host.Program("State.C",Jump(0)); host.Install("State.C");
    Vm::Limits limits; limits.instructions = 8;
    Check(Vm::ResumeState(host,"Actor",limits).status == Vm::Status::Budget && host.states.at("Actor").frame->statementIndex == 0,
        "state loop bypassed shared instruction budget or rollback"); ++rejections;
    const Vm::Property local{"State.D.Local","Local",Value::Integer(0),0,1}; host.properties[31] = local;
    host.Program("State.D",Join({Let(Ref(0x00,31),Int(99)),Native(801),{0x08}}),{local}); host.Install("State.D");
    Check(!Vm::ResumeState(host,"Actor").passed() && host.states.at("Actor").frame->locals[0].values[0].integer == 0 &&
        host.states.at("Actor").frame->statementIndex == 0,"state local storage did not roll back with failed slice"); ++rejections;
    host.states.at("Actor").frame->locals.clear();
    Check(!Vm::ResumeState(host,"Actor").passed(),"missing persistent locals were silently fabricated"); ++rejections;
    host.states.at("Actor").frame->latent = StateLatent::Stop;
    host.states.at("Actor").frame->statementIndex = 0xffffffffu;
    Check(Vm::ResumeState(host,"Actor").passed(),"stopped frame's intentionally retained stale ordinal was validated as runnable");
    host.states.at("Actor").frameOverride = false;
    const auto mutations = host.mutableReads;
    Check(Vm::ResumeState(host,"Actor").passed() && host.mutableReads == mutations,"authored dormant metadata was fabricated into an executable override");
    host.states.clear();
    Check(Vm::ResumeState(host,"Actor").passed(),"absent state was not a read-only successful no-execution result");
}

void OldStatementControlsNewCode() {
    Host host;
    host.Program("State.B",Join({{0x08},Native(800),{0x08}}),{},{{1,1},{8,0}});
    host.Program("State.A",Join({{0x07,0,0},Native(807),{0x08}})); host.Install("State.A");
    const auto jump = Vm::ResumeState(host,"Actor");
    Check(jump.passed() && jump.status == Vm::Status::Stopped && host.effects == 0 &&
        host.states.at("Actor").frame->codePath == "State.B" && host.states.at("Actor").frame->statementIndex == 1,
        "old conditional jump did not apply to newly selected code");
    host.Program("State.A",Join({{0x0d},Native(808),{0x08}})); host.Install("State.A");
    const auto label = Vm::ResumeState(host,"Actor");
    Check(label.passed() && label.status == Vm::Status::Stopped && host.effects == 0,
        "old goto-label result did not apply against current code");
    host.Program("State.A",Return(Native(809))); host.Install("State.A");
    const auto returned = Vm::ResumeState(host,"Actor");
    Check(returned.passed() && returned.value.integer == 42 && host.states.at("Actor").frame->codePath == "State.B" &&
        host.states.at("Actor").frame->statementIndex == 1 && host.states.at("Actor").frame->latent == StateLatent::Continue,
        "Return after transition discarded new state position or changed latent status");
    Check(Vm::ResumeState(host,"Actor").passed() && host.effects == 1,"next slice did not execute new state's selected Begin block");
}

void RecreatedSameCodeLocals() {
    Host host;
    const Vm::Property local{"State.A.Local","Local",Value::Integer(0),0,1}; host.properties[33] = local;
    const auto first = Let(Ref(0x00,33),Int(12));
    const auto transition = Native(113,{Ref(0x21,3)});
    const auto resumedOffset = static_cast<std::uint32_t>(first.size() + transition.size());
    host.Program("State.A",Join({first,transition,Let(Ref(0x00,33),Int(27)),{0x08}}),{local},{{1,0},{10,resumedOffset}});
    host.Program("State.B",{0x08});
    host.Function("State.B.BeginState",Join({Native(113,{Ref(0x21,2),Ref(0x21,10)}),Return()}));
    host.Install("State.A");
    const auto initialRevision = host.localRevisions.at("Actor");
    const auto result = Vm::ResumeState(host,"Actor");
    Check(result.passed() && result.status == Vm::Status::Stopped && host.localRevisions.at("Actor") == initialRevision + 2,
        "A->B->A reentrant state slice did not finish using recreated storage");
    Check(host.states.at("Actor").frame->codePath == "State.A" && host.states.at("Actor").frame->locals[0].values[0].integer == 27,
        "same-code recreated locals reused stale references or kept old values");
}

void InCodeLabelPreservesLatent() {
    Host host;
    const auto expression = Join({{0x0d},Native(810)});
    host.Program("State.A",Join({expression,{0x08}}),{},{{1,0},{8,static_cast<std::uint32_t>(expression.size())}});
    host.Install("State.A");
    const auto result = Vm::ResumeState(host,"Actor");
    Check(!result.passed() && !result.committed && result.status == Vm::Status::Unsupported &&
        result.error == "State latent action requires its runtime handler",
        "in-code label wrongly cleared a latent action set while evaluating its label");
    Check(host.states.at("Actor").frame->latent == StateLatent::Continue && host.states.at("Actor").frame->statementIndex == 0,
        "unsupported latent-label slice did not restore the original frame");
    ++rejections;
}

std::vector<std::string> EffectTrace(const Host& host) {
    std::vector<std::string> result;
    for (const auto& entry : host.trace) if (entry.starts_with("effect:")) result.push_back(entry);
    return result;
}
bool SameState(const StateObject& a, const StateObject& b) {
    if (a.hasStack != b.hasStack || a.frameOverride != b.frameOverride || a.disabled != b.disabled ||
        a.frame.has_value() != b.frame.has_value()) return false;
    if (!a.frame) return true;
    const auto& x = *a.frame; const auto& y = *b.frame;
    if (x.codePath != y.codePath || x.localsCodePath != y.localsCodePath || x.statementIndex != y.statementIndex ||
        x.latent != y.latent || x.locals.size() != y.locals.size()) return false;
    for (std::size_t i = 0; i < x.locals.size(); ++i) {
        if (x.locals[i].key != y.locals[i].key || x.locals[i].values.size() != y.locals[i].values.size()) return false;
        for (std::size_t j = 0; j < x.locals[i].values.size(); ++j)
            if (!Vm::Equal(x.locals[i].values[j],y.locals[i].values[j])) return false;
    }
    return true;
}
void SwitchRollback(Host& host, const Vm::Status status, const std::string& description,
    const Vm::Limits& limits = {}) {
    const auto state = host.states.at("Actor"); const auto values = host.values;
    const auto revisions = host.localRevisions; const auto effects = host.effects;
    const auto rollbacks = host.rollbacks;
    const auto result = Vm::ResumeState(host,"Actor",limits);
    Check(!result.passed() && !result.committed && result.status == status,description + " did not fail explicitly: " + result.error);
    Check(host.rollbacks == rollbacks + 1 && SameState(state,host.states.at("Actor")) &&
        host.localRevisions == revisions && host.effects == effects,description + " lost complete frame/effect/disabled rollback");
    Check(values.size() == host.values.size(),description + " changed instance storage shape");
    for (const auto& [key,value] : values) Check(Vm::Equal(value,host.values.at(key)),description + " changed instance value");
    ++rejections;
}
Vm::Property SwitchLocal(Host& host) {
    const Vm::Property local{"State.A.SwitchLocal","SwitchLocal",Value::Integer(0),0,1};
    host.properties[33] = local;
    host.properties[20] = {"Class.Count","Count",Value::Integer(0),0,1}; host.values["Class.Count"] = Value::Integer(0);
    return local;
}

void StateSwitchSelection() {
    for (const auto selected : {1,2,99}) {
        Host host;
        const auto program = Choice(Int(selected),Int(1),Native(800,{Int(1)}),Int(2),Native(800,{Int(2)}),Native(800,{Int(3)}));
        host.Program("State.A",program.body); host.Install("State.A");
        const auto result = Vm::ResumeState(host,"Actor");
        Check(result.passed() && result.status == Vm::Status::Stopped,"simple state Switch failed");
        Check(EffectTrace(host) == std::vector<std::string>{"effect:" + std::to_string(selected == 99 ? 3 : selected)},
            "state Switch selected wrong branch or failed to break");
    }
    {
        Host host;
        auto inner = Choice(Int(2),Int(1),Native(800,{Int(101)}),Int(2),Native(800,{Int(102)}),
            Native(800,{Int(103)}),true,{0x0b});
        Relocate(inner,static_cast<std::uint32_t>(Switch(Int(1)).size()+Case(Int(1)).size()));
        const auto outer = Choice(Int(1),Int(1),Join({inner.body,Native(800,{Int(104)})}),Int(2),Native(801),Native(801));
        host.Program("State.A",outer.body); host.Install("State.A");
        Check(Vm::ResumeState(host,"Actor").passed() && EffectTrace(host) == std::vector<std::string>{"effect:102","effect:104"},
            "nested state Switch retained an obsolete selector or branch target");
    }
    {
        Host host;
        // Fall-through executes Case as a no-op, never evaluating its value.
        const auto program = Choice(Int(1),Int(1),Native(800,{Int(1)}),Native(801),Native(800,{Int(2)}),Native(801),false);
        host.Program("State.A",program.body); host.Install("State.A");
        Check(Vm::ResumeState(host,"Actor").passed() && EffectTrace(host) == std::vector<std::string>{"effect:1","effect:2"},
            "Case fall-through evaluated skipped selector expressions");
    }
    {
        Host host; const auto local = SwitchLocal(host);
        const auto caseValue = Native(814,{Ref(0x01,20),Native(802),Int(1)});
        const auto program = Choice(Int(1),caseValue,Let(Ref(0x00,33),Int(27)),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body,{local}); host.Install("State.A");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        const auto layout = Vm::AnalyzeProgram(host,host.programs.at("State.A"));
        const auto caseOrdinal = std::find(layout.statementOffsets.begin(),layout.statementOffsets.end(),program.first)-layout.statementOffsets.begin();
        Check(Vm::ResumeState(host,"Actor").passed() && host.values.at("Class.Count").integer == caseOrdinal+1,
            "Case did not advance live current-code ordinal before evaluating its expression");
        Check(host.states.at("Actor").frame->localsCodePath == "State.A" && host.states.at("Actor").frame->locals[0].values[0].integer == 27,
            "ordinary Switch reset retained state-local ownership/storage");
    }
    {
        Host host; const auto local = SwitchLocal(host);
        const auto program = Choice(Ref(0x00,33),Native(814,{Ref(0x00,33),Int(19),Int(19)}),Native(800,{Int(19)}),Int(12),Native(801),Native(801));
        host.Program("State.A",program.body,{local}); host.Install("State.A");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        Check(Vm::ResumeState(host,"Actor").passed() && EffectTrace(host) == std::vector<std::string>{"effect:19"} &&
            host.states.at("Actor").frame->locals[0].values[0].integer == 19,
            "Switch eagerly detached a selector alias before evaluating the Case");
    }
    {
        Host host; const auto local = SwitchLocal(host);
        const auto program = Choice(Ref(0x00,33),Native(814,{Ref(0x00,33),Int(19),Int(18)}),Native(801),
            Native(814,{Ref(0x00,33),Int(21),Int(21)}),Native(800,{Int(21)}),Native(801));
        host.Program("State.A",program.body,{local}); host.Install("State.A");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        Check(Vm::ResumeState(host,"Actor").passed() && EffectTrace(host) == std::vector<std::string>{"effect:21"},
            "Switch did not reload selector alias after every visited Case");
    }
}

void StateSwitchSelectorTransitions() {
    for (const auto mode : {0,1,2,3}) {
        Host host; const auto local = SwitchLocal(host);
        const auto selector = Native(811,{Ref(0x21,12),Int(2)});
        const auto branch = Join({Let(Ref(0x01,20),Ref(0x00,33)),Let(Ref(0x00,33),Int(27)),Native(800,{Int(2)})});
        const auto program = Choice(selector,Int(1),Native(801),Int(2),branch,Native(801));
        host.Program("State.A",program.body,{local},{{1,0},{10,program.second}});
        if (mode == 0) host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,2),Ref(0x21,10)}),Return()}));
        else if (mode == 1) {
            const auto other = Choice(Int(0),Int(2),branch,Int(1),Native(801),Native(801));
            host.Program("State.B",other.body,{local},{{1,other.first}});
            host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
        } else if (mode == 2) {
            host.Program("State.B",{0x08});
            host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
            host.Function("State.B.BeginState",Join({Native(113,{Ref(0x21,2),Ref(0x21,10)}),Return()}));
        } else {
            const auto inherited = Choice(Int(0),Int(2),branch,Int(1),Native(801),Native(801));
            host.Program("Parent.A",inherited.body,{local},{{10,inherited.first}});
            host.labelChain["A"] = {"State.A","Parent.A"};
            host.Function("State.A.Probe",Join({Native(816,{Ref(0x21,10)}),Return()}));
            // State.A has no ContinueHere here: force inherited label selection.
            host.Program("State.A",program.body,{local});
        }
        host.Install("State.A"); const auto initialRevision = host.localRevisions.at("Actor");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        const auto result = Vm::ResumeState(host,"Actor");
        Check(result.passed() && EffectTrace(host) == std::vector<std::string>{"effect:2"},
            "selector callback did not search Cases at live code/label PC, mode " + std::to_string(mode) + ": " + result.error);
        const auto& frame = *host.states.at("Actor").frame;
        Check(frame.codePath == (mode == 1 ? "State.B" : mode == 3 ? "Parent.A" : "State.A") &&
            frame.localsCodePath == (mode == 1 ? "State.B" : "State.A") && frame.locals[0].values[0].integer == 27,
            "selector transition used old code, locals owner or invalidated aliases");
        Check(host.localRevisions.at("Actor") == initialRevision + (mode == 1 ? 1 : mode == 2 ? 2 : 0),
            "selector transition unexpectedly recreated or retained state locals");
        Check(host.values.at("Class.Count").integer == (mode == 0 || mode == 3 ? 12 : 0),
            "selector transition failed to retain old locals or zero newly recreated storage before branch mutation");
    }
}

void StateSwitchCaseTransitions() {
    for (const auto mode : {0,1,2,3}) {
        Host host; const auto local = SwitchLocal(host);
        const auto caseValue = Native(811,{Ref(0x21,12),Int(mode == 2 ? 0 : 1)});
        const auto branchBody = Join({Let(Ref(0x01,20),Ref(0x00,33)),Let(Ref(0x00,33),Int(27)),Native(800,{Int(2)})});
        const auto program = Choice(Int(1),caseValue,Native(801),Int(2),branchBody,Native(801));
        const auto secondBody = program.second + static_cast<std::uint32_t>(Case(Int(2)).size());
        host.Program("State.A",program.body,{local},{{1,0},{10,secondBody}});
        if (mode == 0) host.Function("State.A.Probe",Join({Native(816,{Ref(0x21,10)}),Return()}));
        else if (mode == 1) {
            host.Program("State.B",Join({branchBody,{0x08}}),{local});
            host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
        } else if (mode == 2) {
            // The old Case NextOffset points at a Case in B, not B's Begin.
            // Deliberately use a different first Case expression so offsets
            // cannot accidentally match by statement ordinal.
            Bytes other(program.second,0x0b);
            Append(other,Case(Int(1))); const auto newCase = program.second;
            Append(other,Join({branchBody,Jump(0)}));
            const auto branch = other.size()-3u; const auto fallback = other.size();
            Append(other,{0x0a,0xff,0xff}); Append(other,Native(801)); const auto end = other.size(); Append(other,{0x08});
            Target(other,newCase,fallback); Target(other,branch,end);
            host.Program("State.B",other,{local},{{1,0}});
            host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
        } else {
            host.Program("State.B",{0x08});
            host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
            host.Function("State.B.BeginState",Join({Native(113,{Ref(0x21,2),Ref(0x21,10)}),Return()}));
        }
        host.Install("State.A"); const auto revision = host.localRevisions.at("Actor");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        const auto result = Vm::ResumeState(host,"Actor");
        Check(result.passed() && EffectTrace(host) == std::vector<std::string>{"effect:2"},
            "Case callback match/mismatch did not preserve or map live code PC, mode " + std::to_string(mode) + ": " + result.error);
        Check(host.states.at("Actor").frame->locals[0].values[0].integer == 27 &&
            host.localRevisions.at("Actor") == revision + (mode == 0 ? 0 : mode == 3 ? 2 : 1),
            "Case callback changed storage without rebinding current state locals");
        Check(host.values.at("Class.Count").integer == (mode == 0 ? 12 : 0),
            "Case callback failed to preserve old locals or zero recreated storage before selected branch mutation");
    }
}

void StateSwitchCallbackStopsAndClears() {
    for (const auto selectorStops : {false,true}) {
        Host host;
        const auto program = Choice(selectorStops ? Native(815,{Int(1)}) : Int(1),
            selectorStops ? Int(1) : Native(815,{Int(1)}),Native(801),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body); host.Install("State.A");
        const auto layout = Vm::AnalyzeProgram(host,host.programs.at("State.A"));
        const auto caseOrdinal = std::find(layout.statementOffsets.begin(),layout.statementOffsets.end(),program.first)-layout.statementOffsets.begin();
        const auto result = Vm::ResumeState(host,"Actor");
        Check(result.passed() && result.status == Vm::Status::Stopped && host.effects == 0 &&
            host.states.at("Actor").frame->statementIndex == caseOrdinal+1,
            "Switch stopped linked search prematurely or executed body after callback Stop");
    }
    {
        Host host; SwitchLocal(host);
        const auto program = Choice(Int(9),Native(815,{Int(1)}),Native(801),
            Native(814,{Ref(0x01,20),Int(29),Int(2)}),Native(801),Native(801));
        host.Program("State.A",program.body); host.Install("State.A");
        const auto layout = Vm::AnalyzeProgram(host,host.programs.at("State.A"));
        const auto fallbackOrdinal = std::find(layout.statementOffsets.begin(),layout.statementOffsets.end(),program.fallback)-layout.statementOffsets.begin();
        Check(Vm::ResumeState(host,"Actor").passed() && host.values.at("Class.Count").integer == 29 &&
            host.states.at("Actor").frame->statementIndex == fallbackOrdinal+1,
            "Stop during nonmatching Case truncated linked search before default selection");
    }
    for (const auto native : {817,818}) {
        Host host;
        const auto program = Choice(Native(native,{Int(1)}),Int(1),Native(801),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body); host.Install("State.A");
        const auto result = Vm::ResumeState(host,"Actor");
        Check(result.passed() && result.committed && host.effects == 0 &&
            (native == 818 ? !host.states.at("Actor").frame : host.states.at("Actor").frame->codePath.empty()),
            "selector-cleared frame executed old Case chain");
        const auto second = Choice(Int(1),Native(native,{Int(1)}),Native(801),Int(2),Native(801),Native(801));
        host.Program("State.A",second.body); host.Install("State.A");
        SwitchRollback(host,Vm::Status::Invalid,"Case cleared active frame");
    }
}

void StateSwitchFailureAndBudgets() {
    for (const auto inCase : {false,true}) {
        Host host; const auto local = SwitchLocal(host);
        const auto callback = Native(811,{Ref(0x21,12),Int(1)});
        const auto program = Choice(inCase ? Int(1) : callback,inCase ? callback : Int(1),Native(800),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body,{local}); host.Install("State.A");
        host.states.at("Actor").disabled["C"].insert("Worker");
        host.Function("State.A.Probe",Join({Let(Ref(0x01,20),Int(19)),Native(800),Native(118,{Ref(0x21,7)}),Native(801),Return()}));
        SwitchRollback(host,Vm::Status::Unsupported,inCase ? "Case callback failure" : "selector callback failure");
    }
    {
        Host host; const auto local = SwitchLocal(host);
        const auto selected = Join({Let(Ref(0x00,33),Int(19)),Let(Ref(0x01,20),Int(29)),Native(800),Native(118,{Ref(0x21,7)}),Native(801)});
        const auto program = Choice(Int(1),Int(1),selected,Int(2),Native(801),Native(801));
        host.Program("State.A",program.body,{local}); host.Install("State.A");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        host.states.at("Actor").disabled["C"].insert("Worker");
        SwitchRollback(host,Vm::Status::Unsupported,"selected Switch branch failure");
        host.Program("State.A",Choice(Int(1),Int(1),
            Join({Let(Ref(0x00,33),Int(19)),Native(811,{Ref(0x21,12),Int(0)})}),Int(2),Native(801),Native(801)).body,{local});
        host.Function("State.A.Probe",Join({Native(800),Let(Ref(0x01,20),Int(29)),Native(118,{Ref(0x21,7)}),Return()}));
        Vm::Limits writes; writes.writes = 1;
        SwitchRollback(host,Vm::Status::Budget,"Switch callback shared write budget",writes);
        Vm::Limits depth; depth.callDepth = 1;
        SwitchRollback(host,Vm::Status::Budget,"Switch callback shared call depth budget",depth);
        const auto successful = Vm::ResumeState(host,"Actor");
        Check(successful.passed(),"Switch budget fixture's unbounded control failed");
        host.Install("State.A"); host.values["Class.Count"] = Value::Integer(0); host.effects = 0;
        Vm::Limits instructions; instructions.instructions = successful.instructions-1u;
        SwitchRollback(host,Vm::Status::Budget,"Switch linked search shared instruction budget",instructions);
    }
    {
        Host host; const auto local = SwitchLocal(host);
        const auto program = Choice(Ref(0x00,33),Native(811,{Ref(0x21,12),Int(12)}),Native(801),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body,{local}); host.Program("State.B",{0x08}); host.Install("State.A");
        host.states.at("Actor").frame->locals[0].values[0] = Value::Integer(12);
        host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
        host.Function("State.B.BeginState",Join({Native(113,{Ref(0x21,2)}),Return()}));
        SwitchRollback(host,Vm::Status::Invalid,"recreated state-local Switch selector alias");
    }
    {
        Host host; const auto local = SwitchLocal(host);
        const auto program = Choice(Ref(0x01,20),Native(811,{Ref(0x21,12),Int(12)}),Native(801),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body,{local}); host.Program("State.B",Join({Native(800,{Int(2)}),{0x08}}),{local}); host.Install("State.A");
        host.values["Class.Count"] = Value::Integer(12);
        host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
        Check(Vm::ResumeState(host,"Actor").passed() && EffectTrace(host) == std::vector<std::string>{"effect:2"},
            "Switch invalidated a stable instance selector alias when only state locals were replaced");
    }
    for (const auto selectorChanges : {false,true}) {
        Host host;
        const auto callback = Native(811,{Ref(0x21,12),Int(0)});
        const auto program = Choice(selectorChanges ? callback : Int(1),selectorChanges ? Int(1) : callback,Native(801),Int(2),Native(801),Native(801));
        host.Program("State.A",program.body);
        Bytes other(program.second+1u,0x0b); other.back() = 0x08;
        host.Program("State.B",other); host.Install("State.A");
        host.Function("State.A.Probe",Join({Native(800),Native(113,{Ref(0x21,3)}),Return()}));
        SwitchRollback(host,Vm::Status::Invalid,selectorChanges ? "selector selected non-Case current PC" : "Case NextOffset maps to non-Case current code");
    }
    {
        Host host;
        Bytes body = Switch(Int(99)); const auto firstCase = body.size(); Append(body,Case(Int(1))); Append(body,{0x08});
        Target(body,firstCase,firstCase); host.Program("State.A",body); host.Install("State.A");
        Vm::Limits limits; limits.instructions = 9;
        SwitchRollback(host,Vm::Status::Budget,"cyclic state Case linked search",limits);
    }
}

void StateSwitchWithIterators() {
    for (const auto mode : {0,1,2,3,4}) {
        Host host;
        Bytes code{0x2f}; Append(code,Native(819)); const auto endPosition = code.size(); Append(code,{0,0});
        const auto branch = mode == 1 ? Bytes{0x08} : mode == 2 ? Return() : Native(800,{Int(1)});
        const auto choice = Choice(mode >= 3 ? Native(811,{Ref(0x21,12),Int(1)}) : Int(1),Int(1),branch,Int(2),Native(801),Native(801),true,{0x0b});
        Append(code,choice.body); Append(code,{0x31}); const auto pop = code.size(); Append(code,{0x30,0x08});
        code.at(endPosition) = static_cast<std::uint8_t>(pop); code.at(endPosition+1u) = static_cast<std::uint8_t>(pop >> 8u);
        // Choice was assembled from offset zero; rebase every Case/Jump target.
        const auto start = endPosition+2u;
        Target(code,start+choice.first,start+choice.second); Target(code,start+choice.second,start+choice.fallback);
        Target(code,start+choice.second-3u,start+choice.end); Target(code,start+choice.fallback-3u,start+choice.end);
        host.Program("State.A",code); host.Program("State.B",{0x08}); host.Install("State.A");
        if (mode == 4) {
            // Begin points at a valid effectful Case. A post-search guard
            // would still roll back effects, but wrongly visit this label.
            auto replacement = Case(Native(800,{Int(999)}));
            const auto fallback = replacement.size(); Append(replacement,{0x0a,0xff,0xff,0x08});
            Target(replacement,0,fallback); host.Program("State.B",replacement);
        }
        host.Function("State.A.Probe",Join({Native(113,{Ref(0x21,3)}),Return()}));
        if (mode == 0) Check(Vm::ResumeState(host,"Actor").passed() && host.effects == 1 && host.iteratorSteps == 2,
            "synchronous Switch inside state iterator failed to Pop before Stop");
        else SwitchRollback(host,Vm::Status::Unsupported,"Switch live iterator continuation/code change");
        if (mode == 4) Check(std::find(host.trace.begin(),host.trace.end(),"effect:999") == host.trace.end(),
            "Switch visited a replacement Case before refusing its live iterator");
        Check(host.iteratorCreates == 1 && host.iteratorDestroys == 1,"Switch leaked a state iterator after exit/rollback");
    }
}
} // namespace

int main() {
    try {
        SuccessStopAndReturn(); ReentryAndSameState(); InheritedLabelsAndLocals();
        CallbackRollbackAndBudgets(); PreparationAfterArguments(); StateRejectionControls();
        OldStatementControlsNewCode();
        RecreatedSameCodeLocals();
        InCodeLabelPreservesLatent();
        StateSwitchSelection(); StateSwitchSelectorTransitions(); StateSwitchCaseTransitions();
        StateSwitchCallbackStopsAndClears(); StateSwitchFailureAndBudgets(); StateSwitchWithIterators();
        std::cout << "PASS state execution checks=" << checks << " rejections=" << rejections << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "State execution test failed: " << error.what() << '\n'; return 1;
    }
}
