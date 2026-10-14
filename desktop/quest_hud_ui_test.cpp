#include "quest_hud_ui.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace QuestVr;
std::size_t checks{};
void Require(bool result,const char* message) { ++checks;if (!result) throw std::runtime_error(message); }
template<class Action> void Reject(Action&& action,const char* message) {
    bool refused{};try {action();} catch (const std::runtime_error&) {refused=true;}Require(refused,message);
}
PersonaUiImage Solid(std::uint32_t width,std::uint32_t height,std::array<std::uint8_t,4> color) {
    auto result=EmptyHudCanvas(width,height);
    for (std::size_t pixel=0u;pixel<result.rgba.size();pixel+=4u)
        std::copy(color.begin(),color.end(),result.rgba.begin()+pixel);
    return result;
}
std::array<std::uint8_t,4> Pixel(const PersonaUiImage& image,std::uint32_t x,std::uint32_t y) {
    const auto pixel=(static_cast<std::size_t>(y)*image.width+x)*4u;
    return {image.rgba[pixel],image.rgba[pixel+1u],image.rgba[pixel+2u],image.rgba[pixel+3u]};
}
constexpr HudTheme white{{255u,255u,255u},{255u,255u,255u},{255u,255u,255u},{255u,255u,255u}};
struct Glyph {std::uint32_t pageIndex{},x{},y{},width{1u},height{1u};};
struct Font {std::uint32_t lineHeight{7u};std::array<PersonaUiImage,1> pages;};
void Compass() {
    HudCompassArtwork art{Solid(64u,32u,{0u,0u,0u,0u}),Solid(128u,64u,{0u,0u,0u,0u}),
        Solid(256u,32u,{255u,0u,255u,255u}),Solid(64u,32u,{0u,0u,0u,0u})};
    for (std::uint32_t y=0u;y<15u;++y) for (std::uint32_t x=0u;x<240u;++x)
        SetPersonaUiPixel(art.ticks,x,y,static_cast<std::uint8_t>(x),static_cast<std::uint8_t>(y),0u);
    // These are binary32 evaluation results, not exact-real arithmetic:
    // e.g. yaw8192 / (65536.0f/240) is 29.999998f, then trunc(-2.999998).
    for (const auto& [yaw,expected]:{std::pair{0,27},std::pair{1,26},std::pair{8192,-2},
        std::pair{16384,-32},std::pair{32768,-92},std::pair{49152,-152},std::pair{65535,-212}}) {
        const auto positions=OriginalHudCompassPositions(yaw,0);
        if (positions.draw!=expected) std::cerr << "Compass fixture yaw=" << yaw << " actual=" << positions.draw << " expected=" << expected << '\n';
        Require(positions.draw==expected,"Compass float-to-int truncation differs from original");
        const auto image=BuildHudCompassCanvas(art,yaw,0,white);
        Require(image.width==73u && image.height==40u,"Compass canvas size differs");
        for (std::uint32_t y=0u;y<40u;++y) for (std::uint32_t x=0u;x<73u;++x) {
            const auto pixel=Pixel(image,x,y);
            if (x>=13u && x<68u && y>=7u && y<22u) {
                int source=static_cast<int>(x-13u)-positions.draw;
                source=(source%240+240)%240;
                Require(pixel==std::array<std::uint8_t,4>{static_cast<std::uint8_t>(source),static_cast<std::uint8_t>(y-7u),0u,255u},"Compass two-window wrap or source crop differs");
            } else Require(pixel[3u]==0u,"Compass tick leaked outside original clip/15px child");
        }
        const auto turned=BuildHudCompassCanvas(art,yaw+65536,65536,white);
        Require(turned.rgba==image.rgba,"Compass map-relative/full-turn invariance differs");
    }
    Require(OriginalHudCompassPositions(std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()).draw==26,"Compass signed wrap overflows");
    auto broken=art;broken.ticks.width=239u;
    Reject([&] {(void)BuildHudCompassCanvas(broken,0,0);},"Compass malformed image accepted");
    broken=art;broken.ticks=Solid(239u,15u,{0u,0u,0u,255u});
    Reject([&] {(void)BuildHudCompassCanvas(broken,0,0);},"Compass truncated logical strip accepted");
    auto target=Solid(2u,2u,{3u,4u,5u,255u});auto black=Solid(1u,1u,{0u,0u,0u,255u});
    CopyHudPiece(target,black,0,0,1u,1u,white.text,{0u,0u,2u,2u});
    Require(Pixel(target,0u,0u)==std::array<std::uint8_t,4>{0u,0u,0u,255u},"Opaque black was mistaken for alpha mask");
    black.rgba[3u]=0u;CopyHudPiece(target,black,1,1,1u,1u,white.text,{0u,0u,2u,2u});
    Require(Pixel(target,1u,1u)==std::array<std::uint8_t,4>{3u,4u,5u,255u},"Masked texel overwrote prior layer");
    black.rgba[3u]=255u;CopyHudPiece(target,black,1,1,1u,1u,white.text,{0u,0u,2u,2u},0u,0u,HudPieceBlend::Translucent);
    Require(Pixel(target,1u,1u)==std::array<std::uint8_t,4>{3u,4u,5u,255u},"Translucent black incorrectly became an opaque rectangle");
    auto green=Solid(1u,1u,{0u,128u,0u,255u});CopyHudPiece(target,green,1,1,1u,1u,white.text,{0u,0u,2u,2u},0u,0u,HudPieceBlend::Translucent);
    Require(Pixel(target,1u,1u)==std::array<std::uint8_t,4>{3u,130u,5u,255u},"UE translucent screen blend became ordinary alpha blend");
}
HudBeltArtwork BeltArt() {
    HudBeltArtwork art{Solid(16u,64u,{1u,1u,1u,255u}),Solid(16u,64u,{2u,2u,2u,255u}),
        Solid(64u,64u,{3u,3u,3u,255u}),{}, {}};
    for (auto& border:art.borders) border=Solid(256u,128u,{0u,0u,0u,0u});
    for (auto& selection:art.selection) selection=Solid(1u,1u,{255u,255u,255u,255u});
    art.selection[8]=Solid(1u,1u,{0u,0u,0u,0u});
    return art;
}
void Belt() {
    auto art=BeltArt();auto icon=Solid(64u,64u,{9u,10u,11u,255u});
    HudBeltSnapshot snapshot;
    const auto empty=BuildHudBeltCanvas(art,snapshot,white);
    Require(empty.width==541u && empty.height==69u,"Belt canvas geometry differs");
    for (std::size_t position=0u;position<10u;++position) {
        const auto rect=OriginalHudBeltSlot(position);
        Require(rect.x==(position==0u?469u:10u+static_cast<std::uint32_t>(position-1u)*51u),"Original belt order not 1..9,0");
        Require(rect.width==(position==0u?44u:51u),"Special keyring slot did not retain width44");
        Require(Pixel(empty,rect.x+2u,10u)[0u]==3u,"Empty belt invented item icon/state");
        snapshot.slots[position]=HudBeltItem{"Fixture.Item"+std::to_string(position),&icon,"","",position==0u,false,false};
    }
    const auto full=BuildHudBeltCanvas(art,snapshot,white);
    for (std::size_t position=0u;position<10u;++position) {
        const auto rect=OriginalHudBeltSlot(position);
        Require(Pixel(full,rect.x+1u,9u)==std::array<std::uint8_t,4>{9u,10u,11u,255u},"Belt icon origin differs from (1,3)");
        Require(Pixel(full,rect.x+42u,45u)[0u]==9u && Pixel(full,rect.x+43u,45u)[0u]==3u,"Belt source42x37 crop stretched padded icon");
    }
    snapshot.slots[1u]->dragging=true;
    Require(Pixel(BuildHudBeltCanvas(art,snapshot,white),11u,9u)[0u]==3u,"Dragged icon should not be rendered in source slot");
    snapshot.slots[2u]->selected=true;
    Require(Pixel(BuildHudBeltCanvas(art,snapshot,white),61u,8u)[0u]==255u,"Actual selected item did not use original nine-slice");
    auto invalid=snapshot;invalid.slots[0u]->keyRing=false;
    Reject([&] {(void)BuildHudBeltCanvas(art,invalid);},"Non-keyring occupied special position0");
    invalid=snapshot;invalid.slots[2u]->actorPath=invalid.slots[1u]->actorPath;
    Reject([&] {(void)BuildHudBeltCanvas(art,invalid);},"Duplicate actor populated two belt slots");
    invalid=snapshot;auto tiny=Solid(41u,37u,{0u,0u,0u,255u});invalid.slots[3u]->icon=&tiny;
    Reject([&] {(void)BuildHudBeltCanvas(art,invalid);},"Truncated original belt icon accepted");
    Reject([&] {(void)OriginalHudBeltSlot(10u);},"Invalid belt index accepted");
    Font font{7u,{Solid(1u,1u,{255u,255u,255u,255u})}};Glyph glyph;
    auto image=empty;DrawHudBeltText(image,HudBeltSnapshot{},font,[&](unsigned) {return &glyph;},white);
    for (std::size_t position=0u;position<10u;++position)
        Require(Pixel(image,OriginalHudBeltSlot(position).x+42u,9u)[0u]==255u,"Empty-slot object number was not rendered right aligned");
    auto previous=image.rgba;glyph.width=2u;
    Reject([&] {DrawHudText(image,font,"1",{0u,0u,5u,7u},white.text,0u,[&](unsigned) {return &glyph;});},"Invalid glyph source accepted");
    Require(previous==image.rgba,"Invalid glyph modified destination before validation");
}
struct PlayerItem {
    std::string actorPath,iconPath,beltDescription;
    bool inObjectBelt{};
    std::int32_t beltPosition{};
};
struct PlayerSnapshot {
    bool available{},bound{};
    std::string playerPath,keyRingPath;
    std::vector<PlayerItem> inventory;
};
void BoundSnapshot() {
    auto icon=Solid(64u,64u,{5u,7u,9u,255u});
    std::size_t lookups{};
    const auto lookup=[&](const std::string& path) -> const PersonaUiImage* {++lookups;return path=="Fixture.Icon"?&icon:nullptr;};
    PlayerSnapshot player;
    Require(!BuildBoundPlayerHudBelt(player,lookup),"Unbound/unknown player produced a belt");
    player.available=true;player.playerPath="Fixture.Player";
    Require(!BuildBoundPlayerHudBelt(player,lookup),"Detached player inspection pretended viewport possession");
    Require(lookups==0u,"Unavailable player performed icon lookup");
    player.bound=true;
    const auto empty=BuildBoundPlayerHudBelt(player,lookup);
    Require(empty && std::none_of(empty->slots.begin(),empty->slots.end(),[](const auto& item) {return item.has_value();}),"Bound empty inventory invented starter ring");
    player.keyRingPath="Fixture.Ring";
    player.inventory={{"Fixture.Ring","Fixture.Icon","KEY RING",false,7},{"Fixture.Crowbar","Fixture.Icon","CROWBAR",true,1},{"Fixture.Pocket","Fixture.Icon","POCKET",false,2}};
    const auto mapped=BuildBoundPlayerHudBelt(player,lookup);
    Require(mapped && mapped->slots[0u] && mapped->slots[0u]->actorPath==player.keyRingPath && mapped->slots[0u]->keyRing,"Actual owned ring did not map to original special slot0");
    Require(mapped->slots[1u] && mapped->slots[1u]->actorPath=="Fixture.Crowbar" && !mapped->slots[2u],"Actual inObjectBelt/position mapping differs");
    Require(!mapped->slots[1u]->selected,"Inventory presence incorrectly became equipped selection");
    auto invalid=player;invalid.inventory.pop_back();invalid.inventory[1u].beltPosition=0;
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Ordinary item occupied special keyring slot0");
    invalid=player;invalid.inventory[2u].inObjectBelt=true;invalid.inventory[2u].beltPosition=1;
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Duplicate actual belt positions accepted");
    invalid=player;invalid.inventory.erase(invalid.inventory.begin());
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Absent owned keyring was synthesized");
    invalid=player;invalid.inventory[1u].beltPosition=10;
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Out-of-range actual belt position accepted");
    invalid=player;invalid.inventory.resize(1025u);
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Unbounded owned inventory snapshot accepted");
    invalid=player;invalid.playerPath=std::string("Fixture.Player\0Foreign",22u);
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Embedded-NUL bound player identity accepted");
    invalid=player;invalid.inventory[2u].actorPath.clear();
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Invalid non-belt owned identity escaped validation");
    invalid=player;invalid.inventory[2u].actorPath=invalid.inventory[1u].actorPath;
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Duplicate non-belt inventory identity escaped validation");
    invalid=player;invalid.inventory[1u].beltDescription.resize(4097u,'x');
    Reject([&] {(void)BuildBoundPlayerHudBelt(invalid,lookup);},"Bound belt description exceeded rendering bound");
}
void Health() {
    HudHealthArtwork art{Solid(64u,128u,{5u,6u,7u,255u}),Solid(128u,128u,{0u,0u,0u,0u}),Solid(64u,128u,{0u,0u,0u,0u}),{}};
    for (auto& part:art.parts) part=Solid(16u,64u,{255u,255u,255u,255u});
    HudHealthSnapshot state;
    auto absent=BuildHudHealthCanvas(art,state,white);
    Require(Pixel(absent,39u,17u)==std::array<std::uint8_t,4>{5u,6u,7u,255u},"Missing limbs were invented as healthy");
    const std::array<int,6> values{100,80,60,40,10,0};
    for (std::size_t part=0u;part<6u;++part) state.parts[part].displayedHealth=values[part];
    state.energy=25.0f;state.energyMax=100.0f;state.breathPercent=50.0f;state.underwater=true;
    auto image=BuildHudHealthCanvas(art,state,white);
    for (std::size_t part=0u;part<6u;++part) {
        const auto rect=kHudHealthPartRects[part];const auto color=OriginalHudScaledColor(static_cast<float>(values[part])/100.0f);
        // Choose a non-overlapping interior pixel (torso legs meet at44).
        const auto sampleY=part>=4u?52u:(part==1u?30u:rect.y+1u);
        const auto screen=[](std::uint8_t source,std::uint8_t destination) {return static_cast<std::uint8_t>(source+(destination*(255u-source)+127u)/255u);};
        Require(Pixel(image,rect.x+1u,sampleY)==std::array<std::uint8_t,4>{screen(color.red,5u),screen(color.green,6u),screen(color.blue,7u),255u},"Independent body-part tint/translucent screen blend differs");
    }
    Require(Pixel(image,15u,62u)[0u]==255u && Pixel(image,15u,61u)[0u]==5u,"Energy bar did not truncate 25% of55px to13px");
    Require(Pixel(image,61u,48u)[3u]==255u && Pixel(image,61u,47u)[0u]==5u,"Breath bar height differs");
    state.parts[0u].damageFlashFraction=1.0f;
    Require(OriginalHudPartColor(state.parts[0u]).red==255u && OriginalHudPartColor(state.parts[0u]).blue==255u,"Damage flash did not approach white");
    Require(OriginalHudScaledColor(0.0f).red==0u && OriginalHudScaledColor(0.1f).green==0u && OriginalHudScaledColor(0.4f).green==255u && OriginalHudScaledColor(0.8f).red==0u,"Original scaled-color thresholds differ");
    auto invalid=state;invalid.energyMax=0.0f;
    Reject([&] {(void)BuildHudHealthCanvas(art,invalid);},"Zero energy denominator accepted");
    invalid=state;invalid.parts[4u].damageFlashFraction=2.0f;
    Reject([&] {(void)BuildHudHealthCanvas(art,invalid);},"Out-of-range damage fraction accepted");
    invalid=state;invalid.breathPercent=std::numeric_limits<float>::quiet_NaN();
    Reject([&] {(void)BuildHudHealthCanvas(art,invalid);},"Nonfinite breath accepted");
    invalid=state;invalid.energy.reset();
    Reject([&] {(void)BuildHudHealthCanvas(art,invalid);},"Partial energy state accepted");
    invalid=state;invalid.energy=std::numeric_limits<float>::max();invalid.energyMax=std::numeric_limits<float>::min();
    Reject([&] {(void)BuildHudHealthCanvas(art,invalid);},"Energy ratio overflow accepted");
    auto shortCanvas=EmptyHudCanvas(4u,4u);
    Reject([&] {DrawHudProgressBar(shortCanvas,0u,100.0f);},"Progress bar leaked beyond destination");
    Reject([&] {(void)OriginalHudScaledColor(std::numeric_limits<float>::infinity());},"Nonfinite health color accepted");
    const auto tinyBar=OriginalHudProgressBarPresentation(1.9f);
    Require(tinyBar.height==0u && tinyBar.color.red==255u,"Progress current value was not truncated before height/color");
    const auto halfway=OriginalHudProgressBarPresentation(40.99f);
    const auto halfColor=OriginalHudScaledColor(0.4f);
    Require(halfway.height==22u && halfway.color.red==halfColor.red && halfway.color.green==halfColor.green,
        "Progress color bypassed original integer-valued current state");
    for(unsigned n=0u;n<=1000u;++n) {
        const float percent=static_cast<float>(n)*0.1f;
        const auto current=static_cast<std::int32_t>(percent);
        const float fraction=static_cast<float>(current)/100.0f;
        const auto expectedColor=OriginalHudScaledColor(fraction);
        const auto actual=OriginalHudProgressBarPresentation(percent);
        Require(actual.height==static_cast<std::uint32_t>(fraction*55.0f) && actual.color.red==expectedColor.red &&
            actual.color.green==expectedColor.green && actual.color.blue==expectedColor.blue,
            "Progress percentage or store order differs from original UpdateBars");
    }
    Reject([&] {(void)OriginalHudProgressBarPresentation(std::numeric_limits<float>::max());},"Out-of-domain progress conversion accepted");
    Font font{7u,{Solid(1u,1u,{255u,255u,255u,255u})}};Glyph glyph;
    state.energy.reset();state.energyMax.reset();state.breathPercent=5.0f;
    for(const auto& [timer,red]:{std::pair{0.25f,0u},std::pair{std::nextafter(0.25f,1.0f),255u},
        std::pair{0.5f,0u},std::pair{0.76f,255u},std::pair{-0.26f,0u},std::pair{-0.0f,0u}}) {
        state.swimTimer=timer;auto oxygen=EmptyHudCanvas(kHudHealthWidth,kHudHealthHeight);
        DrawHudHealthText(oxygen,state,font,[&](unsigned) {return &glyph;});
        Require(Pixel(oxygen,61u,74u)[0u]==red,"Original low-oxygen modulo/strict flash threshold differs");
    }
    state.underwater=false;auto dry=EmptyHudCanvas(kHudHealthWidth,kHudHealthHeight);
    DrawHudHealthText(dry,state,font,[&](unsigned) {return &glyph;});
    Require(Pixel(dry,61u,74u)[3u]==0u,"Dry HUD displayed oxygen label");
}
}
int main() {
    try {Compass();Belt();BoundSnapshot();Health();std::cout << "PASS original HUD compositor: " << checks << " generated checks; compass wrap/crop, 1..9/0 belt mapping, explicit owned-item snapshots, genuine binding gate, six independent limbs, energy/breath and bounds\n";return 0;}
    catch (const std::exception& error) {std::cerr << "FAIL HUD compositor: " << error.what() << '\n';return 1;}
}
