#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_actor_geometry.h"
#include "quest_mesh_animation.h"
#include "quest_model_point_trace.h"
#include "quest_decal_projection.h"
#include "quest_decal_state.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"
#include "quest_script_random.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using QuestVr::Vm::Evaluation;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Status;
using QuestVr::Vm::Value;
void Require(const bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
Evaluation Name(const std::string& value) { return {Value::Text(Kind::Name, value), {}}; }
Evaluation Number(const float value) { return {Value::Float(value), {}}; }
Evaluation Vector(const std::array<float, 3>& value) { return {Value::Vector(value), {}}; }
QuestVr::Vm::Result Call(const std::string& actor, const std::string& function,
    const std::vector<Evaluation>& arguments = {}) {
    auto result = ExecutePortableActorFunction(actor, function, arguments);
    Require(result.passed(), function + " failed: " + result.error +
        " at " + result.function + ':' + std::to_string(result.offset));
    return result;
}
bool IsA(const PortableActorSnapshot& actor, const std::string& type) {
    return QuestVr::Vm::ToBool(Call(actor.objectPath, "IsA", {Name(type)}).value);
}
PortableActorSnapshot Snapshot(const std::string& actor) {
    const auto snapshots = GetPortableRuntimeMapActors();
    const auto found = std::find_if(snapshots.begin(), snapshots.end(), [&](const auto& value) {
        return value.objectPath == actor;
    });
    Require(found != snapshots.end(), "Original actor disappeared during scoped execution");
    return *found;
}
bool Same(const PortableActorSnapshot& a, const PortableActorSnapshot& b) {
    const auto& x = a.animation; const auto& y = b.animation;
    if (!(x.sequence == y.sequence && x.frame == y.frame && x.rate == y.rate &&
        x.last == y.last && x.minRate == y.minRate && x.tweenRate == y.tweenRate &&
        x.oldRate == y.oldRate && x.loop == y.loop && x.notify == y.notify && x.finished == y.finished &&
        x.previous.vertexOffset0 == y.previous.vertexOffset0 && x.previous.vertexOffset1 == y.previous.vertexOffset1 &&
        x.previous.fraction == y.previous.fraction && a.prePivotX == b.prePivotX &&
        a.prePivotY == b.prePivotY && a.prePivotZ == b.prePivotZ && a.fatness == b.fatness &&
        a.animByOwner == b.animByOwner && a.ownerPath == b.ownerPath && a.animationSourcePath == b.animationSourcePath)) return false;
    for (std::size_t i = 0; i < x.blends.size(); ++i) {
        const auto& u = x.blends[i]; const auto& v = y.blends[i];
        if (std::tie(u.sequence,u.frame,u.rate,u.last,u.minRate,u.tweenRate,u.oldRate,u.loop,
                u.previous.vertexOffset0,u.previous.vertexOffset1,u.previous.fraction) !=
            std::tie(v.sequence,v.frame,v.rate,v.last,v.minRate,v.tweenRate,v.oldRate,v.loop,
                v.previous.vertexOffset0,v.previous.vertexOffset1,v.previous.fraction)) return false;
    }
    return true;
}
bool SameStack(const std::optional<PortableObjectStack>& a,
               const std::optional<PortableObjectStack>& b) {
    return a.has_value() == b.has_value() && (!a ||
        std::tie(a->functionReference, a->stateReference, a->probeMask,
                 a->latentAction, a->logicalOffset) ==
        std::tie(b->functionReference, b->stateReference, b->probeMask,
                 b->latentAction, b->logicalOffset));
}
void VerifyAuthoredStack(const std::filesystem::path& root, const std::string& actor) {
    const auto dot = actor.find('.');
    Require(dot != std::string::npos, "Original actor fixture lacks source map identity");
    const auto source = LoadPortablePackageTables((root / "Maps" /
        (actor.substr(0, dot) + ".dx")).string());
    const auto expected = LoadPortableExportProperties(source,
        FindPortableExport(source, actor.substr(dot + 1))).stack;
    Require(expected && SameStack(expected, ReadPortableActorSerializedStack(actor)),
        "Runtime discarded or reinterpreted authored HasStack metadata: " + actor);
    Require(expected->logicalOffset && *expected->logicalOffset == -1,
        "Original dormant class-backed fixture is not a stopped record");
    std::string folded = actor;
    for (char& c : folded) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    Require(SameStack(expected, ReadPortableActorSerializedStack(folded)),
        "Runtime authored stack lookup lost UE case-insensitive identity");
    std::cout << "ORIGINAL AUTHORED STACK " << actor << " retained without state execution\n";
}
void VerifyAuthoredStateMetadata(const std::filesystem::path& root) {
    for (const auto& fixture : {std::pair{"Engine", "Actor"},
            std::pair{"DeusEx", "ScriptedPawn.Standing"}}) {
        const auto source = LoadPortablePackageTables((root / "System" /
            (std::string(fixture.first) + ".u")).string());
        const auto index = FindPortableExport(source, fixture.second);
        const auto expected = source.exports[index].ObjClass == 0 ?
            LoadPortableClassDescriptor(source, index).state : LoadPortableStateDescriptor(source, index);
        const auto actual = ReadPortableRuntimeAuthoredStateDescriptor(
            std::string(fixture.first) + '.' + fixture.second);
        Require(std::tie(expected.objectPath, expected.baseField, expected.nextField,
                expected.scriptText, expected.children, expected.friendlyName, expected.line,
                expected.textPos, expected.logicalSize, expected.rawBytes, expected.bytecode,
                expected.probeMask, expected.ignoreMask, expected.labelTableOffset, expected.stateFlags) ==
            std::tie(actual.objectPath, actual.baseField, actual.nextField,
                actual.scriptText, actual.children, actual.friendlyName, actual.line,
                actual.textPos, actual.logicalSize, actual.rawBytes, actual.bytecode,
                actual.probeMask, actual.ignoreMask, actual.labelTableOffset, actual.stateFlags),
            "Runtime lost authored State/Class dispatch metadata");
    }
    bool wrongDescriptor{}, wrongActor{};
    try { ReadPortableRuntimeAuthoredStateDescriptor("Engine.Actor.BeginPlay"); }
    catch (const std::exception&) { wrongDescriptor = true; }
    try { ReadPortableActorSerializedStack("Engine.Actor.BeginPlay"); }
    catch (const std::exception&) { wrongActor = true; }
    Require(wrongDescriptor && wrongActor && !GetPortableRuntimeScriptStatePresent(),
        "Metadata inspection accepted wrong identities or created live script state");
    std::cout << "ORIGINAL STATE/CLASS METADATA retained; readonly identity controls passed\n";
}
// Only table identities are available during structural analysis. Any attempt
// to execute, resolve a callee or access live variables must fail this audit.
struct OriginalProgramHost final : QuestVr::Vm::Host {
    explicit OriginalProgramHost(const PortablePackageTables& source) : package(source) {}
    const PortablePackageTables& package;
    std::size_t unexpected{};
    [[noreturn]] void Reject() { ++unexpected; throw std::runtime_error("Structural original audit requested effects"); }
    void Begin() override { Reject(); }
    void Commit() override { Reject(); }
    void Rollback() noexcept override { ++unexpected; }
    QuestVr::Vm::Property ResolveProperty(const QuestVr::Vm::Function&, std::int32_t) override { Reject(); }
    std::string ResolveName(const QuestVr::Vm::Function& function, const std::int32_t index) override {
        Require(function.source == package.sourcePath && index >= 0 &&
            static_cast<std::size_t>(index) < package.names.size(), "Original structural name table mismatch");
        return package.names[static_cast<std::size_t>(index)].Name.ToString();
    }
    std::string ResolveObject(const QuestVr::Vm::Function& function, const std::int32_t reference) override {
        Require(function.source == package.sourcePath, "Original structural object table mismatch");
        return GetPortableObjectPath(package, reference);
    }
    std::shared_ptr<const QuestVr::Vm::Function> ResolveFunction(const QuestVr::Vm::Function&,
        const std::string&, const QuestVr::Vm::Invocation&) override { Reject(); }
    std::shared_ptr<QuestVr::Vm::Reference> Variable(const std::string&,
        const QuestVr::Vm::Property&, QuestVr::Vm::Scope) override { Reject(); }
    Evaluation Native(std::uint16_t, const std::string&, const std::vector<Evaluation>&,
        const QuestVr::Vm::Function*) override { Reject(); }
};
// Independently decode source Children/Next and SuperField links. A detached
// runtime graph must not silently substitute alphabetical/FName order or
// same-name receiver states for the original selected state identity.
void VerifyOriginalDispatchMetadata(const std::vector<PortablePackageTables>& tables,
    const PortableRuntimeSummary& runtime) {
    namespace Dispatch = QuestVr::ScriptDispatch;
    std::map<std::string, Dispatch::Class> expectedClasses;
    std::map<std::string, Dispatch::State> expectedStates;
    std::vector<Dispatch::Class> actualClasses;
    std::size_t linkedStates{}, childFields{}, explicitParents{}, callbackQueries{};
    for (const auto& package : tables) {
        const auto reflection = BuildPortableReflectionGraph(package);
        const auto prefix = std::filesystem::path(package.sourcePath).stem().string()+'.';
        const auto path = [&](const std::int32_t reference) {
            if (!reference) return std::string{};
            const auto name = GetPortableObjectPath(package,reference);
            return reference > 0 ? prefix+name : name;
        };
        const auto children = [&](const std::string& owner,const std::int32_t first) {
            std::vector<std::size_t> result;
            std::set<std::int32_t> visited;
            auto reference = first;
            while (reference) {
                Require(reference > 0 && static_cast<std::size_t>(reference) <= reflection.objects.size() &&
                    visited.size() < 8192u && visited.insert(reference).second,
                    "Original source Children/Next is nonlocal, cyclic or invalid: "+owner);
                const auto index = static_cast<std::size_t>(reference-1);
                const auto& field = reflection.objects[index];
                Require(field.outerPath == owner,"Original source child has a foreign owner: "+owner);
                result.push_back(index); ++childFields;
                if (field.metaClass == "Function") reference = LoadPortableFunctionScript(package,index).nextField;
                else if (field.metaClass == "State") reference = LoadPortableStateDescriptor(package,index).nextField;
                else if (field.metaClass == "Class") reference = LoadPortableClassDescriptor(package,index).state.nextField;
                else if (field.metaClass.size() >= 8u && field.metaClass.compare(field.metaClass.size()-8u,8u,"Property") == 0)
                    reference = LoadPortablePropertyDescriptor(package,index).nextField;
                else reference = LoadPortableFieldLinks(package,index).nextField;
            }
            return result;
        };
        for (std::size_t index=0;index<reflection.objects.size();++index) {
            const auto& object = reflection.objects[index];
            if (object.metaClass != "Class") continue;
            const auto descriptor = LoadPortableClassDescriptor(package,index);
            Dispatch::Class expected;
            expected.path = prefix+object.objectPath;
            expected.parentPath = path(descriptor.state.baseField);
            expected.probeMask = descriptor.state.probeMask;
            for (const auto child : children(object.objectPath,descriptor.state.children)) {
                const auto& field = reflection.objects[child];
                const auto name = package.names.at(static_cast<std::size_t>(package.exports[child].ObjName)).Name;
                if (field.metaClass == "Function")
                    expected.functions.emplace(Dispatch::FoldName(name.ToString()),prefix+field.objectPath);
                else if (field.metaClass == "State") {
                    const auto source = LoadPortableStateDescriptor(package,child);
                    Dispatch::State state;
                    state.path = prefix+field.objectPath; state.name = name.ToString();
                    state.compareIndex = static_cast<std::uint32_t>(name.GetCompareIndex());
                    state.flags = source.stateFlags; state.parentPath = path(source.baseField);
                    explicitParents += !state.parentPath.empty();
                    for (const auto member : children(field.objectPath,source.children)) {
                        if (reflection.objects[member].metaClass != "Function") continue;
                        const auto memberName = package.names.at(static_cast<std::size_t>(package.exports[member].ObjName)).Name.ToString();
                        state.functions.emplace(Dispatch::FoldName(memberName),prefix+reflection.objects[member].objectPath);
                    }
                    expected.stateChildOrder.push_back(state.path);
                    expectedStates.emplace(Dispatch::FoldName(state.path),state);
                    expected.states.emplace(Dispatch::FoldName(state.name),std::move(state)); ++linkedStates;
                }
            }
            auto actual = ReadPortableRuntimeDispatchClass(expected.path);
            Require(std::tie(actual.path,actual.parentPath,actual.probeMask,actual.functions,actual.stateChildOrder) ==
                std::tie(expected.path,expected.parentPath,expected.probeMask,expected.functions,expected.stateChildOrder) &&
                actual.states.size() == expected.states.size(),
                "Original runtime class Children/Next/SuperField metadata diverged: "+expected.path);
            for (const auto& [name,state] : expected.states) {
                const auto found = actual.states.find(name);
                Require(found != actual.states.end(),"Original runtime lost owned state: "+state.path);
                const auto& value = found->second;
                Require(std::tie(value.path,value.name,value.compareIndex,value.flags,value.functions,value.parentPath) ==
                    std::tie(state.path,state.name,state.compareIndex,state.flags,state.functions,state.parentPath),
                    "Original runtime selected state SuperField/functions diverged: "+state.path);
            }
            expectedClasses.emplace(Dispatch::FoldName(expected.path),std::move(expected));
            actualClasses.push_back(std::move(actual));
        }
    }
    Require(actualClasses.size() == runtime.classes && linkedStates == runtime.states && explicitParents != 0u,
        "Original dispatch metadata audit lost class/state/inheritance coverage");
    const Dispatch::Graph actual(std::move(actualClasses));
    const auto classAt = [&](const std::string& path) -> const Dispatch::Class& {
        const auto found = expectedClasses.find(Dispatch::FoldName(path));
        Require(found != expectedClasses.end(),"Original source class link is unavailable: "+path);
        return found->second;
    };
    const auto sourceFunction = [&](const std::string& receiver,const std::string& selected,
        const std::string& name,const Dispatch::LookupKind kind) -> std::optional<std::string> {
        auto node = kind == Dispatch::LookupKind::Virtual ? selected : std::string{};
        std::set<std::string> visited;
        while (!node.empty()) {
            Require(visited.size() < 256u && visited.insert(Dispatch::FoldName(node)).second,
                "Original source selected-node ancestry is cyclic/deep: "+selected);
            const auto state = expectedStates.find(Dispatch::FoldName(node));
            const auto& functions = state != expectedStates.end() ? state->second.functions : classAt(node).functions;
            const auto function = functions.find(Dispatch::FoldName(name));
            if (function != functions.end()) return function->second;
            node = state != expectedStates.end() ? state->second.parentPath : classAt(node).parentPath;
        }
        node = receiver;
        visited.clear();
        while (!node.empty()) {
            Require(visited.size() < 256u && visited.insert(Dispatch::FoldName(node)).second,
                "Original source receiver-class ancestry is cyclic/deep: "+receiver);
            const auto& cls = classAt(node);
            const auto function = cls.functions.find(Dispatch::FoldName(name));
            if (function != cls.functions.end()) return function->second;
            node = cls.parentPath;
        }
        return {};
    };
    std::size_t autoSelections{}, noAuto{};
    for (const auto& [key,cls] : expectedClasses) {
        std::optional<std::string> expectedAuto;
        auto node = cls.path;
        std::set<std::string> visited;
        while (!node.empty() && !expectedAuto) {
            Require(visited.size() < 256u && visited.insert(Dispatch::FoldName(node)).second,
                "Original source Auto class ancestry is cyclic/deep: "+cls.path);
            const auto& source = classAt(node);
            for (const auto& statePath : source.stateChildOrder) {
                if (expectedStates.at(Dispatch::FoldName(statePath)).flags & 2u) { expectedAuto=statePath; break; }
            }
            node = source.parentPath;
        }
        Require(Dispatch::ResolveState(actual,cls.path,"Auto") == expectedAuto,
            "Original Auto lost exact source Children/Next selection: "+cls.path);
        autoSelections += expectedAuto.has_value(); noAuto += !expectedAuto;
        for (const auto& [name,state] : cls.states) {
            std::set<std::string> names{"tick","beginstate","endstate","nosuchauthoredevent"};
            for (const auto& [function,path] : state.functions) names.insert(function);
            for (const auto& function : names) for (const auto kind : {Dispatch::LookupKind::Virtual,Dispatch::LookupKind::Global}) {
                Require(Dispatch::ResolveFunctionAtSelectedNode(actual,cls.path,state.path,function,kind) ==
                    sourceFunction(cls.path,state.path,function,kind),
                    "Original selected-node callback lost source SuperField/fallback: "+state.path+'.'+function);
                ++callbackQueries;
            }
        }
    }
    Require(autoSelections != 0u && noAuto != 0u && callbackQueries != 0u && !GetPortableRuntimeScriptStatePresent(),
        "Original dispatch selection audit missed controls or created live script state");
    bool wrongClass{};
    try { ReadPortableRuntimeDispatchClass("Engine.Actor.BeginPlay"); } catch (const std::exception&) { wrongClass=true; }
    Require(wrongClass,"Readonly class dispatch inspection accepted a Function identity");
    std::cout << "ORIGINAL DISPATCH SOURCE LINKS classes="<<runtime.classes<<" states="<<linkedStates<<
        " childFields="<<childFields<<" explicitStateParents="<<explicitParents<<" autoSelections="<<autoSelections<<
        " noAuto="<<noAuto<<" selectedCallbackQueries="<<callbackQueries<<"; readonly, not full native hash precedence\n";
}
void VerifyOriginalDispatchPrograms(const std::vector<PortablePackageTables>& tables,
    const PortableRuntimeSummary& runtime) {
    const auto graph = ReadPortableRuntimeDispatchSummary();
    Require(graph.classes == runtime.classes && graph.states == runtime.states &&
        graph.classFunctions + graph.stateFunctions == runtime.functions,
        "Original Children/Next dispatch graph lost class/state/functions");
    std::size_t states{}, classes{}, labels{}, terminalTables{}, statements{};
    std::size_t functions{}, functionStatements{}, switches{}, cases{}, switchFunctions{};
    for (const auto& package : tables) {
        const auto reflection = BuildPortableReflectionGraph(package);
        OriginalProgramHost inspection(package);
        for (std::size_t index=0;index<reflection.objects.size();++index) {
            const auto& object=reflection.objects[index];
            if (object.metaClass == "Function") {
                const auto script = LoadPortableFunctionScript(package, index);
                QuestVr::Vm::Function function;
                function.path = std::filesystem::path(package.sourcePath).stem().string()+'.'+script.objectPath;
                function.source = package.sourcePath; function.bytecode = script.bytecode;
                function.nativeIndex = script.nativeIndex; function.flags = script.functionFlags;
                QuestVr::Vm::ProgramLayout layout;
                try { layout = QuestVr::Vm::AnalyzeProgram(inspection, function); }
                catch (const std::exception& error) {
                    throw std::runtime_error("Original function layout failed at "+function.path+": "+error.what());
                }
                ++functions; functionStatements += layout.statementOffsets.size();
                bool hasSwitch{};
                for (const auto offset : layout.statementOffsets) {
                    Require(offset < function.bytecode.size(), "Original function statement offset escaped code");
                    switches += function.bytecode[offset] == 0x05u;
                    cases += function.bytecode[offset] == 0x0au;
                    hasSwitch = hasSwitch || function.bytecode[offset] == 0x05u;
                }
                switchFunctions += hasSwitch;
                continue;
            }
            if (object.metaClass != "State" && object.metaClass != "Class") continue;
            const auto path=std::filesystem::path(package.sourcePath).stem().string()+'.'+object.objectPath;
            const auto layout=ReadPortableRuntimeStateProgram(path);
            statements += layout.statementOffsets.size(); labels += layout.labels.size();
            terminalTables += layout.terminalLabelTable;
            if (object.metaClass == "State") ++states; else ++classes;
            const auto descriptor=ReadPortableRuntimeAuthoredStateDescriptor(path);
            Require(std::is_sorted(layout.statementOffsets.begin(),layout.statementOffsets.end()),
                "Original normalized program lost statement order");
            // The pinned FindLabelIndex inspects the final parsed statement,
            // not the serialized header offset. Independently walk fixed-width
            // normalized table entries and compare their actual source names.
            if (layout.terminalLabelTable) {
                Require(!layout.statementOffsets.empty(), "Original terminal table has no statement");
                std::size_t cursor=layout.statementOffsets.back();
                Require(cursor<descriptor.bytecode.size() && descriptor.bytecode[cursor++]==0x0cu,
                    "Original terminal statement is not a LabelTable: "+path);
                const auto word=[&]() {
                    Require(cursor<=descriptor.bytecode.size() && descriptor.bytecode.size()-cursor>=4u,
                        "Original terminal label entry truncated: "+path);
                    std::uint32_t value{};
                    for (unsigned i=0;i<4u;++i) value|=std::uint32_t(descriptor.bytecode[cursor++])<<(8u*i);
                    return value;
                };
                std::size_t entry{};
                while (true) {
                    const auto name=word(); const auto offset=word();
                    Require(name<package.names.size(), "Original terminal label name invalid: "+path);
                    const auto spelling=package.names[name].Name.ToString();
                    if (QuestVr::ScriptDispatch::FoldName(spelling)=="none") break;
                    Require(entry<layout.labels.size() && layout.labels[entry].name==spelling &&
                        layout.labels[entry].offset==offset &&
                        std::binary_search(layout.statementOffsets.begin(),layout.statementOffsets.end(),offset),
                        "Original terminal label spelling/order/target diverged: "+path);
                    ++entry;
                }
                Require(entry==layout.labels.size() && cursor==descriptor.bytecode.size(),
                    "Original terminal label count/end diverged: "+path);
            } else {
                Require(layout.labels.empty(), "Nonterminal label table contributed labels: "+path);
            }
        }
        Require(inspection.unexpected == 0u, "Original structural function inspection attempted execution");
    }
    Require(states==runtime.states && classes==runtime.classes && functions==runtime.functions &&
        switches != 0u && cases > switches && labels!=0u && !GetPortableRuntimeScriptStatePresent(),
        "Original program inspection lost coverage or created execution state");
    std::cout << "ORIGINAL DISPATCH GRAPH classes="<<graph.classes<<" states="<<graph.states<<
        " classFunctions="<<graph.classFunctions<<" stateFunctions="<<graph.stateFunctions<<" commonFields="<<graph.commonFields<<'\n';
    std::cout << "ORIGINAL PROGRAM LAYOUT states="<<states<<" classes="<<classes<<" statements="<<statements<<
        " labels="<<labels<<" terminalTables="<<terminalTables<<"; inspection only\n";
    std::cout << "ORIGINAL FUNCTION LAYOUT functions="<<functions<<" statements="<<functionStatements<<
        " switchFunctions="<<switchFunctions<<" switches="<<switches<<" cases="<<cases<<
        "; structural table-only inspection, not execution feasibility\n";
}
void VerifyStoppedDispatch(const std::filesystem::path& root,const std::string& actor) {
    const auto context=ReadPortableActorDispatchContext(actor);
    const auto before=Snapshot(actor);
    const auto stack=ReadPortableActorSerializedStack(actor);
    Require(stack && !context.codePath.empty() && context.codeMasks && context.liveProbeMask==stack->probeMask,
        "Original dormant frame context lost class code or its actual positive serialized ProbeMask");
    const auto mapName=actor.substr(0u,actor.find('.'));
    const auto source=LoadPortablePackageTables((root/"Maps"/(mapName+".dx")).string());
    const auto qualified=[&](const std::int32_t reference) {
        const auto path=GetPortableObjectPath(source,reference);
        return reference>0 ? mapName+'.'+path : path;
    };
    Require(context.framePresent && context.exactSelected && !context.codePresent &&
        context.selectedPath==qualified(stack->stateReference) &&
        context.runningPath==qualified(stack->functionReference) && context.codePath==context.selectedPath,
        "Original stopped inspection conflated selected/running identities or fabricated executable Code");
    std::set<std::string> negativeNames;
    for (std::uint8_t index=0u;index<64u;++index) {
        const auto event=QuestVr::ScriptDispatch::ProbeEventName(index);
        const bool eligible=(stack->probeMask & (std::uint64_t{1}<<index))!=0u;
        if (!eligible) negativeNames.insert(QuestVr::ScriptDispatch::FoldName(event));
        Require(QuestVr::ScriptDispatch::IsEnabled(event,context.classProbeMask,context.codeMasks,context.disabledNames,context.liveProbeMask)==eligible &&
            QuestVr::ScriptDispatch::IsEnabled(event,0u,QuestVr::ScriptDispatch::CodeMasks{},context.disabledNames,context.liveProbeMask)==eligible,
            "Original stopped IsProbing recomputed or inverted serialized positive bit for "+event);
    }
    Require(context.disabledNames==negativeNames,"Original dormant negative probe names differ from actual unset serialized bits");
    Require(context.stateName==context.codePath.substr(context.codePath.find_last_of('.')+1u),
        "Stopped class-backed GetStateName was incorrectly normalized to None");
    const auto stateName=Call(actor,"GetStateName");
    Require(stateName.value.kind==Kind::Name && stateName.value.text==context.stateName,
        "Original GetStateName did not return stopped code identity");
    const auto inState=Call(actor,"IsInState",{Name(context.stateName)});
    Require(inState.value.kind==Kind::Bool && inState.value.boolean && Same(before,Snapshot(actor)) &&
        !GetPortableRuntimeScriptStatePresent(), "Readonly state query altered NPC state");
    const auto level=ReadPortableActorScriptProperty(actor,"Level");
    Require(level.kind==Kind::Object && !level.text.empty() &&
        QuestVr::Vm::ToBool(Call(level.text,"IsA",{Name("LevelInfo")}).value),
        "Original event receiver lost its authored Level reference");
    Require(!QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(level.text,"bBegunPlay")),
        "Readonly map loading incorrectly began world startup");
    const bool animEndEligible=QuestVr::ScriptDispatch::IsEnabled("AnimEnd",context.classProbeMask,
        context.codeMasks,context.disabledNames,context.liveProbeMask);
    const auto animEnd=ExecutePortableActorEvent(actor,"AnimEnd",true);
    Require(animEnd.passed() && animEnd.value.kind==Kind::Nothing && animEnd.instructions==0u && Same(before,Snapshot(actor)),
        animEndEligible ? "Eligible authored AnimEnd bypassed the bound Level's before-begun-play gate" : "Disabled authored AnimEnd executed");
    Require(QuestVr::ScriptDispatch::IsEnabled("NoSuchAuthoredEvent",context.classProbeMask,
        context.codeMasks,context.disabledNames,context.liveProbeMask),"Original nonprobe event was mistaken for a disabled probe");
    const auto absent=ExecutePortableActorEvent(actor,"NoSuchAuthoredEvent");
    Require(absent.passed() && absent.value.kind==Kind::Nothing && absent.instructions==0u &&
        !GetPortableRuntimeScriptStatePresent(), "Nonprobe event bypassed the before-begun-play gate or created script state");
    std::cout << "ORIGINAL STOPPED DISPATCH "<<actor<<" code="<<context.codePath<<" stateName="<<context.stateName<<
        " positiveProbeMask="<<*context.liveProbeMask<<" negativeProbes="<<context.disabledNames.size()<<
        " AnimEndGate="<<(animEndEligible ? "before-begun-play" : "disabled")<<"; all64 serialized bits verified; no startup or ticking\n";
}
std::uint32_t Word(const std::vector<std::uint8_t>& bytes,const std::size_t offset) {
    Require(offset <= bytes.size() && bytes.size()-offset >= 4,"Generated checkpoint word truncated");
    std::uint32_t value{};for (unsigned i=0;i<4;++i) value|=std::uint32_t(bytes[offset+i])<<(i*8u);return value;
}
void PutWord(std::vector<std::uint8_t>& bytes,const std::size_t offset,const std::uint32_t value) {
    Require(offset <= bytes.size() && bytes.size()-offset >= 4,"Generated checkpoint write escaped payload");
    for (unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
}
std::vector<std::uint8_t> CheckpointBytes(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes;
    Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Generated checkpoint could not be read");
    return bytes;
}
// Independent exact v3 prefix walk. No guessed search for blob signatures or
// trailer bytes: v4-v13 append one bounded length + codec payload after every
// existing gameplay/progress field, preserving the old layout verbatim.
std::size_t ScriptTailOffset(const std::vector<std::uint8_t>& bytes) {
    Require(Word(bytes,0)==0x53515844u && Word(bytes,4)>=4u && Word(bytes,4)<=13u,"Script checkpoint is not runtime v4-v13");
    std::size_t cursor=8;
    const auto skip=[&](const std::size_t count) {
        Require(cursor<=bytes.size() && count<=bytes.size()-cursor,"Generated checkpoint prefix truncated");cursor+=count;
    };
    const auto word=[&]() {const auto value=Word(bytes,cursor);cursor+=4;return value;};
    const auto string=[&]() {const auto size=word();Require(size<=1'048'576u,"Generated prefix string too long");skip(size);};
    const auto list=[&]() {const auto count=word();Require(count<=100'000u,"Generated prefix list too long");for(std::uint32_t i=0;i<count;++i) string();};
    list();list();list();skip(4); // inventory, inactive, activated, player health.
    const auto damaged=word();Require(damaged<=100'000u,"Generated damaged list too long");
    for(std::uint32_t i=0;i<damaged;++i) {string();skip(4);}
    skip(8);list();list();list();list(); // credits, skills, flags, goals, notes, applied effects.
    const auto size=Word(bytes,cursor);
    Require(size>0 && cursor+4<=bytes.size() && size==bytes.size()-cursor-4,"Script blob length does not cover exact script tail");
    return cursor;
}
std::vector<std::uint8_t> ScriptBlob(const std::vector<std::uint8_t>& bytes) {
    const auto offset=ScriptTailOffset(bytes);
    return {bytes.begin()+static_cast<std::ptrdiff_t>(offset+4),bytes.end()};
}
using PropertyValues=std::vector<std::pair<std::string,Value>>;
using InventorySlots=std::array<Value,8u>;
Value InventoryItem(const std::string& inventory={},const std::int32_t count=0) {
    Value value;value.kind=Kind::Struct;
    value.fields.emplace("inventory",Value::Text(Kind::Object,inventory));
    value.fields.emplace("count",Value::Integer(count));return value;
}
InventorySlots InventoryProperties(const std::string& actor) {
    InventorySlots values;
    const auto slots=ReadPortableActorScriptPropertySlots(actor,"InitialInventory",0u,
        static_cast<std::uint32_t>(values.size()));
    Require(slots.size()==values.size(),"Original InitialInventory batch omitted fixed slots");
    for(std::uint32_t i=0;i<values.size();++i) {
        values[i]=slots[i];
        Require(values[i].kind==Kind::Struct && values[i].fields.size()==2u &&
            values[i].fields.at("inventory").kind==Kind::Object &&
            values[i].fields.at("count").kind==Kind::Int,
            "Original InitialInventory slot lost authored InventoryItem member kinds");
    }
    return values;
}
void SameInventoryProperties(const std::string& actor,const InventorySlots& expected,const std::string& context) {
    const auto actual=InventoryProperties(actor);
    for(std::size_t i=0;i<actual.size();++i)
        Require(QuestVr::Vm::Equal(actual[i],expected[i]),context+" InitialInventory["+std::to_string(i)+']');
}
std::string QualifiedReference(const PortablePackageTables& package,const std::int32_t reference) {
    const auto path=GetPortableObjectPath(package,reference);
    return reference>0 ? std::filesystem::path(package.sourcePath).stem().string()+'.'+path : path;
}
// Independent reader of the original InventoryItem value wire. The production
// struct decoder is deliberately not used to calculate these expected values:
// Inventory is one package-local compact class reference; Count is signed LE32.
Value RawInventoryItem(const PortablePackageTables& package,const PortableTaggedProperty& property) {
    Require(property.type==10u && property.structName=="InventoryItem" && property.value.size()>=5u,
        "Original InventoryItem tag/type/size changed");
    std::size_t cursor{};
    const auto byte=[&]() {
        Require(cursor<property.value.size(),"Original InventoryItem compact class reference truncated");
        return property.value[cursor++];
    };
    auto next=byte();const bool negative=(next&0x80u)!=0u;
    std::uint32_t magnitude=next&0x3fu;bool more=(next&0x40u)!=0u;unsigned shift=6u;
    while(more) {
        Require(shift<32u,"Original InventoryItem compact class reference exceeds int32");
        next=byte();const auto part=static_cast<std::uint32_t>(next&0x7fu);
        Require(part<=(std::numeric_limits<std::uint32_t>::max()>>shift),
            "Original InventoryItem compact class reference overflowed");
        magnitude|=part<<shift;more=(next&0x80u)!=0u;shift+=7u;
    }
    Require(magnitude<=static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) &&
        cursor<=property.value.size() && property.value.size()-cursor==4u,
        "Original InventoryItem class reference/Count wire was not exact");
    const auto reference=negative ? -static_cast<std::int32_t>(magnitude) : static_cast<std::int32_t>(magnitude);
    Require((reference>=0 && static_cast<std::size_t>(reference)<=package.exports.size()) ||
        (reference<0 && -static_cast<std::int64_t>(reference)<=static_cast<std::int64_t>(package.imports.size())),
        "Original InventoryItem class reference escaped original tables");
    if(reference>0) Require(package.exports[static_cast<std::size_t>(reference-1)].ObjClass==0,
        "Original InventoryItem positive reference is not an actual class export");
    const auto bits=Word(property.value,cursor);std::int32_t count{};std::memcpy(&count,&bits,sizeof(count));
    return InventoryItem(QualifiedReference(package,reference),count);
}
struct InventorySourceCoverage {
    std::size_t actors{},slots{},mapEntries{},inheritedEntries{},negativeEntries{};
};
void VerifyOriginalInventorySources(const std::vector<PortablePackageTables>& packages,
    const PortablePackageTables& map,InventorySourceCoverage& coverage) {
    std::map<std::string,PortableClassDescriptor> classes;
    bool batchBoundsChecked{};
    const auto sourceFor=[&](const std::string& path)->const PortablePackageTables& {
        const auto dot=path.find('.');Require(dot!=std::string::npos,"Original class reference lacks package identity");
        const auto name=path.substr(0,dot);
        if(QuestVr::ScriptDispatch::FoldName(name)==QuestVr::ScriptDispatch::FoldName(
            std::filesystem::path(map.sourcePath).stem().string())) return map;
        const auto found=std::find_if(packages.begin(),packages.end(),[&](const auto& package) {
            return QuestVr::ScriptDispatch::FoldName(std::filesystem::path(package.sourcePath).stem().string())==
                QuestVr::ScriptDispatch::FoldName(name);
        });
        Require(found!=packages.end(),"Original inventory class uses an unavailable package: "+path);return *found;
    };
    const auto classFor=[&](const std::string& path)->const PortableClassDescriptor& {
        auto found=classes.find(path);
        if(found==classes.end()) {
            const auto& source=sourceFor(path);
            const auto index=FindPortableExport(source,path.substr(path.find('.')+1u));
            Require(source.exports[index].ObjClass==0,"Original actor class reference is not a class");
            found=classes.emplace(path,LoadPortableClassDescriptor(source,index)).first;
        }
        return found->second;
    };
    for(const auto& actor:GetPortableRuntimeMapActors()) {
        if(!actor.pawn || !IsA(actor,"ScriptedPawn")) continue;
        InventorySlots expected;for(auto& slot:expected) slot=InventoryItem();
        std::array<bool,8u> assigned{};
        const auto apply=[&](const PortablePackageTables& source,
            const std::vector<PortableTaggedProperty>& properties,const bool instance) {
            for(auto property=properties.rbegin();property!=properties.rend();++property) {
                if(property->name!="InitialInventory") continue;
                Require(property->arrayIndex<expected.size(),"Original InitialInventory tag escaped 8-slot declaration");
                const auto index=property->arrayIndex;if(assigned[index]) continue;
                expected[index]=RawInventoryItem(source,*property);assigned[index]=true;
                if(!expected[index].fields.at("inventory").text.empty() && expected[index].fields.at("count").integer!=0) {
                    if(instance) ++coverage.mapEntries;else ++coverage.inheritedEntries;
                    if(expected[index].fields.at("count").integer<0) ++coverage.negativeEntries;
                }
            }
        };
        const auto actorIndex=FindPortableExport(map,actor.objectPath.substr(actor.objectPath.find('.')+1u));
        apply(map,LoadPortableExportProperties(map,actorIndex).properties,true);
        std::string current=actor.classPath;std::size_t depth{};
        while(!current.empty()) {
            Require(++depth<=256u,"Original inventory class chain cycled/exceeded depth");
            const auto& source=sourceFor(current);const auto& descriptor=classFor(current);
            apply(source,descriptor.defaults,false);
            const auto index=FindPortableExport(source,current.substr(current.find('.')+1u));
            current=QualifiedReference(source,source.exports[index].ObjBase);
        }
        SameInventoryProperties(actor.objectPath,expected,"Raw map/inherited compact-classref + signed Count mismatch");
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor.objectPath,"iNiTiAlInVeNtOrY",7u),expected[7]),
            "Original fixed struct property lost case-insensitive property lookup");
        bool rejected{};try {ReadPortableActorScriptProperty(actor.objectPath,"InitialInventory",8u);}
        catch(const std::exception&) {rejected=true;}
        Require(rejected && !GetPortableRuntimeScriptStatePresent(),
            "Readonly InitialInventory inspection accepted an out-of-range slot or created state");
        if(!batchBoundsChecked) {
            const std::array<std::pair<std::uint32_t,std::uint32_t>,7u> invalidRanges{{
                {8u,1u},{7u,2u},{0u,9u},
                {std::numeric_limits<std::uint32_t>::max(),2u},
                {7u,std::numeric_limits<std::uint32_t>::max()},
                {0u,0u},{0u,1025u}}};
            for(const auto& range:invalidRanges) {
                bool batchRejected{};
                try {ReadPortableActorScriptPropertySlots(actor.objectPath,"InitialInventory",range.first,range.second);}
                catch(const std::exception&) {batchRejected=true;}
                Require(batchRejected && !GetPortableRuntimeScriptStatePresent(),
                    "Readonly InitialInventory batch accepted invalid first/count/overflow or created state");
            }
            SameInventoryProperties(actor.objectPath,expected,"Rejected readonly inventory batch changed authored slots");
            Require(!GetPortableRuntimeScriptStatePresent(),"Readonly inventory batch verification created script state");
            batchBoundsChecked=true;
        }
        ++coverage.actors;coverage.slots+=expected.size();
    }
    Require(batchBoundsChecked,"Original map has no ScriptedPawn for readonly inventory batch bounds control");
    std::cout<<"ORIGINAL INVENTORY SOURCE "<<std::filesystem::path(map.sourcePath).stem().string()<<
        " actors="<<coverage.actors<<" slots="<<coverage.slots<<" mapEntries="<<coverage.mapEntries<<
        " inheritedEntries="<<coverage.inheritedEntries<<" negativeEntries="<<coverage.negativeEntries<<
        "; independent original classref/int32 values, not spawned inventory\n";
}
PropertyValues ActorProperties(const std::string& actor) {
    PropertyValues values;
    for(const auto* name:{"PrePivot","DesiredPrePivot","PrePivotTime","AnimSequence","AnimFrame","AnimRate",
        "AnimLast","AnimMinRate","TweenRate","OldAnimRate","bAnimLoop","bAnimNotify","bAnimFinished"})
        values.emplace_back(name,ReadPortableActorScriptProperty(actor,name));
    return values;
}
void SameProperties(const std::string& actor,const PropertyValues& expected,const std::string& context) {
    for(const auto& property:expected)
        Require(QuestVr::Vm::Equal(property.second,ReadPortableActorScriptProperty(actor,property.first)),context+": "+property.first);
}
std::vector<std::uint8_t> ReplaceScriptBlob(const std::vector<std::uint8_t>& checkpoint,const std::vector<std::uint8_t>& blob) {
    const auto offset=ScriptTailOffset(checkpoint);
    Require(blob.size()<=std::numeric_limits<std::uint32_t>::max(),"Generated script blob exceeds uint32");
    std::vector<std::uint8_t> bytes(checkpoint.begin(),checkpoint.begin()+static_cast<std::ptrdiff_t>(offset+4));
    PutWord(bytes,offset,static_cast<std::uint32_t>(blob.size()));bytes.insert(bytes.end(),blob.begin(),blob.end());return bytes;
}
void ScriptSchemaPositiveReferences(const std::vector<std::uint8_t>& serialized,const PortableActorSnapshot& expected,
    const PropertyValues& properties,const std::filesystem::path& originalCheckpoint,const std::filesystem::path& directory) {
    auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(serialized));
    const auto actor=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object) {return object.path==expected.objectPath;});
    Require(actor!=state.objects.end() && actor->clock,"Positive reference fixture lacks its saved actor/native clock");
    Require(!expected.meshPath.empty(),"Positive reference fixture lacks its actual authored mesh");
    const auto region=ReadPortableActorScriptProperty(expected.objectPath,"Region");
    Require(region.kind==Kind::Struct && region.fields.size()==3u &&
        region.fields.at("zone").kind==Kind::Object && !region.fields.at("zone").text.empty() &&
        region.fields.at("ileaf").kind==Kind::Int && region.fields.at("zonenumber").kind==Kind::Byte,
        "Positive reference fixture lacks the exact typed authored PointRegion");
    Require(QuestVr::Vm::ToBool(Call(region.fields.at("zone").text,"IsA",{Name("LevelInfo")}).value),
        "Positive reference fixture did not resolve its actual LevelInfo zone");
    std::vector<QuestVr::ScriptSavedProperty> references{
        {"Engine.Actor.Mesh","Mesh",0,Value::Text(Kind::Object,expected.meshPath)},
        {"Engine.Actor.Owner","Owner",0,Value::Text(Kind::Object,expected.objectPath)},
        {"Engine.Actor.Region","Region",0,region}};
    bool textureFound{};
    for(std::uint32_t index=0;index<8u && !textureFound;++index) {
        const auto value=ReadPortableActorScriptProperty(expected.objectPath,"MultiSkins",index);
        Require(value.kind==Kind::Object,"Authored MultiSkins slot did not retain its Object property kind");
        if(!value.text.empty() && value.text!="None") {
            references.push_back({"Engine.Actor.MultiSkins","MultiSkins",index,value});textureFound=true;
        }
    }
    if(!textureFound) {
        const auto value=ReadPortableActorScriptProperty(expected.objectPath,"Skin");
        Require(value.kind==Kind::Object,"Authored Skin did not retain its Object property kind");
        if(!value.text.empty() && value.text!="None") {
            references.push_back({"Engine.Actor.Skin","Skin",0,value});textureFound=true;
        }
    }
    if(!textureFound) std::cout<<"SKIP positive Texture reference: actor has no authored MultiSkins[0..7]/Skin value\n";
    for(const auto& property:references) {
        const auto found=std::find_if(actor->properties.begin(),actor->properties.end(),[&](const auto& value) {
            return value.key==property.key && value.index==property.index;
        });
        if(found==actor->properties.end()) actor->properties.push_back(property);
        else *found=property;
    }
    const auto blob=QuestVr::EncodeScriptSavedState(state);
    const auto checkpoint=directory/"schema-positive-references-v4.sav";
    const auto inspection=directory/"schema-positive-references-restored-v4.sav";
    Require(QuestVr::WriteDurableSaveFile(checkpoint.string(),ReplaceScriptBlob(serialized,blob)),
        "Could not write positive authored reference fixture");
    Require(ValidatePortableRuntimeState(checkpoint.string(),state.mapName) && Same(expected,Snapshot(expected.objectPath)),
        "Valid authored Mesh/Owner/PointRegion/Texture references were rejected or readonly validation changed native state");
    SameProperties(expected.objectPath,properties,"Positive reference validation changed live properties");
    Require(LoadPortableRuntimeState(checkpoint.string()),"Valid authored Mesh/Owner/PointRegion/Texture references could not be loaded");
    for(const auto& property:references)
        Require(QuestVr::Vm::Equal(property.value,ReadPortableActorScriptProperty(expected.objectPath,property.name,property.index)),
            "Loaded positive reference changed its exact typed value: "+property.name);
    SameProperties(expected.objectPath,properties,"Positive reference load changed unrelated properties");
    Require(SavePortableRuntimeState(inspection.string()) && ScriptBlob(CheckpointBytes(inspection))==blob,
        "Positive authored references did not survive a stable complete script/native clock roundtrip");
    Require(LoadPortableRuntimeState(originalCheckpoint.string()) && Same(expected,Snapshot(expected.objectPath)),
        "Could not restore original script/native state after positive reference controls");
    SameProperties(expected.objectPath,properties,"Positive reference reset changed original properties");
    Require(SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==serialized,
        "Positive reference reset retained added references or changed original saved state");
    std::cout<<"ORIGINAL v4 positive authored reference controls="<<references.size()
        <<" (Mesh/Owner/PointRegion"<<(textureFound ? "/Texture" : "")<<"); complete roundtrip remained stable\n";
}
void VerifyEventLevelBindings(const std::vector<std::uint8_t>& serialized,const PortableActorSnapshot& expected,
    const std::filesystem::path& originalCheckpoint,const std::filesystem::path& directory) {
    const auto baseline=QuestVr::DecodeScriptSavedState(ScriptBlob(serialized));
    const auto authoredLevel=ReadPortableActorScriptProperty(expected.objectPath,"Level");
    Require(authoredLevel.kind==Kind::Object && !authoredLevel.text.empty(),"Event fixture has no authored Level");
    const auto levelSnapshot=Snapshot(authoredLevel.text);
    const auto set=[](QuestVr::ScriptSavedObject& object,QuestVr::ScriptSavedProperty property) {
        const auto found=std::find_if(object.properties.begin(),object.properties.end(),[&](const auto& item) {
            return item.key==property.key && item.index==property.index;
        });
        if (found==object.properties.end()) object.properties.push_back(std::move(property));
        else *found=std::move(property);
    };
    const auto fixture=directory/"event-level-binding-v4.sav";
    const auto inspection=directory/"event-level-binding-unchanged-v4.sav";
    const auto apply=[&](const bool nullLevel,const bool begun,const bool deleted) {
        auto state=baseline;
        auto actor=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object) {
            return object.path==expected.objectPath;
        });
        Require(actor!=state.objects.end(),"Event fixture lost its existing actor");
        set(*actor,{"Engine.Actor.Level","Level",0u,nullLevel ? Value::Text(Kind::Object,{}) : authoredLevel});
        set(*actor,{"Engine.Actor.bDeleteMe","bDeleteMe",0u,Value::Bool(deleted)});
        auto level=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object) {
            return object.path==levelSnapshot.objectPath;
        });
        if (level==state.objects.end()) {
            state.objects.push_back({levelSnapshot.objectPath,levelSnapshot.classPath,{},{}});
            level=state.objects.end()-1;
        }
        set(*level,{"Engine.LevelInfo.bBegunPlay","bBegunPlay",0u,Value::Bool(begun)});
        Require(QuestVr::WriteDurableSaveFile(fixture.string(),ReplaceScriptBlob(serialized,QuestVr::EncodeScriptSavedState(state))) &&
            ValidatePortableRuntimeState(fixture.string()) && LoadPortableRuntimeState(fixture.string()),
            "Valid typed Level/lifecycle overlay fixture was rejected");
        Require(Same(expected,Snapshot(expected.objectPath)),"Event fixture altered actor native clock");
    };
    const auto unchanged=[&]() {
        Require(SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==CheckpointBytes(fixture),
            "Readonly event dispatch changed full saved state");
    };
    apply(true,true,false);
    const auto authoredContext=ReadPortableActorDispatchContext(expected.objectPath);
    Require(QuestVr::ScriptDispatch::IsEnabled("AnimEnd",authoredContext.classProbeMask,authoredContext.codeMasks,
        authoredContext.disabledNames,authoredContext.liveProbeMask),"Original authored AnimEnd lacks its positive serialized probe bit");
    const auto nullProbe=ExecutePortableActorEvent(expected.objectPath,"AnimEnd",true);
    Require(!nullProbe.passed() && nullProbe.error.find("Level binding")!=std::string::npos && nullProbe.instructions==0u,
        "Eligible authored AnimEnd incorrectly short-circuited its null Level binding");
    unchanged();
    // Genuine native Disable on the owned authored frame, not the old fixture's
    // inverted interpretation of positive serialized ProbeMask bits.
    Call(expected.objectPath,"Disable",{Name("AnimEnd")});
    const auto disabledContext=ReadPortableActorDispatchContext(expected.objectPath);
    const auto disabledObject=ReadPortableActorStateObject(expected.objectPath);
    Require(!QuestVr::ScriptDispatch::IsEnabled("AnimEnd",disabledContext.classProbeMask,disabledContext.codeMasks,
        disabledContext.disabledNames,disabledContext.liveProbeMask) && disabledObject && disabledObject->hasStack &&
        !disabledObject->frameOverride && !disabledObject->frame && SavePortableRuntimeState(fixture.string()) &&
        Word(CheckpointBytes(fixture),4u)==5u,
        "Actual Disable AnimEnd did not retain its raw unknown-latent state-only v5 fixture");
    const auto disabled=ExecutePortableActorEvent(expected.objectPath,"AnimEnd",true);
    Require(disabled.passed() && disabled.value.kind==Kind::Nothing && disabled.instructions==0u,
        "Disabled event incorrectly dereferenced a null Level binding");
    const auto unbound=ExecutePortableActorEvent(expected.objectPath,"GetStateName");
    Require(!unbound.passed() && unbound.error.find("Level binding")!=std::string::npos && unbound.instructions==0u,
        "Eligible event replaced null receiver Level with the first map LevelInfo");
    unchanged();
    apply(false,false,false);
    const auto beforeStartup=ExecutePortableActorEvent(expected.objectPath,"GetStateName");
    Require(beforeStartup.passed() && beforeStartup.value.kind==Kind::Nothing && beforeStartup.instructions==0u,
        "Event ignored its bound LevelInfo's before-startup gate");
    unchanged();
    apply(false,true,false);
    const auto begun=ExecutePortableActorEvent(expected.objectPath,"GetStateName");
    Require(begun.passed() && begun.value.kind==Kind::Name && begun.value.text==ReadPortableActorDispatchContext(expected.objectPath).stateName,
        "Eligible name event failed to honor bound LevelInfo's begun-play overlay");
    unchanged();
    apply(false,true,true);
    const auto deleted=ExecutePortableActorEvent(expected.objectPath,"GetStateName");
    Require(deleted.passed() && deleted.value.kind==Kind::Nothing && deleted.instructions==0u,
        "Name event ignored receiver bDeleteMe overlay");
    unchanged();
    Require(LoadPortableRuntimeState(originalCheckpoint.string()) && SavePortableRuntimeState(inspection.string()) &&
        CheckpointBytes(inspection)==serialized,"Could not restore complete original state after event gate controls");
    std::cout<<"ORIGINAL EVENT LEVEL GATES null-reference rejection / disabled short-circuit / begun-play / deleted; readonly state stable\n";
}
void ScriptSchemaRejections(const std::vector<std::uint8_t>& serialized,const PortableActorSnapshot& expected,
    const PropertyValues& properties,const std::filesystem::path& directory) {
    const auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(serialized));
    const auto source=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object) {return object.path==expected.objectPath;});
    Require(source!=state.objects.end() && source->clock && !source->properties.empty(),"Real v4 checkpoint omitted actor properties or native clock");
    const auto actorIndex=static_cast<std::size_t>(source-state.objects.begin());
    const auto inspect=directory/"schema-unchanged-v4.sav";
    const auto rejectBytes=[&](const std::vector<std::uint8_t>& bytes,const std::string& description) {
        const auto path=directory/"schema-invalid-v4.sav";
        Require(QuestVr::WriteDurableSaveFile(path.string(),bytes),"Could not write generated "+description+" fixture");
        Require(!ValidatePortableRuntimeState(path.string()) && !LoadPortableRuntimeState(path.string()),
            description+" checkpoint was accepted");
        Require(Same(expected,Snapshot(expected.objectPath)),description+" rejection changed live native clock");
        SameProperties(expected.objectPath,properties,description+" rejection changed live properties");
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==serialized,
            description+" rejection changed gameplay prefix or complete script/native saved state");
    };
    const auto reject=[&](const std::function<void(QuestVr::ScriptSavedState&)>& mutate,const std::string& description) {
        auto invalid=state;mutate(invalid);
        rejectBytes(ReplaceScriptBlob(serialized,QuestVr::EncodeScriptSavedState(invalid)),description);
    };
    reject([](auto& saved) {saved.mapName="NoSuchOwnedMap";},"Unknown map identity");
    reject([&](auto& saved) {saved.objects[actorIndex].path=state.mapName+".NoSuchActor";},"Unknown object identity");
    reject([&](auto& saved) {saved.objects[actorIndex].classPath="Engine.Actor";},"Wrong actor class schema");
    reject([&](auto& saved) {saved.objects[actorIndex].properties.front().key="Engine.Actor.NoSuchProperty";},"Unknown property identity");
    reject([&](auto& saved) {saved.objects[actorIndex].properties.front().name="NoSuchProperty";},"Property name/identity mismatch");
    reject([&](auto& saved) {saved.objects[actorIndex].properties.front().index=std::numeric_limits<std::uint32_t>::max();},"Out-of-range fixed property index");
    reject([&](auto& saved) {saved.objects[actorIndex].properties.front().value=Value::Text(Kind::String,"Wrong typed value");},"Wrong property value kind");
    reject([&](auto& saved) {
        saved.objects[actorIndex].properties.push_back({"Engine.Actor.Owner","Owner",0,Value::Text(Kind::Object,state.mapName+".NoSuchActor")});
    },"Unresolved object property target");
    reject([&](auto& saved) {
        saved.objects[actorIndex].properties.push_back({"Engine.Actor.Mesh","Mesh",0,Value::Text(Kind::Object,expected.objectPath)});
    },"Object property target violates referenced class");
    reject([&](auto& saved) {saved.objects[actorIndex].clock->main.rate+=0.5f;},"Clock/property value inconsistency");
    // Region is a supported nested struct. Validate its exact schema and the
    // referenced ZoneInfo class even when the value was not previously overlaid.
    const auto region=ReadPortableActorScriptProperty(expected.objectPath,"Region");
    Require(region.kind==Kind::Struct,"Schema test actor lacks supported Region struct");
    reject([&](auto& saved) {
        auto value=region;value.fields.erase("zone");
        saved.objects[actorIndex].properties.push_back({"Engine.Actor.Region","Region",0,std::move(value)});
    },"Missing nested struct field");
    reject([&](auto& saved) {
        auto value=region;value.fields["zone"]=Value::Text(Kind::Object,expected.objectPath);
        saved.objects[actorIndex].properties.push_back({"Engine.Actor.Region","Region",0,std::move(value)});
    },"Nested object target violates ZoneInfo class");
    // The codec intentionally refuses NaN at encode. Patch one precisely
    // identified generated vector property's payload, then exercise read-side
    // codec/runtime guards with a structurally sized but non-finite trailer.
    const auto vector=std::find_if(source->properties.begin(),source->properties.end(),[](const auto& value) {return value.value.kind==Kind::Vector;});
    Require(vector!=source->properties.end(),"Generated schema fixture has no vector property");
    std::vector<std::uint8_t> prefix;
    const auto append=[&](const std::uint32_t word) {for(unsigned i=0;i<4;++i) prefix.push_back(static_cast<std::uint8_t>(word>>(i*8u)));};
    for(const auto* text:{&vector->key,&vector->name}) {append(static_cast<std::uint32_t>(text->size()));prefix.insert(prefix.end(),text->begin(),text->end());}
    append(vector->index);prefix.push_back(8); // Stable codec Vector tag.
    auto blob=ScriptBlob(serialized);const auto found=std::search(blob.begin(),blob.end(),prefix.begin(),prefix.end());
    Require(found!=blob.end() && std::search(found+1,blob.end(),prefix.begin(),prefix.end())==blob.end(),
        "Generated vector property wire identity is not unique");
    const auto offset=static_cast<std::size_t>(found-blob.begin())+prefix.size();PutWord(blob,offset,0x7fc00000u);
    rejectBytes(ReplaceScriptBlob(serialized,blob),"Non-finite vector payload");
    auto extra=serialized;extra.push_back(0);rejectBytes(extra,"Trailing data beyond exact v4 blob length");
    auto length=serialized;PutWord(length,ScriptTailOffset(length),0xffffffffu);rejectBytes(length,"Oversized script blob length");
    std::cout<<"ORIGINAL v4 schema rejection controls=15; live gameplay/properties/fullclock remained byte-identical\n";
}
struct TemporaryCheckpoint {
    std::filesystem::path directory;
    explicit TemporaryCheckpoint(const std::filesystem::path& original) {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        Require(parent.string().rfind(std::filesystem::canonical(original).string(), 0u) != 0u,
            "Generated checkpoint must be outside original installation");
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = parent / ("deusex-actor-script-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::filesystem::canonical(candidate);
                Require(directory.parent_path() == parent &&
                    directory.filename().string().rfind("deusex-actor-script-test-", 0u) == 0u,
                    "Generated checkpoint escaped temporary parent");
                return;
            }
        }
        throw std::runtime_error("Could not create isolated script test checkpoint");
    }
    ~TemporaryCheckpoint() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
};

