#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_actor_geometry.h"
#include "quest_mesh_animation.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
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
void VerifyStoppedDispatch(const std::string& actor) {
    const auto context=ReadPortableActorDispatchContext(actor);
    const auto before=Snapshot(actor);
    Require(!context.codePath.empty() && context.codeMasks && context.disabledNames.size()==64u,
        "Original dormant frame context lost class code or serialized disabled bits");
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
    const auto disabled=ExecutePortableActorEvent(actor,"AnimEnd",true);
    Require(disabled.passed() && disabled.value.kind==Kind::Nothing && disabled.instructions==0u &&
        Same(before,Snapshot(actor)), "Disabled authored AnimEnd executed");
    const auto absent=ExecutePortableActorEvent(actor,"NoSuchAuthoredEvent");
    Require(absent.passed() && absent.value.kind==Kind::Nothing && absent.instructions==0u &&
        !GetPortableRuntimeScriptStatePresent(), "Missing or before-begun-play event was treated as execution/failure");
    std::cout << "ORIGINAL STOPPED DISPATCH "<<actor<<" code="<<context.codePath<<" stateName="<<context.stateName<<
        " disabledProbes="<<context.disabledNames.size()<<"; no startup or ticking\n";
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
// trailer bytes: v4-v9 append one bounded length + codec payload after every
// existing gameplay/progress field, preserving the old layout verbatim.
std::size_t ScriptTailOffset(const std::vector<std::uint8_t>& bytes) {
    Require(Word(bytes,0)==0x53515844u && Word(bytes,4)>=4u && Word(bytes,4)<=9u,"Script checkpoint is not runtime v4-v9");
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
    equalProperty("ObjectFlags",Value::Integer(0x4000));
    equalProperty("Owner",Value::Text(Kind::Object,actor));
    equalProperty("Level",level);equalProperty("XLevel",xLevel);
    equalProperty("Outer",Value::Text(Kind::Object,expectedOuter));
    equalProperty("Tag",Value::Text(Kind::Name,"WeaponPistol"));
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
    Require(!ReadPortableActorStateObject(born) &&
        spawned.meshPath==ReadPortableClassDefault(weaponClass,"Mesh").text &&
        spawned.drawScale==QuestVr::Vm::ToFloat(ReadPortableClassDefault(weaponClass,"DrawScale")),
        "Original dormant Spawn ran startup/state callbacks or lost the concrete original mesh/scale defaults");
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
    Require(Word(bytes,4u)==8u && state.births.size()==1u &&
        state.births.front().path==born && state.births.front().classPath==weaponClass &&
        state.births.front().frozenDefaults.empty(),
        "Original dormant birth did not retain codec5/envelope8 with immutable original CDO baseline");
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
    const auto stableGc=GC::GetStats();
    for(unsigned iteration=0u;iteration<3u;++iteration) {
        Require(LoadPortableRuntimeState(saved.string()),"Original v8 birth replacement failed");
        const auto currentGc=GC::GetStats();
        Require(currentGc.numObjects==stableGc.numObjects && currentGc.memoryUsage==stableGc.memoryUsage,
            "Repeated original v8 birth replacement retained unrooted retired allocations");
    }
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent() &&
        SavePortableRuntimeState(inspect.string()) && CheckpointBytes(inspect)==originalBytes,
        "Original dormant Spawn control did not completely restore authored legacy state");
    std::cout<<"ORIGINAL DORMANT SPAWN "<<born<<" original WeaponPistol CDO/mesh/materials/owner/native Level/XLevel, "<<
        "codec5/v8 cold replacement/GC/legacy reset; actual Level before-begun, no campaign startup claim\n";
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
    Call(actor,"Disable",{Name("Quest_Inventory_Probe")});
    Require(SavePortableRuntimeState(savedV5.string()),"InventoryItem and independent authored-state disabled sets could not compose");
    const auto v5=CheckpointBytes(savedV5);Require(Word(v5,4u)==5u,"State + InventoryItem composition lost v5 envelope");
    auto composed=QuestVr::DecodeScriptSavedState(ScriptBlob(v5));
    Require(findActor(composed).state && !findActor(composed).clock &&
        QuestVr::Vm::Equal(findItem(composed).value,negative[slot]),
        "InventoryItem composition replaced state-only metadata or fabricated a native clock");
    Call(actor,"Enable",{Name("Quest_Inventory_Probe")});
    Require(LoadPortableRuntimeState(savedV5.string()) &&
        ReadPortableActorDispatchContext(actor).disabledNames.count("quest_inventory_probe")==1u,
        "v5 composition did not restore independent disabled-event state");
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
    Require(Word(inventoryBytes,4u)==8u && inventoryState.births.size()==2u,
        "Original initialized weapon/ammo inventory omitted its codec5/v8 birth graph");
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
    // already initialized. Its next unsupported dependency remains a separate
    // evidence gate; successful inventory creation is not campaign startup.
    Require(LoadPortableRuntimeState(generated.string()),"Could not reset actual StartUp inventory fixture");
    unchanged(initializedBytes,"Original StartUp pre-initialization baseline");
    Call(ReadPortableActorScriptProperty(actor,"Level").text,"InitEventManager");
    Call(actor,"SetInitialState");
    Require(SavePortableRuntimeState(inspect.string()),"Actual StartUp + inventory continuation could not checkpoint");
    const auto startup=CheckpointBytes(inspect);
    const auto startupActors=GetPortableRuntimeMapActors(true).size();
    const auto startupRevision=GetPortableRuntimeWorldRevision();
    const auto startupGc=GC::GetStats();
    const auto slice=ResumePortableActorState(actor);
    std::cout<<"ORIGINAL STARTUP NEXT DEPENDENCY status="<<static_cast<int>(slice.status)<<
        " committed="<<slice.committed<<" error="<<slice.error<<" at "<<slice.function<<':'<<slice.offset<<
        " opcode="<<static_cast<unsigned>(slice.opcode)<<'\n';
    Require(slice.status==Status::Unsupported && !slice.committed &&
        slice.function=="DeusEx.ScriptedPawn.StartUp" && slice.offset==9u &&
        slice.opcode==195u && slice.error=="Unsupported runtime native 195",
        "Actual StartUp did not advance through head animation/Switch to its next original native dependency: "+slice.error);
    Require(GetPortableRuntimeMapActors(true).size()==startupActors &&
        GetPortableRuntimeWorldRevision()==startupRevision &&
        GC::GetStats().numObjects==startupGc.numObjects &&
        GC::GetStats().memoryUsage==startupGc.memoryUsage,
        "Rejected actual StartUp retained provisional inventory births, roots or world publication");
    unchanged(startup,"Actual StartUp inventory dependency rollback");
    Require(LoadPortableRuntimeState(legacy.string()) && !GetPortableRuntimeScriptStatePresent(),
        "Legacy restore retained InventoryItem overlay or composed state");
    SameInventoryProperties(actor,originalAuthored,"Legacy restore failed to recover all8 authored/inherited inventory slots");
    unchanged(originalLegacyBytes,"Legacy inventory/state reset");
    std::cout<<"ORIGINAL INVENTORY MEMBER writes/negative Count/optional zero/default class constraint, v4/v5 composition; rejections="<<
        rejections<<"; actual InitializeInventory Return PC770, original owned WeaponPistol/ammo/GiveTo/base/Idle2/native links/v8 cold GC graph; StartUp remains a separate dependency gate\n";
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
    Call(actor,"Disable",{Name("Quest_Test_Event")});
    auto dynamic = ReadPortableActorStateObject(actor);
    Require(dynamic && !dynamic->frameOverride && !dynamic->frame && dynamic->hasStack &&
        ReadPortableActorDispatchContext(actor).disabledNames.count("quest_test_event")==1u &&
        SavePortableRuntimeState(saved.string()), "Disable did not preserve dormant raw context in a state-only record");
    const auto dormant = CheckpointBytes(saved);
    Require(Word(dormant,4)==5u && ScriptBlob(dormant).at(6)==2u,
        "State-only capture did not select runtime v5 / script codec v2");
    Call(actor,"Enable",{Name("QUEST_TEST_EVENT")});
    Require(ReadPortableActorDispatchContext(actor).disabledNames.count("quest_test_event")==0u &&
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
        startup->frame->localsCodePath==startup->frame->codePath &&
        startup->frame->latent==QuestVr::StateLatent::Continue && startup->frame->statementIndex==ordinal &&
        context.stateName=="StartUp" && context.disabledNames.empty(),
        "Pre-begun-play SetInitialState did not select actual Auto/Begin code with independent disabled sets");
    Call(actor,"Disable",{Name("Quest_Test_Event")});
    Require(SavePortableRuntimeState(saved.string()),"Actual selected state frame could not be saved");
    const auto selected = CheckpointBytes(saved);
    const auto slice = ResumePortableActorState(actor);
    Require(!slice.passed() && !slice.error.empty(),"Original StartUp slice was silently treated as fully implemented");
    unchanged(selected,"Unsupported original state slice rollback");
    std::cout<<"ORIGINAL STATE SLICE explicit refusal: "<<slice.error<<" at "<<slice.function<<':'<<slice.offset<<'\n';
    Call(actor,"GotoState",{Name("None")});
    const auto cleared = ReadPortableActorStateObject(actor);
    Require(cleared && cleared->frame && cleared->frame->codePath.empty() && cleared->frame->locals.empty() &&
        cleared->frame->localsCodePath.empty() && cleared->frame->statementIndex==startup->frame->statementIndex &&
        cleared->frame->latent==startup->frame->latent &&
        Call(actor,"GetStateName").value.text=="None", "GotoState(None) fabricated a PC/latent reset or retained code locals");
    Call(actor,"GotoState",{Name(authored.stateName)}); // Named class is not a State, so remains None.
    Require(Call(actor,"GetStateName").value.text=="None","Class-backed raw state identity was accepted as an executable named State");
    Require(LoadPortableRuntimeState(saved.string()),"Selected StartUp continuation did not restore");
    unchanged(selected,"Selected frame roundtrip");
    Call(actor,"GotoState",{Name("StartUp"),Name("Quest_No_Such_Label")});
    const auto stopped = ReadPortableActorStateObject(actor);
    Require(stopped && stopped->frame && stopped->frame->latent==QuestVr::StateLatent::Stop &&
        stopped->frame->statementIndex==startup->frame->statementIndex,
        "Same-state missing transition label did not stop while preserving PC");
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
    reject([](auto& value) {value.hasStack=false;},"Cleared authored HasStack");
    reject([](auto& value) {value.frame->codePath="Engine.Actor";},"Class as state code");
    reject([](auto& value) {value.frame->codePath="DeusEx.ScriptedPawn.NoSuchState";},"Unknown state code");
    reject([](auto& value) {value.frame->codePath="DeusEx.ScriptedPawn.Standing";},"Different running/local state name");
    reject([](auto& value) {value.frame->localsCodePath="DeusEx.Robot.Standing";},"Local owner outside receiver ancestry");
    reject([](auto& value) {value.frame->statementIndex=0xffffffffu;},"Running PC out of code");
    reject([](auto& value) {value.frame->latent=QuestVr::StateLatent::Sleep;},"Unhandled saved latent action");
    reject([](auto& value) {value.frame->locals.push_back({"Engine.Actor.NoSuchLocal",{Value::Integer(0)}});},"Incomplete/mismatched local schema");
    auto wrongEnvelope=selected;PutWord(wrongEnvelope,4,4u);rejectBytes(wrongEnvelope,"State codec in v4 envelope");
    auto absent=baseline;absent.objects[index].state.reset();
    rejectBytes(ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(absent)),"Legacy codec in v5 envelope");
    auto truncated=selected;truncated.pop_back();rejectBytes(truncated,"Truncated v5 tail");
    auto extra=selected;extra.push_back(0);rejectBytes(extra,"Trailing v5 byte");
    // Positive stale Stop ordinal, exact dormant-state sets, and a cleared code
    // frame are valid pinned representations, not fabricated reset positions.
    auto stale=baseline;stale.objects[index].state->frame->latent=QuestVr::StateLatent::Stop;
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
    auto folded=baseline;
    auto& sets=folded.objects[index].state->disabled;
    const auto current=sets.find("startup");Require(current!=sets.end(),"Selected state fixture lacks its disabled set");
    const auto names=current->second;sets.erase(current);sets["StArTuP"]=names;
    sets["StArTuP"].erase("quest_test_event");sets["StArTuP"].insert("QuEsT_TeSt_EvEnT");
    Require(QuestVr::WriteDurableSaveFile(generated.string(),ReplaceScriptBlob(selected,QuestVr::EncodeScriptSavedState(folded))) &&
        LoadPortableRuntimeState(generated.string()) && ReadPortableActorDispatchContext(actor).disabledNames.count("quest_test_event")==1u,
        "Mixed-case state/event disabled identities did not restore canonically");
    Call(actor,"Enable",{Name("QUEST_TEST_EVENT")});
    Require(ReadPortableActorDispatchContext(actor).disabledNames.count("quest_test_event")==0u,
        "Enable failed to remove a restored mixed-case disabled event");
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
    std::cout<<"ORIGINAL v5 state-only/Auto/Begin/None/Stop/disabled-set roundtrip; schema rejections="<<rejected<<
        "; atomic slice/callback failure; no world startup/tick implied\n";
}

