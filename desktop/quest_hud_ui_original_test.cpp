#include "Precomp.h"
#include "quest_hud_ui.h"
#include "surreal_portable_package_tables.h"

#include <filesystem>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace {
std::size_t checks{};
void Require(bool value,const char* message) { ++checks;if (!value) throw std::runtime_error(message); }
QuestVr::PersonaUiImage Image(const PortablePackageTables& package,const char* path) {
    auto decoded=DecodePortableIndexedTexture(package,path,true);
    return {decoded.width,decoded.height,std::move(decoded.rgba)};
}
void Audit(const PortablePackageTables& package,const char* className) {
    const auto descriptor=LoadPortableClassDescriptor(package,FindPortableExport(package,className));
    std::cout << "CLASS " << className << '\n';
    for (const auto& property:descriptor.defaults) {
        std::cout << property.name.ToString() << '[' << property.arrayIndex << "] type=" << unsigned(property.type) << " bytes=" << property.value.size();
        if (property.type==5u) std::cout << " object=" << GetPortableObjectPath(package,DecodePortableObjectReference(property));
        else if (property.type==2u && property.value.size()==4u) { std::int32_t value{};std::memcpy(&value,property.value.data(),4u);std::cout << " int=" << value; }
        else if (property.type==7u || property.type==10u || property.type==12u) {
            std::cout << " struct=" << property.structName.ToString() << " raw=";
            for (auto byte:property.value) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
            std::cout << std::dec;
        }
        else if (property.type==4u && property.value.size()==4u) { float value{};std::memcpy(&value,property.value.data(),4u);std::cout << " float=" << value; }
        else if (property.type==13u) std::cout << " text=" << DecodePortableStringProperty(property);
        else if (property.type==6u) std::cout << " name=" << DecodePortableNameProperty(package,property);
        std::cout << '\n';
    }
    for (std::size_t index=0u;index<package.exports.size();++index) {
        const auto path=GetPortableObjectPath(package,static_cast<std::int32_t>(index+1u));
        if (path.rfind(std::string(className)+'.',0u)==0u &&
            GetPortableObjectPath(package,package.exports[index].ObjClass)=="Core.Function") std::cout << "FUNCTION " << path << '\n';
    }
}
const PortableTaggedProperty& Property(const std::vector<PortableTaggedProperty>& values,const char* name,std::uint32_t slot=0u) {
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto& value) {return value.name.ToString()==name && value.arrayIndex==slot;});
    if (found==values.end()) throw std::runtime_error(std::string("Missing original HUD property: ")+name);
    return *found;
}
std::int32_t Integer(const PortableTaggedProperty& value) {
    Require(value.type==2u && value.value.size()==4u,"HUD integer metadata type/size mismatch");
    std::int32_t result{};std::memcpy(&result,value.value.data(),4u);return result;
}
std::string ObjectPath(const PortablePackageTables& package,const PortableTaggedProperty& value) {
    Require(value.type==5u,"HUD object metadata has wrong type");
    const auto reference=DecodePortableObjectReference(value);auto path=GetPortableObjectPath(package,reference);
    if (reference>0) path=std::filesystem::path(package.sourcePath).stem().string()+'.'+path;
    return path;
}
QuestVr::HudRgb Color(const PortableTaggedProperty& value) {
    Require(value.type==10u && value.structName.ToString()=="Color" && value.value.size()==4u,"HUD Color metadata type/size mismatch");
    Require(value.value[3u]==0u,"Original HUD Color.A no longer zero; must not conflate palette tint and opacity");
    return {value.value[0u],value.value[1u],value.value[2u]};
}
void SameColor(QuestVr::HudRgb actual,QuestVr::HudRgb expected,const char* message) {
    Require(actual.red==expected.red && actual.green==expected.green && actual.blue==expected.blue,message);
}
std::uint64_t SourceHash(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if (!input) throw std::runtime_error("Could not fingerprint original HUD source");
    std::array<char,65536> bytes{};std::uint64_t hash=14695981039346656037ull;
    while (input) {input.read(bytes.data(),bytes.size());for (std::streamsize index=0;index<input.gcount();++index) {hash^=static_cast<unsigned char>(bytes[index]);hash*=1099511628211ull;}}
    if (!input.eof()) throw std::runtime_error("Original HUD fingerprint read failed");
    return hash;
}
void Contract(const PortablePackageTables& script) {
    const auto compass=LoadPortableClassDescriptor(script,FindPortableExport(script,"HUDCompassDisplay")).defaults;
    Require(Integer(Property(compass,"clipWidth"))==static_cast<int>(QuestVr::kHudCompassClip.width),"Original compass clip width differs");
    Require(Integer(Property(compass,"tickWidth"))==static_cast<int>(QuestVr::kHudCompassTickWidth),"Original compass logical strip width differs");
    SameColor(Color(Property(compass,"colTickMarks")),QuestVr::kOriginalAmberHudTheme.tickMarks,"Original compass tick tint differs");
    Require(ObjectPath(script,Property(compass,"texBackground"))==std::string("DeusExUI.")+QuestVr::kHudCompassBackground,"Original compass background identity differs");
    Require(ObjectPath(script,Property(compass,"texBorder"))==std::string("DeusExUI.")+QuestVr::kHudCompassBorder,"Original compass border identity differs");
    Require(ObjectPath(script,Property(compass,"texTickBox"))==std::string("DeusExUI.")+QuestVr::kHudCompassTickBox,"Original compass tick box identity differs");
    const auto belt=LoadPortableClassDescriptor(script,FindPortableExport(script,"HUDObjectBelt")).defaults;
    Require(ObjectPath(script,Property(belt,"texBackgroundLeft"))==std::string("DeusExUI.")+QuestVr::kHudBeltBackgroundLeft,"Original belt left background identity differs");
    Require(ObjectPath(script,Property(belt,"texBackgroundRight"))==std::string("DeusExUI.")+QuestVr::kHudBeltBackgroundRight,"Original belt right background identity differs");
    for (std::uint32_t piece=0u;piece<3u;++piece)
        Require(ObjectPath(script,Property(belt,"texBorder",piece))==std::string("DeusExUI.")+QuestVr::kHudBeltBorders[piece],"Original belt border identity differs");
    const auto slot=LoadPortableClassDescriptor(script,FindPortableExport(script,"HUDObjectSlot")).defaults;
    for (const auto& [name,expected]:{std::pair{"slotFillWidth",42},std::pair{"slotFillHeight",37},std::pair{"borderWidth",44},
        std::pair{"borderHeight",50},std::pair{"slotIconX",1},std::pair{"slotIconY",3},std::pair{"slotNumberX",38},std::pair{"slotNumberY",3}})
        Require(Integer(Property(slot,name))==expected,"Original belt slot integer metadata differs");
    Require(ObjectPath(script,Property(slot,"texBackground"))==std::string("DeusExUI.")+QuestVr::kHudBeltBackgroundCell,"Original belt cell identity differs");
    const auto hit=LoadPortableClassDescriptor(script,FindPortableExport(script,"HUDHitDisplay")).defaults;
    Require(ObjectPath(script,Property(hit,"texBackground"))==std::string("DeusExUI.")+QuestVr::kHudHealthBackground,"Original health background identity differs");
    Require(ObjectPath(script,Property(hit,"texBorder"))==std::string("DeusExUI.")+QuestVr::kHudHealthBorder,"Original health border identity differs");
    Require(DecodePortableStringProperty(Property(hit,"EnergyText"))=="BE","Original energy label differs");
    Require(DecodePortableStringProperty(Property(hit,"O2Text"))=="O2","Original breath label differs");
    const auto names=LoadPortableClassDescriptor(script,FindPortableExport(script,"ColorThemeHUD")).defaults;
    const auto amber=LoadPortableClassDescriptor(script,FindPortableExport(script,"ColorThemeHUD_Amber")).defaults;
    const auto themeColor=[&](const char* name) {
        const auto found=std::find_if(names.begin(),names.end(),[&](const auto& value) {return value.name.ToString()=="colorNames" && DecodePortableNameProperty(script,value)==name;});
        if (found==names.end()) throw std::runtime_error(std::string("Original HUD theme name missing: ")+name);
        return Color(Property(amber,"Colors",found->arrayIndex));
    };
    SameColor(themeColor("HUDColor_Background"),QuestVr::kOriginalAmberHudTheme.background,"Original Amber background tint differs");
    SameColor(themeColor("HUDColor_Borders"),QuestVr::kOriginalAmberHudTheme.border,"Original Amber border tint differs");
    SameColor(themeColor("HUDColor_NormalText"),QuestVr::kOriginalAmberHudTheme.text,"Original Amber text tint differs");
}
QuestVr::HudBeltArtwork Belt(const PortablePackageTables& ui,const PortablePackageTables& script) {
    QuestVr::HudBeltArtwork result{Image(ui,QuestVr::kHudBeltBackgroundLeft),Image(ui,QuestVr::kHudBeltBackgroundRight),Image(ui,QuestVr::kHudBeltBackgroundCell),{}, {}};
    for (std::size_t piece=0u;piece<3u;++piece) result.borders[piece]=Image(ui,QuestVr::kHudBeltBorders[piece]);
    const auto slot=LoadPortableClassDescriptor(script,FindPortableExport(script,"HUDObjectSlot")).defaults;
    for (std::uint32_t piece=0u;piece<9u;++piece) {
        const auto name=ObjectPath(script,Property(slot,"texBorders",piece));
        Require(name.rfind("DeusExUI.UserInterface.PersonaItemHighlight_",0u)==0u,"Belt selection texture is not original Persona nine-slice");
        result.selection[piece]=Image(ui,name.substr(9u).c_str());
    }
    return result;
}
QuestVr::HudHealthArtwork Health(const PortablePackageTables& ui) {
    QuestVr::HudHealthArtwork result{Image(ui,QuestVr::kHudHealthBackground),Image(ui,QuestVr::kHudHealthBorder),Image(ui,QuestVr::kHudHealthBody),{}};
    for (std::size_t part=0u;part<6u;++part) result.parts[part]=Image(ui,QuestVr::kHudHealthPartNames[part]);
    return result;
}
void Stamp(QuestVr::PersonaUiImage& canvas,const QuestVr::PersonaUiImage& part,std::uint32_t x,std::uint32_t y) {
    QuestVr::CopyHudPiece(canvas,part,static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),part.width,part.height,
        {255u,255u,255u},{0u,0u,canvas.width,canvas.height});
}
void Word(std::ofstream& file,std::uint16_t value) { file.put(static_cast<char>(value));file.put(static_cast<char>(value>>8u)); }
void Dword(std::ofstream& file,std::uint32_t value) { Word(file,static_cast<std::uint16_t>(value));Word(file,static_cast<std::uint16_t>(value>>16u)); }
void Preview(const std::filesystem::path& path,const QuestVr::PersonaUiImage& image) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if (!file) throw std::runtime_error("Could not create original HUD preview");
    const auto rowSize=(image.width*3u+3u)&~3u;
    Word(file,0x4d42u);Dword(file,54u+rowSize*image.height);Dword(file,0u);Dword(file,54u);
    Dword(file,40u);Dword(file,image.width);Dword(file,image.height);Word(file,1u);Word(file,24u);
    Dword(file,0u);Dword(file,rowSize*image.height);Dword(file,0u);Dword(file,0u);Dword(file,0u);Dword(file,0u);
    for (std::uint32_t y=image.height;y>0u;--y) {
        for (std::uint32_t x=0u;x<image.width;++x) {
            const auto offset=(static_cast<std::size_t>(y-1u)*image.width+x)*4u;
            for (int channel=2;channel>=0;--channel)
                file.put(static_cast<char>((image.rgba[offset+channel]*image.rgba[offset+3u]+24u*(255u-image.rgba[offset+3u])+127u)/255u));
        }
        for (auto padding=image.width*3u;padding<rowSize;++padding) file.put(0);
    }
    if (!file) throw std::runtime_error("Could not complete original HUD preview");
}
}
int main(int argc,char** argv) {
    try {
        if (argc==1) {std::cout << "SKIP: HUD artwork test requires owned original installation\n";return 77;}
        if (argc<2 || argc>3) throw std::runtime_error("Usage: quest_hud_ui_original_test GAME_ROOT [OUTPUT.bmp|--audit]");
        const auto root=std::filesystem::canonical(argv[1]);
        const auto scriptHash=SourceHash(root/"System/DeusEx.u"),uiHash=SourceHash(root/"System/DeusExUI.u");
        const auto script=LoadPortablePackageTables((root/"System/DeusEx.u").string());
        const auto ui=LoadPortablePackageTables((root/"System/DeusExUI.u").string());
        if (argc==3 && std::string(argv[2])=="--audit") {
            for (const auto* name:{"HUDBaseWindow","HUDObjectBelt","HUDObjectSlot","HUDHitDisplay","HUDCompassDisplay","ProgressBarWindow"}) Audit(script,name);
            for (std::size_t index=0u;index<script.exports.size();++index) {
                const auto path=GetPortableObjectPath(script,static_cast<std::int32_t>(index+1u));
                if (path.ends_with(".GetColorScaled")) std::cout << "COLOR_FUNCTION " << path << '\n';
                if (script.exports[index].ObjClass==0 && path.find("ColorThemeHUD")!=std::string::npos) Audit(script,path.c_str());
            }
            return 0;
        }
        Contract(script);
        const QuestVr::HudCompassArtwork compassArt{Image(ui,QuestVr::kHudCompassBackground),Image(ui,QuestVr::kHudCompassBorder),Image(ui,QuestVr::kHudCompassTicks),Image(ui,QuestVr::kHudCompassTickBox)};
        Require(compassArt.ticks.width==256u && compassArt.ticks.height==32u,"Original compass padded strip dimensions differ");
        const auto north=QuestVr::BuildHudCompassCanvas(compassArt,0,0);
        const auto northeast=QuestVr::BuildHudCompassCanvas(compassArt,8192,0);
        const auto wrapped=QuestVr::BuildHudCompassCanvas(compassArt,65535,0);
        const auto offset=QuestVr::BuildHudCompassCanvas(compassArt,16384,8192);
        Require(north.width==73u && north.height==40u,"Original compass canvas size mismatch");
        Require(offset.rgba==northeast.rgba,"Original-art compass did not honor map TrueNorth offset");
        Require(north.rgba!=northeast.rgba && north.rgba!=wrapped.rgba,"Original-art compass fixtures did not scroll/wrap");
        const auto beltArt=Belt(ui,script);
        const auto healthArt=Health(ui);
        const auto tinyFont=DecodePortableBitmapFont(ui,QuestVr::kHudTinyFont);
        Require(tinyFont.glyphs.size()>=256u,"Original FontTiny is missing single-byte glyphs");
        const auto glyph=[&](std::uint32_t letter) {return GetPortableBitmapGlyph(tinyFont,letter);};
        const auto ringDefaults=LoadPortableClassDescriptor(script,FindPortableExport(script,"NanoKeyRing")).defaults;
        const auto ringPath=ObjectPath(script,Property(ringDefaults,"Icon"));
        Require(ringPath=="DeusExUI.Icons.BeltIconNanoKeyRing","Original NanoKeyRing belt icon differs");
        auto ringIcon=Image(ui,ringPath.substr(9u).c_str());
        const auto ringDescription=DecodePortableStringProperty(Property(ringDefaults,"beltDescription"));
        Require(!ringDescription.empty(),"Original NanoKeyRing has no belt description");
        // Deliberately detached presentation fixtures. A preview ring/icon is
        // not evidence that player startup spawned/owns a live NanoKeyRing.
        QuestVr::HudBeltSnapshot trainingFixture;
        trainingFixture.slots[0u]=QuestVr::HudBeltItem{"Fixture.NanoKeyRing",&ringIcon,ringDescription,"",true,false,false};
        auto belt=QuestVr::BuildHudBeltCanvas(beltArt,trainingFixture);
        QuestVr::DrawHudBeltText(belt,trainingFixture,tinyFont,glyph);
        const auto emptyBelt=QuestVr::BuildHudBeltCanvas(beltArt,{});
        Require(emptyBelt.rgba!=belt.rgba,"Explicit fixture ring did not change rendered special slot");
        QuestVr::HudHealthSnapshot healthyFixture;
        for (auto& part:healthyFixture.parts) part.displayedHealth=100;
        healthyFixture.energy=100.0f;healthyFixture.energyMax=100.0f;
        auto healthy=QuestVr::BuildHudHealthCanvas(healthArt,healthyFixture);
        QuestVr::DrawHudHealthText(healthy,healthyFixture,tinyFont,glyph);
        auto damagedFixture=healthyFixture;
        const std::array<std::int32_t,6> healthValues{100,60,40,10,0,80};
        for (std::size_t part=0u;part<6u;++part) damagedFixture.parts[part].displayedHealth=healthValues[part];
        damagedFixture.energy=25.0f;damagedFixture.underwater=true;damagedFixture.breathPercent=5.0f;damagedFixture.swimTimer=0.30f;
        auto damaged=QuestVr::BuildHudHealthCanvas(healthArt,damagedFixture);
        QuestVr::DrawHudHealthText(damaged,damagedFixture,tinyFont,glyph);
        const auto unknown=QuestVr::BuildHudHealthCanvas(healthArt,{});
        Require(healthy.rgba!=damaged.rgba && unknown.rgba!=healthy.rgba,"Independent limb fixture states/unknown-state rendering did not differ");
        auto result=QuestVr::EmptyHudCanvas(640u,480u);
        Stamp(result,healthy,0u,0u);Stamp(result,northeast,0u,110u);Stamp(result,belt,99u,411u);
        if (argc==3) {
            const auto output=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[2]));
            const auto relative=output.lexically_relative(root);
            Require(!relative.empty() && *relative.begin()=="..","HUD preview overlaps original game installation");
            Require(std::find(output.begin(),output.end(),std::filesystem::path("artifacts"))!=output.end() && output.extension()==".bmp",
                "Commercial HUD previews must stay in ignored artifacts and use BMP format");
            std::filesystem::create_directories(output.parent_path());
            Preview(output,result);
            const auto child=[&](const char* suffix,const auto& image) {Preview(output.parent_path()/(output.stem().string()+suffix+".bmp"),image);};
            child("-belt",belt);child("-health-healthy",healthy);child("-health-damaged",damaged);child("-health-unknown",unknown);
            child("-compass-north",north);child("-compass-northeast",northeast);child("-compass-wrapped",wrapped);
        }
        Require(SourceHash(root/"System/DeusEx.u")==scriptHash && SourceHash(root/"System/DeusExUI.u")==uiHash,"Original HUD source changed during read-only test");
        std::cout << "PASS original HUD artwork: " << checks << " checks; authored art identities/slot dimensions/Amber tint with Color.A=0; FontTiny height="
            << tinyFont.lineHeight << "; belt541x69 1..9,0, explicit ring fixture; health84x106 six independent fixture limbs, energy/breath; compass73x40 crop240x15 and map-relative wrap.\n"
            << "Preview states are detached fixtures, NOT live player startup/equipment/damage verification. Original source files unchanged.\n";
        return 0;
    } catch (const std::exception& error) {std::cerr << "FAIL HUD artwork: " << error.what() << '\n';return 1;}
}