void VerifyOriginalDormantSpawn(const std::filesystem::path& root,const std::string& actor,
    const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Original dormant Spawn did not begin from untouched authored state");
    const auto originalBytes=CheckpointBytes(legacy);
    const auto originalActors=GetPortableRuntimeMapActors(true);
    const auto initialGc=GC::GetStats();
    const auto level=ReadPortableActorScriptProperty(actor,"Level");
    const auto xLevel=ReadPortableActorScriptProperty(actor,"XLevel");
    Require(level.kind==Kind::Object && !level.text.empty() && xLevel.kind==Kind::Object && !xLevel.text.empty() &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(level.text,"bBegunPlay")),
        "Original dormant Spawn lacks actual native Level/XLevel or incorrectly began the campaign");
    const auto mapName=actor.substr(0u,actor.find('.'));
    const auto map=LoadPortablePackageTables((root/"Maps"/(mapName+".dx")).string());
    Require(xLevel.text.rfind(mapName+'.',0u)==0u,"Original XLevel binding is not an actual current-map object");
    const auto levelIndex=FindPortableExport(map,xLevel.text.substr(mapName.size()+1u));
    Require(GetPortableObjectPath(map,map.exports.at(levelIndex).ObjClass)=="Engine.Level",
        "Original native XLevel binding is not the serialized ULevel");
    const auto expectedOuter=QualifiedReference(map,map.exports.at(levelIndex).ObjOuter);
    const auto spawnerLocation=ReadPortableActorScriptProperty(actor,"Location");
    const auto spawnerRotation=ReadPortableActorScriptProperty(actor,"Rotation");
    const auto beforeRevision=GetPortableRuntimeWorldRevision();
    const std::string weaponClass="DeusEx.WeaponPistol";
    const auto result=Call(actor,"Engine.Actor.Spawn",{
        {Value::Text(Kind::Object,weaponClass),{}},{Value::Text(Kind::Object,actor),{}}});
    Require(result.committed && result.value.kind==Kind::Object && !result.value.text.empty() &&
        result.value.text.rfind(mapName+".WeaponPistol",0u)==0u,
        "Original native Spawn did not allocate/commit an actual map-qualified WeaponPistol identity");
    const auto born=result.value.text;
    const auto spawned=Snapshot(born);
    Require(GetPortableRuntimeMapActors(true).size()==originalActors.size()+1u &&
        GetPortableRuntimeWorldRevision()==beforeRevision+1u && GC::GetStats().numObjects==initialGc.numObjects+1u,
        "Original Spawn did not publish/root exactly one born actor and one world revision");
    Require(spawned.classPath==weaponClass && spawned.inventory && !spawned.meshPath.empty() &&
        spawned.ownerPath==actor && spawned.hasLocation,
        "Original spawned weapon lost its real class, inventory ancestry, mesh, explicit owner or transform");
    const auto equalProperty=[&](const std::string& name,const Value& expected) {
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(born,name),expected),
            "Original spawned weapon initialization differs at "+name);
    };
    equalProperty("Class",Value::Text(Kind::Object,weaponClass));
    equalProperty("Name",Value::Text(Kind::Name,born.substr(born.find_last_of('.')+1u)));
    equalProperty("ObjectFlags",Value::Integer(0x02004000));
    equalProperty("Owner",Value::Text(Kind::Object,actor));
    equalProperty("Level",level);equalProperty("XLevel",xLevel);
    equalProperty("Outer",Value::Text(Kind::Object,expectedOuter));
    equalProperty("Tag",Value::Text(Kind::Name,"None"));
    equalProperty("bTicked",ReadPortableActorScriptProperty(actor,"bTicked"));
    equalProperty("Instigator",ReadPortableActorScriptProperty(actor,"Instigator"));
    equalProperty("Brush",Value::Text(Kind::Object,{}));
    equalProperty("Location",spawnerLocation);equalProperty("OldLocation",spawnerLocation);
    equalProperty("Rotation",spawnerRotation);
    const auto region=ReadPortableActorScriptProperty(born,"Region");
    Require(region.kind==Kind::Struct && QuestVr::Vm::Equal(region.fields.at("zone"),level),
        "Original before-begun Spawn prematurely replaced initial Level Region.Zone");
    for(const auto* property:{"CollisionRadius","CollisionHeight","PickupAmmoCount","InitialState","Mesh"})
        equalProperty(property,ReadPortableClassDefault(weaponClass,property));
    const auto bornState=ReadPortableActorStateObject(born);
    Require(bornState && bornState->hasStack && bornState->frameOverride && bornState->frame && bornState->frame->control &&
        bornState->frame->codePath==weaponClass && bornState->frame->control->selectedPath==weaponClass &&
        !bornState->frame->control->codePresent && bornState->frame->control->liveProbeMask==~std::uint64_t{0} &&
        bornState->frame->localsCodePath.empty() && bornState->frame->locals.empty() &&
        spawned.meshPath==ReadPortableClassDefault(weaponClass,"Mesh").text &&
        spawned.drawScale==QuestVr::Vm::ToFloat(ReadPortableClassDefault(weaponClass,"DrawScale")),
        "Original dormant Spawn omitted post-SetOwner InitExecution or lost original mesh/scale defaults");
    const auto mesh=GetPortableRuntimeMesh(spawned.meshPath);
    Require(!mesh.triangles.empty() && !mesh.texturePaths.empty(),
        "Original spawned weapon mesh has no decoded original geometry/materials");
    const auto skin=ReadPortableClassDefault(weaponClass,"Skin");
    const auto texture=ReadPortableClassDefault(weaponClass,"Texture");
    equalProperty("Skin",skin);equalProperty("Texture",texture);
    Require(spawned.materialOverrides.skin.path==skin.text && spawned.materialOverrides.texture.path==texture.text &&
        spawned.texturePath==texture.text && (skin.text.empty() || spawned.materialOverrides.skin.specified) &&
        (texture.text.empty() || spawned.materialOverrides.texture.specified),
        "Original born snapshot discarded original Skin/Texture defaults");
    for(std::uint32_t index=0u;index<spawned.materialOverrides.multiSkins.size();++index) {
        const auto expected=ReadPortableClassDefault(weaponClass,"MultiSkins",index);
        Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(born,"MultiSkins",index),expected) &&
            spawned.materialOverrides.multiSkins[index].path==expected.text &&
            (expected.text.empty() || spawned.materialOverrides.multiSkins[index].specified),
            "Original born snapshot discarded independently inherited fixed MultiSkins slot");
    }
    for(const auto& vertex:mesh.triangles) {
        const auto material=QuestVr::ResolveActorMeshMaterial(spawned.materialOverrides,
            mesh.texturePaths,mesh.materialTextureIndices,vertex.material);
        Require(material.validMaterial && !material.texturePath.empty(),
            "Original spawned weapon has an unresolved original mesh material");
    }
    const auto saved=directory/"original-dormant-birth-v8.sav";
    const auto inspect=directory/"original-dormant-birth-inspect.sav";
    Require(SavePortableRuntimeState(saved.string()),"Original born weapon could not save");
    const auto bytes=CheckpointBytes(saved);
    const auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(bytes));
    Require(Word(bytes,4u)==12u && ScriptBlob(bytes).at(6u)==9u && state.births.size()==1u &&
        state.births.front().path==born && state.births.front().classPath==weaponClass &&
        state.births.front().frozenDefaults.empty(),
        "Original dormant birth did not retain codec9/envelope12 with immutable original CDO baseline");
    Require(ValidatePortableRuntimeState(saved.string(),mapName) &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==bytes,
        "Read-only original born-actor validation changed its committed graph");
    GC::Collect();
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==bytes,
        "Original born weapon or native owner references were not rooted across GC");
    Require(LoadPortableRuntimeState(legacy.string()) && GetPortableRuntimeMapActors(true).size()==originalActors.size() &&
        GC::GetStats().numObjects==initialGc.numObjects && GC::GetStats().memoryUsage==initialGc.memoryUsage,
        "Original legacy restore retained born UObject allocations or publication");
    bool removed{};try {ReadPortableActorScriptProperty(born,"Owner");} catch(const std::exception&) {removed=true;}
    Require(removed,"Original legacy restore retained a retired born VM identity");
    Require(ValidatePortableRuntimeState(saved.string(),mapName) && LoadPortableRuntimeState(saved.string()) &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==bytes && Snapshot(born).meshPath==spawned.meshPath,
        "Original unknown birth could not cold-restore exact native bindings/defaults/mesh/owner graph");
    equalProperty("Tag",Value::Text(Kind::Name,"None"));
    const auto stableGc=GC::GetStats();
    for(unsigned iteration=0u;iteration<3u;++iteration) {
        Require(LoadPortableRuntimeState(saved.string()),"Original v8 birth replacement failed");
        const auto currentGc=GC::GetStats();
        Require(currentGc.numObjects==stableGc.numObjects && currentGc.memoryUsage==stableGc.memoryUsage,
            "Repeated original v8 birth replacement retained unrooted retired allocations");
    }
    const auto tagged=Call(actor,"Engine.Actor.Spawn",{
        {Value::Text(Kind::Object,weaponClass),{}},{Value::Text(Kind::Object,actor),{}},Name("OriginalCustomSpawnTag")}).value;
    Require(tagged.kind==Kind::Object && !tagged.text.empty() && tagged.text!=born &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(tagged.text,"Tag"),Name("OriginalCustomSpawnTag").value),
        "Actual original execSpawn did not publish its explicit post-return Tag");
    const auto taggedSave=directory/"original-tagged-birth.sav";
    Require(SavePortableRuntimeState(taggedSave.string()),"Actual original tagged Spawn could not save");
    const auto taggedBytes=CheckpointBytes(taggedSave);
    Require(LoadPortableRuntimeState(legacy.string()) && LoadPortableRuntimeState(taggedSave.string()) &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(tagged.text,"Tag"),Name("OriginalCustomSpawnTag").value) &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(born,"Tag"),Name("None").value) &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==taggedBytes,
        "Actual original custom/omitted SpawnTag did not survive a cold exact graph restore");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent() &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==originalBytes,
        "Original dormant Spawn control did not completely restore authored legacy state");
    std::cout<<"ORIGINAL DORMANT SPAWN "<<born<<" original WeaponPistol CDO/mesh/materials/owner/native Level/XLevel, "<<
        "codec9/v12 class-backed InitExecution, original omitted/custom post-return SpawnTag, cold replacement/GC/legacy reset; actual Level before-begun, no campaign startup claim\n";
}

