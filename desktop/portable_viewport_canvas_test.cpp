#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_viewport_canvas.h"
#include "portable_viewport_input.h"
#include "portable_object_lookup.h"
#include "portable_quest_input_source.h"
#include "quest_engine_config.h"
#include "quest_save_bundle.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {
using QuestVr::Vm::Kind;using QuestVr::Vm::Value;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}
const Value& Field(const PortableViewportCanvasSnapshot& canvas,const char* name) {
    const auto key="Engine.Canvas."+std::string(name);
    const auto found=std::find_if(canvas.fields.begin(),canvas.fields.end(),[&](const auto& field){return field.declaration==key;});
    Require(found!=canvas.fields.end(),"Canvas omitted field "+key);return found->value;
}
std::vector<std::uint8_t> Save(const std::filesystem::path& path) {
    Require(SavePortableRuntimeState(path.string()),"Cannot checkpoint Canvas fixture");std::vector<std::uint8_t> bytes;
    Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read Canvas checkpoint");return bytes;
}
void Original(const PortableViewportCanvasSnapshot& canvas,std::uint64_t viewport) {
    const auto owner=ReadPortableRuntimePlayerSession(viewport);
    Require(canvas.prepared&&canvas.initialized&&canvas.classPath=="Engine.Canvas"&&canvas.outerPath=="Transient"&&
        canvas.viewportPath==owner.viewportPath&&canvas.objectFlags==0u&&canvas.objectIndex&&canvas.classIndex&&canvas.outerIndex&&
        canvas.viewportIndex!=0u&&canvas.fields.size()==22u,"Canvas identity/Outer/flags/Viewport graph differs");
    Require(Field(canvas,"Viewport").kind==Kind::Object&&Field(canvas,"Viewport").text==owner.viewportPath,
        "Canvas.Init did not store its actual Viewport");
    for(const auto& field:canvas.fields)if(field.declaration!="Engine.Canvas.Viewport") {
        const auto name=field.declaration.substr(std::string("Engine.Canvas.").size());
        const auto expected=ReadPortableClassDefault("Engine.Canvas",name);
        Require(QuestVr::Vm::Equal(field.value,expected),"Canvas.Init changed copied CDO field "+name);
    }
    Require(Field(canvas,"SizeX").integer==0&&Field(canvas,"SizeY").integer==0&&
        Field(canvas,"ClipX").floating==0&&Field(canvas,"ClipY").floating==0,
        "Canvas constructor fabricated Update/frame dimensions");
}
}
int main(int argc,char** argv) {
    std::filesystem::path temporary;std::shared_ptr<QuestVr::NativeClientLifetime> lifetime;
    try {
        std::cout.setf(std::ios::unitbuf);
        std::filesystem::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
        else throw std::runtime_error("Usage: portable_viewport_canvas_test [--game-root OWNED_GAME_ROOT]");
        if(root.empty()){std::cout<<"SKIP owned Canvas: supply --game-root\n";return 77;}
        Require(QuestVr::EngineConfig::ParseCanvasClass("[Engine.Engine]\nCanvas=Engine.Canvas\n")==std::optional<std::string>("Engine.Canvas"),
            "Canvas selector ignores owned INI");
        bool conflict{};try{static_cast<void>(QuestVr::EngineConfig::ParseCanvasClass("[Engine.Engine]\nCanvas=Engine.Canvas\nCanvas=Wrong.Canvas\n"));}
        catch(const std::exception&){conflict=true;}
        Require(conflict,"Canvas selector accepted conflicting duplicates");++refusals;
        ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();
        std::vector<PortablePackageTables> sources;
        for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire",
            "MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"})
            sources.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
        Require(InitializePortableRuntime(sources).passed,"Cannot initialize owned Canvas graph");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&PreparePortableRuntimeInputSchema().prepared&&
            PreparePortableRuntimeInputConfig((root/"System"/"User.ini").string()).prepared,"Cannot prepare actual native Canvas dependencies");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training");
        lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,991u);
        for(const auto viewport:{992u,993u,994u}) {RegisterPortableRuntimeViewport(991u,{viewport,1440u,1440u,3u,2u});static_cast<void>(PreparePortableRuntimePlayerObjects(viewport));}
        const auto ini=(root/"System"/"DeusEx.ini").string();
        bool missingSource{};try{static_cast<void>(ReadPortableRuntimeQuestInputSource(992u));}catch(...){missingSource=true;}
        Require(missingSource,"Viewport metadata fabricated a Quest input source");++refusals;
        auto source=std::make_shared<QuestVr::QuestInputSource>(lifetime,991u,992u);
        auto foreign=std::make_shared<QuestVr::QuestInputSource>(std::make_shared<QuestVr::NativeClientLifetime>(),991u,992u);
        bool foreignRefused{};try{BindPortableRuntimeQuestInputSource(992u,foreign);}catch(...){foreignRefused=true;}
        Require(foreignRefused,"Quest input source accepted matching IDs with a different lifetime");++refusals;
        BindPortableRuntimeQuestInputSource(992u,source);BindPortableRuntimeQuestInputSource(992u,source);
        QuestVr::QuestInputObservation observation;observation.displayTime=1;observation.focused=true;
        source->Publish(observation);
        QuestVr::QuestInputCursor inputCursor;
        const auto stagedInput=StagePortableRuntimeQuestInputSource(992u,inputCursor,true);
        Require(stagedInput.count==1u&&stagedInput.after.sequence==1u&&inputCursor.sequence==0u&&
            ReadPortableRuntimeQuestInputSource(992u).latest==1u,"Runtime Quest source staging consumed input or lost the real binding");
        bool wrongViewport{};try{BindPortableRuntimeQuestInputSource(993u,source);}catch(...){wrongViewport=true;}
        Require(wrongViewport,"Quest source escaped its actual viewport");++refusals;
        temporary=std::filesystem::temp_directory_path()/("quest-canvas-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(temporary),"Cannot create Canvas diagnostic directory");
        const auto checkpoint=temporary/"checkpoint.sav";const auto baseline=Save(checkpoint);
        const auto before=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();const auto revision=GetPortableRuntimeWorldRevision();
        const auto inventory=ReadPortableRuntimeViewportCanvas(992u);
        const auto unchanged=[&] {
            GC::Collect();const auto gc=GC::GetStats();const auto current=ReadPortableRuntimeObjectLookup();
            const auto restored=ReadPortableRuntimeViewportCanvas(992u);
            Require(!restored.prepared&&restored.inventoryCapacity==inventory.inventoryCapacity&&restored.inventoryBytes==inventory.inventoryBytes&&
                gc.numObjects==before.numObjects&&gc.memoryUsage==before.memoryUsage&&
                current.objects==lookup.objects&&current.generation==lookup.generation&&GetPortableRuntimeWorldRevision()==revision&&Save(checkpoint)==baseline,
                "Canvas refusal retained native objects/counters/save/revision/lookup state");
        };
        const auto probe=PreparePortableRuntimeViewportCanvas(992u,ini,false);
        Require(!probe.execution.passed()&&!probe.execution.committed&&probe.execution.error==
            "Non-publishing Canvas initialization probe completed; rollback required","Canvas failed before complete original initialization: "+probe.execution.error);
        Original(probe.canvas,992u);unchanged();
        for(const auto writes:{0u,1u,4u,5u}) {
            QuestVr::Vm::Limits limits;limits.writes=writes;
            const auto failed=PreparePortableRuntimeViewportCanvas(992u,ini,true,limits);
            Require(!failed.execution.passed()&&!failed.execution.committed,"Canvas ignored a shared write budget");++refusals;unchanged();
        }
        for(const auto work:{0u,1u,8u}) {
            QuestVr::Vm::Limits limits;limits.nativeWork=work;
            const auto failed=PreparePortableRuntimeViewportCanvas(992u,ini,true,limits);
            Require(!failed.execution.passed()&&!failed.execution.committed,"Canvas ignored a shared native-work budget");++refusals;unchanged();
        }
        const auto ready=PreparePortableRuntimeViewportCanvas(992u,ini);
        Require(ready.execution.passed()&&ready.execution.committed&&ready.execution.writes==6u,"Canvas constructor/Init failed: "+ready.execution.error);
        Original(ready.canvas,992u);
        Require(ready.canvas.inventoryBytes>=ready.canvas.propertyRootBytes&&ready.canvas.propertyRootBytes>0u&&ready.canvas.inventoryCapacity>0u,
            "Canvas accounting omitted its reflected root graph/inventory allocation");
        const auto readOnly=ReadPortableRuntimeViewportCanvas(992u);
        Require(readOnly.objectIndex==ready.canvas.objectIndex&&readOnly.inventoryBytes==ready.canvas.inventoryBytes&&
            readOnly.inventoryCapacity==ready.canvas.inventoryCapacity&&readOnly.propertyRootBytes==ready.canvas.propertyRootBytes,
            "Read-only Canvas snapshot changed identity or accounting");
        Require(ready.canvas.objectPath==probe.canvas.objectPath&&ready.canvas.uniqueCounter==probe.canvas.uniqueCounter,
            "Canvas rollback changed unique name counter");
        const auto again=PreparePortableRuntimeViewportCanvas(992u,ini);
        Require(again.execution.passed()&&again.execution.writes==0u&&again.canvas.objectIndex==ready.canvas.objectIndex,"Canvas preparation duplicated identity");
        const auto other=PreparePortableRuntimeViewportCanvas(993u,ini);Require(other.execution.passed()&&other.canvas.objectIndex!=ready.canvas.objectIndex,
            "Distinct viewport reused Canvas ownership");Original(other.canvas,993u);
        Require(PreparePortableRuntimeViewportInput(992u,PortableInputLoggingProfile::Ordinary).constructed,"Original Canvas-before-Input preparation failed");
        Require(PreparePortableRuntimeViewportInput(994u,PortableInputLoggingProfile::Ordinary).constructed,"Input order-control fixture failed");
        const auto late=PreparePortableRuntimeViewportCanvas(994u,ini);
        Require(!late.execution.passed()&&!late.execution.committed&&late.execution.error.find("precede viewport Input")!=std::string::npos,
            "Canvas inserted after Input and claimed original construction order");++refusals;
        Require(Save(checkpoint)==baseline&&GetPortableRuntimeWorldRevision()==revision,"Canvas changed map/checkpoint state");
        Require(LoadPortableRuntimeMap(map).passed&&ReadPortableRuntimeViewportCanvas(992u).objectIndex==ready.canvas.objectIndex,
            "Map reload discarded viewport Canvas");
        lifetime->Revoke();ClearPortableRuntimeClient();lifetime.reset();GC::Collect();
        bool inputRevoked{};try{static_cast<void>(StagePortableRuntimeQuestInputSource(992u,stagedInput.after));}catch(...){inputRevoked=true;}
        Require(inputRevoked,"Revoked runtime source remained usable");++refusals;
        bool revoked{};try{static_cast<void>(ReadPortableRuntimeViewportCanvas(992u));}catch(...){revoked=true;}
        Require(revoked,"Revoked Canvas exposed live viewport service");++refusals;
        ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
        Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Canvas lifecycle retained GC objects");
        std::filesystem::remove(checkpoint);std::filesystem::remove(temporary);temporary.clear();
        std::cout<<"PASS original Canvas constructor/Init: "<<checks<<" checks / "<<refusals<<" refusals; actual ownership/defaults, no Reset/Update/frame, "
            "transaction/name/save/GC rollback, distinct viewports, map retention and client teardown. Not Console, drawing, Input.Init, FExec or VR gameplay.\n";
        return 0;
    }catch(const std::exception& error) {
        if(lifetime)lifetime->Revoke();
        ClearPortableRuntimeClient();ShutdownPortableRuntime();GC::Collect();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
