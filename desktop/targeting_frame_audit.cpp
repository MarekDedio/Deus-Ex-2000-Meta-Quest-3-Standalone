#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "portable_unreal_runtime.h"
#include "quest_target_description.h"
#include "quest_targeting_frame.h"
#include "visual_renderer.h"
#include <cstring>
#include <iostream>

namespace {
void PreviewFocusLabel(const std::filesystem::path& directory,const std::string& name,
    const QuestVr::FrobLabelCanvas& label,const QuestVr::FrobDescriptionPalette& palette,
    float radiusUnits,float heightUnits,bool mover) {
    constexpr float unitsToMeters=1.0f/52.5f;
    const float radius=(mover?QuestVr::OriginalFrobMoverHalfExtentUnits:radiusUnits)*unitsToMeters;
    const float height=(mover?QuestVr::OriginalFrobMoverHalfExtentUnits:heightUnits)*unitsToMeters;
    std::array<QuestVr::TargetFramePoint,8> points;std::size_t index{};
    for (const float x:{-radius,radius}) for (const float y:{-height,height})
        for (const float z:{-1.5f-radius,-1.5f+radius}) points[index++]={x,y,z};
    const auto box=QuestVr::ProjectTargetFrameBounds(points,{});
    if (!box) throw std::runtime_error("Original reference focus bounds failed to project");
    const auto layout=QuestVr::LayoutOriginalFrobFrame(*box,mover?0.0f:radiusUnits,mover?0.0f:heightUnits);
    const auto strips=QuestVr::BuildOriginalFrobCornerStrips(*box,0.1f,640,480,
        mover?0.0f:radiusUnits,mover?0.0f:heightUnits);
    if (!layout || !strips) throw std::runtime_error("Original reference focus layout failed");
    const auto info=QuestVr::LayoutOriginalFrobLabel(*layout,label.textWidth,label.textHeight);
    if (!info) throw std::runtime_error("Original reference focus information layout failed");
    questvisual::Image image{640u,480u,std::vector<std::uint8_t>(640u*480u*3u,12u)};
    const auto pixel=[&](int x,int y,const std::array<std::uint8_t,3>& rgb) {
        if (x<0 || y<0 || x>=640 || y>=480) return;
        const auto at=(static_cast<std::size_t>(y)*640u+static_cast<std::size_t>(x))*3u;
        for (std::size_t c=0u;c<3u;++c) image.rgb[at+c]=rgb[c];
    };
    for (std::size_t i=0u;i<strips->size();++i) {
        const auto& strip=(*strips)[i];
        for (int y=static_cast<int>(std::floor(strip.y));y<static_cast<int>(std::ceil(strip.y+strip.height));++y)
            for (int x=static_cast<int>(std::floor(strip.x));x<static_cast<int>(std::ceil(strip.x+strip.width));++x)
                pixel(x,y,i<8u?std::array<std::uint8_t,3>{0u,0u,0u}:palette.header);
    }
    for (std::uint32_t y=0u;y<label.image.height;++y) for (std::uint32_t x=0u;x<label.image.width;++x) {
        const auto at=(static_cast<std::size_t>(y)*label.image.width+x)*4u;
        if (!label.image.rgba[at+3u]) continue;
        pixel(static_cast<int>(info->x)+static_cast<int>(x),static_cast<int>(info->y)+static_cast<int>(y),
            {label.image.rgba[at],label.image.rgba[at+1u],label.image.rgba[at+2u]});
    }
    questvisual::WriteBmp(directory/name,image);
}
}