void VerifyOriginalActorLookup(const std::filesystem::path& root,const std::string& actor,
    const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()),"Could not reset original actor lookup baseline");
    const auto before=CheckpointBytes(legacy);
    const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();
    const auto none=Value::Text(Kind::Object,{});
    Require(QuestVr::Vm::Equal(Call(actor,"GetPlayerPawn").value,none) &&
        QuestVr::Vm::Equal(Call(actor,"FindTaggedActor",{Name("None")}).value,none),
        "Dormant original Level fabricated a player binding or substituted a PlayerStart/NPC");
    // Independent oracle: original serialized Level order, live Tag and
    // Location values. No production tag index or iterator enumeration is used.
    const auto mapName=actor.substr(0u,actor.find('.'));
    const auto source=LoadPortablePackageTables((root/"Maps"/(mapName+".dx")).string());
    const auto snapshots=GetPortableRuntimeMapActors(true);
    std::map<std::string,std::vector<std::pair<std::string,Value>>> groups;
    for (const auto reference:ReadPortableLevel68ActorOrder(source)) {
        if (reference==0) continue;
        Require(reference>0,"Original lookup oracle requires local serialized Level slots");
        const auto path=mapName+'.'+GetPortableObjectPath(source,reference);
        if (path==actor || std::none_of(snapshots.begin(),snapshots.end(),[&](const auto& candidate) {
            return candidate.objectPath==path;
        })) continue;
        auto tag=ReadPortableActorScriptProperty(path,"Tag");
        auto location=ReadPortableActorScriptProperty(path,"Location");
        Require(tag.kind==Kind::Name && location.kind==Kind::Vector,"Original lookup oracle lost typed Tag/Location");
        if (tag.text.empty() || QuestVr::Vm::Equal(tag,Name("None").value)) continue;
        for (auto& character:tag.text) if (character>='A' && character<='Z') character+= 'a'-'A';
        groups[tag.text].push_back({path,std::move(location)});
    }
    const auto group=std::find_if(groups.begin(),groups.end(),[](const auto& entry) {return entry.second.size()>=2u;});
    Require(group!=groups.end(),"Original lookup oracle found no repeated authored Tag");
    const auto location=ReadPortableActorScriptProperty(actor,"Location");
    std::string nearest;float best=1000000.0f;
    for (const auto& [path,candidate]:group->second) {
        const float x=candidate.vector[0u]-location.vector[0u];
        const float y=candidate.vector[1u]-location.vector[1u];
        const float z=candidate.vector[2u]-location.vector[2u];
        const float distance=std::sqrt(x*x+y*y+z*z);
        if (nearest.empty() || distance<best) { nearest=path;best=distance; }
    }
    const auto nearestValue=Value::Text(Kind::Object,nearest);
    Require(QuestVr::Vm::Equal(Call(actor,"FindTaggedActor",{Name(group->first)}).value,nearestValue),
        "Original tagged lookup differs from independent Level-order nearest-actor oracle");
    std::string mixedTag=group->first;
    for (std::size_t i=0u;i<mixedTag.size();i+=2u)
        if (mixedTag[i]>='a' && mixedTag[i]<='z') mixedTag[i]-= 'a'-'A';
    Require(QuestVr::Vm::Equal(Call(actor,"FindTaggedActor",{Name(mixedTag),{Value::Bool(false),{}},
        {Value::Text(Kind::Object,"Engine.Actor"),{}}}).value,nearestValue),
        "Original explicit Class/tag case-alias lookup changed its result");
    Require(!groups.count("questvr_missing_lookup_tag"),"Original missing-Tag fixture collides with real source data");
    Require(QuestVr::Vm::Equal(Call(actor,"FindTaggedActor",{Name("QuestVR_Missing_Lookup_Tag")}).value,none) &&
        QuestVr::Vm::Equal(Call(actor,"FindTaggedActor",{Name(group->first),{Value::Bool(false),{}},
            {Value::Text(Kind::Object,"Engine.PlayerPawn"),{}}}).value,none),
        "Original empty/class-filtered iterator fabricated an actor result");
    const auto inspect=directory/"original-readonly-actor-lookup.sav";
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==before &&
        GetPortableRuntimeWorldRevision()==revision && GC::GetStats().numObjects==gc.numObjects &&
        GC::GetStats().memoryUsage==gc.memoryUsage,
        "Original read-only lookup changed actor state, save bytes, GC roots or world revision");
    const auto initialized=Call(actor,"InitializeHomeBase");
    Require(initialized.function=="DeusEx.ScriptedPawn.InitializeHomeBase" && initialized.offset==186u &&
        initialized.opcode==0x0bu,"Original InitializeHomeBase did not complete its actual compiled Return");
    const auto homeRot=ReadPortableActorScriptProperty(actor,"HomeRot");
    Require(homeRot.kind==Kind::Vector && std::fabs(std::sqrt(homeRot.vector[0u]*homeRot.vector[0u]+
        homeRot.vector[1u]*homeRot.vector[1u]+homeRot.vector[2u]*homeRot.vector[2u])-100.0f)<0.001f,
        "Original HomeRot multiplication assignment did not retain its 100-unit forward vector");
    Require(LoadPortableRuntimeState(legacy.string()) && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect)==before,"Original HomeBase helper failed complete legacy reset");
    std::cout<<"ORIGINAL ACTOR LOOKUP native720 absent-player None; actual FindTaggedActor native304/225 nearest oracle Tag="<<
        group->first<<" candidates="<<group->second.size()<<" result="<<nearest<<
        "; case/class/empty controls, byte-identical readonly state/GC; InitializeHomeBase Return PC186/native221 verified\n";
}
void VerifyColdOriginalAnimationAssets(const std::filesystem::path& root,
    const std::filesystem::path& legacy, const std::filesystem::path& directory) {
    const auto doctor = Snapshot("00_Training.Doctor1");
    const auto actors = GetPortableRuntimeMapActors();
    auto jaime = std::find_if(actors.begin(), actors.end(), [](const auto& actor) {
        return actor.objectPath == "00_Training.JaimeReyes0" && !actor.hidden && actor.drawType != 0u;
    });
    if (jaime == actors.end()) jaime = std::find_if(actors.begin(), actors.end(), [](const auto& actor) {
        return actor.objectPath == "00_Training.JaimeReyes1" && !actor.hidden && actor.drawType != 0u;
    });
    Require(!doctor.hidden && jaime != actors.end() && doctor.meshPath == "DeusExCharacters.GM_Trench" &&
        jaime->meshPath == "DeusExCharacters.GM_Trench_F", "Cold asset controls need two rendered original meshes");
    const auto original = LoadPortablePackageTables((root / "System" / "DeusExCharacters.u").string());
    const auto expectedPaths = [&](const PortableActorSnapshot& actor) {
        const auto mesh = LoadPortableLodMesh(original,
            FindPortableExport(original, actor.meshPath.substr(actor.meshPath.find('.') + 1u)));
        std::vector<std::string> paths;
        for (const auto reference : mesh.textures) {
            auto path = GetPortableObjectPath(original, reference);
            if (reference > 0 && !path.empty()) path = "DeusExCharacters." + path;
            paths.push_back(std::move(path));
        }
        Require(!mesh.triangles.empty() && !paths.empty(), "Independent original mesh lacks geometry/material slots");
        return paths;
    };
    const auto doctorPaths = expectedPaths(doctor), jaimePaths = expectedPaths(*jaime);
    const auto requireCold = [&](const std::string& path) {
        bool unavailable{};
        try { static_cast<void>(GetPortableRuntimeMesh(path)); } catch (const std::exception&) { unavailable = true; }
        Require(unavailable, "Cold native asset regression was incidentally warmed: " + path);
    };
    requireCold(doctor.meshPath); requireCold(jaime->meshPath);
    const auto inspect = directory / "original-cold-animation-inspect.sav";
    const auto saved = directory / "original-cold-animation-v4.sav";
    const auto authored = CheckpointBytes(legacy); const auto revision = GetPortableRuntimeWorldRevision();
    const auto stats = GC::GetStats();
    Require(QuestVr::Vm::ToBool(Call(doctor.objectPath, "HasAnim", {Name("Still")}).value),
        "Cold original HasAnim did not find Still");
    const auto queried = GetPortableRuntimeMesh(doctor.meshPath);
    Require(queried.texturePaths == doctorPaths && queried.texturePaths.size() == queried.textures.size() &&
        !GetPortableRuntimeScriptStatePresent() && GetPortableRuntimeWorldRevision() == revision &&
        GC::GetStats().numObjects == stats.numObjects && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect) == authored, "Cold HasAnim published a partial mesh or changed actor/save state");
    // A read-only query must not warm unrelated geometry. The first mutating
    // native therefore takes the other mesh's lazy path before Commit's first
    // map-wide decode, exactly as isolated desktop helper capture does.
    requireCold(jaime->meshPath);
    Call(jaime->objectPath, "TweenBlendAnim", {Name("Still"), Number(0.3f)});
    const auto tweened = Snapshot(jaime->objectPath);
    Require(tweened.animation.blends[0].sequence == "Still" && tweened.animation.blends[0].frame < 0 &&
        GetPortableRuntimeMesh(jaime->meshPath).texturePaths == jaimePaths,
        "Cold TweenBlendAnim did not publish its real pose and fully resolved mesh");
    Require(DecodePortableRuntimeActorMeshes().passed, "Cold native cache prevented complete map asset decoding");
    const auto textures = BuildPortableRuntimeActorTextureArray(16u, 16u);
    Require(textures.passed, "Cold native cache prevented original actor texture-array creation");
    std::size_t materialChecks{};
    for (const auto& actor : {doctor, tweened}) {
        const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
        const auto& paths = actor.meshPath == doctor.meshPath ? doctorPaths : jaimePaths;
        Require(mesh.texturePaths == paths && mesh.texturePaths.size() == mesh.textures.size() &&
            mesh.animation && QuestVr::PrepareMeshPose(mesh, QuestVr::BuildSnapshotMeshAnimationState(actor)).drawable,
            "Map-wide decode lost the fully hydrated native mesh or drawable original pose");
        for (const auto& vertex : mesh.triangles) {
            const auto material = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                mesh.texturePaths, mesh.materialTextureIndices, vertex.material);
            Require(material.validMaterial && !material.texturePath.empty() &&
                std::find(textures.texturePaths.begin(), textures.texturePaths.end(), material.texturePath) != textures.texturePaths.end(),
                "Cold native mesh lost original MultiSkins/material-layer coverage");
            ++materialChecks;
        }
    }
    Require(SavePortableRuntimeState(saved.string()), "Cold blend command could not save its complete clock");
    const auto committed = CheckpointBytes(saved);
    QuestVr::Vm::Limits limits; limits.writes = 1u;
    const auto failed = ExecutePortableActorFunction(tweened.objectPath, "PlayAnimPivot",
        {Name("Still"), Number(3.0f), Number(0.5f), Vector({9.0f, 8.0f, 7.0f})}, limits);
    Require(failed.status == Status::Budget && !failed.committed && Same(tweened, Snapshot(tweened.objectPath)) &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == committed &&
        GetPortableRuntimeMesh(tweened.meshPath).texturePaths == jaimePaths,
        "Failed original animation transaction damaged actor/save state or immutable material cache");
    Require(LoadPortableRuntimeState(legacy.string()) && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect) == authored && GetPortableRuntimeMesh(doctor.meshPath).texturePaths == doctorPaths &&
        GetPortableRuntimeMesh(jaime->meshPath).texturePaths == jaimePaths,
        "Legacy reset changed actor state or discarded complete cold-loaded assets");
    std::cout << "ORIGINAL COLD ASSETS HasAnim " << doctor.objectPath << " and TweenBlendAnim " << jaime->objectPath
        << " before first decode; exact original texture references, drawable poses," << materialChecks
        << " vertex-material checks, readonly v3/save rollback and retained complete immutable cache\n";
}

void VerifyOriginalBlendCommands(const std::filesystem::path& root, const std::string& actor,
    const std::filesystem::path& legacy, const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()), "Could not reset original blend fixture");
    const auto initial = Snapshot(actor);
    const auto mesh = LoadPortableLodMesh(LoadPortablePackageTables(
        (root / "System" / "DeusExCharacters.u").string()),
        FindPortableExport(LoadPortablePackageTables((root / "System" / "DeusExCharacters.u").string()),
            initial.meshPath.substr(initial.meshPath.find('.') + 1u)));
    Require(mesh.animation != nullptr, "Original human fixture has no animation metadata");
    const auto saved = directory / "original-blend-v4.sav", inspect = directory / "original-blend-inspect.sav";
    const auto generated = directory / "original-blend-seeded.sav";
    Call(actor,"PlayAnim",{Name("Still"),Number(1),Number(0)});
    Require(SavePortableRuntimeState(saved.string()),"Original blend baseline could not save");
    const auto baselineBytes = CheckpointBytes(saved);
    auto baseline = QuestVr::DecodeScriptSavedState(ScriptBlob(baselineBytes));
    const auto index = static_cast<std::size_t>(std::find_if(baseline.objects.begin(),baseline.objects.end(),
        [&](const auto& value) { return value.path==actor; }) - baseline.objects.begin());
    Require(index<baseline.objects.size() && baseline.objects[index].clock,"Original clock was not captured");
    const auto replace = [&](QuestVr::ScriptSavedState state) {
        auto bytes=ReplaceScriptBlob(baselineBytes,QuestVr::EncodeScriptSavedState(state));
        Require(QuestVr::WriteDurableSaveFile(generated.string(),bytes) && LoadPortableRuntimeState(generated.string()),
            "Original generated blend seed rejected"); return bytes;
    };
    const auto unchanged = [&](const std::vector<std::uint8_t>& bytes, const std::string& label) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==bytes,label+" changed saved clock/properties");
    };
    auto seeded=baseline;
    seeded.objects[index].properties.push_back({"Engine.Pawn.animTimer","animTimer",1u,Value::Float(1)});
    const auto seedBytes=replace(seeded);
    for (const auto& direction : {std::pair{1,"HeadLeft"},std::pair{2,"HeadRight"},std::pair{3,"HeadUp"},std::pair{4,"HeadDown"}}) {
        Require(LoadPortableRuntimeState(generated.string()),"Could not reset head-turn seed");
        const auto before=Snapshot(actor);
        const auto* sequence=QuestVr::FindMeshAnimationSequence(*mesh.animation,direction.second,false);
        Require(sequence && sequence->numFrames>0,"Original head sequence unavailable");
        const auto result=Call(actor,"Engine.Pawn.PlayTurnHead",{{Value::Byte(static_cast<std::uint8_t>(direction.first)),{}},Number(1),Number(0.2f)});
        const auto after=Snapshot(actor); const auto& blend=after.animation.blends[3];
        auto untouched=after; untouched.animation.blends[3]=before.animation.blends[3];
        Require(QuestVr::Vm::ToBool(result.value) && blend.sequence==direction.second &&
            std::abs(blend.frame + 1.0f/static_cast<float>(sequence->numFrames))<0.00001f &&
            Same(before,untouched),
            "Original PlayTurnHead did not execute native1010 on slot3 independently of main animation");
        const auto sim=ReadPortableActorScriptProperty(actor,"SimBlendAnim",3u);
        Require(sim.kind==Kind::Struct && sim.fields.size()==4u &&
            std::abs(sim.fields.at("x").floating-blend.frame*10000)<0.01 &&
            std::abs(sim.fields.at("y").floating-blend.rate*10000)<0.01 &&
            std::abs(sim.fields.at("z").floating-blend.tweenRate*1000)<0.01 &&
            std::abs(sim.fields.at("w").floating-blend.last*10000)<0.01,
            "Original blend Plane layout not synchronized to actual script properties");
        Require(SavePortableRuntimeState(saved.string()),"Original head blend could not checkpoint");
        const auto bytes=CheckpointBytes(saved);
        Call(actor,"TweenBlendAnim",{Name("Still"),Number(0.3f),{Value::Integer(3),{}}});
        const auto tween=Snapshot(actor).animation.blends[3];
        Require(tween.frame<0 && tween.rate==0 && tween.last==0,"Original TweenBlendAnim retained pinned positive frame");
        Require(LoadPortableRuntimeState(saved.string()) && Same(after,Snapshot(actor)),"Original head blend roundtrip changed pose/history");
        unchanged(bytes,"Original head blend v4 roundtrip");
        for (const auto slot : {-1,4,256}) {
            Call(actor,"PlayBlendAnim",{Name("Still"),Number(1),Number(0),{Value::Integer(slot),{}}});
            unchanged(bytes,"Original invalid signed blend slot return");
        }
        Call(actor,"PlayBlendAnim",{Name("Quest_No_Such_Sequence")});
        unchanged(bytes,"Original missing blend sequence return");
    }
    Require(LoadPortableRuntimeState(generated.string()),"Could not reset head rollback seed");
    QuestVr::Vm::Limits limits; limits.writes=3u;
    const auto rollback=ExecutePortableActorFunction(actor,"Engine.Pawn.PlayTurnHead",{{Value::Byte(1),{}},Number(1),Number(0.2f)},limits);
    Require(rollback.status==Status::Budget && !rollback.committed,"Head-turn write budget did not refuse transaction");
    unchanged(seedBytes,"Original head helper rollback");
    for (const bool partial : {true,false}) {
        auto invalid=baseline; auto& properties=invalid.objects[index].properties;
        if (partial) properties.erase(std::remove_if(properties.begin(),properties.end(),[](const auto& value) {
            return value.name=="SimBlendAnim" && value.index==1u; }),properties.end());
        else for (auto& property : properties) if (property.name=="SimBlendAnim" && property.index==2u)
            property.value.fields.at("x")=Value::Float(17);
        const auto bytes=ReplaceScriptBlob(baselineBytes,QuestVr::EncodeScriptSavedState(invalid));
        const auto rejected=directory/"original-blend-rejected.sav";
        Require(QuestVr::WriteDurableSaveFile(rejected.string(),bytes) &&
            !ValidatePortableRuntimeState(rejected.string()) && !LoadPortableRuntimeState(rejected.string()),
            "Partial/disagreeing reflected blend Plane was accepted");
        unchanged(seedBytes,"Rejected reflected blend Plane rollback");
    }
    // A reflected Plane overlay without a native clock is legal. The first
    // actual command must import it, not overwrite it with synthetic zeros.
    auto reflected=baseline; reflected.objects[index].clock.reset();
    for (auto& property : reflected.objects[index].properties)
        if (property.name=="SimBlendAnim" && property.index==2u) property.value.fields.at("w")=Value::Float(123);
    replace(reflected); Call(actor,"PlayAnim",{Name("Still"),Number(1),Number(0)});
    Require(SavePortableRuntimeState(inspect.string()),"Authored Plane import could not checkpoint");
    const auto imported=QuestVr::DecodeScriptSavedState(ScriptBlob(CheckpointBytes(inspect)));
    Require(imported.objects[index].clock && imported.objects[index].clock->blends[2u].simulated[3u]==123 &&
        ReadPortableActorScriptProperty(actor,"SimBlendAnim",2u).fields.at("w").floating==123,
        "First native command discarded the reflected SimBlendAnim overlay");
    auto legacyClock=baseline;
    auto& properties=legacyClock.objects[index].properties;
    properties.erase(std::remove_if(properties.begin(),properties.end(),[](const auto& value) {
        return value.name=="SimBlendAnim"; }),properties.end());
    replace(legacyClock);
    unchanged(baselineBytes,"Legacy clock derives omitted SimBlendAnim without changing native state");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),"Blend legacy reset retained state");
    std::cout<<"ORIGINAL BLEND actual PlayTurnHead directions1–4 Return/native1010 slot3, Tween1012 negative frame/Plane, signed slots/missing sequence, rollback, reflected import/partial/disagree rejection, v4/legacy clock roundtrip; no automatic ticking\n";
}

void VerifyOriginalSwitchHeadTurn(const std::string& actor, const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()), "Could not reset original Switch fixture");
    Require(QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bCanTurnHead")),
        "Original human Switch fixture does not permit head turns");
    const auto saved = directory / "original-switch-v4.sav";
    const auto generated = directory / "original-switch-seeded.sav";
    const auto inspect = directory / "original-switch-inspect.sav";
    Call(actor, "PlayAnim", {Name("Still"), Number(1), Number(0)});
    Require(SavePortableRuntimeState(saved.string()), "Original Switch baseline could not save");
    const auto baselineBytes = CheckpointBytes(saved);
    auto seeded = QuestVr::DecodeScriptSavedState(ScriptBlob(baselineBytes));
    const auto object = std::find_if(seeded.objects.begin(), seeded.objects.end(),
        [&](const auto& value) { return value.path == actor; });
    Require(object != seeded.objects.end() && object->clock, "Original Switch fixture has no native clock");
    object->properties.push_back({"Engine.Pawn.animTimer", "animTimer", 1u, Value::Float(1)});
    object->properties.push_back({"Engine.Pawn.AIAddViewRotation", "AIAddViewRotation", 0u,
        Value::Rotator({123, -456, 789})});
    const auto seedBytes = ReplaceScriptBlob(baselineBytes, QuestVr::EncodeScriptSavedState(seeded));
    Require(QuestVr::WriteDurableSaveFile(generated.string(), seedBytes) &&
        ValidatePortableRuntimeState(generated.string()), "Original typed Switch input rejected");
    struct Direction { std::uint8_t value; const char* sequence; std::array<std::int32_t, 3> rotation; };
    for (const auto& direction : {Direction{0, "Still", {0, 0, 0}}, Direction{1, "HeadLeft", {0, -5461, 0}},
            Direction{2, "HeadRight", {0, 5461, 0}}, Direction{3, "HeadUp", {5461, 0, 0}},
            Direction{4, "HeadDown", {-5461, 0, 0}}, Direction{255, "Still", {0, 0, 0}}}) {
        Require(LoadPortableRuntimeState(generated.string()), "Could not reset original Switch direction");
        const auto before = Snapshot(actor);
        const auto args = std::vector<Evaluation>{{Value::Byte(direction.value), {}}, Number(1), Number(0.2f)};
        const auto result = Call(actor, "DeusEx.ScriptedPawn.PlayTurnHead", args);
        const auto after = Snapshot(actor);
        const auto rotation = ReadPortableActorScriptProperty(actor, "AIAddViewRotation");
        auto untouched = after; untouched.animation.blends[3] = before.animation.blends[3];
        // The actual override returns Nothing at PC208, hence its Bool return
        // property supplies false even when the base helper successfully turns.
        Require(result.status == Status::Returned && result.value.kind == Kind::Bool && !result.value.boolean &&
            result.function == "DeusEx.ScriptedPawn.PlayTurnHead" && result.offset == 208u && result.opcode == 0x0bu &&
            rotation.kind == Kind::Rotator && rotation.rotation == direction.rotation &&
            after.animation.blends[3].sequence == direction.sequence && Same(before, untouched),
            "Actual original Switch selected an incorrect head/view branch or fabricated its Bool return");
        Require(ReadPortableActorScriptProperty(actor, "animTimer", 1u).floating == 0.0f &&
            SavePortableRuntimeState(saved.string()), "Original Switch did not retain the base helper's timer write");
        const auto committed = CheckpointBytes(saved);
        // Same sequence with an exhausted timer makes the real base helper
        // return false, and the override must skip its reset/Switch entirely.
        const auto skipped = Call(actor, "DeusEx.ScriptedPawn.PlayTurnHead", args);
        Require(skipped.value.kind == Kind::Bool && !skipped.value.boolean &&
            SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == committed,
            "Original false head-turn branch evaluated Switch or changed its clock/view properties");
        Require(LoadPortableRuntimeState(saved.string()) && Same(after, Snapshot(actor)) &&
            ReadPortableActorScriptProperty(actor, "AIAddViewRotation").rotation == direction.rotation &&
            SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == committed,
            "Original Switch view/clock checkpoint did not roundtrip exactly");
    }
    Require(LoadPortableRuntimeState(generated.string()), "Could not reset original Switch rollback input");
    QuestVr::Vm::Limits limits; limits.writes = 6u;
    const auto failed = ExecutePortableActorFunction(actor, "DeusEx.ScriptedPawn.PlayTurnHead",
        {{Value::Byte(1), {}}, Number(1), Number(0.2f)}, limits);
    Require(failed.status == Status::Budget && !failed.committed && failed.offset == 64u && failed.opcode == 0x0fu &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == seedBytes,
        "Original selected Switch body did not share write budget or roll back base animation/view writes");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Legacy reset retained original Switch properties or native clock");
    std::cout << "ORIGINAL SWITCH actual DeusEx head override directions0–4/default255, exact yaw/pitch +/-5461, "
        "native1010/base timer, original false Bool return, short-circuit, selected-body rollback and view/clock v4 roundtrip; no live NPC tick\n";
}

void VerifyOriginalRandom(const std::string& actor, const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()), "Original random fixture could not reset to legacy seed1");
    const auto inspect=directory/"original-random-v10.sav";
    const auto generated=directory/"original-random-seeded-v10.sav";
    const auto baseline=CheckpointBytes(legacy);
    const auto pose=Snapshot(actor);
    const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();
    const auto unchanged=[&](const std::vector<std::uint8_t>& expected,const std::string& context) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==expected,
            context+" changed full serialized state/random stream");
    };
    for (const auto bound:{0,-1,std::numeric_limits<std::int32_t>::min()}) {
        const auto result=Call(actor,"Core.Object.Rand",{{Value::Integer(bound),{}}});
        Require(result.value.kind==Kind::Int && result.value.integer==0 && !GetPortableRuntimeScriptStatePresent(),
            "Original Rand(nonpositive) drew or created saved state");
        unchanged(baseline,"Original Rand(nonpositive)");
    }
    // Independent uint64 recurrence, not the implementation under test.
    std::uint32_t seed=1u;
    const auto raw=[&]() {
        seed=static_cast<std::uint32_t>(std::uint64_t(seed)*214013ull+2531011ull);
        return (seed>>16u)&32767u;
    };
    const auto fraction=[&]() {return static_cast<float>(raw())*std::bit_cast<float>(0x38000100u);};
    const auto first=Call(actor,"Core.Object.FRand");
    Require(first.value.kind==Kind::Float && first.value.floating==fraction() &&
        GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState(),
        "Original FRand failed first shared draw or created map-scoped state");
    const auto actors=GetPortableRuntimeMapActors();
    const auto other=std::find_if(actors.begin(),actors.end(),[&](const auto& candidate) {
        return candidate.pawn && candidate.objectPath!=actor;
    });
    Require(other!=actors.end(),"Original shared-stream control lacks second actor");
    const auto second=Call(other->objectPath,"Core.Object.Rand",{{Value::Integer(100000),{}}});
    Require(second.value.kind==Kind::Int && second.value.integer==static_cast<std::int32_t>(raw()),
        "Original Rand large-bound/cross-actor stream differed");
    Require(Call(actor,"Core.Object.Rand",{{Value::Integer(1),{}}}).value.integer==0,
        "Original Rand(1) did not return zero");
    raw(); // Max1 consumes exactly one draw.
    for (const auto& range:{std::pair{10.0f,-5.0f},std::pair{7.0f,7.0f},std::pair{-2.0f,4.0f}}) {
        const auto value=Call(actor,"Core.Object.RandRange",{Number(range.first),Number(range.second)});
        const float scaled=(range.second-range.first)*fraction();
        const float expected=range.first+scaled;
        Require(value.function=="Core.Object.RandRange" && value.value.kind==Kind::Float &&
            std::bit_cast<std::uint32_t>(value.value.floating)==std::bit_cast<std::uint32_t>(expected),
            "Actual scripted original RandRange did not evaluate Min+(Max-Min)*FRand");
    }
    Require(SavePortableRuntimeState(inspect.string()),"Original random-only state could not save");
    const auto saved=CheckpointBytes(inspect);
    const auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(saved));
    Require(Word(saved,4)==10u && ScriptBlob(saved).at(6)==7u && state.randomSeed==seed && state.objects.empty() &&
        state.classDefaults.empty() && state.births.empty() && state.aiManagers.empty() &&
        Same(pose,Snapshot(actor)) && GetPortableRuntimeWorldRevision()==revision &&
        GC::GetStats().numObjects==gc.numObjects && GC::GetStats().memoryUsage==gc.memoryUsage,
        "Original random-only save changed actors/assets/GC/world or omitted exact seed");
    Require(ValidatePortableRuntimeState(inspect.string()),"Original v10 random-only read-only validation failed");
    unchanged(saved,"Original random-only validation");
    const float next=fraction();
    Require(Call(actor,"Core.Object.FRand").value.floating==next && LoadPortableRuntimeState(inspect.string()) &&
        Call(actor,"Core.Object.FRand").value.floating==next,
        "Original restored random-only stream did not continue at the next draw");
    for (const auto& endpoint:{std::pair{20057u,0.0f},std::pair{30091u,1.0f}}) {
        auto injected=state;injected.randomSeed=endpoint.first;
        const auto bytes=ReplaceScriptBlob(saved,QuestVr::EncodeScriptSavedState(injected));
        Require(QuestVr::WriteDurableSaveFile(generated.string(),bytes) && LoadPortableRuntimeState(generated.string()),
            "Original endpoint fixture could not restore seed");
        Require(Call(actor,"Core.Object.FRand").value.floating==endpoint.second,
            "Original FRand omitted inclusive binary32 endpoint");
    }
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Original legacy reset retained random-only state");
    unchanged(baseline,"Original random legacy reset");
    std::cout<<"ORIGINAL RANDOM actual native167/195, nonpositive no-draw, Max1 draw, large-bound/shared-actor stream; "
        "scripted RandRange native0 normal/reversed/equal ranges, inclusive0/1; random-only codec7/envelope10 and continuation\n";
}

void VerifyOriginalBirdRandom(const std::string& actor,const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    const auto inspect=directory/"original-bird-random-v10.sav";
    const auto generated=directory/"original-bird-random-seeded-v10.sav";
    Require(SavePortableRuntimeState(inspect.string()),"Original Bird clock baseline could not save");
    const auto baseline=CheckpointBytes(inspect);
    const auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(baseline));
    for (const auto& branch:{std::pair{20057u,"Idle2"},std::pair{30091u,"Idle1"}}) {
        auto injected=state;injected.randomSeed=branch.first;
        auto bytes=ReplaceScriptBlob(baseline,QuestVr::EncodeScriptSavedState(injected));PutWord(bytes,4,10u);
        Require(QuestVr::WriteDurableSaveFile(generated.string(),bytes) && LoadPortableRuntimeState(generated.string()),
            "Original Bird random branch seed could not restore");
        const auto before=Snapshot(actor);const auto revision=GetPortableRuntimeWorldRevision();
        QuestVr::Vm::Limits limits;limits.writes=0u;
        const auto failed=ExecutePortableActorFunction(actor,"TweenToWaiting",{Number(0.2f)},limits);
        Require(failed.status==Status::Budget && !failed.committed && Same(before,Snapshot(actor)) &&
            GetPortableRuntimeWorldRevision()==revision && SavePortableRuntimeState(inspect.string()) &&
            CheckpointBytes(inspect)==bytes,"Original Bird failed selected assignment retained random draw or clock");
        const auto result=Call(actor,"TweenToWaiting",{Number(0.2f)});
        const auto pose=Snapshot(actor);
        std::uint32_t expectedSeed=static_cast<std::uint32_t>(std::uint64_t(branch.first)*214013ull+2531011ull);
        Require(result.function=="DeusEx.Bird.TweenToWaiting" && result.value.kind==Kind::Nothing &&
            ReadPortableActorScriptProperty(actor,"WaitAnim").text==branch.second &&
            pose.animation.sequence==branch.second && pose.animation.frame<0.0f &&
            SavePortableRuntimeState(inspect.string()),"Original Bird FRand/compare/Let/Tween branch did not complete");
        const auto saved=CheckpointBytes(inspect);
        Require(Word(saved,4)==10u && QuestVr::DecodeScriptSavedState(ScriptBlob(saved)).randomSeed==expectedSeed &&
            ValidatePortableRuntimeState(inspect.string()) && LoadPortableRuntimeState(inspect.string()) &&
            Same(pose,Snapshot(actor)) && SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==saved,
            "Original Bird mixed random/property/native-clock save did not roundtrip exactly");
    }
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Original Bird legacy reset retained random or clock state");
    std::cout<<"ORIGINAL BIRD "<<actor<<" PlayWaiting/native optional; actual TweenToWaiting FRand endpoints "
        "Idle1/Idle2, post-draw selected-write rollback and mixed clock/RNG v10 roundtrip\n";
}

