#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_mesh_animation.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
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
// trailer bytes: v4 must append one bounded length + codec payload after every
// existing gameplay/progress field, preserving the old layout verbatim.
std::size_t ScriptTailOffset(const std::vector<std::uint8_t>& bytes) {
    Require(Word(bytes,0)==0x53515844u && Word(bytes,4)==4u,"Script checkpoint is not runtime v4");
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
    Require(size>0 && cursor+4<=bytes.size() && size==bytes.size()-cursor-4,"Script blob length does not cover exact v4 tail");
    return cursor;
}
std::vector<std::uint8_t> ScriptBlob(const std::vector<std::uint8_t>& bytes) {
    const auto offset=ScriptTailOffset(bytes);
    return {bytes.begin()+static_cast<std::ptrdiff_t>(offset+4),bytes.end()};
}
using PropertyValues=std::vector<std::pair<std::string,Value>>;
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

void TestOriginal(const std::filesystem::path& root) {
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
    Require(InitializePortableRuntime(tables).passed, "Original script runtime initialization failed");
    TemporaryCheckpoint temporary(root);
    const auto checkpoint = (temporary.directory / "original-authored.sav").string();
    std::size_t humanTests{}, robotTests{}, birdTests{};
    for (const auto* map : {"00_Training", "01_NYC_UNATCOIsland", "00_Intro"}) {
        Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root / "Maps" /
            (std::string(map) + ".dx")).string())).passed, "Original map load failed");
        Require(!GetPortableRuntimeUnsavedScriptState(), "Map replacement retained retired VM state");
        Require(SavePortableRuntimeState(checkpoint), "Untouched authored runtime save failed");
        const auto checkpointBytes = std::filesystem::file_size(checkpoint);
        Require(Word(CheckpointBytes(checkpoint),4)==3u,"Untouched runtime unexpectedly changed legacy v3 save format");
        const auto actors = GetPortableRuntimeMapActors();
        for (const auto& actor : actors) {
            if (humanTests != 0u && robotTests != 0u && birdTests != 0u) break;
            if (!actor.pawn || actor.meshPath.empty() || !IsA(actor, "ScriptedPawn")) continue;
            if (humanTests == 0u && !IsA(actor, "Robot") && !IsA(actor, "Animal")) {
                const auto region = ReadPortableActorScriptProperty(actor.objectPath, "Region");
                Require(region.kind == Kind::Struct && !region.fields.at("zone").text.empty(),
                    "Original BSP Region did not resolve a real ZoneInfo/LevelInfo");
                const auto handed = Call(actor.objectPath, "HasTwoHandedWeapon");
                Require(handed.value.kind == Kind::Bool, "Original lazy weapon query did not return bool");
                Call(actor.objectPath, "PlayWaiting");
                Require(Snapshot(actor.objectPath).animation.sequence != "None",
                    "Original PlayWaiting failed to select the original script sequence");
                Call(actor.objectPath, "DeusEx.ScriptedPawn.Standing.AnimEnd");
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
                // Preflight may resolve the saved original map schema while a
                // different map is loaded; applying v4 still requires that map
                // to be current, with no cross-map mutation as a side effect.
                Require(LoadPortableRuntimeMap(LoadPortablePackageTables((root/"Maps"/"00_TrainingCombat.dx").string())).passed,
                    "Different-map v4 preflight fixture failed");
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
                const auto beforeStartup = Snapshot(actor.objectPath);
                const auto startup = ExecutePortableActorFunction(actor.objectPath, "SetInitialState");
                Require(startup.status == Status::Unsupported && Same(beforeStartup, Snapshot(actor.objectPath)) &&
                    !GetPortableRuntimeUnsavedScriptState(), "Unsupported original state startup was faked or leaked changes");
                ++humanTests;
                std::cout << "ORIGINAL HUMAN " << actor.objectPath << " PlayWaiting/Standing.AnimEnd/Play/Loop/Tween/rollback/save passed\n";
            }
            if (robotTests == 0u && IsA(actor, "Robot")) {
                Call(actor.objectPath, "PlayWaiting");
                const auto robot = Snapshot(actor.objectPath);
                Require(!robot.animation.sequence.empty() && robot.animation.frame < 0.0f,
                    "Original Robot.PlayWaiting optional helper parameters were not executed");
                ++robotTests;
                std::cout << "ORIGINAL ROBOT " << actor.objectPath << " PlayWaiting passed\n";
            }
            if (birdTests == 0u && IsA(actor, "Bird")) {
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
    Require(humanTests == 1u && robotTests == 1u && birdTests == 1u,
        "Original maps did not cover required human, robot and bird execution fixtures");
    UnloadPortableRuntimeMap();
    Require(!GetPortableRuntimeUnsavedScriptState(), "Map unload leaked actor VM overlays");
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cout << "SKIP: supply readonly original Deus Ex installation for real actor script integration\n";
            return 77;
        }
        TestOriginal(std::filesystem::path(argv[1]));
        std::cout << "PASS original actor bytecode/natives/BSP Region, transactional failure, and current-map v4 script/clock persistence\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