// Read-only original class-default evidence. This never runs commercial code.
int main(int argc, char** argv) {
    try {
        if (argc!=2 && !(argc==4 && std::string(argv[2])=="--output"))
            throw std::runtime_error("Usage: targeting_frame_audit GAME_ROOT [--output DIRECTORY]");
        std::filesystem::path output;
        if (argc==4) {
            const auto original=std::filesystem::weakly_canonical(argv[1]);
            output=std::filesystem::weakly_canonical(argv[3]);
            if (output.empty() || output==output.root_path()) throw std::runtime_error("Focus preview requires a dedicated output directory");
            auto a=original.begin(),b=output.begin();
            const auto folded=[](std::string text){for (auto& c:text) if (c>='A'&&c<='Z') c+=32;return text;};
            while (a!=original.end() && b!=output.end() && folded(a->string())==folded(b->string())) {++a;++b;}
            if (a==original.end()) throw std::runtime_error("Focus preview must not modify original game content");
            std::filesystem::create_directories(output);
        }
        const auto package = LoadPortablePackageTables(std::string(argv[1])+"/System/DeusEx.u");
        for (const auto* name : {"FrobDisplayWindow", "ColorThemeHUD", "ColorThemeHUD_Default","ColorThemeHUD_Amber"}) {
            const auto cls = LoadPortableClassDescriptor(package,FindPortableExport(package,name));
            std::cout << "CLASS " << name << '\n';
            for (const auto& p : cls.defaults) {
                std::cout << p.name.ToString() << '[' << p.arrayIndex << "] type=" << unsigned(p.type) << " value=";
                if (p.type == 13u) std::cout << DecodePortableStringProperty(p);
                else if (p.type == 6u) std::cout << DecodePortableNameProperty(package,p);
                else if (p.type == 4u && p.value.size() == 4u) {
                    float number{}; std::memcpy(&number,p.value.data(),4u); std::cout << number;
                } else for (auto byte : p.value) std::cout << unsigned(byte) << ',';
                std::cout << '\n';
            }
        }
        const auto ui=LoadPortablePackageTables(std::string(argv[1])+"/System/DeusExUI.u");
        const auto font=DecodePortableBitmapFont(ui,"FontMenuSmall_DS");
        const auto originalBackground=DecodePortableIndexedTexture(ui,"UserInterface.ConWindowBackground",true);
        const QuestVr::PersonaUiImage background{originalBackground.width,originalBackground.height,originalBackground.rgba};
        QuestVr::FrobDescriptionPalette palette;
        const auto amber=LoadPortableClassDescriptor(package,FindPortableExport(package,"ColorThemeHUD_Amber"));
        for (const auto& p:amber.defaults) {
            if (p.name!="Colors" || p.type!=10u || p.value.size()!=4u) continue;
            const std::array<std::uint8_t,3> rgb{{p.value[0],p.value[1],p.value[2]}};
            if (p.arrayIndex==0u) palette.background=rgb;
            else if (p.arrayIndex==1u) palette.border=rgb;
            else if (p.arrayIndex==7u) palette.header=rgb;
        }
        std::vector<PortablePackageTables> scripts;
        for (const char* name:{"Core","Engine","Extension","DeusEx","DeusExItems",
            "ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters"})
            scripts.push_back(LoadPortablePackageTables(std::string(argv[1])+"/System/"+name+".u"));
        if (!InitializePortableRuntime(scripts).passed) throw std::runtime_error("Original script runtime did not initialize");
        const auto moverClass=ReadPortableRuntimeDispatchClass("DeusEx.DeusExMover");
        std::cout << "Original mover ancestry " << moverClass.path << " -> " << moverClass.parentPath << '\n';
        const auto moverParent=ReadPortableRuntimeDispatchClass(moverClass.parentPath);
        std::cout << "Original mover ancestor " << moverParent.path << " -> " << moverParent.parentPath << '\n';
        const auto training=LoadPortablePackageTables(std::string(argv[1])+"/Maps/00_Training.dx");
        if (!LoadPortableRuntimeMap(training).passed) throw std::runtime_error("Original Training actors did not load");
        std::size_t lamps{},unlocked{};
        std::string sourceLampName;QuestVr::FrobLabelCanvas label;
        const auto actors=GetPortableRuntimeMapActors();
        std::cout << "Original focus audit map actors=" << actors.size() << '\n';
        for (const auto& actor:actors) {
            bool authoredLamp{};
            try {
                authoredLamp=ReadPortableActorDeclaredScriptProperty(actor.objectPath,"DeusEx.Lamp.bOn").kind==QuestVr::Vm::Kind::Bool;
            } catch (const std::exception&) {}
            if (authoredLamp) {
                std::cout << "Original lamp identity " << actor.objectPath << " class=" << actor.classPath << '\n';
                QuestVr::FrobDescriptionFields fields;fields.decoration=true;
                const auto readString=[&](const char* path) {
                    const auto value=ReadPortableActorDeclaredScriptProperty(actor.objectPath,path);
                    if (value.kind!=QuestVr::Vm::Kind::String) throw std::runtime_error("Original lamp name field is not String");
                    return value.text;
                };
                fields.itemName=readString("DeusEx.DeusExDecoration.ItemName");
                fields.familiarName=readString("Engine.Actor.FamiliarName");
                fields.unfamiliarName=readString("Engine.Actor.UnfamiliarName");
                const auto conversation=ReadPortableActorDeclaredScriptProperty(actor.objectPath,"Engine.Actor.LastConEndTime");
                if (conversation.kind!=QuestVr::Vm::Kind::Float) throw std::runtime_error("Original lamp conversation time is not Float");
                fields.lastConversationEndTime=conversation.floating;
                const auto name=QuestVr::BuildOriginalFrobDescription(fields,{});
                if (!name || name->empty()) throw std::runtime_error("Original Training Lamp name fields yielded no focus description");
                sourceLampName=*name;
                label=QuestVr::BuildOriginalFrobLabelCanvas(font,*name,background,
                    [&](std::uint32_t c){return GetPortableBitmapGlyph(font,c);},palette);
                if (label.textWidth==0u || label.textHeight==0u || label.image.rgba.size()!=512u*128u*4u)
                    throw std::runtime_error("Original focus name/font/background did not form a bounded label");
                if (!output.empty()) {
                    const auto radius=ReadPortableActorDeclaredScriptProperty(actor.objectPath,"Engine.Actor.CollisionRadius");
                    const auto height=ReadPortableActorDeclaredScriptProperty(actor.objectPath,"Engine.Actor.CollisionHeight");
                    if (radius.kind!=QuestVr::Vm::Kind::Float || height.kind!=QuestVr::Vm::Kind::Float)
                        throw std::runtime_error("Original Lamp focus extents are not Float");
                    PreviewFocusLabel(output,"original-lamp-focus.bmp",label,palette,radius.floating,height.floating,false);
                }
                ++lamps;
            }
            try {
                const auto locked=ReadPortableActorDeclaredScriptProperty(actor.objectPath,"DeusEx.DeusExMover.bLocked");
                if (locked.kind==QuestVr::Vm::Kind::Bool && !locked.boolean) ++unlocked;
            } catch (const std::exception&) {} // Qualified ownership proves the original family, not class-name matching.
        }
        if (lamps==0u || unlocked==0u) throw std::runtime_error("Original Training focus name/unlocked-door controls were absent");
        if (!output.empty()) {
            const auto cls=LoadPortableClassDescriptor(package,FindPortableExport(package,"FrobDisplayWindow"));
            std::string unlockedText;
            for (const auto& p:cls.defaults)
                if (p.name=="msgUnlocked" && p.type==13u) unlockedText=DecodePortableStringProperty(p);
            const auto door=QuestVr::BuildOriginalFrobLabelCanvas(font,unlockedText,background,
                [&](std::uint32_t c){return GetPortableBitmapGlyph(font,c);},palette);
            PreviewFocusLabel(output,"original-door-focus.bmp",door,palette,0,0,true);
        }
        ShutdownPortableRuntime();
        std::cout << "PASS original focus label ItemName='" << sourceLampName << "' FontMenuSmall_DS=" <<
            label.textWidth << 'x' << label.textHeight << " lamps=" << lamps << " unlockedMovers=" << unlocked << '\n';
        return 0;
    } catch (const std::exception& error) { ShutdownPortableRuntime();std::cerr << error.what() << '\n'; return 1; }
}