void VerifyOriginalAIEvents(const std::string& actor, const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()), "Could not reset original AI fixture");
    const auto inspect = directory / "original-ai-events.sav";
    const auto original = CheckpointBytes(legacy);
    const auto initialGc = GC::GetStats();
    Call(actor, "UpdateReactionCallbacks");
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == original &&
        GC::GetStats().numObjects == initialGc.numObjects,
        "Original absent-manager wrapper branch fabricated native state");
    const auto level = ReadPortableActorScriptProperty(actor, "Level").text;
    const auto revision = GetPortableRuntimeWorldRevision();
    Call(level, "InitEventManager");
    Require(GetPortableRuntimeScriptStatePresent() && GetPortableRuntimeWorldRevision() == revision &&
        SavePortableRuntimeState(inspect.string()), "Original native650 failed to retain an empty manager independently of geometry");
    const auto emptyBytes = CheckpointBytes(inspect);
    auto graph = QuestVr::DecodeScriptSavedState(ScriptBlob(emptyBytes));
    Require(Word(emptyBytes, 4u) == 9u && graph.aiManagers.size() == 1u &&
        graph.aiManagers.front().ownerPath == level && graph.aiManagers.front().eventTypes.empty(),
        "Original empty native manager did not select envelope9/codec6");
    const auto managerGc = GC::GetStats();
    Call(level, "InitEventManager");
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == emptyBytes &&
        GC::GetStats().numObjects == managerGc.numObjects, "Repeated original initialization replaced or duplicated its manager");
    Call(actor, "AISetEventCallback", {Name("Futz"), Name("Quest_Test_Handler")});
    const auto callbacks = Call(actor, "UpdateReactionCallbacks");
    Require(callbacks.function == "DeusEx.ScriptedPawn.UpdateReactionCallbacks" && callbacks.offset == 484u &&
        callbacks.opcode == 0x0bu && SavePortableRuntimeState(inspect.string()),
        "Original reaction registration bytecode did not complete its actual Return");
    graph = QuestVr::DecodeScriptSavedState(ScriptBlob(CheckpointBytes(inspect)));
    const auto& manager = graph.aiManagers.front();
    const auto futz = std::find_if(manager.receivers.begin(), manager.receivers.end(), [&](const auto& receiver) {
        return receiver.actor == actor && manager.eventTypes.at(receiver.eventType - 1u).name == "Futz";
    });
    const bool expectedFutz = QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bReactFutz")) &&
        QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor, "bLookingForFutz"));
    Require(futz != manager.receivers.end() && futz->deleted != expectedFutz &&
        (!expectedFutz || (futz->callback == "HandleFutz" && futz->flags == QuestVr::Ai::PerceptionFlags{})),
        "Original Futz registration/clear branch lost its native identity or perception flags");
    const Evaluation visual{Value::Byte(0u), {}};
    Call(actor, "AIStartEvent", {Name("Distress"), visual, Number(0.75f), Number(640.0f)});
    const auto distress = Call(actor, "SetDistress", {{Value::Bool(false), {}}});
    Require(distress.function == "DeusEx.ScriptedPawn.SetDistress" && distress.offset == 57u &&
        SavePortableRuntimeState(inspect.string()), "Original SetDistress did not execute native715 and return");
    const auto saved = CheckpointBytes(inspect);
    graph = QuestVr::DecodeScriptSavedState(ScriptBlob(saved));
    const auto& emitted = graph.aiManagers.front();
    const auto sender = std::find_if(emitted.senders.begin(), emitted.senders.end(), [&](const auto& candidate) {
        return candidate.actor == actor && emitted.eventTypes.at(candidate.eventType - 1u).name == "Distress";
    });
    Require(sender != emitted.senders.end() && sender->current.visibility == 0.0f &&
        sender->history.at(emitted.historyCursor).visibility == 0.75f,
        "Original SetDistress end erased sensory history or left a persistent emission");
    Require(ValidatePortableRuntimeState(inspect.string()) && LoadPortableRuntimeState(inspect.string()) &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == saved,
        "Original manager graph did not roundtrip exact portable snapshot bytes");
    GC::Collect();
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == saved,
        "Original manager graph was not rooted across GC");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent() &&
        GC::GetStats().numObjects == initialGc.numObjects && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect) == original, "Legacy reset retained native AI state or allocations");
    std::cout << "ORIGINAL AI native650/710/711/714/715; actual UpdateReactionCallbacks ReturnPC484 and SetDistress ReturnPC57; empty ownership, history, codec6/envelope9/GC/reset. No AI processing or world startup claim.\n";
}

float VerifyOriginalStandingSleep(const std::string& actor,const QuestVr::Vm::Result& result,const std::string& context) {
    Require(result.passed() && result.committed && result.status==Status::Waiting && result.error.empty() && result.instructions!=0u &&
        result.function=="DeusEx.ScriptedPawn.Standing" && result.offset==304u && result.opcode==0x61u,
        context+" did not reach the exact original Standing Sleep: "+result.error);
    const auto frame=ReadPortableActorStateObject(actor);const auto timer=ReadPortableActorLatentTimeLeft(actor);
    Require(frame && frame->frame && frame->frame->codePath=="DeusEx.ScriptedPawn.Standing" &&
        frame->frame->control && frame->frame->control->selectedPath==frame->frame->codePath &&
        frame->frame->control->codePresent && frame->frame->localsCodePath.empty() && frame->frame->locals.empty() &&
        frame->frame->latent==QuestVr::StateLatent::Sleep &&
        timer && std::isfinite(*timer) && *timer>=8.0f && *timer<=22.0f && Call(actor,"GetStateName").value.text=="Standing",
        context+" lost its authored Standing frame or Sleep(FRand()*14+8) timer");
    const auto layout=ReadPortableRuntimeStateProgram(frame->frame->codePath);
    Require(frame->frame->statementIndex<layout.statementOffsets.size() && layout.statementOffsets[frame->frame->statementIndex]==319u,
        context+" did not retain the next authored Standing statement at offset319");
    return *timer;
}

void VerifyOriginalInventoryTransactions(const std::string& actor,const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Inventory transactions did not start from authored legacy state");
    const auto originalAuthored=InventoryProperties(actor);
    auto authored=originalAuthored;
    const auto firstEmpty=[&](const InventorySlots& slots) {
        return static_cast<std::size_t>(std::find_if(slots.begin(),slots.end(),[](const auto& value) {
            return value.fields.at("inventory").text.empty() && value.fields.at("count").integer<=0;
        })-slots.begin());
    };
    const auto savedV4=directory/"inventory-members-v4.sav";
    const auto savedV5=directory/"inventory-members-v5.sav";
    const auto generated=directory/"inventory-generated.sav";
    const auto inspect=directory/"inventory-unchanged.sav";
    const auto unchanged=[&](const std::vector<std::uint8_t>& bytes,const std::string& description) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==bytes,
            description+" changed complete gameplay/property/clock/state saved payload");
    };
    const auto originalLegacyBytes=CheckpointBytes(legacy);
    auto initial=originalLegacyBytes;
    const auto itemClass=Value::Text(Kind::Object,"DeusEx.WeaponPistol");
    const auto available=static_cast<std::size_t>(std::count_if(authored.begin(),authored.end(),[](const auto& value) {
        return value.fields.at("inventory").text.empty() && value.fields.at("count").integer<=0;
    }));
    if (available<2u) {
        if (available==0u) {
            const auto full=Call(actor,"AddInitialInventory",{{itemClass,{}},{Value::Integer(-7),{}}});
            Require(full.value.kind==Kind::Bool && !full.value.boolean,
                "Original helper fabricated an available slot in authored inventory");
            SameInventoryProperties(actor,originalAuthored,"Authored full inventory helper changed entries");
            unchanged(originalLegacyBytes,"Authored full inventory helper");
        }
        // The original selected NPC has no two slots satisfying the compiled
        // helper's Inventory==None && Count<=0 predicate. Keep that real
        // refusal above. Use an explicitly generated, isolated v4 overlay for
        // positive member-write controls; this is not a changed game default.
        for (auto& value:authored) value=InventoryItem();
        QuestVr::ScriptSavedState fixture;
        fixture.mapName=actor.substr(0u,actor.find('.'));
        QuestVr::ScriptSavedObject object;
        object.path=actor; object.classPath=Snapshot(actor).classPath;
        for(std::uint32_t i=0;i<authored.size();++i)
            object.properties.push_back({"DeusEx.ScriptedPawn.InitialInventory","InitialInventory",i,authored[i]});
        fixture.objects.push_back(std::move(object));
        const auto blob=QuestVr::EncodeScriptSavedState(fixture);
        auto fixtureBytes=originalLegacyBytes;PutWord(fixtureBytes,4u,4u);
        const auto lengthOffset=fixtureBytes.size();fixtureBytes.resize(lengthOffset+4u);
        PutWord(fixtureBytes,lengthOffset,static_cast<std::uint32_t>(blob.size()));
        fixtureBytes.insert(fixtureBytes.end(),blob.begin(),blob.end());
        Require(QuestVr::WriteDurableSaveFile(generated.string(),fixtureBytes) && LoadPortableRuntimeState(generated.string()),
            "Explicit empty-slot v4 transaction fixture could not load");
        SameInventoryProperties(actor,authored,"Generated empty-slot control lost independently typed slots");
        Require(SavePortableRuntimeState(inspect.string()),"Could not capture canonical generated inventory baseline");
        initial=CheckpointBytes(inspect);
        std::cout<<"ORIGINAL INVENTORY TRANSACTION "<<actor<<" authored available="<<available<<
            "; positive writes use isolated generated empty-slot v4 fixture, not authored defaults\n";
    }
    const auto slot=firstEmpty(authored);Require(slot<authored.size(),"Inventory write fixture has no available slot");
    QuestVr::Vm::Limits limits;limits.writes=slot+2u; // i=0, each loop increment, then Inventory member.
    const auto failed=ExecutePortableActorFunction(actor,"AddInitialInventory",{{itemClass,{}},{Value::Integer(-7),{}}},limits);
    Require(failed.status==Status::Budget && failed.writes==limits.writes && failed.offset==130u,
        "Original member-write rollback did not fail after Inventory but before Count: "+failed.error+
        " at "+failed.function+':'+std::to_string(failed.offset));
    SameInventoryProperties(actor,authored,"Budget after actual Inventory member write leaked struct mutation");
    unchanged(initial,"Failed original member-writing helper");
    for(const auto& invalid:{Value::Text(Kind::Object,"Engine.Actor"),Value::Text(Kind::Object,actor),
            Value::Text(Kind::Object,"DeusEx.NoSuchInventoryClass")}) {
        const auto rejected=ExecutePortableActorFunction(actor,"AddInitialInventory",{{invalid,{}},{Value::Integer(3),{}}});
        Require(!rejected.passed(),"Live class<Inventory> assignment accepted an unrelated class, instance or missing identity");
        SameInventoryProperties(actor,authored,"Rejected live class constraint changed InventoryItem");
        unchanged(initial,"Rejected live Inventory class identity");
    }
    const auto add=Call(actor,"AddInitialInventory",{{itemClass,{}},{Value::Integer(-7),{}}});
    Require(add.value.kind==Kind::Bool && add.value.boolean,"Original member-writing helper did not return true");
    auto negative=authored;negative[slot]=InventoryItem(itemClass.text,-7);
    SameInventoryProperties(actor,negative,"Original helper coerced negative Count or flattened fixed struct slots");
    Require(SavePortableRuntimeState(savedV4.string()),"Inventory member overlay could not save without fabricated state/clock");
    const auto v4=CheckpointBytes(savedV4);Require(Word(v4,4u)==4u,"Property-only InventoryItem changed v4 envelope");
    const auto findActor=[&](QuestVr::ScriptSavedState& state)->QuestVr::ScriptSavedObject& {
        const auto found=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& value) {return value.path==actor;});
        Require(found!=state.objects.end(),"Generated InventoryItem fixture omitted original actor");return *found;
    };
    const auto findItem=[&](QuestVr::ScriptSavedState& state)->QuestVr::ScriptSavedProperty& {
        auto& object=findActor(state);
        const auto found=std::find_if(object.properties.begin(),object.properties.end(),[&](const auto& value) {
            return value.key=="DeusEx.ScriptedPawn.InitialInventory" && value.index==slot;
        });
        Require(found!=object.properties.end(),"Original struct member write did not retain its qualified fixed-slot property");
        return *found;
    };
    auto original=QuestVr::DecodeScriptSavedState(ScriptBlob(v4));
    Require(!findActor(original).state && !findActor(original).clock &&
        QuestVr::Vm::Equal(findItem(original).value,negative[slot]),
        "Property-only InventoryItem save fabricated state/clock or lost negative Count");
    // A second legitimate class verifies inherited Engine.Inventory constraints,
    // while optional zero newCount is executed by the authored helper, not C++.
    const auto second=firstEmpty(negative);Require(second<negative.size(),"Inventory fixture needs a second empty authored slot");
    Call(actor,"AddInitialInventory",{{Value::Text(Kind::Object,"Engine.Inventory"),{}},{Value::Integer(0),{}}});
    Require(ReadPortableActorScriptProperty(actor,"InitialInventory",static_cast<std::uint32_t>(second)).fields.at("count").integer==1,
        "Original optional-zero Count behavior was not executed");
    Require(LoadPortableRuntimeState(savedV4.string()),"Original struct members could not restore from v4");
    SameInventoryProperties(actor,negative,"v4 InitialInventory restore changed slot/class/negative Count");
    unchanged(v4,"v4 struct canonical roundtrip");
    Call(actor,"Disable",{Name("Tick")});
    Require(SavePortableRuntimeState(savedV5.string()),"InventoryItem and retained raw-state disabled sets could not compose");
    const auto v5=CheckpointBytes(savedV5);Require(Word(v5,4u)==5u,"Raw state + InventoryItem composition lost v5 envelope");
    auto composed=QuestVr::DecodeScriptSavedState(ScriptBlob(v5));
    Require(findActor(composed).state && !findActor(composed).state->frame && !findActor(composed).clock &&
        QuestVr::Vm::Equal(findItem(composed).value,negative[slot]),
        "InventoryItem composition replaced raw-state metadata or fabricated a native clock");
    Call(actor,"Enable",{Name("TICK")});
    Require(LoadPortableRuntimeState(savedV5.string()) &&
        ReadPortableActorDispatchContext(actor).disabledNames.count("tick")==1u,
        "v5 composition did not restore its raw-state disabled-event overlay");
    SameInventoryProperties(actor,negative,"v5 struct/state composition changed InventoryItem");
    unchanged(v5,"v5 struct/state canonical roundtrip");
    std::size_t rejections{};
    for(const auto& baseline:{v4,v5}) {
        const auto baselinePath=Word(baseline,4u)==4u ? savedV4 : savedV5;
        Require(LoadPortableRuntimeState(baselinePath.string()),"Could not reset InventoryItem schema fixture");
        const auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(baseline));
        const auto rejectBlob=[&](const std::vector<std::uint8_t>& blob,const std::string& description) {
            Require(QuestVr::WriteDurableSaveFile(generated.string(),ReplaceScriptBlob(baseline,blob)) &&
                !ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()),
                description+" InventoryItem was accepted");
            SameInventoryProperties(actor,negative,description+" rejection changed live fixed structs");
            unchanged(baseline,description+" rejection");++rejections;
        };
        const auto reject=[&](const std::function<void(Value&)>& mutate,const std::string& description) {
            auto invalid=state;mutate(findItem(invalid).value);
            rejectBlob(QuestVr::EncodeScriptSavedState(invalid),description);
        };
        reject([](auto& value) {value.fields.erase("count");},"Missing Count field");
        reject([](auto& value) {value.fields.erase("inventory");},"Missing Inventory field");
        reject([](auto& value) {value.fields["extra"]=Value::Integer(0);},"Extra authored struct field");
        auto collision=state;auto& collisionFields=findItem(collision).value.fields;
        collisionFields["Inventory"]=collisionFields.at("inventory");
        bool encodeRejected{};
        try {QuestVr::EncodeScriptSavedState(collision);} catch(const std::runtime_error&) {encodeRejected=true;}
        Require(encodeRejected,"Codec writer accepted canonical case-colliding struct fields");
        // Encode a unique same-length non-colliding marker, then patch its
        // precisely identified field-name bytes to exercise read-side guards.
        collisionFields.erase("Inventory");
        const std::string marker="InventorZ";
        collisionFields[marker]=collisionFields.at("inventory");
        auto collisionBlob=QuestVr::EncodeScriptSavedState(collision);
        std::vector<std::uint8_t> prefix(4u);
        PutWord(prefix,0u,static_cast<std::uint32_t>(marker.size()));
        prefix.insert(prefix.end(),marker.begin(),marker.end());prefix.push_back(6u); // Stable codec Object tag.
        const auto found=std::search(collisionBlob.begin(),collisionBlob.end(),prefix.begin(),prefix.end());
        Require(found!=collisionBlob.end() && std::search(found+1,collisionBlob.end(),prefix.begin(),prefix.end())==collisionBlob.end(),
            "Generated colliding struct field wire identity is not unique");
        collisionBlob[static_cast<std::size_t>(found-collisionBlob.begin())+4u+marker.size()-1u]='y';
        rejectBlob(collisionBlob,"Case-colliding struct field");
        reject([](auto& value) {value.fields["count"]=Value::Bool(true);},"Incorrect Count value kind");
        reject([](auto& value) {value.fields["inventory"]=Value::Text(Kind::String,"DeusEx.WeaponPistol");},"Incorrect Inventory value kind");
        reject([](auto& value) {value.fields["inventory"]=Value::Text(Kind::Object,"DeusEx.NoSuchInventoryClass");},"Missing class identity");
        reject([](auto& value) {value.fields["inventory"]=Value::Text(Kind::Object,"Engine.Actor");},"Class outside Engine.Inventory ancestry");
        reject([&](auto& value) {value.fields["inventory"]=Value::Text(Kind::Object,actor);},"Instance used as inventory class");
        auto mixed=state;auto& fields=findItem(mixed).value.fields;
        const auto inventory=fields.at("inventory"),count=fields.at("count");fields.clear();
        fields.emplace("Inventory",inventory);fields.emplace("COUNT",count);
        Require(QuestVr::WriteDurableSaveFile(generated.string(),ReplaceScriptBlob(baseline,
            QuestVr::EncodeScriptSavedState(mixed))) && ValidatePortableRuntimeState(generated.string()) &&
            LoadPortableRuntimeState(generated.string()),"Valid mixed-case authored member names were rejected");
        SameInventoryProperties(actor,negative,"Mixed-case restored struct fields were not normalized");
        unchanged(baseline,"Mixed-case canonical restore");
    }
    // Only the fixed InitialInventory input is generated. Actor creation,
    // GiveTo/Pawn.AddInventory, ammo linkage and base/state changes must all
    // come from the unchanged original InitializeInventory bytecode.
    auto initialized=original;auto& properties=findActor(initialized).properties;
    properties.erase(std::remove_if(properties.begin(),properties.end(),[](const auto& property) {
        return property.key=="DeusEx.ScriptedPawn.InitialInventory";
    }),properties.end());
    InventorySlots positive;for(auto& value:positive) value=InventoryItem();positive[0]=InventoryItem(itemClass.text,1);
    for(std::uint32_t i=0;i<positive.size();++i)
        properties.push_back({"DeusEx.ScriptedPawn.InitialInventory","InitialInventory",i,positive[i]});
    const auto initializedBytes=ReplaceScriptBlob(v4,QuestVr::EncodeScriptSavedState(initialized));
    Require(QuestVr::WriteDurableSaveFile(generated.string(),initializedBytes) && LoadPortableRuntimeState(generated.string()),
        "Positive original Inventory class/count initialization fixture could not load");
    SameInventoryProperties(actor,positive,"Initialization fixture did not retain all8 independently typed slots");
    const auto inventoryBefore=ReadPortableActorScriptProperty(actor,"Inventory");
    Require(inventoryBefore.kind==Kind::Object && inventoryBefore.text.empty(),
        "Positive original inventory creation control requires the selected pawn's actual empty linked inventory");
    const auto actorsBeforeInitialization=GetPortableRuntimeMapActors(true).size();
    const auto gcBeforeInitialization=GC::GetStats();
    const auto revisionBeforeInitialization=GetPortableRuntimeWorldRevision();
    const auto ammoClass=ReadPortableClassDefault(itemClass.text,"AmmoName");
    Require(ammoClass.kind==Kind::Object && !ammoClass.text.empty() && ammoClass.text!="DeusEx.AmmoNone",
        "Actual original WeaponPistol no longer supplies its required ammunition class");
    const auto initialize=ExecutePortableActorFunction(actor,"InitializeInventory");
    Require(initialize.passed() && initialize.committed && initialize.function=="DeusEx.ScriptedPawn.InitializeInventory" &&
        initialize.offset==770u && initialize.opcode==0x0bu && initialize.value.kind==Kind::Nothing &&
        initialize.instructions!=0u && initialize.writes!=0u,
        "Original InitializeInventory did not execute through its actual final Return: "+initialize.error+
        " at "+initialize.function+':'+std::to_string(initialize.offset));
    Require(GetPortableRuntimeMapActors(true).size()==actorsBeforeInitialization+2u &&
        GetPortableRuntimeWorldRevision()==revisionBeforeInitialization+1u &&
        GC::GetStats().numObjects==gcBeforeInitialization.numObjects+2u,
        "Original Count=1 inventory initialization did not publish/root exactly one weapon and one ammo in one transaction");
    SameInventoryProperties(actor,positive,"Original InitializeInventory changed its fixed class/count inputs");
    const auto inventorySaved=directory/"original-initialized-inventory-v8.sav";
    Require(SavePortableRuntimeState(inventorySaved.string()),"Original weapon/ammo inventory graph could not save");
    const auto inventoryBytes=CheckpointBytes(inventorySaved);
    const auto inventoryState=QuestVr::DecodeScriptSavedState(ScriptBlob(inventoryBytes));
    Require(Word(inventoryBytes,4u)==12u && ScriptBlob(inventoryBytes).at(6u)==9u && inventoryState.births.size()==2u,
        "Original initialized weapon/ammo inventory omitted its codec9/v12 birth graph");
    const auto weaponBirth=std::find_if(inventoryState.births.begin(),inventoryState.births.end(),[&](const auto& birth) {
        return birth.classPath==itemClass.text;
    });
    const auto ammoBirth=std::find_if(inventoryState.births.begin(),inventoryState.births.end(),[&](const auto& birth) {
        return birth.classPath==ammoClass.text;
    });
    Require(weaponBirth!=inventoryState.births.end() && ammoBirth!=inventoryState.births.end() && weaponBirth->path!=ammoBirth->path,
        "Original inventory creation substituted unrelated or placeholder born classes");
    const auto weapon=weaponBirth->path,ammo=ammoBirth->path;
    const auto verifyInventoryGraph=[&]() {
        const auto equal=[&](const std::string& target,const std::string& property,const Value& expected) {
            Require(QuestVr::Vm::Equal(ReadPortableActorScriptProperty(target,property),expected),
                "Original initialized inventory graph differs at "+target+'.'+property);
        };
        equal(actor,"Inventory",Value::Text(Kind::Object,ammo));
        equal(ammo,"Inventory",Value::Text(Kind::Object,weapon));
        equal(weapon,"Inventory",inventoryBefore);
        equal(weapon,"AmmoType",Value::Text(Kind::Object,ammo));
        equal(weapon,"AmmoName",ammoClass);
        equal(ammo,"AmmoAmount",ReadPortableClassDefault(ammoClass.text,"AmmoAmount"));
        equal(weapon,"PickupAmmoCount",ReadPortableClassDefault(itemClass.text,"PickupAmmoCount"));
        equal(weapon,"ClipCount",ReadPortableClassDefault(itemClass.text,"ClipCount"));
        for(const auto& born:{weapon,ammo}) {
            equal(born,"Owner",Value::Text(Kind::Object,actor));
            equal(born,"Instigator",Value::Text(Kind::Object,actor));
            equal(born,"Base",Value::Text(Kind::Object,actor));
            equal(born,"InitialState",Value::Text(Kind::Name,"Idle2"));
            equal(born,"Physics",Value::Byte(0u));
            for(const auto* flag:{"bHidden","bOnlyOwnerSee","bCarriedItem"}) equal(born,flag,Value::Bool(true));
            for(const auto* flag:{"bCollideActors","bBlockActors","bBlockPlayers"}) equal(born,flag,Value::Bool(false));
            Require(ReadPortableActorDispatchContext(born).stateName=="Idle2",
                "Original GiveTo did not execute its real Idle2 state transition");
        }
        Require(Snapshot(weapon).meshPath==ReadPortableClassDefault(itemClass.text,"PlayerViewMesh").text &&
            Snapshot(weapon).hidden && Snapshot(ammo).hidden,
            "Original BecomeItem did not select the real PlayerViewMesh and hidden carried-item snapshots");
    };
    verifyInventoryGraph();
    const auto pawnRecord=std::find_if(inventoryState.objects.begin(),inventoryState.objects.end(),[&](const auto& object) {
        return object.path==actor;
    });
    Require(pawnRecord!=inventoryState.objects.end() && pawnRecord->lifecycle &&
        pawnRecord->lifecycle->children==std::vector<std::string>{weapon,ammo} &&
        pawnRecord->lifecycle->basedActors==std::vector<std::string>{weapon,ammo},
        "Original inventory owner/base operations did not retain their actual ordered native reverse lists");
    Require(ValidatePortableRuntimeState(inventorySaved.string()),"Original initialized inventory v8 graph failed read-only validation");
    unchanged(inventoryBytes,"Original initialized inventory read-only validation");
    GC::Collect();verifyInventoryGraph();unchanged(inventoryBytes,"Original initialized inventory GC rooting");
    Require(LoadPortableRuntimeState(generated.string()) &&
        GetPortableRuntimeMapActors(true).size()==actorsBeforeInitialization &&
        GC::GetStats().numObjects==gcBeforeInitialization.numObjects,
        "Pre-initialization restore retained retired weapon/ammo UObject allocations");
    unchanged(initializedBytes,"Original initialized inventory removal");
    Require(ValidatePortableRuntimeState(inventorySaved.string()) && LoadPortableRuntimeState(inventorySaved.string()),
        "Original unknown weapon/ammo inventory graph could not cold-restore");
    verifyInventoryGraph();unchanged(inventoryBytes,"Original initialized inventory cold v8 restore");
    // StartUp must begin from the same input rather than a graph that was
    // already initialized. This bounded original StartUp/Standing slice is
    // not automatic campaign startup or a complete AI scheduler.
    Require(LoadPortableRuntimeState(generated.string()),"Could not reset actual StartUp inventory fixture");
    unchanged(initializedBytes,"Original StartUp pre-initialization baseline");
    Call(ReadPortableActorScriptProperty(actor,"Level").text,"InitEventManager");
    Call(actor,"SetInitialState");
    Require(SavePortableRuntimeState(inspect.string()),"Actual StartUp + inventory continuation could not checkpoint");
    const auto startup=CheckpointBytes(inspect);
    const auto preStartupActors=GetPortableRuntimeMapActors(true).size();
    const auto slice=ResumePortableActorState(actor);
    std::cout<<"ORIGINAL STARTUP NEXT DEPENDENCY status="<<static_cast<int>(slice.status)<<
        " committed="<<slice.committed<<" error="<<slice.error<<" at "<<slice.function<<':'<<slice.offset<<
        " opcode="<<static_cast<unsigned>(slice.opcode)<<'\n';
    Require(slice.passed() && slice.status==Status::Waiting && slice.committed &&
        slice.function=="DeusEx.ScriptedPawn.StartUp" && slice.offset==6u && slice.opcode==0x61u &&
        ReadPortableActorLatentTimeLeft(actor).has_value() && SavePortableRuntimeState(inspect.string()),
        "Actual StartUp did not commit its original FRand/Sleep statement: "+slice.error);
    const auto waiting=CheckpointBytes(inspect);
    Require(Word(waiting,4)==12u && ScriptBlob(waiting).at(6)==9u && waiting!=startup,
        "Original StartUp wait did not capture codec9/envelope12");
    const auto startupActors=GetPortableRuntimeMapActors(true).size();
    Require(startupActors==preStartupActors+2u,"Actual StartUp did not commit its original weapon/ammo inventory before Sleep");
    verifyInventoryGraph();
    const auto wake=AdvancePortableActorState(actor,1.0f);
    std::cout<<"ORIGINAL STARTUP AFTER SLEEP status="<<static_cast<int>(wake.status)<<
        " committed="<<wake.committed<<" error="<<wake.error<<" at "<<wake.function<<':'<<wake.offset<<
        " opcode="<<static_cast<unsigned>(wake.opcode)<<'\n';
    const auto standingTimer=VerifyOriginalStandingSleep(actor,wake,"Actual inventory StartUp wake");
    Require(GetPortableRuntimeMapActors(true).size()==startupActors && SavePortableRuntimeState(inspect.string()),
        "Original inventory StartUp/Standing slice changed its exact two committed births or could not save");
    verifyInventoryGraph();const auto standing=CheckpointBytes(inspect);
    Require(Word(standing,4)==12u && ScriptBlob(standing).at(6)==9u && standing!=waiting &&
        ValidatePortableRuntimeState(inspect.string()),"Original Standing continuation did not retain shared codec9/envelope12");
    unchanged(standing,"Actual inventory Standing read-only preflight");
    Require(LoadPortableRuntimeState(inspect.string()),"Actual inventory Standing warm continuation did not restore");
    verifyInventoryGraph();unchanged(standing,"Actual inventory Standing warm restore");
    const auto noTime=ResumePortableActorState(actor);
    Require(noTime.passed() && noTime.status==Status::Waiting && noTime.instructions==0u,
        "Actual inventory Standing no-time continuation polled Sleep or ran its next statement");
    unchanged(standing,"Actual inventory Standing no-time continuation");
    const auto startupRevision=GetPortableRuntimeWorldRevision();const auto startupGc=GC::GetStats();
    QuestVr::Vm::Limits noWakeInstructions;noWakeInstructions.instructions=0u;
    const auto refused=AdvancePortableActorState(actor,standingTimer,noWakeInstructions);
    Require(refused.status==Status::Budget && !refused.committed && !refused.error.empty(),
        "Original Standing forced-budget wake silently committed its native release");
    std::cout<<"ORIGINAL STARTUP ROLLBACK counts="<<startupActors<<'/'<<GetPortableRuntimeMapActors(true).size()<<
        " revision="<<startupRevision<<'/'<<GetPortableRuntimeWorldRevision()<<" objects="<<startupGc.numObjects<<'/'<<GC::GetStats().numObjects<<
        " memory="<<startupGc.memoryUsage<<'/'<<GC::GetStats().memoryUsage<<'\n';
    Require(GetPortableRuntimeMapActors(true).size()==startupActors &&
        GetPortableRuntimeWorldRevision()==startupRevision &&
        GC::GetStats().numObjects==startupGc.numObjects &&
        GC::GetStats().memoryUsage==startupGc.memoryUsage,
        "Rejected actual Standing wake changed inventory births, roots or world publication");
    verifyInventoryGraph();unchanged(standing,"Actual inventory Standing forced-budget wake rollback");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Legacy restore retained InventoryItem overlay or composed state");
    SameInventoryProperties(actor,originalAuthored,"Legacy restore failed to recover all8 authored/inherited inventory slots");
    unchanged(originalLegacyBytes,"Legacy inventory/state reset");
    std::cout<<"ORIGINAL INVENTORY MEMBER writes/negative Count/optional zero/default class constraint, v4/v5 composition; rejections="<<
        rejections<<"; actual InitializeInventory Return PC770, original owned WeaponPistol/ammo/GiveTo/base/Idle2/native links/v8 cold GC graph; StartUp/Standing Sleep304 saved continuation and forced-budget rollback\n";
}

