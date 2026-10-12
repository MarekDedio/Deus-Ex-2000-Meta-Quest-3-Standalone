#include "quest_dialogue_ui.h"

#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}
template<class Action> void Reject(Action&& action,const char* description) {
    bool rejected{};
    try { action(); } catch (const std::runtime_error&) { rejected=true; }
    Require(rejected,description); ++refusals;
}
QuestVr::PersonaUiImage Solid(std::uint32_t width,std::uint32_t height,
    std::array<std::uint8_t,4> color) {
    QuestVr::PersonaUiImage result{width,height,std::vector<std::uint8_t>(width*height*4u)};
    for (std::size_t i=0;i<result.rgba.size();i+=4u) std::copy(color.begin(),color.end(),result.rgba.begin()+i);
    return result;
}
}
int main() {
    try {
        using namespace QuestVr;
        const std::array<PersonaUiImage,2> backgrounds{{Solid(256u,128u,{100u,200u,50u,255u}),
            Solid(256u,128u,{200u,100u,100u,255u})}};
        const std::array<PersonaUiImage,2> borders{{Solid(256u,128u,{0u,0u,0u,0u}),
            Solid(256u,128u,{0u,0u,0u,0u})}};
        auto canvas=BuildDataLinkUiCanvas(backgrounds,borders);
        Require(canvas.width==431u && canvas.height==114u && canvas.rgba.size()==431u*114u*4u,"Datalink original logical dimensions differ");
        Require(canvas.rgba[0u]==50u && canvas.rgba[1u]==100u && canvas.rgba[2u]==25u,"Original default background tint differs");
        Require(canvas.rgba[256u*4u]==100u && canvas.rgba[(431u*113u+430u)*4u]==100u,"Datalink padded second texture was stretched or omitted");
        const auto before=canvas.rgba;
        AddDataLinkPortrait(canvas,Solid(64u,64u,{20u,30u,40u,255u}));
        for (std::uint32_t y=0;y<114u;++y) for (std::uint32_t x=0;x<431u;++x) {
            const auto offset=(y*431u+x)*4u;
            const bool portrait=x>=20u && x<84u && y>=25u && y<89u;
            Require(canvas.rgba[offset]==(portrait?20u:before[offset]),"Portrait draw escaped original child window");
        }
        Require(DataLinkDisplayName("JaimeReyes")=="Jaime Reyes" &&
            DataLinkDisplayName("jAiMeReYeS").empty() &&
            DataLinkDisplayName("Unknown_Bind").empty(),"Original display-name table or unknown fallback differs");
        Require(DataLinkPortraitName("JaimeReyes",0)=="JaimeReyes" &&
            DataLinkPortraitName("BobPage",15)=="BobPageAug" &&
            DataLinkPortraitName("BobPage",1)=="BobPage" &&
            DataLinkPortraitName("PaulDenton",1,4u)=="PaulDenton_5" &&
            DataLinkPortraitName("12345678901234567890",0)=="1234567890123456","Original exact portrait selection differs");
        Reject([&] { (void)DataLinkPortraitName("PaulDenton",0,5u); },"Invalid player skin accepted");
        Reject([&] { (void)DataLinkPortraitName("",0); },"Empty speaker accepted");
        auto invalid=backgrounds; invalid[0].width=128u;
        Reject([&] { (void)BuildDataLinkUiCanvas(invalid,borders); },"Malformed original texture accepted");
        Reject([&] { AddDataLinkPortrait(canvas,Solid(128u,64u,{0u,0u,0u,255u})); },"Wrong portrait dimensions accepted");
        const auto partial=BuildDataLinkTextWindow("Hello world",5u);
        Require(partial.lines[0]=="Hello" && partial.cursorColumn==5u && !partial.allTextVisible,"Progressive datalink characters differ");
        const auto wrapped=BuildDataLinkTextWindow(std::string(40u,'a')+" bcde");
        Require(wrapped.lines[0]==std::string(40u,'a')+" " && wrapped.lines[1]=="bcde","Original fixed-column word wrap differs");
        const auto longWord=BuildDataLinkTextWindow(std::string(43u,'b'));
        Require(longWord.lines[0].size()==42u && longWord.lines[1]=="b","Oversized original speech word did not hard-wrap");
        const auto scrolled=BuildDataLinkTextWindow("one\ntwo\nthree\nfour\nfive\nsix\nseven\neight");
        Require(scrolled.lines[0]=="three" && scrolled.lines[5]=="eight" && scrolled.cursorRow==5u,"Long speech did not scroll six-row window");
        Reject([&] { (void)BuildDataLinkTextWindow(std::string(16385u,'a')); },"Unbounded speech accepted");
        float remaining=6.0f,elapsed=0.0f;
        Require(AdvanceDialoguePresentationClock(1.0f,remaining,elapsed) && remaining==5.0f && elapsed==1.0f,
            "Tracked speech presentation clock diverged");
        // The same update runs before the application returns for tracking
        // loss. Skipping the renderer must not skip its typewriter time.
        Require(AdvanceDialoguePresentationClock(2.0f,remaining,elapsed) && remaining==3.0f && elapsed==3.0f,
            "Tracking-loss speech presentation clock diverged");
        const auto resumed=BuildDataLinkTextWindow(std::string(180u,'a'),
            static_cast<std::size_t>(elapsed/kDataLinkSecondsPerCharacter));
        Require(resumed.lines[0].size()+resumed.lines[1].size()+resumed.lines[2].size()==100u,
            "Tracking resume retained stale typed characters");
        Require(AdvanceDialoguePresentationClock(1.25f,remaining,elapsed) && remaining==1.75f && elapsed==4.25f,
            "Long frame separately clamped the typewriter clock");
        const auto deadline=remaining+elapsed;
        Require(AdvanceDialoguePresentationClock(0.75f,remaining,elapsed) && remaining+elapsed==deadline,
            "Speech expiry and typewriter did not share one deadline");
        remaining=4.0f; // The decoded original audio extends the same active line.
        Require(AdvanceDialoguePresentationClock(2.0f,remaining,elapsed) && remaining==2.0f && elapsed==7.0f,
            "Audio duration extension reset or desynchronized the speech clock");
        const auto savedRemaining=remaining,savedElapsed=elapsed;
        Require(!AdvanceDialoguePresentationClock(-1.0f,remaining,elapsed) &&
            remaining==savedRemaining && elapsed==savedElapsed,"Negative clock step published a partial update");
        Require(!AdvanceDialoguePresentationClock(std::numeric_limits<float>::quiet_NaN(),remaining,elapsed) &&
            remaining==savedRemaining && elapsed==savedElapsed,"Nonfinite clock step published a partial update");
        Require(AdvanceDialoguePresentationClock(100.0f,remaining,elapsed) && remaining==0.0f && elapsed==9.0f,
            "Expired speech clock advanced past the actual active interval");
        Require(AdvanceDialoguePresentationClock(1.0f,remaining,elapsed) && remaining==0.0f && elapsed==9.0f,
            "Inactive speech clock continued typing");
        std::cout << "Datalink original-layout compositor: " << checks << " checks, " << refusals << " expected refusals\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