void TestOriginal(const std::filesystem::path& root, const bool inventoryOnly = false,
    const bool switchOnly = false) {
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
        VerifyOriginalDispatchPrograms(tables,runtime);
    }
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
        if (std::string(map) == "00_Training") VerifyColdOriginalAnimationAssets(root, checkpoint, temporary.directory);
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
                VerifyStoppedDispatch(actor.objectPath);
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
                const auto disabled=Call(actor.objectPath, "DeusEx.ScriptedPawn.Standing.AnimEnd");
                Require(disabled.value.kind==Kind::Nothing && disabled.instructions==0u && Same(beforeDisabled,Snapshot(actor.objectPath)),
                    "Fully-qualified probe function bypassed original callback eligibility");
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
                VerifyStoppedDispatch(actor.objectPath);
                Call(actor.objectPath, "PlayWaiting");
                const auto robot = Snapshot(actor.objectPath);
                Require(!robot.animation.sequence.empty() && robot.animation.frame < 0.0f,
                    "Original Robot.PlayWaiting optional helper parameters were not executed");
                ++robotTests;
                std::cout << "ORIGINAL ROBOT " << actor.objectPath << " PlayWaiting passed\n";
            }
            if (birdTests == 0u && IsA(actor, "Bird")) {
                VerifyAuthoredStack(root, actor.objectPath);
                VerifyStoppedDispatch(actor.objectPath);
                const auto wait = ReadPortableActorScriptProperty(actor.objectPath, "WaitAnim");
                Call(actor.objectPath, "PlayWaiting");
                const auto bird = Snapshot(actor.objectPath);
                const auto mesh = GetPortableRuntimeMesh(actor.meshPath);
                const auto* expected = QuestVr::FindMeshAnimationSequence(*mesh.animation, wait.text, true);
                Require(expected && bird.animation.sequence == expected->name && bird.animation.loop,
                    "Original Bird.PlayWaiting native optional defaults were not executed");
                const auto before = bird;
                const auto random = ExecutePortableActorFunction(actor.objectPath, "TweenToWaiting", {Number(0.2f)});
                Require(random.status == Status::Unsupported && Same(before, Snapshot(actor.objectPath)),
                    "Unsupported original FRand branch was replaced or leaked mutations: " + random.error);
                ++birdTests;
                std::cout << "ORIGINAL BIRD " << actor.objectPath << " PlayWaiting/native optional/FRand refusal passed\n";
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
                std::string(argv[2]) == "--switch-only")),
            "Usage: portable_actor_script_test GAME_ROOT [--inventory-only|--switch-only]");
        const bool switchOnly = argc == 3 && std::string(argv[2]) == "--switch-only";
        TestOriginal(std::filesystem::path(argv[1]), argc == 3 && !switchOnly, switchOnly);
        if (switchOnly) { std::cout << "PASS focused original cold-assets/head-Switch suite\n"; return 0; }
        std::cout << "PASS original actor authored struct inspection, member writes, typed references, v4/v5 composition and rollback"
            << (argc == 2 ? "; full actor bytecode/natives/BSP Region/state/clock suite" : "; focused inventory suite") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