void VerifyOriginalSleep(const std::filesystem::path& root,const std::vector<PortablePackageTables>& tables,
    const std::string& actor,const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    const auto saved=directory/"original-sleep-v11.sav", inspect=directory/"sleep-unchanged.sav", generated=directory/"generated-sleep.sav";
    const auto unchanged=[&](const std::vector<std::uint8_t>& expected,const std::string& context) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==expected,context+" changed complete runtime state");
    };
    Require(LoadPortableRuntimeState(legacy.string()),"Could not reset original Sleep fixture");
    const auto legacyBytes=CheckpointBytes(legacy);
    const auto unframed=ExecutePortableActorFunction(actor,"Sleep",{Number(1.0f)});
    Require(!unframed.passed() && !ReadPortableActorLatentTimeLeft(actor),"Sleep fabricated an executable frame from dormant authored metadata");
    unchanged(legacyBytes,"Unframed original Sleep refusal");
    Call(actor,"SetInitialState");Call(actor,"Sleep",{Number(1.5f)});
    Require(ReadPortableActorLatentTimeLeft(actor)==1.5f && SavePortableRuntimeState(saved.string()),"Original Sleep declaration failed to retain signed seconds");
    auto bytes=CheckpointBytes(saved);auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(bytes));
    Require(Word(bytes,4)==12u && ScriptBlob(bytes).at(6)==9u && !state.randomSeed,"Sleep-only original fixture invented an RNG stream or wrong envelope");
    auto& record=*std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& object){return object.path==actor;});
    const auto layout=ReadPortableRuntimeStateProgram(record.state->frame->codePath);
    const auto descriptor=ReadPortableRuntimeAuthoredStateDescriptor(record.state->frame->codePath);
    const auto stop=std::find_if(layout.statementOffsets.begin(),layout.statementOffsets.end(),[&](const auto offset){
        return offset<descriptor.bytecode.size() && descriptor.bytecode[offset]==0x08u;
    });
    Require(stop!=layout.statementOffsets.end(),"Original StartUp has no Stop boundary for isolated native-poll control");
    record.state->frame->statementIndex=static_cast<std::uint32_t>(stop-layout.statementOffsets.begin());
    record.properties.push_back({"Engine.Actor.Role","Role",0u,Value::Byte(4u)});
    const auto publish=[&](const QuestVr::ScriptSavedState& value,std::uint32_t version=0u) {
        const auto blob=QuestVr::EncodeScriptSavedState(value);
        auto composed=ReplaceScriptBlob(bytes,blob);PutWord(composed,4u,version ? version : 3u+blob.at(6u));
        Require(QuestVr::WriteDurableSaveFile(generated.string(),composed),"Could not write generated original Sleep checkpoint");
        return composed;
    };
    const auto baseline=publish(state);Require(LoadPortableRuntimeState(generated.string()),"Original waiting Stop-boundary fixture did not restore");
    const auto waiting=ResumePortableActorState(actor);
    Require(waiting.passed() && waiting.status==Status::Waiting && waiting.instructions==0u,"Existing Sleep ran script during a no-time ResumeState");
    unchanged(baseline,"Existing original Sleep no-time resume");
    const auto equal=AdvancePortableActorState(actor,1.0f);
    Require(equal.passed() && equal.status==Status::Waiting && ReadPortableActorLatentTimeLeft(actor)==0.5f,
        "Original half-frame equality did not preserve Sleep");
    const auto wake=AdvancePortableActorState(actor,1.0f);
    Require(wake.passed() && wake.status==Status::Stopped && ReadPortableActorLatentTimeLeft(actor)==-0.5f,
        "Original signed Sleep wake did not execute its actual Stop boundary");
    Call(actor,"StopWaiting");Require(ReadPortableActorLatentTimeLeft(actor)==-0.5f,"StopWaiting overwrote a counter when the actor was not sleeping");
    Call(actor,"Sleep",{Number(0.0f)});Call(actor,"StopWaiting");
    const auto stillStopped=ResumePortableActorState(actor);
    Require(ReadPortableActorLatentTimeLeft(actor)==-1.0f && stillStopped.passed() &&
        stillStopped.status==Status::Stopped && stillStopped.instructions==0u &&
        !ReadPortableActorStateObject(actor)->frame->control->codePresent,
        "Original Sleep/StopWaiting resurrected Code after the authored Stop or lost the negative timer");
    // Native Sleep does not resurrect Code or rewind PC; restore the known authored Stop before
    // exercising the zero-elapsed release of StopWaiting's negative counter.
    auto negative=state;negative.objects[0].latentTimeLeft=-1.0f;publish(negative);
    Require(LoadPortableRuntimeState(generated.string()) && AdvancePortableActorState(actor,0.0f).status==Status::Stopped &&
        ReadPortableActorLatentTimeLeft(actor)==-1.0f,"Negative Sleep counter did not wake with zero elapsed time");
    for (std::size_t mode=0u;mode<2u;++mode) {
        auto ineligible=state;auto& actorRecord=ineligible.objects[0];
        if (mode==0u) actorRecord.properties.back().value=Value::Byte(3u);
        else actorRecord.properties.push_back({"Engine.Actor.bDeleteMe","bDeleteMe",0u,Value::Bool(true)});
        const auto gated=publish(ineligible);
        Require(LoadPortableRuntimeState(generated.string()) && AdvancePortableActorState(actor,1.0f).passed() &&
            ReadPortableActorLatentTimeLeft(actor)==1.5f,"Original ProcessState role/delete gate consumed a native wait");
        unchanged(gated,"Original ProcessState eligibility gate");
    }
    publish(state);Require(LoadPortableRuntimeState(generated.string()),"Could not reset original Sleep budget fixture");
    QuestVr::Vm::Limits limit;limit.instructions=0u;
    const auto failed=AdvancePortableActorState(actor,2.0f,limit);
    Require(failed.status==Status::Budget && !failed.committed && ReadPortableActorLatentTimeLeft(actor)==1.5f,
        "Original poll-before-budget failure did not roll back its native timer");unchanged(baseline,"Original Sleep instruction budget rollback");
    for (const auto bad : {-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        Require(!AdvancePortableActorState(actor,bad).passed(),"Invalid original state elapsed time accepted");unchanged(baseline,"Invalid original elapsed");
    }
    Call(actor,"GotoState",{Name("None")});
    Require(ReadPortableActorLatentTimeLeft(actor)==1.5f && ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::Continue &&
        AdvancePortableActorState(actor,2.0f).passed() && ReadPortableActorLatentTimeLeft(actor)==1.5f,
        "GotoState(None) failed to cancel Sleep, discarded the actor-owned timer, or polled null code");
    Call(actor,"StopWaiting");Require(ReadPortableActorLatentTimeLeft(actor)==1.5f,"StopWaiting changed a null-code actor's retained native timer");
    auto absent=state;absent.objects[0].state.reset();const auto absentBytes=publish(absent);
    Require(LoadPortableRuntimeState(generated.string()) && !ReadPortableActorStateObject(actor) && ReadPortableActorLatentTimeLeft(actor)==1.5f,
        "Actor native timer could not restore independently of a state frame");unchanged(absentBytes,"Frameless native timer restore");
    auto missing=state;missing.objects[0].latentTimeLeft.reset();publish(missing);
    Require(!ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()),"Sleeping original actor restored without native timer evidence");
    unchanged(absentBytes,"Missing native Sleep timer refusal");
    auto combined=state;combined.randomSeed=0xfedcba98u;
    const auto combinedBytes=publish(combined);
    Require(LoadPortableRuntimeState(generated.string()) && SavePortableRuntimeState(saved.string()),"Could not stage original Sleep/RNG cold checkpoint");
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorLatentTimeLeft(actor),"Legacy runtime save retained an actor timer");
    ShutdownPortableRuntime();Require(InitializePortableRuntime(tables).passed,"Cold Sleep runtime initialization failed");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed &&
        ValidatePortableRuntimeState(saved.string()) && !ReadPortableActorLatentTimeLeft(actor),"Readonly cold Sleep preflight published native state");
    Require(LoadPortableRuntimeState(saved.string()) && ReadPortableActorLatentTimeLeft(actor)==1.5f,"Cold Sleep restore lost its native actor timer");
    unchanged(combinedBytes,"Cold original Sleep/RNG restore");
    Require(AdvancePortableActorState(actor,2.0f).status==Status::Stopped,"Cold restored original Sleep could not resume actual authored code");
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorLatentTimeLeft(actor),"Cold legacy reset retained native Sleep state");
    unchanged(legacyBytes,"Cold legacy Sleep reset");
    std::cout<<"ORIGINAL SLEEP native256/StopWaiting0, strict half-frame signed timer, nested VM coverage, role/delete/null-code gates, rollback, codec9/envelope12, cold restore and legacy reset passed\n";
}

void VerifyOriginalLanding(const std::filesystem::path& root,const std::vector<PortablePackageTables>& tables,
    const std::string& actor,const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    const auto saved=directory/"original-landing-v11.sav",inspect=directory/"landing-unchanged.sav",generated=directory/"generated-landing.sav";
    const auto unchanged=[&](const std::vector<std::uint8_t>& expected,const std::string& context) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==expected,context+" changed complete runtime state");
    };
    Require(LoadPortableRuntimeState(legacy.string()),"Could not reset original landing fixture");
    const auto legacyBytes=CheckpointBytes(legacy);
    const auto unchangedToggle=[&](const std::string& receiver,const std::string& event,
        const std::vector<std::uint8_t>& expected,const std::string& context) {
        const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
        for (const auto* operation:{"Disable","Enable"}) {
            Call(receiver,operation,{Name(event)});
            Require(GetPortableRuntimeWorldRevision()==revision && GC::GetStats().numObjects==gc.numObjects &&
                GC::GetStats().memoryUsage==gc.memoryUsage,context+" mutated native state on "+operation);
            unchanged(expected,context+" "+operation);
        }
    };
    unchangedToggle(actor,"LongFall",legacyBytes,"Frameless original nonprobe LongFall");
    Require(!ReadPortableActorStateObject(actor),"Nonprobe Enable/Disable fabricated a portable actor frame");
    Call(actor,"SetInitialState");
    Require(SavePortableRuntimeState(inspect.string()),"Could not capture original landing frame fixture");
    const auto prefix=CheckpointBytes(inspect);
    auto state=QuestVr::DecodeScriptSavedState(ScriptBlob(prefix));
    const auto recordFor=[&](QuestVr::ScriptSavedState& value)->QuestVr::ScriptSavedObject& {
        const auto found=std::find_if(value.objects.begin(),value.objects.end(),[&](const auto& object){return object.path==actor;});
        Require(found!=value.objects.end(),"Original landing actor record disappeared");return *found;
    };
    const auto property=[](QuestVr::ScriptSavedObject& record,const std::string& key,const std::string& name,const Value& value) {
        const auto found=std::find_if(record.properties.begin(),record.properties.end(),[&](const auto& entry){return entry.name==name && entry.index==0u;});
        if (found==record.properties.end()) record.properties.push_back({key,name,0u,value});else found->value=value;
    };
    auto& record=recordFor(state);
    Require(record.state && record.state->frame,"Original landing has no established portable frame");
    const auto layout=ReadPortableRuntimeStateProgram(record.state->frame->codePath);
    const auto descriptor=ReadPortableRuntimeAuthoredStateDescriptor(record.state->frame->codePath);
    const auto stop=std::find_if(layout.statementOffsets.begin(),layout.statementOffsets.end(),[&](const auto offset){
        return offset<descriptor.bytecode.size() && descriptor.bytecode[offset]==0x08u;
    });
    Require(stop!=layout.statementOffsets.end(),"Original StartUp lacks isolated landing Stop boundary");
    record.state->frame->statementIndex=static_cast<std::uint32_t>(stop-layout.statementOffsets.begin());
    property(record,"Engine.Actor.Role","Role",Value::Byte(4u));
    property(record,"Engine.Actor.Physics","Physics",Value::Byte(2u));
    const auto publish=[&](const QuestVr::ScriptSavedState& value,std::uint32_t version=0u) {
        const auto blob=QuestVr::EncodeScriptSavedState(value);
        auto composed=ReplaceScriptBlob(prefix,blob);PutWord(composed,4u,version ? version : 3u+blob.at(6u));
        Require(QuestVr::WriteDurableSaveFile(generated.string(),composed),"Could not write original landing checkpoint");return composed;
    };
    publish(state);Require(LoadPortableRuntimeState(generated.string()),"Original falling frame fixture did not restore");
    const auto command=Call(actor,"Engine.Pawn.WaitForLanding");
    Require(command.function=="Engine.Pawn.WaitForLanding" && command.value.kind==Kind::Nothing &&
        ReadPortableActorLatentTimeLeft(actor)==2.5f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::WaitForLanding &&
        SavePortableRuntimeState(saved.string()),"Original native527 failed its falling wait or fixed native timer");
    const auto baseline=CheckpointBytes(saved);state=QuestVr::DecodeScriptSavedState(ScriptBlob(baseline));
    Require(Word(baseline,4)==12u && ScriptBlob(baseline).at(6)==9u && !state.randomSeed,
        "Landing wait invented a new schema/RNG or failed shared codec9/envelope12");
    unchangedToggle(actor,"LongFall",baseline,"Waiting original nonprobe LongFall");
    Call(actor,"StopWaiting");
    Require(ReadPortableActorLatentTimeLeft(actor)==2.5f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::WaitForLanding,
        "StopWaiting changed the counter or latent action of a live landing wait");
    unchanged(baseline,"StopWaiting on actual original landing wait");
    const auto waiting=ResumePortableActorState(actor);
    Require(waiting.passed() && waiting.status==Status::Waiting && waiting.instructions==0u,
        "No-time ResumeState polled an original landing wait or ran its continuation");
    unchanged(baseline,"Original landing no-time continuation");
    Require(ValidatePortableRuntimeState(saved.string()),"Warm landing read-only preflight failed");
    unchanged(baseline,"Warm original landing preflight");
    Require(AdvancePortableActorState(actor,0.0f).status==Status::Waiting && ReadPortableActorLatentTimeLeft(actor)==2.5f,
        "Zero elapsed consumed a positive original landing timer");unchanged(baseline,"Original landing zero elapsed");
    const auto zero=AdvancePortableActorState(actor,2.5f);
    Require(zero.passed() && zero.status==Status::Waiting && ReadPortableActorLatentTimeLeft(actor)==0.0f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::WaitForLanding,
        "Original landing countdown at exact zero invoked timeout or released the wait");
    Require(SavePortableRuntimeState(inspect.string()),"Could not capture zero landing countdown");
    const auto zeroBytes=CheckpointBytes(inspect);
    Require(AdvancePortableActorState(actor,0.0f).status==Status::Waiting && ReadPortableActorLatentTimeLeft(actor)==0.0f,
        "Original zero landing timer timed out on zero elapsed");unchanged(zeroBytes,"Original landing strict-zero continuation");
    const auto levelValue=ReadPortableActorScriptProperty(actor,"Level");
    Require(levelValue.kind==Kind::Object && !levelValue.text.empty() &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(levelValue.text,"bBegunPlay")),
        "Original landing timeout control lacks an authored pre-begun-play Level");
    const auto gatedTimeout=AdvancePortableActorState(actor,0.5f);
    Require(gatedTimeout.passed() && gatedTimeout.committed && gatedTimeout.status==Status::Waiting &&
        gatedTimeout.instructions==0u && ReadPortableActorLatentTimeLeft(actor)==-0.5f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::WaitForLanding,
        "Pre-begun-play LongFall event gate failed to commit signed timeout while retaining the wait");
    Require(SavePortableRuntimeState(inspect.string()),"Could not capture gated negative landing timer");
    const auto negativeBytes=CheckpointBytes(inspect);
    Require(AdvancePortableActorState(actor,0.0f).status==Status::Waiting && ReadPortableActorLatentTimeLeft(actor)==-0.5f,
        "Gated negative landing timer was reset or cleared on repeat poll");
    unchanged(negativeBytes,"Repeated original pre-begun-play landing timeout");
    unchangedToggle(levelValue.text,"LongFall",negativeBytes,"Actual LevelInfo nonprobe LongFall");
    if (!ReadPortableActorSerializedStack(levelValue.text)) {
        Require(!ReadPortableActorStateObject(levelValue.text),"Authored frameless LevelInfo unexpectedly has portable state");
        unchangedToggle(levelValue.text,"Tick",negativeBytes,"Actual frameless LevelInfo fixed Tick probe");
        Require(!ReadPortableActorStateObject(levelValue.text),"Fixed probe Enable/Disable fabricated a frameless LevelInfo stack");
    }
    // Explicit Level fixture for the real dynamic callback; this does not
    // pretend that automatic campaign startup or every callback native exists.
    auto begun=state;const auto levelSnapshot=Snapshot(levelValue.text);
    begun.objects.push_back({levelSnapshot.objectPath,levelSnapshot.classPath,
        {{"Engine.LevelInfo.bBegunPlay","bBegunPlay",0u,Value::Bool(true)}},{}});
    publish(begun);Require(LoadPortableRuntimeState(generated.string()),"Actual LongFall begun-play Level fixture did not restore");
    Require(ReadPortableActorDispatchContext(actor).disabledNames.empty() && SavePortableRuntimeState(inspect.string()),
        "Actual LongFall fixture invented legacy state-keyed disabled names");
    const auto legacyDisabledBytes=CheckpointBytes(inspect);
    Call(actor,"Disable",{Name("LongFall")});unchanged(legacyDisabledBytes,"Disable original nonprobe LongFall is unchanged");
    const auto callback=AdvancePortableActorState(actor,3.0f);
    std::cout<<"ORIGINAL LANDING LONGFALL status="<<static_cast<int>(callback.status)<<" committed="<<callback.committed<<
        " instructions="<<callback.instructions<<" error="<<callback.error<<" at "<<callback.function<<':'<<callback.offset<<
        " opcode="<<static_cast<unsigned>(callback.opcode)<<'\n';
    Require(callback.passed() && callback.committed && callback.status==Status::Waiting && callback.instructions==286u &&
        callback.error.empty() && callback.function=="DeusEx.ScriptedPawn.FallingState" && callback.offset==20u && callback.opcode==0x61u,
        "Actual LongFall did not reach its exact authored FallingState Sleep boundary");
    const auto frame=ReadPortableActorStateObject(actor);const auto fall=ResolvePortableActorState(actor,"FallingState");
    Require(fall && frame && frame->frame && frame->frame->codePath==*fall &&
        frame->frame->latent==QuestVr::StateLatent::Sleep && ReadPortableActorLatentTimeLeft(actor)==0.7f &&
        Call(actor,"GetStateName").value.text=="FallingState",
        "Actual LongFall failed authored state replacement or overwrote its replacement Sleep timer");
    const auto fallingLayout=ReadPortableRuntimeStateProgram(*fall);
    Require(frame->frame->statementIndex<fallingLayout.statementOffsets.size() &&
        fallingLayout.statementOffsets[frame->frame->statementIndex]==28u && SavePortableRuntimeState(inspect.string()),
        "Actual LongFall replacement did not retain its next authored FallingState statement at offset28");
    const auto replaced=CheckpointBytes(inspect);
    Require(ValidatePortableRuntimeState(inspect.string()) && LoadPortableRuntimeState(inspect.string()),
        "Successful actual LongFall replacement could not persist");unchanged(replaced,"Actual original LongFall replacement roundtrip");
    Require(LoadPortableRuntimeState(saved.string()),"Warm landing checkpoint did not restore");unchanged(baseline,"Warm original landing restore");
    const auto nonPawnRevision=GetPortableRuntimeWorldRevision();const auto nonPawnGc=GC::GetStats();
    const auto nonPawn=ExecutePortableActorFunction(levelValue.text,"Engine.Pawn.WaitForLanding");
    Require(nonPawn.status==Status::Unsupported && !nonPawn.committed &&
        nonPawn.error=="WaitForLanding receiver is not Engine.Pawn" &&
        GetPortableRuntimeWorldRevision()==nonPawnRevision && GC::GetStats().numObjects==nonPawnGc.numObjects &&
        GC::GetStats().memoryUsage==nonPawnGc.memoryUsage,
        "Actual LevelInfo native527 receiver was accepted or leaked state on typed refusal");
    unchanged(baseline,"Original non-Pawn landing native atomic refusal");
    auto typedReceiver=state;QuestVr::ScriptSavedObject levelRecord;
    levelRecord.path=levelSnapshot.objectPath;levelRecord.classPath=levelSnapshot.classPath;
    levelRecord.state=QuestVr::StateObject{};levelRecord.state->hasStack=true;levelRecord.state->frameOverride=true;
    levelRecord.state->frame=QuestVr::StateFrame{};levelRecord.latentTimeLeft=2.5f;
    levelRecord.state->frame->codePath=levelSnapshot.classPath;
    levelRecord.state->frame->control=QuestVr::StateControl{levelSnapshot.classPath,false,~std::uint64_t{0}};
    typedReceiver.objects.push_back(levelRecord);publish(typedReceiver);
    Require(ValidatePortableRuntimeState(generated.string()) && GetPortableRuntimeWorldRevision()==nonPawnRevision,
        "Actual LevelInfo null-code frame/native timer control was not structurally and semantically valid");
    unchanged(baseline,"Original non-Pawn null-code Continue read-only preflight");
    typedReceiver.objects.back().state->frame->latent=QuestVr::StateLatent::WaitForLanding;publish(typedReceiver);
    Require(!ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()) &&
        GetPortableRuntimeWorldRevision()==nonPawnRevision && GC::GetStats().numObjects==nonPawnGc.numObjects &&
        GC::GetStats().memoryUsage==nonPawnGc.memoryUsage,
        "Actual LevelInfo null-code landing wait with native timer bypassed typed save preflight or leaked state");
    unchanged(baseline,"Original non-Pawn null-code landing preflight atomic refusal");
    for (std::size_t mode=0u;mode<2u;++mode) {
        auto missing=state;auto& entry=recordFor(missing);entry.latentTimeLeft.reset();
        if (mode==1u) entry.state->frame->control->codePresent=false;
        publish(missing);const auto revision=GetPortableRuntimeWorldRevision();
        Require(!ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()) &&
            GetPortableRuntimeWorldRevision()==revision,"Landing wait without native timer validated/loaded, including null code");
        unchanged(baseline,"Missing original landing timer atomic refusal");
    }
    for (const auto bad:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        Require(!AdvancePortableActorState(actor,bad).passed(),"Invalid elapsed accepted for original landing wait");
        unchanged(baseline,"Invalid original landing elapsed");
    }
    auto grounded=state;property(recordFor(grounded),"Engine.Actor.Physics","Physics",Value::Byte(0u));
    const auto groundedBytes=publish(grounded);Require(LoadPortableRuntimeState(generated.string()),"Nonfalling landing wait could not restore");
    QuestVr::Vm::Limits noInstructions;noInstructions.instructions=0u;
    const auto failed=AdvancePortableActorState(actor,10.0f,noInstructions);
    Require(failed.status==Status::Budget && !failed.committed && ReadPortableActorLatentTimeLeft(actor)==2.5f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::WaitForLanding,
        "Post-landing instruction refusal retained native release or changed timer");
    unchanged(groundedBytes,"Original landing release/budget rollback");
    const auto done=AdvancePortableActorState(actor,10.0f);
    Require(done.passed() && done.status==Status::Stopped && ReadPortableActorLatentTimeLeft(actor)==2.5f,
        "Physics None failed to release original landing wait without decrementing timer");
    Call(actor,"StopWaiting");Require(ReadPortableActorLatentTimeLeft(actor)==2.5f,
        "StopWaiting changed a stopped original landing actor timer");
    auto stopped=grounded;recordFor(stopped).state->frame->control->codePresent=false;
    recordFor(stopped).state->frame->latent=QuestVr::StateLatent::Continue;recordFor(stopped).latentTimeLeft=-1.0f;
    publish(stopped);Require(LoadPortableRuntimeState(generated.string()),"Stopped native timer fixture did not restore");
    Call(actor,"Engine.Pawn.WaitForLanding");
    Require(ReadPortableActorLatentTimeLeft(actor)==2.5f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::Continue &&
        !ReadPortableActorStateObject(actor)->frame->control->codePresent,
        "Native527 nonfalling call failed to overwrite timer or replaced unrelated latent state");
    auto sleeping=grounded;recordFor(sleeping).state->frame->latent=QuestVr::StateLatent::Sleep;recordFor(sleeping).latentTimeLeft=1.5f;
    publish(sleeping);Require(LoadPortableRuntimeState(generated.string()),"Shared sleeping native timer fixture did not restore");
    Call(actor,"Engine.Pawn.WaitForLanding");
    Require(ReadPortableActorLatentTimeLeft(actor)==2.5f && ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::Sleep,
        "Nonfalling native527 failed to share Sleep's actor counter or replaced its existing latent action");
    auto frameless=state;recordFor(frameless).state.reset();const auto framelessBytes=publish(frameless);
    Require(LoadPortableRuntimeState(generated.string()) && !ReadPortableActorStateObject(actor) && ReadPortableActorLatentTimeLeft(actor)==2.5f,
        "Landing actor-owned timer could not restore without frame");unchanged(framelessBytes,"Frameless landing timer restore");
    const auto noFrame=ExecutePortableActorFunction(actor,"Engine.Pawn.WaitForLanding");
    Require(!noFrame.passed() && !noFrame.committed && !noFrame.error.empty() && !ReadPortableActorStateObject(actor) &&
        ReadPortableActorLatentTimeLeft(actor)==2.5f,"Falling frameless native527 invented a frame or silently committed");
    unchanged(framelessBytes,"Falling frameless native527 atomic refusal");
    auto nativeFrameless=frameless;recordFor(nativeFrameless).latentTimeLeft=-1.0f;
    property(recordFor(nativeFrameless),"Engine.Actor.Physics","Physics",Value::Byte(0u));
    publish(nativeFrameless);Require(LoadPortableRuntimeState(generated.string()),"Nonfalling frameless native527 fixture did not restore");
    Call(actor,"Engine.Pawn.WaitForLanding");
    Require(!ReadPortableActorStateObject(actor) && ReadPortableActorLatentTimeLeft(actor)==2.5f,
        "Nonfalling native527 refused its frameless receiver or invented portable state");
    auto combined=state;combined.randomSeed=0xfedcba98u;
    const auto combinedBytes=publish(combined);
    Require(LoadPortableRuntimeState(generated.string()) && SavePortableRuntimeState(saved.string()),"Could not stage original landing/RNG cold checkpoint");
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorLatentTimeLeft(actor),"Legacy reset retained landing native timer");
    ShutdownPortableRuntime();Require(InitializePortableRuntime(tables).passed,"Cold landing runtime initialization failed");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed &&
        ValidatePortableRuntimeState(saved.string()) && !ReadPortableActorLatentTimeLeft(actor),
        "Cold landing read-only preflight published native timer");
    Require(LoadPortableRuntimeState(saved.string()) && ReadPortableActorLatentTimeLeft(actor)==2.5f &&
        ReadPortableActorStateObject(actor)->frame->latent==QuestVr::StateLatent::WaitForLanding,
        "Cold original landing checkpoint lost native timer/latent action");unchanged(combinedBytes,"Cold original landing/RNG restore");
    const auto cold=AdvancePortableActorState(actor,1.0f);
    Require(cold.passed() && cold.status==Status::Waiting && ReadPortableActorLatentTimeLeft(actor)==1.5f,
        "Cold restored falling landing wait did not continue its native countdown");
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorLatentTimeLeft(actor) && !GetPortableRuntimeScriptStatePresent(),
        "Final legacy reset retained original landing state");unchanged(legacyBytes,"Cold legacy landing reset");
    std::cout<<"ORIGINAL LANDING native527 fixed2.5/falling-only, no-time/strictzero/PhysicsNone release, retained actor timer, "
        "post-release budget rollback, codec9/envelope12 warm/cold/RNG, typed receiver and missing-timer atomic preflight passed\n";
}

