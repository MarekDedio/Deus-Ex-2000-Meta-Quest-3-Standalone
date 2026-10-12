#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_dialogue_ui.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks{};
void Require(bool value,const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}
const PortableTaggedProperty& Property(const std::vector<PortableTaggedProperty>& values,
    const char* name,std::uint32_t slot=0u) {
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto& p) {
        return p.name.ToString()==name && p.arrayIndex==slot;
    });
    if (found==values.end()) throw std::runtime_error(std::string("Missing authored UI property ")+name);
    return *found;
}
std::string Object(const PortablePackageTables& table,const PortableTaggedProperty& property) {
    const auto reference=DecodePortableObjectReference(property);
    auto result=GetPortableObjectPath(table,reference);
    if (reference>0) result=std::filesystem::path(table.sourcePath).stem().string()+'.'+result;
    return result;
}
void Word(std::ofstream& file,std::uint16_t value) {
    file.put(static_cast<char>(value)); file.put(static_cast<char>(value>>8u));
}
void Dword(std::ofstream& file,std::uint32_t value) {
    Word(file,static_cast<std::uint16_t>(value)); Word(file,static_cast<std::uint16_t>(value>>16u));
}
void Preview(const std::filesystem::path& path,const QuestVr::PersonaUiImage& image) {
    // Generated preview only; keep commercial assets in ignored output paths.
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    if (!file) throw std::runtime_error("Could not create original datalink preview");
    const auto rowSize=(image.width*3u+3u)&~3u;
    Word(file,0x4d42u); Dword(file,54u+rowSize*image.height); Dword(file,0u); Dword(file,54u);
    Dword(file,40u); Dword(file,image.width); Dword(file,image.height); Word(file,1u); Word(file,24u);
    Dword(file,0u); Dword(file,rowSize*image.height); Dword(file,0u); Dword(file,0u); Dword(file,0u); Dword(file,0u);
    for (std::uint32_t y=image.height;y>0u;--y) {
        for (std::uint32_t x=0u;x<image.width;++x) {
            const auto offset=(static_cast<std::size_t>(y-1u)*image.width+x)*4u;
            for (int channel=2;channel>=0;--channel)
                file.put(static_cast<char>((image.rgba[offset+channel]*image.rgba[offset+3u]+
                    24u*(255u-image.rgba[offset+3u])+127u)/255u));
        }
        for (auto padding=image.width*3u;padding<rowSize;++padding) file.put(0);
    }
    if (!file) throw std::runtime_error("Could not complete original datalink preview");
}
}
int main(int argc,char** argv) {
    try {
        if (argc==1) { std::cout << "SKIP: original datalink UI needs an owned game installation\n"; return 77; }
        if (argc!=2 && argc!=3) throw std::runtime_error("Usage: dialogue_ui_original_test GAME_ROOT [OUTPUT.bmp]");
        const auto root=std::filesystem::canonical(argv[1]);
        const auto script=LoadPortablePackageTables((root/"System/DeusEx.u").string());
        const auto ui=LoadPortablePackageTables((root/"System/DeusExUI.u").string());
        const auto portraits=LoadPortablePackageTables((root/"Textures/InfoPortraits.utx").string());
        const auto defaults=LoadPortableClassDescriptor(script,FindPortableExport(script,"HUDInfoLinkDisplay")).defaults;
        Require(Object(script,Property(defaults,"FontName"))==std::string("DeusExUI.")+QuestVr::kDataLinkNameFont,"Original datalink name font differs");
        Require(Object(script,Property(defaults,"fontText"))==std::string("DeusExUI.")+QuestVr::kDataLinkTextFont,"Original datalink speech font differs");
        for (const auto& [name,expected]:{std::pair{"fontTextX",7},std::pair{"fontTextY",10}}) {
            const auto& field=Property(defaults,name);
            std::int32_t value{}; Require(field.type==2u && field.value.size()==4u,"Original font cell field malformed");
            std::memcpy(&value,field.value.data(),4u); Require(value==expected,"Original datalink fixed cell size differs");
        }
        std::array<PortableTextureImage,2> backgrounds,borders;
        for (std::size_t i=0;i<2u;++i) {
            Require(Object(script,Property(defaults,"texBackgrounds",static_cast<std::uint32_t>(i)))==
                std::string("DeusExUI.")+QuestVr::kDataLinkBackgroundNames[i],"Original datalink background identity differs");
            Require(Object(script,Property(defaults,"texBorders",static_cast<std::uint32_t>(i)))==
                std::string("DeusExUI.")+QuestVr::kDataLinkBorderNames[i],"Original datalink border identity differs");
            backgrounds[i]=DecodePortableIndexedTexture(ui,QuestVr::kDataLinkBackgroundNames[i],true);
            borders[i]=DecodePortableIndexedTexture(ui,QuestVr::kDataLinkBorderNames[i],true);
        }
        auto canvas=QuestVr::BuildDataLinkUiCanvas(backgrounds,borders);
        const auto portrait=DecodePortableIndexedTexture(portraits,QuestVr::DataLinkPortraitName("JaimeReyes",0),false);
        QuestVr::AddDataLinkPortrait(canvas,{portrait.width,portrait.height,portrait.rgba});
        const auto nameFont=DecodePortableBitmapFont(ui,QuestVr::kDataLinkNameFont);
        const auto textFont=DecodePortableBitmapFont(ui,QuestVr::kDataLinkTextFont);
        Require(nameFont.glyphs.size()>=256u && textFont.glyphs.size()>=256u,"Original datalink font omitted byte glyphs");
        const auto con=LoadPortablePackageTables((root/"System/DeusExConText.u").string());
        const auto speech=LoadPortableExportProperties(con,FindPortableExport(con,"ConSpeech79"));
        const auto text=DecodePortableStringProperty(Property(speech.properties,"Speech"));
        Require(text.size()>100u,"Actual Training introduction did not resolve");
        // Static preview shows the actual line at the completed typing frame;
        // synthetic compositor tests cover progressive/scrolling timing.
        QuestVr::DrawDataLinkUiText(canvas,nameFont,textFont,QuestVr::DataLinkDisplayName("JaimeReyes"),
            QuestVr::BuildDataLinkTextWindow(text),false,
            [&](std::uint32_t c) { return GetPortableBitmapGlyph(nameFont,c); },
            [&](std::uint32_t c) { return GetPortableBitmapGlyph(textFont,c); });
        std::uint64_t hash=14695981039346656037ull;
        for (auto byte:canvas.rgba) { hash^=byte; hash*=1099511628211ull; }
        if (argc==3) {
            const auto output=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[2]));
            const auto relative=output.lexically_relative(root);
            Require(!relative.empty() && *relative.begin()=="..","Preview output overlaps original installation");
            Preview(output,canvas);
        }
        std::cout << "Original datalink UI: " << checks << " checks; 431x114; JaimeReyes 64x64; fonts "
            << nameFont.lineHeight << '/' << textFont.lineHeight << "; actual DL_Start bytes=" << text.size()
            << "; RGBA FNV1a=" << std::hex << hash << std::dec << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