void VerifyModernStateControl(const std::filesystem::path& root,const std::vector<PortablePackageTables>& tables,
    const std::string& actor,const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    const auto saved=directory/"generated-modern-control-v12.sav",inspect=directory/"modern-control-unchanged.sav",
        generated=directory/"generated-modern-control-input.sav";
    const auto unchanged=[&](const std::vector<std::uint8_t>& expected,const std::string& message) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==expected,
            message+" changed complete runtime gameplay/script/native payload");
    };
    Require(LoadPortableRuntimeState(legacy.string()),"Could not reset generated modern-control fixture");
    const auto legacyBytes=CheckpointBytes(legacy);
    const auto level=ReadPortableActorScriptProperty(actor,"Level");
    Require(level.kind==Kind::Object && !level.text.empty(),"Original no-frame control lacks an actual bound LevelInfo");
    if (!ReadPortableActorSerializedStack(level.text)) {
        Require(!ReadPortableActorStateObject(level.text),"Original frameless LevelInfo unexpectedly owns a portable frame");
        for (const auto* requested:{"Auto","None","Quest_NoSuchState"}) {
            Call(level.text,"GotoState",{Name(requested),Name("Quest_NoSuchLabel")});
            Require(!ReadPortableActorStateObject(level.text),"Original no-frame GotoState fabricated an execution frame");
            unchanged(legacyBytes,"Actual frameless LevelInfo GotoState is a no-op");
        }
    }
    // Production state entry is now modern. Separately labelled generated
    // controls below vary retained Node/Locals/nullable Code without claiming
    // those sparse combinations arose from automatic campaign startup.
    Call(actor,"SetInitialState"); Call(actor,"Sleep",{Number(1.5f)});
    Require(SavePortableRuntimeState(saved.string()),"Could not capture original modern StartUp frame");
    const auto prefix=CheckpointBytes(saved);
    auto baseline=QuestVr::DecodeScriptSavedState(ScriptBlob(prefix));
    const auto recordFor=[&](QuestVr::ScriptSavedState& graph)->QuestVr::ScriptSavedObject& {
        const auto found=std::find_if(graph.objects.begin(),graph.objects.end(),[&](const auto& value) {return value.path==actor;});
        Require(found!=graph.objects.end() && found->state && found->state->frame,"Generated control lost original actor frame schema");
        return *found;
    };
    Require(Word(prefix,4u)==12u && ScriptBlob(prefix).at(6u)==9u && recordFor(baseline).state->frame->control &&
        recordFor(baseline).state->frame->control->selectedPath=="DeusEx.ScriptedPawn.StartUp" &&
        recordFor(baseline).state->frame->control->codePresent &&
        recordFor(baseline).state->frame->codePath=="DeusEx.ScriptedPawn.StartUp" &&
        recordFor(baseline).state->frame->localsCodePath.empty() && recordFor(baseline).state->frame->locals.empty(),
        "Original SetInitialState omitted modern identity or fabricated null original Locals");
    const auto startup=ReadPortableRuntimeStateProgram("DeusEx.ScriptedPawn.StartUp");
    const auto startupDescriptor=ReadPortableRuntimeAuthoredStateDescriptor("DeusEx.ScriptedPawn.StartUp");
    const auto stop=std::find_if(startup.statementOffsets.begin(),startup.statementOffsets.end(),[&](const auto offset) {
        return offset<startupDescriptor.bytecode.size() && startupDescriptor.bytecode[offset]==0x08u;
    });
    Require(stop!=startup.statementOffsets.end(),"Original StartUp lacks required actual EX_Stop control");
    const auto stopOrdinal=static_cast<std::uint32_t>(stop-startup.statementOffsets.begin());
    const auto property=[](QuestVr::ScriptSavedObject& record,const std::string& key,const std::string& name,Value value) {
        const auto found=std::find_if(record.properties.begin(),record.properties.end(),[&](const auto& item) {return item.key==key && item.index==0u;});
        if (found==record.properties.end()) record.properties.push_back({key,name,0u,std::move(value)});
        else found->value=std::move(value);
    };
    auto& baselineRecord=recordFor(baseline);
    baselineRecord.state->disabled.clear();
    // Explicit retained empty-schema storage owner; production original data
    // has NULL Locals, so this declaration identity is generated independently.
    baselineRecord.state->frame->localsCodePath="DeusEx.ScriptedPawn.StartUp";
    baselineRecord.state->frame->codePath="DeusEx.ScriptedPawn.FallingState";
    baselineRecord.state->frame->statementIndex=std::numeric_limits<std::uint32_t>::max();
    baselineRecord.state->frame->control=QuestVr::StateControl{"DeusEx.ScriptedPawn.Standing",false,0xaaaaaaaa55555555ull};
    property(baselineRecord,"Engine.Actor.Role","Role",Value::Byte(4u));
    const auto publish=[&](const QuestVr::ScriptSavedState& graph,const std::uint32_t version=12u) {
        const auto blob=QuestVr::EncodeScriptSavedState(graph);
        Require(blob.at(6u)==9u,"Generated modern fixture did not explicitly select codec9");
        auto bytes=ReplaceScriptBlob(prefix,blob); PutWord(bytes,4u,version);
        Require(QuestVr::WriteDurableSaveFile(generated.string(),bytes),"Could not write generated modern-control checkpoint");
        return bytes;
    };
    const auto verifyFrame=[&](const QuestVr::ScriptSavedState& expected) {
        auto copy=expected; const auto& entry=recordFor(copy); const auto& want=*entry.state->frame;
        const auto actual=ReadPortableActorStateObject(actor); const auto context=ReadPortableActorDispatchContext(actor);
        Require(actual && actual->hasStack && actual->frameOverride && actual->frame && actual->frame->control && actual->disabled.empty(),
            "Modern restore lost explicit frame/control provenance or invented disabled sets");
        const auto& frame=*actual->frame;
        Require(frame.codePath==want.codePath && frame.localsCodePath==want.localsCodePath && frame.statementIndex==want.statementIndex &&
            frame.latent==want.latent && frame.control->selectedPath==want.control->selectedPath &&
            frame.control->codePresent==want.control->codePresent && frame.control->liveProbeMask==want.control->liveProbeMask &&
            context.framePresent && context.exactSelected && context.codePresent==want.control->codePresent &&
            context.runningPath==want.codePath && context.selectedPath==want.control->selectedPath &&
            context.codePath==context.selectedPath && context.liveProbeMask==want.control->liveProbeMask && context.disabledNames.empty() &&
            ReadPortableActorLatentTimeLeft(actor)==entry.latentTimeLeft,
            "Modern control conflated selected/running/local owner, Code/PC, positive mask or actor timer");
    };
    auto current=publish(baseline);
    Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),
        "Generated null-Code modern control failed typed warm restore");
    verifyFrame(baseline); unchanged(current,"Generated null-Code warm restore");
    Require(Call(actor,"GetStateName").value.text=="Standing" && QuestVr::Vm::ToBool(Call(actor,"IsInState",{Name("Standing")}).value) &&
        !QuestVr::Vm::ToBool(Call(actor,"IsInState",{Name("FallingState")}).value) &&
        !QuestVr::Vm::ToBool(Call(actor,"IsInState",{Name("StartUp")}).value),
        "Modern selected state queries read running/local declaration identities");
    for (const std::uint64_t mask : {0ull,0xffffffffffffffffull,0xaaaaaaaa55555555ull,0x8000000000000000ull}) {
        auto graph=baseline; recordFor(graph).state->frame->control->liveProbeMask=mask;
        current=publish(graph); Require(LoadPortableRuntimeState(generated.string()),"Modern literal positive mask did not restore"); verifyFrame(graph);
        const auto context=ReadPortableActorDispatchContext(actor);
        for (std::uint8_t index=0u;index<64u;++index) Require(QuestVr::ScriptDispatch::IsEnabled(
            QuestVr::ScriptDispatch::ProbeEventName(index),context.classProbeMask,context.codeMasks,context.disabledNames,context.liveProbeMask)==
            ((mask & (std::uint64_t{1}<<index))!=0u),"Modern positive live mask was recomputed, inverted or truncated");
        const auto noTime=ResumePortableActorState(actor),elapsed=AdvancePortableActorState(actor,100.0f);
        Require(noTime.passed() && noTime.status==Status::Stopped && noTime.instructions==0u && elapsed.passed() &&
            elapsed.status==Status::Stopped && elapsed.instructions==0u && ReadPortableActorLatentTimeLeft(actor)==1.5f,
            "Modern null Code parsed stale PC or polled retained Sleep timer");
        unchanged(current,"Modern null-Code no-time/elapsed control");
    }
    current=publish(baseline); Require(LoadPortableRuntimeState(generated.string()),"Could not reset modern mask mutation control");
    auto mutated=baseline; auto& liveMask=recordFor(mutated).state->frame->control->liveProbeMask;
    const auto authored=ReadPortableActorDispatchContext(actor);
    const auto allowed=(authored.classProbeMask | authored.codeMasks->probeMask) & authored.codeMasks->ignoreMask;
    for (std::uint8_t index=0u;index<64u;++index) {
        const auto name=QuestVr::ScriptDispatch::ProbeEventName(index);
        const auto bit=std::uint64_t{1}<<index;
        Call(actor,"Disable",{Name(name)}); liveMask &= ~bit; verifyFrame(mutated);
        Call(actor,"Enable",{Name(name)}); liveMask |= bit & allowed; verifyFrame(mutated);
    }
    current=publish(mutated); unchanged(current,"Modern original Enable/Disable positive bit mutation");
    Call(actor,"Disable",{Name("LongFall")}); Call(actor,"Enable",{Name("Quest_NoSuchProbe")});
    unchanged(current,"Modern nonprobe mutation no-op");
    const auto beginOrdinal=[&](const std::string& path,const std::string& name) {
        const auto layout=ReadPortableRuntimeStateProgram(path);
        const auto label=std::find_if(layout.labels.begin(),layout.labels.end(),[&](const auto& entry) {
            return QuestVr::ScriptDispatch::FoldName(entry.name)==QuestVr::ScriptDispatch::FoldName(name);
        });
        Require(label!=layout.labels.end(),"Original transition fixture has no actual label: "+path+'.'+name);
        return static_cast<std::uint32_t>(std::lower_bound(layout.statementOffsets.begin(),layout.statementOffsets.end(),label->offset)-layout.statementOffsets.begin());
    };
    Call(actor,"GotoState",{Name("Standing")});
    auto relabeled=mutated; auto& relabel=*recordFor(relabeled).state->frame;
    relabel.codePath="DeusEx.ScriptedPawn.Standing"; relabel.control->codePresent=true;
    relabel.statementIndex=beginOrdinal(relabel.codePath,"Begin"); relabel.latent=QuestVr::StateLatent::Continue;
    verifyFrame(relabeled); current=publish(relabeled); unchanged(current,"Same selected state relabel preserves live mask/Locals/timer");
    Call(actor,"GotoState",{Name("Standing"),Name("")});
    verifyFrame(relabeled); unchanged(current,"Empty Name label is NAME_None and defaults to actual Begin");
    Call(actor,"GotoState",{Name("Standing"),Name("Quest_NoSuchLabel")});
    relabel.control->codePresent=false; verifyFrame(relabeled);
    current=publish(relabeled); unchanged(current,"Missing same-state label clears only Code and latent action");
    const auto transition=Call(actor,"GotoState",{Name("StartUp")});
    auto transitioned=relabeled; auto& entryFrame=*recordFor(transitioned).state->frame;
    entryFrame.codePath="DeusEx.ScriptedPawn.StartUp";
    entryFrame.control->selectedPath=entryFrame.codePath; entryFrame.control->codePresent=true;
    entryFrame.control->liveProbeMask=(authored.classProbeMask | startupDescriptor.probeMask) & startupDescriptor.ignoreMask;
    entryFrame.statementIndex=beginOrdinal(entryFrame.codePath,"Begin");
    Require(transition.committed,"Original modern true state transition was not committed");
    verifyFrame(transitioned); current=publish(transitioned);
    unchanged(current,"True state transition resets authored mask but preserves retained Locals and native timer");
    Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),
        "Production modern transition could not warm-restore");
    verifyFrame(transitioned); unchanged(current,"Production modern transition warm identity/mask/PC/Locals restore");
    // These are actual original SuperField identities, not a seeded synthetic
    // inheritance contract. Retained running Node may differ from selection.
    const auto dyingDescriptor=ReadPortableRuntimeAuthoredStateDescriptor("DeusEx.ScriptedPawn.Dying");
    const auto deusEx=std::find_if(tables.begin(),tables.end(),[](const auto& table) {
        return QuestVr::ScriptDispatch::FoldName(std::filesystem::path(table.sourcePath).stem().string())=="deusex";
    });
    Require(deusEx!=tables.end() && QualifiedReference(*deusEx,dyingDescriptor.baseField)=="Engine.Pawn.Dying",
        "Original Doctor Dying fixture lost its actual Engine.Pawn.Dying SuperField");
    auto inherited=baseline; auto& inheritedFrame=*recordFor(inherited).state->frame;
    inheritedFrame.control->selectedPath="DeusEx.ScriptedPawn.Dying";
    inheritedFrame.codePath="Engine.Pawn.Dying";
    current=publish(inherited); Require(LoadPortableRuntimeState(generated.string()),"Original retained parent Node control did not restore");
    verifyFrame(inherited);
    Require(Call(actor,"GetStateName").value.text=="Dying" && ReadPortableActorDispatchContext(actor).selectedPath=="DeusEx.ScriptedPawn.Dying" &&
        ReadPortableActorDispatchContext(actor).runningPath=="Engine.Pawn.Dying",
        "Original selected derived Dying identity was replaced by its retained parent Node");
    const auto derivedDying=ReadPortableRuntimeStateProgram("DeusEx.ScriptedPawn.Dying");
    const auto parentDying=ReadPortableRuntimeStateProgram("Engine.Pawn.Dying");
    const auto inheritedLabel=std::find_if(parentDying.labels.begin(),parentDying.labels.end(),[&](const auto& label) {
        return std::none_of(derivedDying.labels.begin(),derivedDying.labels.end(),[&](const auto& child) {
            return QuestVr::ScriptDispatch::FoldName(child.name)==QuestVr::ScriptDispatch::FoldName(label.name);
        });
    });
    if (inheritedLabel!=parentDying.labels.end()) {
        Call(actor,"GotoState",{Name("Dying"),Name(inheritedLabel->name)});
        inheritedFrame.control->codePresent=true; inheritedFrame.latent=QuestVr::StateLatent::Continue;
        inheritedFrame.statementIndex=beginOrdinal(inheritedFrame.codePath,inheritedLabel->name);
        verifyFrame(inherited); current=publish(inherited);
        unchanged(current,"Actual unshadowed inherited Dying label preserves selected identity/mask/Locals/timer");
        std::cout<<"ORIGINAL INHERITED LABEL DeusEx.ScriptedPawn.Dying -> Engine.Pawn.Dying."<<inheritedLabel->name<<'\n';
    } else {
        unchanged(current,"Actual Dying parent identity retention without an unshadowed source label");
        std::cout<<"ORIGINAL DYING separate selected/running parent identities; no unshadowed original parent label available\n";
    }
    // The inspected Doctor parent contains only Stop and no labels. Use an
    // actual map mover for a real inherited-label search, rather than assigning
    // the parent running Node ourselves and mistaking that for search coverage.
    const auto actors=GetPortableRuntimeMapActors();
    const auto mover=std::find_if(actors.begin(),actors.end(),[](const auto& value) {
        return value.classPath=="DeusEx.DeusExMover";
    });
    Require(mover!=actors.end(),"Original Training has no actual DeusExMover inherited-label fixture");
    const auto moverChild=ReadPortableRuntimeStateProgram("DeusEx.DeusExMover.TriggerOpenTimed");
    const auto moverParent=ReadPortableRuntimeStateProgram("Engine.Mover.TriggerOpenTimed");
    const auto moverDescriptor=ReadPortableRuntimeAuthoredStateDescriptor("DeusEx.DeusExMover.TriggerOpenTimed");
    Require(moverChild.labels.empty() && QualifiedReference(*deusEx,moverDescriptor.baseField)=="Engine.Mover.TriggerOpenTimed" &&
        std::any_of(moverParent.labels.begin(),moverParent.labels.end(),[](const auto& label) {return label.name=="Close" && label.offset==74u;}),
        "Actual mover inherited Close label/SuperField no longer matches original source data");
    Call(mover->objectPath,"GotoState",{Name("TriggerOpenTimed")});
    Call(mover->objectPath,"Disable",{Name("Tick")});
    const auto moverBefore=ReadPortableActorStateObject(mover->objectPath);
    const auto moverTimer=ReadPortableActorLatentTimeLeft(mover->objectPath);
    Require(moverBefore && moverBefore->frame && moverBefore->frame->control &&
        moverBefore->frame->control->selectedPath=="DeusEx.DeusExMover.TriggerOpenTimed",
        "Production original mover transition did not publish the actual derived selection");
    Call(mover->objectPath,"GotoState",{Name("TriggerOpenTimed"),Name("Close")});
    const auto verifyMover=[&]() {
        const auto value=ReadPortableActorStateObject(mover->objectPath);
        Require(value && value->frame && value->frame->control && value->disabled.empty() &&
            value->frame->control->selectedPath=="DeusEx.DeusExMover.TriggerOpenTimed" &&
            value->frame->codePath=="Engine.Mover.TriggerOpenTimed" && value->frame->control->codePresent &&
            value->frame->statementIndex==beginOrdinal("Engine.Mover.TriggerOpenTimed","Close") &&
            value->frame->control->liveProbeMask==moverBefore->frame->control->liveProbeMask &&
            value->frame->localsCodePath==moverBefore->frame->localsCodePath && value->frame->locals.empty() &&
            value->frame->latent==QuestVr::StateLatent::Continue && ReadPortableActorLatentTimeLeft(mover->objectPath)==moverTimer &&
            Call(mover->objectPath,"GetStateName").value.text=="TriggerOpenTimed" &&
            ReadPortableActorDispatchContext(mover->objectPath).selectedPath=="DeusEx.DeusExMover.TriggerOpenTimed" &&
            ReadPortableActorDispatchContext(mover->objectPath).runningPath=="Engine.Mover.TriggerOpenTimed",
            "Actual inherited mover Close label conflated selected/running identity or reset mask/Locals/timer");
    };
    verifyMover(); Require(SavePortableRuntimeState(inspect.string()),"Actual inherited mover label could not checkpoint");
    const auto moverBytes=CheckpointBytes(inspect);
    Require(ValidatePortableRuntimeState(inspect.string()) && LoadPortableRuntimeState(inspect.string()),
        "Actual inherited mover label could not warm-restore");
    verifyMover(); unchanged(moverBytes,"Actual inherited mover label canonical warm restore");
    std::cout<<"ORIGINAL INHERITED LABEL DeusEx.DeusExMover.TriggerOpenTimed -> Engine.Mover.TriggerOpenTimed.Close@74; no state code executed\n";
    auto classBacked=baseline; auto& classFrame=*recordFor(classBacked).state->frame;
    const auto actorClass=Snapshot(actor).classPath;
    classFrame.codePath=actorClass; classFrame.localsCodePath.clear(); classFrame.locals.clear();
    classFrame.control=QuestVr::StateControl{actorClass,false,0x0123456789abcdefull}; classFrame.latent=QuestVr::StateLatent::Continue;
    current=publish(classBacked); Require(LoadPortableRuntimeState(generated.string()),"Modern class-backed null-Code frame did not restore");
    verifyFrame(classBacked);
    Require(Call(actor,"GetStateName").value.text==actorClass.substr(actorClass.find_last_of('.')+1u) &&
        QuestVr::Vm::ToBool(Call(actor,"IsInState",{Name("Actor")}).value) &&
        QuestVr::Vm::ToBool(Call(actor,"IsInState",{Name("Object")}).value) &&
        !QuestVr::Vm::ToBool(Call(actor,"IsInState",{Name("Standing")}).value),
        "Modern class-backed state queries failed explicit class ancestry");
    unchanged(current,"Modern class-backed readonly selected-node queries");
    // An empty Name is NAME_None, not a distinct missing-state name. The
    // wrapper must preserve this nondefault mask on its same-None shortcut.
    Call(actor,"GotoState",{Name(""),Name("")});
    verifyFrame(classBacked); unchanged(current,"Empty Name same-None preserves class-backed control instead of resetting its mask");
    auto runnable=baseline; auto& run=*recordFor(runnable).state->frame;
    run.codePath="DeusEx.ScriptedPawn.StartUp"; run.statementIndex=stopOrdinal;
    run.control->selectedPath=run.codePath; run.control->codePresent=true; run.latent=QuestVr::StateLatent::Continue;
    current=publish(runnable); Require(LoadPortableRuntimeState(generated.string()),"Modern actual EX_Stop continuation did not restore");
    const auto stopped=ResumePortableActorState(actor);
    Require(stopped.passed() && stopped.status==Status::Stopped && stopped.instructions==1u,
        "Modern original EX_Stop did not execute exactly its actual one statement");
    auto afterStop=runnable; auto& stoppedFrame=*recordFor(afterStop).state->frame;
    stoppedFrame.statementIndex=stopOrdinal+1u; stoppedFrame.control->codePresent=false;
    verifyFrame(afterStop); current=publish(afterStop); unchanged(current,"Original EX_Stop retained modern identities/locals/mask/timer");
    auto sleeping=runnable; recordFor(sleeping).state->frame->latent=QuestVr::StateLatent::Sleep;
    const auto waitingBytes=publish(sleeping); Require(LoadPortableRuntimeState(generated.string()),"Modern actual Stop/Sleep control did not restore");
    QuestVr::Vm::Limits noInstructions; noInstructions.instructions=0u;
    const auto refused=AdvancePortableActorState(actor,2.0f,noInstructions);
    Require(refused.status==Status::Budget && !refused.committed && ReadPortableActorLatentTimeLeft(actor)==1.5f,
        "Modern native Sleep poll failed to roll back on subsequent statement-budget refusal");
    unchanged(waitingBytes,"Modern Sleep poll/budget rollback");
    Require((startupDescriptor.stateFlags & 4u)==0u,"Original non-simulated StartUp Role gate fixture changed");
    auto roleGated=sleeping; property(recordFor(roleGated),"Engine.Actor.Role","Role",Value::Byte(3u));
    current=publish(roleGated); Require(LoadPortableRuntimeState(generated.string()) && AdvancePortableActorState(actor,2.0f).passed() &&
        ReadPortableActorLatentTimeLeft(actor)==1.5f,"Modern selected-state Role gate consumed native timer");
    unchanged(current,"Modern selected-state Role gate");
    current=publish(sleeping); Require(LoadPortableRuntimeState(generated.string()),"Could not reset modern typed rejection baseline");
    std::size_t rejected{};
    const auto reject=[&](QuestVr::ScriptSavedState graph,const std::string& description,const std::uint32_t version=12u) {
        publish(graph,version); const auto beforeRevision=GetPortableRuntimeWorldRevision(); const auto beforeGc=GC::GetStats();
        Require(!ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()) &&
            GetPortableRuntimeWorldRevision()==beforeRevision && GC::GetStats().numObjects==beforeGc.numObjects &&
            GC::GetStats().memoryUsage==beforeGc.memoryUsage,description+" bypassed typed preflight or leaked runtime state");
        unchanged(current,description+" atomic refusal"); ++rejected;
    };
    auto invalid=runnable; recordFor(invalid).state->frame->control->selectedPath="Engine.PlayerPawn"; reject(invalid,"Foreign selected Class owner");
    invalid=runnable; recordFor(invalid).state->frame->codePath="Engine.PlayerPawn"; recordFor(invalid).state->frame->control->codePresent=false;
    reject(invalid,"Foreign retained running Class owner");
    invalid=runnable; recordFor(invalid).state->frame->control->selectedPath="Engine.Actor.BeginPlay"; reject(invalid,"Function as selected StateNode");
    invalid=runnable; recordFor(invalid).state->frame->codePath="Engine.Actor.BeginPlay"; reject(invalid,"Function as retained running State/Class");
    invalid=classBacked; recordFor(invalid).state->frame->control->codePresent=true; reject(invalid,"Runnable Class Node");
    invalid=runnable; recordFor(invalid).state->frame->codePath="DeusEx.ScriptedPawn.FallingState"; reject(invalid,"Runnable sibling outside selected ancestry");
    auto nullLocals=runnable; recordFor(nullLocals).state->frame->localsCodePath.clear();
    const auto nullLocalsBytes=publish(nullLocals);
    Require(ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),
        "Original zero-schema runnable State rejected retained NULL Locals");
    verifyFrame(nullLocals); unchanged(nullLocalsBytes,"Original zero-schema NULL Locals warm restore");
    current=publish(sleeping); Require(LoadPortableRuntimeState(generated.string()),"Could not reset modern rejection baseline after NULL Locals");
    invalid=runnable; recordFor(invalid).state->frame->localsCodePath.clear();
    recordFor(invalid).state->frame->locals.push_back({"Engine.Actor.NoSuchLocal",{Value::Integer(0)}});
    reject(invalid,"Absent modern local owner with retained local values");
    invalid=runnable; recordFor(invalid).state->frame->localsCodePath="Engine.Actor"; reject(invalid,"Class as modern local declaration owner");
    invalid=runnable; recordFor(invalid).state->frame->statementIndex=std::numeric_limits<std::uint32_t>::max(); reject(invalid,"Runnable modern PC overrun");
    reject(runnable,"Codec9 under legacy envelope11",11u);
    auto legacyGraph=QuestVr::DecodeScriptSavedState(ScriptBlob(prefix));
    auto& legacyFrame=*recordFor(legacyGraph).state->frame; legacyFrame.control.reset();
    legacyFrame.localsCodePath=legacyFrame.codePath;
    auto wrongEnvelope=ReplaceScriptBlob(prefix,QuestVr::EncodeScriptSavedState(legacyGraph));
    PutWord(wrongEnvelope,4u,12u); Require(QuestVr::WriteDurableSaveFile(generated.string(),wrongEnvelope) &&
        !ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()),"Envelope12 accepted legacy codec without controls");
    unchanged(current,"Envelope12/legacy codec atomic refusal"); ++rejected;
    auto cold=transitioned; cold.randomSeed=0xfedcba98u; const auto coldBytes=publish(cold);
    Require(LoadPortableRuntimeState(generated.string()) && SavePortableRuntimeState(saved.string()) &&
        Word(CheckpointBytes(saved),4u)==12u && ScriptBlob(CheckpointBytes(saved)).at(6u)==9u,
        "Generated modern control did not persist codec9/envelope12");
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorStateObject(actor) && !ReadPortableActorLatentTimeLeft(actor),
        "Legacy reset retained modern frame or timer");
    ShutdownPortableRuntime(); Require(InitializePortableRuntime(tables).passed,"Cold modern-control runtime initialization failed");
    Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed &&
        ValidatePortableRuntimeState(saved.string()) && !ReadPortableActorStateObject(actor) && !ReadPortableActorLatentTimeLeft(actor),
        "Cold modern-control readonly preflight published generated state");
    Require(LoadPortableRuntimeState(saved.string()),"Cold generated modern control did not restore");
    verifyFrame(cold); unchanged(coldBytes,"Cold generated modern selected/running/local/control/RNG restore");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent() && !ReadPortableActorLatentTimeLeft(actor),
        "Final legacy reset retained generated modern continuation");
    unchanged(legacyBytes,"Final modern-control legacy reset");
    std::cout<<"GENERATED MODERN CONTROL original Doctor/StartUp schema, codec9/envelope12 selected/running/local/null-Code/high masks, "
        "class ancestry, literal Enable/Disable, actual EX_Stop, Sleep rollback/Role gate, warm/cold/RNG and typed rejections="<<rejected<<
        "; production same-state/missing-label/true-transition mask and Locals invariants verified\n";
}

void VerifyOriginalStateExecution(const std::string& actor, const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    const auto saved = directory / "original-state-v5.sav";
    const auto inspect = directory / "state-unchanged-v5.sav";
    const auto generated = directory / "generated-state-v5.sav";
    const auto unchanged = [&](const std::vector<std::uint8_t>& expected, const std::string& description) {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==expected,
            description+" changed complete live gameplay/script/clock/state payload");
    };
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorStateObject(actor),
        "Legacy restore did not clear the portable state override");
    const auto authored = ReadPortableActorDispatchContext(actor);
    const auto authoredPhysics=ReadPortableActorScriptProperty(actor,"Physics");
    const auto authoredVelocity=ReadPortableActorScriptProperty(actor,"Velocity");
    const auto authoredBase=ReadPortableActorScriptProperty(actor,"Base");
    Require(authoredPhysics.kind==Kind::Byte,"Original Actor.Physics is not a byte");
    Call(actor,"SetPhysics",{{Value::Byte(3u),{}}});
    Require(ReadPortableActorScriptProperty(actor,"Physics").integer==3 &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor,"Velocity"),authoredVelocity) &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor,"Base"),authoredBase) && !ReadPortableActorStateObject(actor),
        "Pinned SetPhysics assignment reset velocity/base or fabricated a state frame");
    Call(actor,"SetPhysics",{{Value::Integer(257),{}},{Value::Text(Kind::Object,actor),{}}});
    Require(ReadPortableActorScriptProperty(actor,"Physics").integer==1 &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor,"Base"),authoredBase) && SavePortableRuntimeState(saved.string()),
        "SetPhysics byte conversion or pinned optional-floor behavior diverged");
    const auto physicsBytes=CheckpointBytes(saved);
    Require(Word(physicsBytes,4)==4u,"Physics-only property change fabricated state/clock storage");
    const auto badPhysics=ExecutePortableActorFunction(actor,"SetPhysics",{Name("InvalidPhysics")});
    const auto badFloor=ExecutePortableActorFunction(actor,"SetPhysics",{{Value::Byte(2u),{}},Name("InvalidFloor")});
    Require(!badPhysics.passed() && !badFloor.passed(),"Malformed SetPhysics arguments were accepted");
    unchanged(physicsBytes,"Malformed SetPhysics native argument rollback");
    Call(actor,"SetPhysics",{{Value::Byte(0u),{}}});
    Require(LoadPortableRuntimeState(saved.string()) && ReadPortableActorScriptProperty(actor,"Physics").integer==1,
        "Physics byte overlay did not restore from v4");
    unchanged(physicsBytes,"Physics-only v4 roundtrip");
    Require(LoadPortableRuntimeState(legacy.string()) &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor,"Physics"),authoredPhysics) && !GetPortableRuntimeScriptStatePresent(),
        "Legacy restore did not clear the Physics byte overlay");
    std::cout<<"ORIGINAL SETPHYSICS byte assignment/conversion/optional-floor/native argument rollback/v4/legacy passed; no simulation implied\n";
    Call(actor,"Disable",{Name("Tick")});
    auto dynamic = ReadPortableActorStateObject(actor);
    Require(dynamic && !dynamic->frameOverride && !dynamic->frame && dynamic->hasStack &&
        ReadPortableActorDispatchContext(actor).disabledNames.count("tick")==1u &&
        SavePortableRuntimeState(saved.string()), "Disable did not preserve dormant raw context in a state-only record");
    const auto dormant = CheckpointBytes(saved);
    Require(Word(dormant,4)==5u && ScriptBlob(dormant).at(6)==2u,
        "State-only capture did not select runtime v5 / script codec v2");
    Call(actor,"Enable",{Name("TICK")});
    Require(ReadPortableActorDispatchContext(actor).disabledNames.count("tick")==0u &&
        LoadPortableRuntimeState(saved.string()), "Enable folding or dormant disabled-set restore failed");
    unchanged(dormant,"Dormant disabled-set roundtrip");
    Call(actor,"SetInitialState");
    const auto startup = ReadPortableActorStateObject(actor);
    const auto context = ReadPortableActorDispatchContext(actor);
    const auto layout = ReadPortableRuntimeStateProgram("DeusEx.ScriptedPawn.StartUp");
    const auto begin = std::find_if(layout.labels.begin(),layout.labels.end(),[](const auto& label) {
        return QuestVr::ScriptDispatch::FoldName(label.name)=="begin";
    });
    Require(begin!=layout.labels.end(),"Original StartUp fixture lacks authored Begin label");
    const auto ordinal = std::lower_bound(layout.statementOffsets.begin(),layout.statementOffsets.end(),begin->offset)-layout.statementOffsets.begin();
    Require(startup && startup->frameOverride && startup->frame && startup->hasStack &&
        startup->frame->codePath=="DeusEx.ScriptedPawn.StartUp" &&
        startup->frame->control && startup->frame->control->selectedPath==startup->frame->codePath &&
        startup->frame->control->codePresent && startup->frame->localsCodePath.empty() && startup->frame->locals.empty() &&
        startup->frame->latent==QuestVr::StateLatent::Continue && startup->frame->statementIndex==ordinal &&
        context.stateName=="StartUp" && context.disabledNames.empty(),
        "Pre-begun-play SetInitialState did not select modern Auto/Begin or retain original NULL Locals");
    Call(actor,"Disable",{Name("Tick")});
    Require(SavePortableRuntimeState(saved.string()),"Actual selected state frame could not be saved");
    const auto selected = CheckpointBytes(saved);
    const auto selectedMask=ReadPortableActorStateObject(actor)->frame->control->liveProbeMask;
    const auto slice = ResumePortableActorState(actor);
    Require(slice.passed() && slice.status==Status::Waiting && slice.committed && SavePortableRuntimeState(inspect.string()),
        "Original StartUp did not yield at the actual Sleep statement: "+slice.error);
    const auto waiting=CheckpointBytes(inspect);
    const auto wake=AdvancePortableActorState(actor,1.0f);
    const auto standingTimer=VerifyOriginalStandingSleep(actor,wake,"Original state-only StartUp wake");
    Require(SavePortableRuntimeState(inspect.string()),"Original state-only Standing continuation could not save");
    const auto standing=CheckpointBytes(inspect);
    Require(standing!=waiting && Word(standing,4)==12u && ScriptBlob(standing).at(6)==9u && ValidatePortableRuntimeState(inspect.string()),
        "Original state-only Standing save did not retain shared codec9/envelope12");
    unchanged(standing,"Original state-only Standing read-only preflight");
    Require(LoadPortableRuntimeState(inspect.string()),"Original state-only Standing warm continuation could not restore");
    unchanged(standing,"Original state-only Standing warm restore");
    const auto noTime=ResumePortableActorState(actor);
    Require(noTime.passed() && noTime.status==Status::Waiting && noTime.instructions==0u,
        "Original state-only Standing no-time continuation ran its next statement");
    unchanged(standing,"Original state-only Standing no-time continuation");
    const auto standingRevision=GetPortableRuntimeWorldRevision();const auto standingGc=GC::GetStats();
    const auto standingActors=GetPortableRuntimeMapActors(true).size();
    QuestVr::Vm::Limits noWakeInstructions;noWakeInstructions.instructions=0u;
    const auto refused=AdvancePortableActorState(actor,standingTimer,noWakeInstructions);
    Require(refused.status==Status::Budget && !refused.committed && !refused.error.empty() &&
        GetPortableRuntimeWorldRevision()==standingRevision && GetPortableRuntimeMapActors(true).size()==standingActors &&
        GC::GetStats().numObjects==standingGc.numObjects && GC::GetStats().memoryUsage==standingGc.memoryUsage,
        "Original state-only Standing forced-budget wake leaked native release, RNG or UObject state");
    unchanged(standing,"Original state-only Standing forced-budget wake rollback");
    std::cout<<"ORIGINAL STATE WAKE committed Standing Sleep304, next319, timer="<<standingTimer<<
        "; warm/read-only/no-time saved continuation and forced-budget rollback passed\n";
    Require(LoadPortableRuntimeState(saved.string()),"Could not reset original state entry after Sleep test");
    Call(actor,"GotoState",{Name("None")});
    const auto cleared = ReadPortableActorStateObject(actor);
    const auto actorClass=Snapshot(actor).classPath;
    Require(cleared && cleared->frame && cleared->frame->codePath==actorClass && cleared->frame->control &&
        cleared->frame->control->selectedPath==actorClass && !cleared->frame->control->codePresent && cleared->frame->locals.empty() &&
        cleared->frame->localsCodePath.empty() && cleared->frame->statementIndex==startup->frame->statementIndex &&
        cleared->frame->latent==QuestVr::StateLatent::Continue &&
        Call(actor,"GetStateName").value.text==authored.stateName,
        "GotoState(None) lost class-backed identities, retained PC or NULL Locals");
    Call(actor,"GotoState",{Name(authored.stateName)}); // Class name is not a State: retain class-backed None selection.
    Require(Call(actor,"GetStateName").value.text==authored.stateName &&
        !ReadPortableActorStateObject(actor)->frame->control->codePresent,
        "Class-backed raw state name was accepted as an executable State or normalized to None");
    Require(LoadPortableRuntimeState(saved.string()),"Selected StartUp continuation did not restore");
    unchanged(selected,"Selected frame roundtrip");
    Call(actor,"GotoState",{Name("StartUp"),Name("Quest_No_Such_Label")});
    const auto stopped = ReadPortableActorStateObject(actor);
    Require(stopped && stopped->frame && stopped->frame->control && !stopped->frame->control->codePresent &&
        stopped->frame->latent==QuestVr::StateLatent::Continue && stopped->frame->codePath==startup->frame->codePath &&
        stopped->frame->control->selectedPath==startup->frame->control->selectedPath &&
        stopped->frame->control->liveProbeMask==selectedMask &&
        stopped->frame->statementIndex==startup->frame->statementIndex,
        "Same-state missing transition label did not clear only Code/latent while preserving identity/mask/PC");
    Require(SavePortableRuntimeState(inspect.string()),"Stopped selected state could not save");
    const auto stoppedBytes=CheckpointBytes(inspect);
    const auto stopSlice=ResumePortableActorState(actor);
    Require(stopSlice.passed() && stopSlice.committed && stopSlice.status==Status::Stopped && stopSlice.instructions==0u,
        "Stopped portable frame did not commit a no-op state slice");
    unchanged(stoppedBytes,"Stopped slice");
    Require(LoadPortableRuntimeState(saved.string()),"Could not reset selected frame before schema controls");
    const auto baseline=QuestVr::DecodeScriptSavedState(ScriptBlob(selected));
    const auto index=static_cast<std::size_t>(std::find_if(baseline.objects.begin(),baseline.objects.end(),[&](const auto& object) {
        return object.path==actor;
    })-baseline.objects.begin());
    Require(index<baseline.objects.size() && baseline.objects[index].state && baseline.objects[index].state->frame,
        "Original v5 fixture did not contain its complete state record");
    std::size_t rejected{};
    const auto rejectBytes=[&](const std::vector<std::uint8_t>& bytes,const std::string& description) {
        Require(QuestVr::WriteDurableSaveFile(generated.string(),bytes) &&
            !ValidatePortableRuntimeState(generated.string()) && !LoadPortableRuntimeState(generated.string()),
            description+" checkpoint was accepted");
        unchanged(selected,description+" rejection");++rejected;
    };
    const auto reject=[&](const std::function<void(QuestVr::StateObject&)>& mutate,const std::string& description) {
        auto invalid=baseline;mutate(*invalid.objects[index].state);
        rejectBytes(ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(invalid)),description);
    };
    reject([](auto& value) {value.frame->codePath="Engine.Actor";},"Class as state code");
    reject([](auto& value) {value.frame->codePath="DeusEx.ScriptedPawn.NoSuchState";},"Unknown state code");
    reject([](auto& value) {value.frame->codePath="DeusEx.ScriptedPawn.Standing";},"Running sibling outside selected ancestry");
    reject([](auto& value) {value.frame->localsCodePath="DeusEx.Robot.Standing";},"Local owner outside receiver ancestry");
    reject([](auto& value) {value.frame->statementIndex=0xffffffffu;},"Running PC out of code");
    reject([](auto& value) {value.frame->latent=QuestVr::StateLatent::Sleep;},"Sleep without native actor timer");
    reject([](auto& value) {value.frame->locals.push_back({"Engine.Actor.NoSuchLocal",{Value::Integer(0)}});},"Incomplete/mismatched local schema");
    auto wrongEnvelope=selected;PutWord(wrongEnvelope,4,4u);rejectBytes(wrongEnvelope,"State codec in v4 envelope");
    auto absent=baseline;absent.objects[index].state.reset();
    rejectBytes(ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(absent)),"Legacy codec in modern envelope12");
    auto truncated=selected;truncated.pop_back();rejectBytes(truncated,"Truncated modern state tail");
    auto extra=selected;extra.push_back(0);rejectBytes(extra,"Trailing modern state byte");
    // A null executable Code retains the running/selected identities and stale
    // PC independently; no synthetic Stop or reset position is introduced.
    auto stale=baseline;stale.objects[index].state->frame->control->codePresent=false;
    stale.objects[index].state->frame->statementIndex=0xffffffffu;
    Require(QuestVr::WriteDurableSaveFile(generated.string(),ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(stale))) &&
        ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()) &&
        ReadPortableActorStateObject(actor)->frame->statementIndex==0xffffffffu,
        "Valid stopped stale PC was rejected or reset");
    Require(LoadPortableRuntimeState(saved.string()),"Could not reset state after stale stopped PC control");
    auto terminal=baseline;terminal.objects[index].state->frame->statementIndex=static_cast<std::uint32_t>(layout.statementOffsets.size());
    Require(QuestVr::WriteDurableSaveFile(generated.string(),ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(terminal))) &&
        ValidatePortableRuntimeState(generated.string()) && LoadPortableRuntimeState(generated.string()),
        "Legitimately advanced end-of-code Continue ordinal could not restore");
    Require(SavePortableRuntimeState(inspect.string()),"End-of-code Continue frame became unsaveable");
    const auto endBytes=CheckpointBytes(inspect);
    const auto endSlice=ResumePortableActorState(actor);
    Require(!endSlice.passed() && endSlice.error.find("Unexpected end")!=std::string::npos,
        "End-of-code Continue fabricated a successful next slice");
    unchanged(endBytes,"End-of-code resume failure rollback");
    // Explicit legacy portable continuation: decoding/resumption compatibility
    // is not authority to infer its missing original selected/mask identities.
    auto folded=baseline;
    folded.objects[index].state->frame->control.reset();
    folded.objects[index].state->frame->localsCodePath=folded.objects[index].state->frame->codePath;
    auto& sets=folded.objects[index].state->disabled;
    sets["StArTuP"].insert("TiCk");
    auto legacyPortable=ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(folded));PutWord(legacyPortable,4u,5u);
    Require(QuestVr::WriteDurableSaveFile(generated.string(),legacyPortable) &&
        LoadPortableRuntimeState(generated.string()) && ReadPortableActorDispatchContext(actor).disabledNames.count("tick")==1u,
        "Mixed-case state/event disabled identities did not restore canonically");
    Require(SavePortableRuntimeState(inspect.string()),"Legacy portable continuation could not canonicalize");
    const auto legacyCanonical=CheckpointBytes(inspect);
    const auto ambiguous=ExecutePortableActorFunction(actor,"GotoState",{Name("Standing")});
    Require(ambiguous.status==Status::Unsupported && !ambiguous.committed && !ambiguous.error.empty(),
        "Legacy portable continuation silently inferred original state-control provenance");
    unchanged(legacyCanonical,"Ambiguous legacy portable GotoState atomic refusal");
    Require(LoadPortableRuntimeState(saved.string()),"Could not reset selected state after terminal/mixed-case controls");
    // Explicit begun-play fixture, not automatic world startup. Initialize a
    // real native manager and registration so BeginState's BlockReactions must
    // perform the original clear path rather than use an absent-manager branch.
    auto begun=baseline;
    const auto levelValue=ReadPortableActorScriptProperty(actor,"Level");
    const auto levelSnapshot=Snapshot(levelValue.text);
    begun.objects.push_back({levelSnapshot.objectPath,levelSnapshot.classPath,
        {{"Engine.LevelInfo.bBegunPlay","bBegunPlay",0u,Value::Bool(true)}},{}});
    begun.objects[index].state.reset();
    auto begunBytes=ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(begun));PutWord(begunBytes,4,4u);
    Require(QuestVr::WriteDurableSaveFile(generated.string(),begunBytes) && LoadPortableRuntimeState(generated.string()),
        "Actual BeginState begun-play fixture was rejected");
    Call(levelValue.text,"InitEventManager");
    Call(actor,"AISetEventCallback",{Name("Futz"),Name("Quest_Test_Handler")});
    const auto entry=ExecutePortableActorFunction(actor,"SetInitialState");
    const auto entered=ReadPortableActorStateObject(actor);
    Require(entry.passed() && entry.committed && entered && entered->frame &&
        entered->frame->codePath=="DeusEx.ScriptedPawn.StartUp" &&
        ReadPortableActorDispatchContext(actor).stateName=="StartUp" &&
        QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor,"bInterruptState")) &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor,"bCanConverse")) &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor,"bStasis")) &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(actor,"bDistressed")) &&
        ReadPortableActorScriptProperty(actor,"DestAttempts").integer==0 &&
        QuestVr::Vm::Equal(ReadPortableActorScriptProperty(actor,"LastDestLoc"),
            ReadPortableActorScriptProperty(actor,"LastDestPoint")) && SavePortableRuntimeState(inspect.string()),
        // Registration now has a real manager; this entry must execute its
        // authored effects, not retain the former unsupported-AI expectation.
        "Original synchronous begun-play BeginState did not perform its actual property/state effects: "+entry.error);
    const auto enteredGraph=QuestVr::DecodeScriptSavedState(ScriptBlob(CheckpointBytes(inspect)));
    Require(enteredGraph.aiManagers.size()==1u && enteredGraph.aiManagers.front().receivers.size()==1u &&
        enteredGraph.aiManagers.front().receivers.front().deleted && enteredGraph.aiManagers.front().pendingDeleteCount==1u,
        "Original begun-play BlockReactions bypassed real manager callback deletion");
    std::cout<<"ORIGINAL BEGINSTATE explicit real-manager SetMovementPhysics/SetDistress/BlockReactions/ResetDestLoc completed; not automatic world startup\n";
    Require(LoadPortableRuntimeState(legacy.string()) && !ReadPortableActorStateObject(actor) &&
        !GetPortableRuntimeUnsavedScriptState(),"Legacy restore failed to clear complete portable state override");
    std::cout<<"ORIGINAL raw dormant v5 retention; modern codec9/v12 Auto/Begin/class-backed None/null-Code/live-mask roundtrip; schema rejections="<<rejected<<
        "; atomic slice/callback failure; no world startup/tick implied\n";
}

void VerifyOriginalInitGame(const std::filesystem::path& root,
    const std::filesystem::path& legacy, const std::filesystem::path& directory) {
    const auto original = CheckpointBytes(legacy);
    const auto originalActors = GetPortableRuntimeMapActors(true).size();
    const auto game = Call("00_Training.Doctor1","Engine.Actor.Spawn",
        {{Value::Text(Kind::Object,"DeusEx.DeusExGameInfo"),{}}}).value.text;
    Require(!game.empty(),"Original GameInfo class could not be born before begun-play");
    const auto level = ReadPortableActorScriptProperty(game,"Level").text;
    Require(!QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(level,"bBegunPlay")),
        "Controlled original InitGame input is already a begun world");
    const auto before = directory/"original-initgame-before.sav";
    const auto saved = directory/"original-initgame-complete.sav";
    const auto inspect = directory/"original-initgame-inspect.sav";
    Require(SavePortableRuntimeState(before.string()),"Cannot save original GameInfo birth baseline");
    const auto baseline = CheckpointBytes(before);
    const auto string = [](const std::string& text) { return Evaluation{Value::Text(Kind::String,text),{}}; };
    const std::string options="?mAxPlAyErS=64?Difficulty=2?AdminPassword=fixture-admin?GamePassword=fixture-game?LocalLog=TrUe?WorldLog=TrUe";
    Require(Call(game,"ParseOption",{string(options),string("MAXPLAYERS")}).value.text=="64" &&
        Call(game,"ParseOption",{string(options),string("Absent")}).value.text.empty() &&
        Call(game,"GetIntOption",{string(options),string("Difficulty"),{Value::Integer(7),{}}}).value.integer==2 &&
        Call(game,"GetIntOption",{string(options),string("Absent"),{Value::Integer(7),{}}}).value.integer==7,
        "Original option helper scripts did not preserve lookup/case/default behavior");
    Value error = Value::Text(Kind::String,"unchanged OUT sentinel");
    auto output = std::make_shared<QuestVr::Vm::Reference>();
    output->zero = Value::Text(Kind::String,{});
    output->read = [&] { return error; };
    output->write = [&](const Value& value) { error=value; };
    const auto initialized = Call(game,"InitGame",{string(options),{error,output}});
    const auto baseMutator = ReadPortableActorScriptProperty(game,"BaseMutator");
    Require(initialized.committed && error.text=="unchanged OUT sentinel" &&
        ReadPortableActorScriptProperty(game,"MaxPlayers").integer==32 &&
        ReadPortableActorScriptProperty(game,"Difficulty").integer==2 &&
        ReadPortableActorScriptProperty(game,"AdminPassword").text=="fixture-admin" &&
        ReadPortableActorScriptProperty(game,"GamePassword").text=="fixture-game" &&
        QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(game,"bLocalLog")) &&
        QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(game,"bWorldLog")) &&
        baseMutator.kind==Kind::Object && !baseMutator.text.empty() &&
        QuestVr::Vm::ToBool(Call(baseMutator.text,"IsA",{Name("Mutator")}).value) &&
        GetPortableRuntimeMapActors(true).size()==originalActors+2u,
        "Actual InitGame did not execute original options and actual base Mutator birth");
    Require(SavePortableRuntimeState(saved.string()),"Cannot save original completed InitGame control");
    const auto complete = CheckpointBytes(saved);
    Require(QuestVr::DecodeScriptSavedState(ScriptBlob(complete)).births.size()==2u &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(level,"bBegunPlay")),
        "Scoped InitGame control fabricated world begun-play or lost two original births");
    GC::Collect();
    Require(LoadPortableRuntimeState(saved.string()) && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect)==complete,"Original InitGame warm save/GC changed the complete graph");
    Require(LoadPortableRuntimeState(legacy.string()) && UnloadPortableRuntimeMap() &&
        LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string())).passed &&
        LoadPortableRuntimeState(saved.string()) && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect)==complete && ReadPortableActorScriptProperty(game,"BaseMutator").text==baseMutator.text,
        "Original InitGame cold restore lost actual GameInfo/Mutator identities");
    Require(LoadPortableRuntimeState(before.string()),"Cannot restore original InitGame baseline");
    QuestVr::Vm::Limits budget; budget.instructions=100u;
    auto failed = ExecutePortableActorFunction(game,"InitGame",{string(options),{error,output}},budget);
    Require(!failed.passed() && failed.status==Status::Budget && !failed.committed &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==baseline &&
        error.text=="unchanged OUT sentinel","Bounded original InitGame failed to roll back its partial fields");
    failed=ExecutePortableActorFunction(game,"InitGame",{string("?MaxPlayers=3?Mutator=Engine.Mutator"),{error,output}});
    Require(!failed.passed() && !failed.committed && failed.status==Status::Unsupported &&
        failed.function=="Core.Object.DynamicLoadObject" && failed.offset==0u &&
        failed.callStack==std::vector<std::string>{"Engine.GameInfo.InitGame","Core.Object.DynamicLoadObject"} &&
        failed.error.find("Unsupported runtime native 0 (Core.Object.DynamicLoadObject)")!=std::string::npos &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==baseline &&
        GetPortableRuntimeMapActors(true).size()==originalActors+1u,
        "Unsupported real mutator option did not report exact evidence and roll back nested birth: "+
            failed.function+':'+std::to_string(failed.offset)+" "+failed.error);
    Require(LoadPortableRuntimeState(legacy.string()) && SavePortableRuntimeState(inspect.string()) &&
        CheckpointBytes(inspect)==original,"Scoped original InitGame left state/births behind");
    std::cout<<"ORIGINAL InitGame inherited by actual DeusExGameInfo: options/logging/base Mutator/OUT/rollback/warm+cold save; not automatic world startup\n";
}

void VerifyOriginalModelPointTrace(const std::filesystem::path& root,
    const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    const auto original=CheckpointBytes(legacy);
    const auto revision=GetPortableRuntimeWorldRevision(); const auto gc=GC::GetStats();
    const auto actorCount=GetPortableRuntimeMapActors(true).size();
    const auto model=LoadPortableRootModel68(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string()));
    const auto location=ReadPortableActorScriptProperty("00_Training.JaimeReyes0","Location");
    const auto height=QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty("00_Training.JaimeReyes0","CollisionHeight"));
    Require(location.kind==Kind::Vector && std::isfinite(height) && height>0.0f,
        "Original shadow trace lacks an authored pawn location/collision height");
    // ScriptedPawn.CreateShadow places the birth at Location-CollisionHeight,
    // with Pitch16384; Decal.BeginPlay requests a100-unit -RotationXAxis ray.
    const PortableModelVec3 start{location.vector[0],location.vector[1],location.vector[2]-height};
    const PortableModelVec3 end{start.x,start.y,start.z-100.0f};
    const auto hit=QuestVr::TraceOriginalModelPoint(model,start,end);
    Require(!hit.clear && hit.node>=0 && static_cast<std::size_t>(hit.node)<model.nodes.size() &&
        hit.time>=0.0f && hit.time<=1.0f,"Original Jaime shadow ray did not find a finite blocking BSP node");
    const auto& node=model.nodes.at(static_cast<std::size_t>(hit.node));
    Require(node.surface>=0 && static_cast<std::size_t>(node.surface)<model.surfaces.size() && node.vertexCount>0u &&
        node.planeZ>0.0f && hit.normal.z>0.0f,"Original shadow ray returned an invalid/nonfloor BSP node");
    const auto& surface=model.surfaces.at(static_cast<std::size_t>(node.surface));
    const auto base=model.points.at(static_cast<std::size_t>(surface.basePoint));
    const auto normal=model.vectors.at(static_cast<std::size_t>(surface.normalVector));
    Require(normal.z>0.0f,"Original shadow hit surface does not face the pawn");
    const double exactZ=(node.planeW-static_cast<double>(start.x)*node.planeX-
        static_cast<double>(start.y)*node.planeY)/node.planeZ;
    std::cout<<std::setprecision(10)<<"ORIGINAL SHADOW RAY start="<<start.x<<','<<start.y<<','<<start.z<<
        " node="<<hit.node<<" surface="<<node.surface<<" hit="<<hit.location.z<<" planeZ="<<exactZ<<
        " normal="<<normal.x<<','<<normal.y<<','<<normal.z<<" base="<<base.x<<','<<base.y<<','<<base.z<<'\n';
    Require(start.z-exactZ>=0.0 && start.z-exactZ<0.5 && hit.time==0.0f &&
        hit.location.x==start.x && hit.location.y==start.y && hit.location.z==start.z &&
        std::abs((static_cast<double>(start.x)-base.x)*normal.x+
            (static_cast<double>(start.y)-base.y)*normal.y+(exactZ-base.z)*normal.z)<0.001,
        "Original near-floor shadow ray did not clamp its half-unit backoff to the input start");
    // The actual feet are only0.099815 UE units above this floor, so the native
    // half-unit backoff clamps Time to0. Raise only the diagnostic start by2UE
    // units to exercise the unclamped wrapper without moving the actual actor.
    const PortableModelVec3 raised{start.x,start.y,start.z+2.0f};
    const auto backed=QuestVr::TraceOriginalModelPoint(model,raised,{raised.x,raised.y,raised.z-100.0f});
    Require(!backed.clear && backed.node==hit.node && backed.time>0.0f &&
        std::abs(backed.location.z-(exactZ+0.5))<0.0001 && backed.normal.z>0.0f,
        "Raised original shadow diagnostic ray lost its actual node or half-unit backoff");
    const auto repeat=QuestVr::TraceOriginalModelPoint(model,start,end);
    Require(repeat.node==hit.node && repeat.time==hit.time && repeat.location.z==hit.location.z &&
        repeat.stats.querySteps==hit.stats.querySteps,"Original BSP point query is not repeatable");
    const auto inspect=directory/"readonly-original-model-trace.sav";
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==original &&
        GetPortableRuntimeWorldRevision()==revision && GetPortableRuntimeMapActors(true).size()==actorCount &&
        GC::GetStats().numObjects==gc.numObjects && GC::GetStats().memoryUsage==gc.memoryUsage,
        "Read-only original BSP trace changed map/runtime/save/GC state");
    std::cout<<"ORIGINAL MODEL POINT TRACE Jaime shadow ray node="<<hit.node<<" surface="<<node.surface<<
        " time="<<hit.time<<" validation="<<hit.stats.validationSteps<<" visits="<<hit.stats.nodeVisits<<
        "; exact surface-plane/0.5UE backoff/read-only repeatability; NOT AttachDecal or rendered shadow completion\n";
}

void VerifyOriginalShadowDecalGeometry(const std::filesystem::path& root,
    const std::filesystem::path& legacy,const std::filesystem::path& directory) {
    // The actual class defaults and texture descriptor drive this probe. Never
    // reduce MultiDecalLevel to0 to make Training's required Shadow look valid.
    const auto multi=QuestVr::Vm::ToInt(ReadPortableClassDefault("DeusEx.Shadow","MultiDecalLevel"));
    const auto scale=QuestVr::Vm::ToFloat(ReadPortableClassDefault("DeusEx.Shadow","DrawScale"));
    const auto texture=ReadPortableClassDefault("DeusEx.Shadow","Texture");
    Require(multi==4 && scale==1.0f && texture.kind==Kind::Object && texture.text=="DeusExItems.Skins.FlatFXTex40",
        "Original inherited Shadow defaults no longer match audited MultiDecalLevel4/DrawScale1/FlatFXTex40");
    const auto package=LoadPortablePackageTables((root/"System"/"DeusExItems.u").string());
    // Package-table export paths are local to their package; runtime object
    // identities above are qualified. Strip only the proven package prefix.
    const auto textureExport=FindPortableTextureExport(package,texture.text.substr(std::string("DeusExItems.").size()));
    const auto properties=LoadPortableExportProperties(package,textureExport);
    std::int32_t uSize{},vSize{};
    for (const auto& property:properties.properties) {
        if (property.name!="USize" && property.name!="VSize") continue;
        Require(property.value.size()==4u,"Original shadow texture size is not INT");
        std::int32_t value{}; std::memcpy(&value,property.value.data(),4u);
        if (property.name=="USize") uSize=value; else vSize=value;
    }
    Require(uSize==32 && vSize==32,"Original Shadow texture is not authored32x32");
    const auto original=CheckpointBytes(legacy); const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats(); const auto actorCount=GetPortableRuntimeMapActors(true).size();
    const auto model=LoadPortableRootModel68(LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string()));
    const auto location=ReadPortableActorScriptProperty("00_Training.JaimeReyes0","Location");
    const auto height=QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty("00_Training.JaimeReyes0","CollisionHeight"));
    const PortableModelVec3 start{location.vector[0],location.vector[1],location.vector[2]-height};
    std::uint32_t seed=1u,repeatSeed=1u;
    const auto plan=QuestVr::ProjectOriginalDecalGeometry(model,start,{0,0,1},100,{},uSize,scale,multi,{},seed);
    const auto repeat=QuestVr::ProjectOriginalDecalGeometry(model,start,{0,0,1},100,{},uSize,scale,multi,{},repeatSeed);
    Require(plan.hasPrimaryGeometry && plan.primaryTraceNode==1848 && plan.primarySelectedNode==1852 &&
        plan.primaryCenter.z==-96.0f && !plan.surfaces.empty() && plan.surfaces.front().surface==1861 &&
        plan.surfaces.front().primary && plan.randomDraws==6 && plan.secondarySamples==16,
        "Actual original Shadow geometry did not cover trace/selected surface/exact floor/default sixteen-sample mode");
    Require(seed==repeatSeed && plan.surfaces.size()==repeat.surfaces.size() && plan.workSteps==repeat.workSteps,
        "Original Shadow projection is not repeatable");
    QuestVr::DecalState state(model.surfaces.size(),model.nodes.size());
    const auto resolver=[&](const std::string&) -> std::optional<std::string> { return texture.text; };
    std::size_t unrestricted{};
    for (std::size_t index=0;index<plan.surfaces.size();++index) {
        const auto& surface=plan.surfaces[index]; const auto& again=repeat.surfaces[index];
        Require(surface.surface==again.surface && surface.nodeMembership==again.nodeMembership,
            "Actual Shadow geometry lost repeatable node membership");
        if (surface.nodeMembership.empty()) ++unrestricted;
        for (const auto node:surface.nodeMembership)
            Require(node>=0 && static_cast<std::size_t>(node)<model.nodes.size() &&
                model.nodes[static_cast<std::size_t>(node)].surface==surface.surface,
                "Original Shadow membership node does not belong to its selected surface");
        QuestVr::DecalSurfaceRecord record; record.ownerIdentity="OriginalProbe.Shadow";
        record.nodeMembership=surface.nodeMembership;
        for (std::size_t corner=0;corner<4;++corner) {
            const auto p=surface.relativeCorners[corner],q=again.relativeCorners[corner];
            Require(p.x==q.x && p.y==q.y && p.z==q.z && std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                "Original Shadow relative corners differ/nonfinite");
            record.relativeCorners[corner]={p.x,p.y,p.z};
        }
        state.AttachPrimary(surface.surface,record,texture.text,resolver);
    }
    Require(state.Usage().records==plan.surfaces.size() && state.Usage().nodeMembership==plan.membershipEntries,
        "Original projected Shadow plan not publishable in ordered bounded state");
    const auto detached=state.Detach("OriginalProbe.Shadow");
    Require(detached.recordsRemoved==plan.surfaces.size() && !state.Usage().records && !state.Usage().surfListEntries,
        "Original projected Shadow geometry failed exact detach");
    const auto inspect=directory/"readonly-original-shadow-geometry.sav";
    Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==original &&
        GetPortableRuntimeWorldRevision()==revision && GetPortableRuntimeMapActors(true).size()==actorCount &&
        GC::GetStats().numObjects==gc.numObjects && GC::GetStats().memoryUsage==gc.memoryUsage,
        "Read-only original shadow geometry changed runtime/save/GC state");
    std::cout<<"ORIGINAL SHADOW GEOMETRY USize="<<uSize<<" DrawScale="<<scale<<" Multi="<<multi<<
        " primary="<<plan.surfaces.front().surface<<" projected-surfaces="<<plan.surfaces.size()<<
        " membership="<<plan.membershipEntries<<" unrestricted="<<unrestricted<<" RNGdraws="<<plan.randomDraws<<" grid="<<plan.secondarySamples<<
        " duplicate="<<plan.secondaryDuplicates<<" misses="<<plan.secondaryMisses<<" work="<<plan.workSteps<<
        "; real floor/coplanar selection/quad/grid/clipping/ordered state/detach/read-only replay; NOT native startup or rendered Shadow completion\n";
}

void VerifyOriginalDecalCheckpoint(const std::filesystem::path& root,
    const std::filesystem::path& legacy, const std::filesystem::path& directory) {
    Require(LoadPortableRuntimeState(legacy.string()), "Decal checkpoint test could not reset authored state");
    const auto baselineGc = GC::GetStats();
    const auto baselineActors = GetPortableRuntimeMapActors(true).size();
    const auto map = LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
    const auto model = LoadPortableRootModel68(map);
    const auto jaime = Snapshot("00_Training.JaimeReyes0");
    const auto height = QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty(jaime.objectPath,"CollisionHeight"));
    std::uint32_t seed = 1u;
    const auto plan = QuestVr::ProjectOriginalDecalGeometry(model,
        {jaime.x,jaime.y,jaime.z-height},{0,0,1},100,{},32,1,4,{},seed);
    Require(plan.hasPrimaryGeometry && plan.surfaces.size()==1u, "Decal checkpoint fixture lost original floor geometry");
    const auto spawn = [&]() {
        const auto value = Call(jaime.objectPath,"Engine.Actor.Spawn",{
            {Value::Text(Kind::Object,"DeusEx.Shadow"),{}},{Value::Text(Kind::Object,jaime.objectPath),{}}}).value;
        Require(value.kind==Kind::Object && !value.text.empty() &&
            ReadPortableActorScriptProperty(value.text,"SurfList").kind==Kind::IntArray,
            "Dormant original Shadow birth lacks a typed dynamic INT SurfList");
        return value.text;
    };
    const auto first=spawn(), second=spawn();
    const auto spawned=directory/"original-decal-dormant.sav";
    const auto fixture=directory/"original-decal-graph-v13.sav";
    const auto inspection=directory/"original-decal-inspect.sav";
    Require(SavePortableRuntimeState(spawned.string()),"Dormant original Shadow births could not save");
    const auto prefix=CheckpointBytes(spawned);
    auto saved=QuestVr::DecodeScriptSavedState(ScriptBlob(prefix));
    const auto surface=plan.surfaces.front().surface;
    QuestVr::DecalState graph(model.surfaces.size(),model.nodes.size());
    QuestVr::DecalSurfaceRecord record;
    record.nodeMembership=plan.surfaces.front().nodeMembership;
    record.nodeMembership.push_back(record.nodeMembership.front()); // Ordered native duplicates survive.
    record.moverRotation={123,-456,789};
    for(std::size_t index=0u;index<4u;++index) {
        const auto& point=plan.surfaces.front().relativeCorners[index];
        record.relativeCorners[index]={point.x,point.y,point.z};
    }
    for(const auto& owner:{first,second,first}) {
        record.ownerIdentity=owner;
        graph.AttachPrimary(surface,record,"DeusExItems.Skins.FlatFXTex40",
            [](const auto&) {return std::optional<std::string>("DeusExItems.Skins.FlatFXTex40");});
    }
    // Legitimate missing/foreign native mirror references: three pops remove
    // only the first owner's two records, leaving the other owner's record.
    graph.ReplaceOwnerSurfList(first,{surface,surface,surface});
    graph.ReplaceOwnerSurfList(second,{});
    for(auto& actor:saved.objects) if(actor.path==first || actor.path==second)
        actor.properties.push_back({"Engine.Decal.SurfList","SurfList",0u,
            Value::IntArray(actor.path==first ? std::vector<std::int32_t>{surface,surface,surface} : std::vector<std::int32_t>{})});
    saved.nativeDecals.emplace();
    auto& native=*saved.nativeDecals;
    native.mapName=saved.mapName;
    native.modelPath=QualifiedReference(map,static_cast<std::int32_t>(model.exportIndex+1u));
    native.levelPath=ReadPortableActorScriptProperty(first,"XLevel").text;
    native.surfaceCount=static_cast<std::uint32_t>(model.surfaces.size());
    native.nodeCount=static_cast<std::uint32_t>(model.nodes.size());
    native.state=graph.ReadOnlySnapshot(); saved.randomSeed=seed;
    auto bytes=ReplaceScriptBlob(prefix,QuestVr::EncodeScriptSavedState(saved)); PutWord(bytes,4u,13u);
    Require(QuestVr::WriteDurableSaveFile(fixture.string(),bytes) && ValidatePortableRuntimeState(fixture.string(),saved.mapName),
        "Original map-bound native decal checkpoint preflight failed");
    Require(LoadPortableRuntimeState(legacy.string()) && LoadPortableRuntimeState(fixture.string()) &&
        SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==bytes,
        "Cold native decal restore changed geometry/order/mirrors/RNG or replayed attachment");
    Require(GetPortableRuntimeMapActors(true).size()==baselineActors+2u &&
        GC::GetStats().numObjects==baselineGc.numObjects+3u && GetPortableRuntimeUnsavedScriptState(),
        "Native decal restore did not retain exactly two real Shadow births and one map GC graph");
    GC::Collect();
    Require(SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==bytes,
        "Native surface owner roots/geometry did not survive collection");
    const auto stableGc=GC::GetStats();
    for(unsigned iteration=0u;iteration<3u;++iteration)
        Require(LoadPortableRuntimeState(fixture.string()) && GC::GetStats().numObjects==stableGc.numObjects &&
            GC::GetStats().memoryUsage==stableGc.memoryUsage,
            "Repeated native decal replacement leaked retired graph/owner allocations");
    auto revision=GetPortableRuntimeWorldRevision();
    const auto reject=[&](auto mutate,const std::string& description) {
        auto invalid=saved; mutate(invalid);
        auto forged=ReplaceScriptBlob(prefix,QuestVr::EncodeScriptSavedState(invalid)); PutWord(forged,4u,13u);
        const auto bad=directory/"original-decal-invalid.sav";
        Require(QuestVr::WriteDurableSaveFile(bad.string(),forged) && !ValidatePortableRuntimeState(bad.string()) &&
            !LoadPortableRuntimeState(bad.string()) && GetPortableRuntimeWorldRevision()==revision &&
            SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==bytes &&
            GC::GetStats().numObjects==stableGc.numObjects,description+" changed or passed native decal preflight");
    };
    reject([](auto& value) {value.nativeDecals->modelPath="00_Training.NoSuchModel";},"Wrong root Model");
    reject([](auto& value) {++value.nativeDecals->nodeCount;},"Wrong Model count");
    reject([&](auto& value) {value.nativeDecals->levelPath=ReadPortableActorScriptProperty(first,"Level").text;},"LevelInfo instead of native Level");
    reject([&](auto& value) {value.nativeDecals->state.surfaces.at(static_cast<std::size_t>(surface)).front().ownerIdentity=jaime.objectPath;},"Non-Decal record owner");
    reject([&](auto& value) {value.nativeDecals->state.surfaces.at(static_cast<std::size_t>(surface)).front().nodeMembership={1848};},"Membership from another surface");
    reject([](auto& value) {value.nativeDecals->state.owners.front().surfaces.clear();},"Conflicting reflected/native mirror");
    reject([&](auto& value) {
        auto& owner=value.nativeDecals->state.surfaces.at(static_cast<std::size_t>(surface)).front().ownerIdentity;
        owner[0]='0'; owner[3]='t';
    },"Noncanonical owner alias");
    // Measure the actual original Destroyed script, then fail its last return
    // instruction after Detach and its original superclass callback completed.
    const auto measuredDetach=Call(first,"Engine.Decal.Destroyed");
    Require(measuredDetach.instructions>1u && ReadPortableActorScriptProperty(first,"SurfList").intArray.empty() &&
        LoadPortableRuntimeState(fixture.string()),"Original Decal.Destroyed did not execute native Detach");
    revision=GetPortableRuntimeWorldRevision();
    QuestVr::Vm::Limits tight; tight.instructions=measuredDetach.instructions-1u;
    const auto rollback=ExecutePortableActorFunction(first,"Engine.Decal.Destroyed",{},tight);
    Require(!rollback.passed() && SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==bytes &&
        GetPortableRuntimeWorldRevision()==revision,"Failed DetachDecal did not restore native/reflected graph atomically");
    Call(first,"Engine.Decal.DetachDecal");
    Require(GetPortableRuntimeWorldRevision()==revision+1u &&
        ReadPortableActorScriptProperty(first,"SurfList").intArray.empty() && SavePortableRuntimeState(inspection.string()),
        "Original DetachDecal did not clear SurfList and publish one revision");
    const auto detached=QuestVr::DecodeScriptSavedState(ScriptBlob(CheckpointBytes(inspection)));
    const auto& remaining=detached.nativeDecals->state.surfaces.at(static_cast<std::size_t>(surface));
    Require(remaining.size()==1u && remaining.front().ownerIdentity==second && detached.randomSeed==saved.randomSeed,
        "Original reverse-pop DetachDecal removed foreign records or changed RNG");
    bool unloadRefused{}; try {UnloadPortableRuntimeMap();} catch(const std::exception&) {unloadRefused=true;}
    Require(unloadRefused && LoadPortableRuntimeState(legacy.string()) &&
        GC::GetStats().numObjects==baselineGc.numObjects && GC::GetStats().memoryUsage==baselineGc.memoryUsage &&
        SavePortableRuntimeState(inspection.string()) && CheckpointBytes(inspection)==CheckpointBytes(legacy),
        "Legacy reset/unload policy lost or leaked native decal graph state");
    std::cout<<"ORIGINAL DECAL CHECKPOINT codec10/envelope13: real Shadow births, original floor geometry, ordered duplicates/mismatches, cold rooted replacement/GC, symbolic refusals, Detach rollback/revision, legacy reset; NOT Attach/startup/render completion\n";
}

void VerifyOriginalWorldStartupProbe(const std::filesystem::path& legacy,
    const std::filesystem::path& directory) {
    // An explicitly owned synthetic stereo target exercises the actual live
    // client guard. This is not a Quest viewport or player-possession claim.
    std::array<std::uint32_t, 8u * 8u * 2u> testStereoTarget{};
    static_cast<void>(testStereoTarget);
    auto clientLifetime = std::make_shared<QuestVr::NativeClientLifetime>();
    Require(!HasPortableRuntimeClient(), "Original world probe inherited an unowned client");
    BindPortableRuntimeClient(clientLifetime, 1u);
    RegisterPortableRuntimeViewport(1u, {1u, 8u, 8u, 1u, 2u});
    struct ClientScope {
        std::shared_ptr<QuestVr::NativeClientLifetime> lifetime;
        ~ClientScope() { lifetime->Revoke(); ClearPortableRuntimeClient(); }
    } clientScope{clientLifetime};
    const auto baseline = CheckpointBytes(legacy);
    const auto actors = GetPortableRuntimeMapActors(true);
    Require(!actors.empty() && !GetPortableRuntimeScriptStatePresent(),
        "Original world probe did not start from untouched map state");
    const auto level = ReadPortableActorScriptProperty(actors.front().objectPath, "Level");
    Require(level.kind == Kind::Object && !level.text.empty() &&
        !QuestVr::Vm::ToBool(ReadPortableActorScriptProperty(level.text, "bBegunPlay")),
        "Original world probe was seeded with begun play");
    const auto originalGame = ReadPortableActorScriptProperty(level.text, "Game");
    Require(originalGame.kind == Kind::Object && originalGame.text.empty(),
        "Original world probe was seeded with a manually constructed GameInfo");
    const auto revision = GetPortableRuntimeWorldRevision();
    const auto gc = GC::GetStats();
    const auto inspect = directory / "world-startup-probe-rollback.sav";
    PortableWorldStartupProbeOptions options;
    options.defaultGameClass = "DeusEx.DeusExGameInfo";
    QuestVr::Vm::Limits limits;
    limits.instructions = 4'000'000u; limits.nodes = 1'048'576u;
    limits.writes = 1'048'576u; limits.retainedBytes = 128u << 20u;
    const auto verifyRollback = [&] {
        Require(SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect) == baseline &&
            GetPortableRuntimeWorldRevision() == revision && !GetPortableRuntimeScriptStatePresent() &&
            GetPortableRuntimeMapActors(true).size() == actors.size() &&
            HasPortableRuntimeClient() && ReadPortableRuntimeClientView().size() == 1u &&
            QuestVr::Vm::Equal(ReadPortableActorScriptProperty(level.text, "Game"), originalGame) &&
            GC::GetStats().numObjects == gc.numObjects && GC::GetStats().memoryUsage == gc.memoryUsage,
            "Original world probe retained partial authored writes/frames/births/RNG/native publication");
    };
    const auto result = ProbePortableWorldStartup(options, limits);
    std::cout << "WORLD PROBE BOUNDARY phase=" << PortableWorldStartupPhaseName(result.phase)
        << " slot=" << result.actorSlot << " actor=" << result.actorPath
        << " function=" << result.execution.function << ':' << result.execution.offset
        << " opcode=" << static_cast<unsigned>(result.execution.opcode)
        << " error=" << result.execution.error << std::endl;
    std::cout << "WORLD PROBE STACK";
    for (const auto& function:result.execution.callStack) std::cout << " -> " << function;
    std::cout << std::endl;
    Require(!result.execution.passed() && !result.execution.committed && !result.restoredWorld &&
        !result.execution.error.empty() && !result.gameInfoPath.empty() &&
        result.visited.at(static_cast<std::size_t>(PortableWorldStartupPhase::InitializeExecution)) == actors.size() + 1u &&
        result.visited.at(static_cast<std::size_t>(PortableWorldStartupPhase::InitGame)) == 1u,
        "Original world prefix did not perform true GameInfo birth + all-world InitExecution + InitGame: " +
            std::string(PortableWorldStartupPhaseName(result.phase)) + " " + result.execution.error);
    Require(result.phase == PortableWorldStartupPhase::PreBeginPlay && result.actorSlot == 1332u &&
        result.actorPath == "00_Training.Camera5" && result.gameInfoPath == "00_Training.DeusExGameInfo0" &&
        result.execution.status == Status::Unsupported && result.execution.function == "Engine.PlayerPawn.ConsoleCommand" &&
        result.execution.offset == 0u && result.execution.opcode == 0u &&
        result.execution.error == "Unsupported runtime native 0 (Engine.PlayerPawn.ConsoleCommand)" &&
        result.execution.callStack.size() >= 2u &&
        result.execution.callStack.back() == "Engine.PlayerPawn.ConsoleCommand",
        "Unmodified Training first startup gap changed without an updated original contract: " +
            std::string(PortableWorldStartupPhaseName(result.phase)) + " " + result.actorPath + " " + result.execution.error);
    verifyRollback();
    const auto again = ProbePortableWorldStartup(options, limits);
    Require(again.phase == result.phase && again.actorSlot == result.actorSlot &&
        again.actorPath == result.actorPath && again.gameInfoPath == result.gameInfoPath &&
        again.execution.function == result.execution.function && again.execution.offset == result.execution.offset &&
        again.execution.opcode == result.execution.opcode && again.execution.error == result.execution.error &&
        again.execution.callStack == result.execution.callStack && again.visited == result.visited,
        "Original world probe did not reproduce exact first-failure/birth name after rollback");
    verifyRollback();
    auto shortBudget = limits; shortBudget.instructions = 2u;
    const auto bounded = ProbePortableWorldStartup(options, shortBudget);
    Require(!bounded.execution.passed() && !bounded.execution.committed &&
        bounded.phase == PortableWorldStartupPhase::InitGame,
        "Original world probe ignored shared script instruction budget");
    verifyRollback();
    std::cout << "ORIGINAL WORLD PROBE " << actors.size() << " actors, direct GameInfo=" << result.gameInfoPath
        << " phase=" << PortableWorldStartupPhaseName(result.phase) << " slot=" << result.actorSlot
        << " actor=" << result.actorPath << " function=" << result.execution.function << ':' << result.execution.offset
        << " opcode=" << static_cast<unsigned>(result.execution.opcode) << " error=" << result.execution.error
        << "; native280 and actual GameInfo.DefaultPlayerName ClassContext passed; next original Camera5 PlayerPawn.ConsoleCommand boundary; owned synthetic client/viewport, exact repeated failure, all state/GC/birth/revision bytes rolled back; NOT world/player completion\n";
}

void TestOriginal(const std::filesystem::path& root, const bool inventoryOnly = false,
    const bool switchOnly = false, const bool randomOnly = false, const bool sleepOnly = false,
    const bool landingOnly = false, const bool dispatchOnly = false, const bool startupOnly = false,
    const bool worldStartupOnly = false) {
    static constexpr const char* packages[] = {
        "ConSys", "Core", "DeusEx", "DeusExCharacters", "DeusExConAudioAIBarks",
        "DeusExConAudioEndGame", "DeusExConAudioHK_Shared", "DeusExConAudioIntro",
        "DeusExConAudioMission00", "DeusExConAudioMission01", "DeusExConAudioMission02",
        "DeusExConAudioMission03", "DeusExConAudioMission04", "DeusExConAudioMission05",
        "DeusExConAudioMission08", "DeusExConAudioMission09", "DeusExConAudioMission10",
        "DeusExConAudioMission11", "DeusExConAudioMission12", "DeusExConAudioMission14",
        "DeusExConAudioMission15", "DeusExConAudioNYShared", "DeusExConText",
        "DeusExConversations", "DeusExDeco", "DeusExItems", "DeusExSounds", "DeusExText",
        "DeusExUI", "Editor", "Engine", "Extension", "Fire", "IpDrv", "IpServer",
        "MPCharacters", "UBrowser", "UWindow"};
    std::vector<PortablePackageTables> tables;
    for (const auto* package : packages)
        tables.push_back(LoadPortablePackageTables((root / "System" / (std::string(package) + ".u")).string()));
    struct Shutdown { ~Shutdown() { ShutdownPortableRuntime(); } } shutdown;
    std::cout << "ORIGINAL RUNTIME initializing " << tables.size() << " script packages\n";
    const auto runtime = InitializePortableRuntime(tables);
    Require(runtime.passed && runtime.states != 0 && runtime.normalizedStateBytecodeBytes != 0,
        "Original script runtime initialization/state metadata failed");
    std::cout << "ORIGINAL RUNTIME indexed objects=" << runtime.objects << " classes=" << runtime.classes <<
        " states=" << runtime.states << '\n';
    if (!inventoryOnly) {
        VerifyAuthoredStateMetadata(root);
        VerifyOriginalDispatchMetadata(tables,runtime);
        VerifyOriginalDispatchPrograms(tables,runtime);
    }
    if (dispatchOnly) return;
    TemporaryCheckpoint temporary(root);
    const auto checkpoint = (temporary.directory / "original-authored.sav").string();
    std::size_t humanTests{}, robotTests{}, birdTests{};
    InventorySourceCoverage inventoryCoverage;
    for (const auto* map : {"00_Training", "01_NYC_UNATCOIsland", "00_Intro"}) {
        std::cout << "ORIGINAL MAP loading " << map << '\n';
        Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root / "Maps" /
            (std::string(map) + ".dx")).string())).passed, "Original map load failed");
        Require(!GetPortableRuntimeUnsavedScriptState(), "Map replacement retained retired VM state");
        Require(SavePortableRuntimeState(checkpoint), "Untouched authored runtime save failed");
        const auto checkpointBytes = std::filesystem::file_size(checkpoint);
        Require(Word(CheckpointBytes(checkpoint),4)==3u,"Untouched runtime unexpectedly changed legacy v3 save format");
        const auto actors = GetPortableRuntimeMapActors();
        if (worldStartupOnly) {
            VerifyOriginalModelPointTrace(root,checkpoint,temporary.directory);
            VerifyOriginalShadowDecalGeometry(root,checkpoint,temporary.directory);
            VerifyOriginalDecalCheckpoint(root,checkpoint,temporary.directory);
            VerifyOriginalWorldStartupProbe(checkpoint,temporary.directory); UnloadPortableRuntimeMap(); return;
        }
        if (startupOnly) { VerifyOriginalInitGame(root,checkpoint,temporary.directory); UnloadPortableRuntimeMap(); return; }
        if (std::string(map) == "00_Training") {
            VerifyOriginalModelPointTrace(root,checkpoint,temporary.directory);
            VerifyOriginalShadowDecalGeometry(root,checkpoint,temporary.directory);
            VerifyColdOriginalAnimationAssets(root, checkpoint, temporary.directory);
            // Native checkpoint restore rebuilds collision and may load actor
            // meshes. Keep the independent cold-asset control genuinely cold.
            VerifyOriginalDecalCheckpoint(root,checkpoint,temporary.directory);
        }
        if (std::string(map)=="00_Training") VerifyOriginalInitGame(root,checkpoint,temporary.directory);
        if (std::string(map) == "00_Training") VerifyOriginalRandom("00_Training.Doctor1",checkpoint,temporary.directory);
        if (std::string(map) == "00_Training") VerifyOriginalSleep(root,tables,"00_Training.Doctor1",checkpoint,temporary.directory);
        if (std::string(map) == "00_Training") VerifyOriginalLanding(root,tables,"00_Training.Doctor1",checkpoint,temporary.directory);
        if (std::string(map) == "00_Training") VerifyModernStateControl(root,tables,"00_Training.Doctor1",checkpoint,temporary.directory);
        if (landingOnly) {UnloadPortableRuntimeMap();return;}
        if (sleepOnly) {UnloadPortableRuntimeMap();return;}
        if (randomOnly) {
            UnloadPortableRuntimeMap();
            Require(!GetPortableRuntimeScriptStatePresent(),"Focused original random suite leaked state");
            return;
        }
        if (switchOnly) {
            VerifyOriginalSwitchHeadTurn("00_Training.Doctor1", checkpoint, temporary.directory);
            UnloadPortableRuntimeMap();
            Require(!GetPortableRuntimeUnsavedScriptState(), "Focused original Switch suite leaked actor state");
            return;
        }
        VerifyOriginalInventorySources(tables, LoadPortablePackageTables((root / "Maps" /
            (std::string(map) + ".dx")).string()), inventoryCoverage);
        for (const auto& actor : actors) {
            if (humanTests != 0u && robotTests != 0u && birdTests != 0u) break;
            if (!actor.pawn || actor.meshPath.empty() || !IsA(actor, "ScriptedPawn")) continue;
            if (humanTests == 0u && !IsA(actor, "Robot") && !IsA(actor, "Animal")) {
                VerifyOriginalDormantSpawn(root,actor.objectPath,checkpoint,temporary.directory);
                VerifyOriginalActorLookup(root,actor.objectPath,checkpoint,temporary.directory);
                VerifyOriginalBlendCommands(root,actor.objectPath,checkpoint,temporary.directory);
                VerifyOriginalSwitchHeadTurn(actor.objectPath,checkpoint,temporary.directory);
                VerifyOriginalAIEvents(actor.objectPath,checkpoint,temporary.directory);
                VerifyOriginalInventoryTransactions(actor.objectPath, checkpoint, temporary.directory);
                if (inventoryOnly) { ++humanTests; continue; }
                VerifyAuthoredStack(root, actor.objectPath);
                VerifyStoppedDispatch(root,actor.objectPath);
                Require(ResolvePortableActorState(actor.objectPath,"Auto")=="DeusEx.ScriptedPawn.StartUp" &&
                    ResolvePortableActorState(actor.objectPath,"Standing")=="DeusEx.ScriptedPawn.Standing" &&
                    ResolvePortableActorFunction(actor.objectPath,"Standing","AnimEnd")=="DeusEx.ScriptedPawn.Standing.AnimEnd" &&
                    ResolvePortableActorFunction(actor.objectPath,"Standing","AnimEnd",QuestVr::ScriptDispatch::LookupKind::Global)=="Engine.Actor.AnimEnd",
                    "Original Auto/named state or Virtual/Global function selection diverged");
                const auto authoredStack = ReadPortableActorSerializedStack(actor.objectPath);
                const auto region = ReadPortableActorScriptProperty(actor.objectPath, "Region");
                Require(region.kind == Kind::Struct && !region.fields.at("zone").text.empty(),
                    "Original BSP Region did not resolve a real ZoneInfo/LevelInfo");
                const auto handed = Call(actor.objectPath, "HasTwoHandedWeapon");
                Require(handed.value.kind == Kind::Bool, "Original lazy weapon query did not return bool");
                Call(actor.objectPath, "PlayWaiting");
                Require(Snapshot(actor.objectPath).animation.sequence != "None",
                    "Original PlayWaiting failed to select the original script sequence");
                const auto beforeDisabled=Snapshot(actor.objectPath);
                const auto beforeProbeContext=ReadPortableActorDispatchContext(actor.objectPath);
                Require(QuestVr::ScriptDispatch::IsEnabled("AnimEnd",beforeProbeContext.classProbeMask,
                    beforeProbeContext.codeMasks,beforeProbeContext.disabledNames,beforeProbeContext.liveProbeMask),
                    "Original positive AnimEnd bit was inverted before the fully-qualified gate control");
                const auto beforeProbeCheckpoint=temporary.directory/"before-qualified-probe-v4.sav";
                Require(SavePortableRuntimeState(beforeProbeCheckpoint.string()),
                    "Could not retain complete original state before explicit probe Disable");
                const auto beforeProbeBytes=CheckpointBytes(beforeProbeCheckpoint);
                Require(Word(beforeProbeBytes,4u)==4u && !ReadPortableActorStateObject(actor.objectPath),
                    "Before-probe original actor unexpectedly owns portable state storage");
                Call(actor.objectPath,"Disable",{Name("AnimEnd")});
                const auto disabled=Call(actor.objectPath, "DeusEx.ScriptedPawn.Standing.AnimEnd");
                Require(disabled.value.kind==Kind::Nothing && disabled.instructions==0u && Same(beforeDisabled,Snapshot(actor.objectPath)),
                    "Fully-qualified probe function bypassed an actual owned-frame Disable");
                Require(LoadPortableRuntimeState(beforeProbeCheckpoint.string()) && !ReadPortableActorStateObject(actor.objectPath) &&
                    Same(beforeDisabled,Snapshot(actor.objectPath)) && SavePortableRuntimeState(beforeProbeCheckpoint.string()) &&
                    CheckpointBytes(beforeProbeCheckpoint)==beforeProbeBytes,
                    "Fully-qualified disabled-probe control retained state or changed the complete v4 baseline");
                const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
                Require(mesh.animation && !mesh.animation->sequences.empty(), "Original animation mesh unavailable");
                const auto usable = std::find_if(mesh.animation->sequences.begin(), mesh.animation->sequences.end(), [&](const auto& s) {
                    return s.numFrames > 1 && QuestVr::HasUsableMeshAnimationSpan(*mesh.animation, s);
                });
                Require(usable != mesh.animation->sequences.end(), "Original human mesh lacks a multi-frame sequence");
                Call(actor.objectPath, "PlayAnimPivot", {Name(usable->name), Number(0.0f), Number(0.0f), Vector({3.0f, 4.0f, 5.0f})});
                auto pose = Snapshot(actor.objectPath);
                Require(std::abs(pose.animation.rate - usable->rate / static_cast<float>(usable->numFrames)) < 0.00001f &&
                    std::abs(pose.animation.tweenRate - 10.0f / static_cast<float>(usable->numFrames)) < 0.00001f,
                    "Original script optional-zero Rate/TweenTime defaults were not executed");
                Call(actor.objectPath, "PlayAnimPivot", {Name(usable->name), Number(1.0f), Number(-1.0f), Vector({3.0f, 4.0f, 5.0f})});
                pose = Snapshot(actor.objectPath);
                const auto desired = ReadPortableActorScriptProperty(actor.objectPath, "DesiredPrePivot");
                Require(desired.kind == Kind::Vector && pose.prePivotX == desired.vector[0] &&
                    pose.prePivotY == desired.vector[1] && pose.prePivotZ == desired.vector[2],
                    "Original PrePivotTime<=0 assignment did not reach rendered snapshot");
                // Restore positive-time helper defaults before the next checks.
                Call(actor.objectPath, "PlayAnimPivot", {Name(usable->name), Number(0.0f), Number(0.0f), Vector({3.0f, 4.0f, 5.0f})});
                Require(QuestVr::Vm::ToFloat(ReadPortableActorScriptProperty(actor.objectPath, "PrePivotTime")) == 0.1f,
                    "Original PlayAnimPivot property assignment is missing");
                Call(actor.objectPath, "LoopAnimPivot", {Name(usable->name), Number(2.0f), Number(0.2f), Number(0.5f), Vector({0.0f, 0.0f, 0.0f})});
                pose = Snapshot(actor.objectPath);
                Require(pose.animation.loop && std::abs(pose.animation.minRate - 0.5f * usable->rate /
                    static_cast<float>(usable->numFrames)) < 0.00001f, "Original LoopAnimPivot/native MinRate mismatch");
                Call(actor.objectPath, "TweenAnimPivot", {Name(usable->name), Number(0.25f), Vector({1.0f, 2.0f, 3.0f})});
                pose = Snapshot(actor.objectPath);
                Require(pose.animation.rate == 0.0f && pose.animation.frame < 0.0f,
                    "Original TweenAnimPivot did not stage actual native tween state");
                // Force failure after native command + first actor-property write.
                // The complete transaction, including captured tween history, must undo.
                const auto timeBefore = ReadPortableActorScriptProperty(actor.objectPath, "PrePivotTime");
                QuestVr::Vm::Limits limits; limits.writes = 1u;
                const auto failed = ExecutePortableActorFunction(actor.objectPath, "PlayAnimPivot",
                    {Name(usable->name), Number(3.0f), Number(0.5f), Vector({9.0f, 8.0f, 7.0f})}, limits);
                Require(failed.status == Status::Budget && Same(pose, Snapshot(actor.objectPath)) &&
                    QuestVr::Vm::Equal(timeBefore, ReadPortableActorScriptProperty(actor.objectPath, "PrePivotTime")),
                    "Failed original script leaked property/clock/tween changes");
                const auto scriptCheckpoint=temporary.directory/"committed-script-v4.sav";
                const auto scriptInspection=temporary.directory/"restored-script-v4.sav";
                const auto savedProperties=ActorProperties(actor.objectPath);
                Require(GetPortableRuntimeUnsavedScriptState() && SavePortableRuntimeState(scriptCheckpoint.string()),
                    "Committed VM properties/native tween history could not be saved as v4");
                const auto serialized=CheckpointBytes(scriptCheckpoint);
                const auto blob=ScriptBlob(serialized);
                Require(std::filesystem::file_size(checkpoint)==checkpointBytes,"v4 save modified the separate legacy checkpoint");
                Call(actor.objectPath,"PlayAnimPivot",{Name(usable->name),Number(4.0f),Number(0.75f),Vector({9.0f,8.0f,7.0f})});
                const auto mutated=Snapshot(actor.objectPath);
                const auto mutatedProperties=ActorProperties(actor.objectPath);
                Require(!Same(pose,mutated),"v4 restore fixture failed to mutate native clock state");
                Require(ValidatePortableRuntimeState(scriptCheckpoint.string()) && Same(mutated,Snapshot(actor.objectPath)),
                    "Readonly v4 validation mutated live native animation state");
                std::string foldedMap=map;
                for(char& c:foldedMap) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
                Require(ValidatePortableRuntimeState(scriptCheckpoint.string(),foldedMap) &&
                    !ValidatePortableRuntimeState(scriptCheckpoint.string(),"00_TrainingCombat") &&
                    Same(mutated,Snapshot(actor.objectPath)),
                    "v4 metadata map binding ignored mismatch/case identity or changed live state");
                SameProperties(actor.objectPath,mutatedProperties,"Readonly v4 validation changed property overlay");
                Require(LoadPortableRuntimeState(scriptCheckpoint.string()) && GetPortableRuntimeUnsavedScriptState() &&
                    Same(pose,Snapshot(actor.objectPath)),"v4 restore lost native rates/flags/main or blend tween history");
                SameProperties(actor.objectPath,savedProperties,"v4 restore lost saved script property value");
                Require(SavePortableRuntimeState(scriptInspection.string()) &&
                    ScriptBlob(CheckpointBytes(scriptInspection))==blob,
                    "v4 roundtrip changed complete clock fields or canonical property payload");
                auto savedState=QuestVr::DecodeScriptSavedState(blob);
                auto restoredState=QuestVr::DecodeScriptSavedState(ScriptBlob(CheckpointBytes(scriptInspection)));
                const auto clockFor=[&](auto& state)->QuestVr::ActorAnimationClock& {
                    const auto found=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto& value) {return value.path==actor.objectPath;});
                    Require(found!=state.objects.end() && found->clock,"Saved native animation clock absent");return *found->clock;
                };
                // Pure native clock continuation from exact saved fields is
                // checked separately from VM notify/state scheduling (not yet
                // dispatched by this test or the scoped actor bridge).
                const auto absent=[](const std::string&) {return false;};
                const auto ignore=[](const QuestVr::ActorAnimationEvent&) {};
                const auto first=QuestVr::AdvanceActorAnimationClock(mesh.animation.get(),clockFor(savedState),0.3f,0.0f,absent,ignore);
                const auto second=QuestVr::AdvanceActorAnimationClock(mesh.animation.get(),clockFor(restoredState),0.3f,0.0f,absent,ignore);
                Require(first.ok && second.ok && !first.budgetExhausted && !second.budgetExhausted &&
                    first.events.size()==second.events.size() &&
                    QuestVr::EncodeScriptSavedState(savedState)==QuestVr::EncodeScriptSavedState(restoredState),
                    "Saved/restored full native tween clock produced a different continuation");
                ScriptSchemaPositiveReferences(serialized,pose,savedProperties,scriptCheckpoint,temporary.directory);
                VerifyEventLevelBindings(serialized,pose,scriptCheckpoint,temporary.directory);
                auto truncated=serialized;truncated.pop_back();
                const auto broken=temporary.directory/"truncated-script-v4.sav";
                Require(QuestVr::WriteDurableSaveFile(broken.string(),truncated) &&
                    !ValidatePortableRuntimeState(broken.string()) && !LoadPortableRuntimeState(broken.string()) &&
                    Same(pose,Snapshot(actor.objectPath)),"Truncated v4 tail was accepted or changed live clock");
                SameProperties(actor.objectPath,savedProperties,"Truncated v4 load changed property overlay");
                ScriptSchemaRejections(serialized,pose,savedProperties,temporary.directory);
                Require(ValidatePortableRuntimeState(checkpoint) && GetPortableRuntimeUnsavedScriptState(),
                    "Readonly checkpoint validation changed committed VM state");
                Require(LoadPortableRuntimeState(checkpoint) && !GetPortableRuntimeUnsavedScriptState(),
                    "Valid legacy checkpoint did not restore authored VM state");
                Require(SameStack(authoredStack, ReadPortableActorSerializedStack(actor.objectPath)),
                    "Legacy save restoration lost retained authored stack identity");
                // Preflight may resolve the saved original map schema while a
                // different map is loaded; applying v4 still requires that map
                // to be current, with no cross-map mutation as a side effect.
                Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_TrainingCombat.dx").string())).passed,
                    "Different-map v4 preflight fixture failed");
                bool retiredActorMissing{};
                try { ReadPortableActorSerializedStack(actor.objectPath); }
                catch (const std::exception&) { retiredActorMissing = true; }
                Require(retiredActorMissing, "Map replacement retained retired actor stack metadata");
                const auto otherBefore=GetPortableRuntimeMapActors();
                Require(ValidatePortableRuntimeState(scriptCheckpoint.string()) && !LoadPortableRuntimeState(scriptCheckpoint.string()),
                    "v4 cross-map schema preflight failed or wrong-current-map application succeeded");
                const auto otherAfter=GetPortableRuntimeMapActors();
                Require(otherAfter.size()==otherBefore.size() && !GetPortableRuntimeUnsavedScriptState(),
                    "Rejected wrong-current-map load changed actor count or script overlays");
                for(std::size_t i=0;i<otherBefore.size();++i)
                    Require(otherBefore[i].objectPath==otherAfter[i].objectPath && Same(otherBefore[i],otherAfter[i]),
                        "Rejected wrong-current-map v4 load changed current actors");
                Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/(std::string(map)+".dx")).string())).passed &&
                    LoadPortableRuntimeState(checkpoint),"Could not reset original map after readonly cross-map preflight");
                VerifyOriginalStateExecution(actor.objectPath,checkpoint,temporary.directory);
                Require(SameStack(authoredStack, ReadPortableActorSerializedStack(actor.objectPath)),
                    "Portable state execution changed immutable authored stack metadata");
                ++humanTests;
                std::cout << "ORIGINAL HUMAN " << actor.objectPath << " PlayWaiting/Standing.AnimEnd/Play/Loop/Tween/rollback/save passed\n";
            }
            if (inventoryOnly) continue;
            if (robotTests == 0u && IsA(actor, "Robot")) {
                VerifyAuthoredStack(root, actor.objectPath);
                VerifyStoppedDispatch(root,actor.objectPath);
                Call(actor.objectPath, "PlayWaiting");
                const auto robot = Snapshot(actor.objectPath);
                Require(!robot.animation.sequence.empty() && robot.animation.frame < 0.0f,
                    "Original Robot.PlayWaiting optional helper parameters were not executed");
                ++robotTests;
                std::cout << "ORIGINAL ROBOT " << actor.objectPath << " PlayWaiting passed\n";
            }
            if (birdTests == 0u && IsA(actor, "Bird")) {
                VerifyAuthoredStack(root, actor.objectPath);
                VerifyStoppedDispatch(root,actor.objectPath);
                const auto wait = ReadPortableActorScriptProperty(actor.objectPath, "WaitAnim");
                Call(actor.objectPath, "PlayWaiting");
                const auto bird = Snapshot(actor.objectPath);
                const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
                const auto* expected = QuestVr::FindMeshAnimationSequence(*mesh.animation, wait.text, true);
                Require(expected && bird.animation.sequence == expected->name && bird.animation.loop,
                    "Original Bird.PlayWaiting native optional defaults were not executed");
                VerifyOriginalBirdRandom(actor.objectPath,checkpoint,temporary.directory);
                ++birdTests;
            }
            if (GetPortableRuntimeUnsavedScriptState()) {
                const auto before = Snapshot(actor.objectPath);
                Require(!LoadPortableRuntimeMap(LoadPortablePackageTables((root / "Maps" /
                    (std::string(map) + ".dx")).string())).passed && Same(before, Snapshot(actor.objectPath)),
                    "Map replacement silently discarded committed script state");
                bool refused{};
                try { UnloadPortableRuntimeMap(); } catch (const std::exception&) { refused = true; }
                Require(refused && Same(before, Snapshot(actor.objectPath)),
                    "Map unload silently discarded committed script state");
                Require(LoadPortableRuntimeState(checkpoint), "Could not explicitly restore authored checkpoint");
            }
        }
    }
    Require(humanTests == 1u && (inventoryOnly || (robotTests == 1u && birdTests == 1u)),
        "Original maps did not cover required human, robot and bird execution fixtures");
    Require(inventoryCoverage.actors != 0u && inventoryCoverage.mapEntries != 0u &&
        inventoryCoverage.inheritedEntries != 0u && inventoryCoverage.slots == inventoryCoverage.actors * 8u,
        "Original inventory inspection did not cover real map overrides and inherited defaults in all fixed slots");
    UnloadPortableRuntimeMap();
    Require(!GetPortableRuntimeUnsavedScriptState(), "Map unload leaked actor VM overlays");
}
} // namespace

int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        if (argc == 1) {
            std::cout << "SKIP: supply readonly original Deus Ex installation for real actor script integration\n";
            return 77;
        }
        Require(argc == 2 || (argc == 3 && (std::string(argv[2]) == "--inventory-only" ||
                std::string(argv[2]) == "--switch-only" || std::string(argv[2]) == "--random-only" || std::string(argv[2]) == "--sleep-only" || std::string(argv[2]) == "--landing-only" || std::string(argv[2]) == "--dispatch-only" || std::string(argv[2]) == "--startup-only" || std::string(argv[2]) == "--world-startup-only")),
            "Usage: portable_actor_script_test GAME_ROOT [--inventory-only|--switch-only|--random-only|--sleep-only|--landing-only|--dispatch-only|--startup-only|--world-startup-only]");
        const bool switchOnly = argc == 3 && std::string(argv[2]) == "--switch-only";
        const bool randomOnly = argc == 3 && std::string(argv[2]) == "--random-only";
        const bool sleepOnly = argc == 3 && std::string(argv[2]) == "--sleep-only";
        const bool landingOnly = argc == 3 && std::string(argv[2]) == "--landing-only";
        const bool dispatchOnly = argc == 3 && std::string(argv[2]) == "--dispatch-only";
        const bool startupOnly = argc == 3 && std::string(argv[2]) == "--startup-only";
        const bool worldStartupOnly = argc == 3 && std::string(argv[2]) == "--world-startup-only";
        TestOriginal(std::filesystem::path(argv[1]), argc == 3 && !switchOnly && !randomOnly && !sleepOnly && !landingOnly && !dispatchOnly && !startupOnly && !worldStartupOnly,
            switchOnly,randomOnly,sleepOnly,landingOnly,dispatchOnly,startupOnly,worldStartupOnly);
        if (worldStartupOnly) {std::cout<<"PASS focused original non-publishing world startup/rollback probe\n";return 0;}
        if (startupOnly) {std::cout<<"PASS focused original InitGame startup primitive suite\n";return 0;}
        if (dispatchOnly) {std::cout<<"PASS focused original readonly dispatch/source-program audit\n";return 0;}
        if (landingOnly) {std::cout<<"PASS focused original WaitForLanding/native actor wait suite\n";return 0;}
        if (sleepOnly) {std::cout<<"PASS focused original Sleep/native actor wait suite\n";return 0;}
        if (switchOnly) { std::cout << "PASS focused original cold-assets/head-Switch suite\n"; return 0; }
        if (randomOnly) { std::cout << "PASS focused original cold-assets/Rand/FRand/RandRange suite\n"; return 0; }
        std::cout << "PASS original actor authored struct inspection, member writes, typed references, v4/v5 composition and rollback"
            << (argc == 2 ? "; full actor bytecode/natives/BSP Region/state/clock suite" : "; focused inventory suite") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
