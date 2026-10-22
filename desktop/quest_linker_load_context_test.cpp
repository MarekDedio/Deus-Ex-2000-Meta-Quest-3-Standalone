#include "quest_linker_load_context.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

// Pure generated/literal controls ONLY: no source map/package is opened,
// no runtime/linker UObject is initialized and no original binary executes.
// Passing this test is not actual first-Training load-context integration.
namespace {
using Profile=QuestVr::OriginalLinkerLoadProfile;
using Action=QuestVr::OriginalLinkerCreateExportAction;
std::size_t checks{},cases{};
void Require(bool value,const char* why) {
    ++checks;if(!value)throw std::runtime_error(why);
}
constexpr Profile None{},Editor{true,false,false},Client{false,true,false},Server{false,false,true},
    StandaloneClientServer{false,true,true},All{true,true,true};
static_assert(None.Mask()==0u && Client.Mask()==0x00010000u && Server.Mask()==0x00020000u &&
    Editor.Mask()==0x00040000u && StandaloneClientServer.Mask()==0x00030000u && All.Mask()==0x00070000u);
static_assert(QuestVr::OriginalLinkerCreateExportDecision(None,0u,true)==Action::UseCached);
static_assert(QuestVr::OriginalLinkerCreateExportDecision(None,0xffffffffu,false)==Action::Exclude);
static_assert(QuestVr::OriginalLinkerCreateExportDecision(Client,0x00100000u,false)==Action::Exclude);
static_assert(QuestVr::OriginalLinkerCreateExportDecision(Client,0x00110000u,false)==Action::Construct);
static_assert(noexcept(QuestVr::OriginalLinkerCreateExportDecision(None,0u,false)));

void Exhaustive() {
    // Profile index bits are Editor/Client/Server. Literal mask table is an
    // independent oracle, not reconstructed with the production Mask method.
    constexpr std::array<std::uint32_t,8u> masks{
        0u,0x00040000u,0x00010000u,0x00050000u,
        0x00020000u,0x00060000u,0x00030000u,0x00070000u};
    // Source index bits instead mean Client/Server/Editor; keep the two
    // enumerations different to expose accidental bit/order correspondence.
    constexpr std::array<std::uint32_t,8u> sourceBits{
        0u,0x00010000u,0x00020000u,0x00030000u,
        0x00040000u,0x00050000u,0x00060000u,0x00070000u};
    constexpr std::array<std::uint32_t,6u> otherBits{
        0u,0x00000001u,0x02000000u,0x040f0025u&~0x00070000u,
        0xff880000u,0xffffffffu&~0x00770000u};
    for(unsigned context=0u;context<8u;++context) {
        const Profile profile{(context&1u)!=0u,(context&2u)!=0u,(context&4u)!=0u};
        const auto preserved=profile;
        Require(profile.Mask()==masks[context],"Explicit editor/client/server profile produced the wrong literal mask");
        Require((profile.Mask()&~0x00070000u)==0u,"Profile admitted bits outside the bounded eight-context family");
        for(unsigned source=0u;source<8u;++source)
            for(unsigned excluded=0u;excluded<8u;++excluded)
                for(const auto noise:otherBits)
                    for(const bool cached:{false,true}) {
                        const auto flags=sourceBits[source]|(static_cast<std::uint32_t>(excluded)<<20u)|noise;
                        const bool logicalAdmission=(profile.editor&&(source&4u)!=0u) ||
                            (profile.client&&(source&1u)!=0u) || (profile.server&&(source&2u)!=0u);
                        const auto expected=cached ? Action::UseCached : logicalAdmission ? Action::Construct : Action::Exclude;
                        Require(QuestVr::OriginalLinkerCreateExportDecision(profile,flags,cached)==expected,
                            "Cached-first decision changed positive OR selection or interpreted unrelated NotFor/header bits");
                        Require(profile==preserved,"Pure CreateExport decision mutated its caller-owned profile");
                        ++cases;
                    }
    }
}
void FirstTrainingLiteralControls() {
    // Static source literals independently recorded in
    // docs/ORIGINAL-LINKER-LOAD-CONTEXT.md, NOT a production table/map oracle.
    // The names are diagnostic labels only; the kernel never receives them.
    struct Control {const char* name;std::uint32_t flags;bool nonEditor;};
    constexpr std::array<Control,9u> controls{{
        {"Camera4",0x02340000u,false},{"Camera5",0x02340000u,false},
        {"Camera6",0x02340000u,false},{"Camera7",0x02340000u,false},
        {"slot1 Brush1540",0x02340001u,false},
        {"LevelInfo0",0x02070001u,true},{"PlayerStart1",0x02070001u,true},
        {"CrateBreakableMedGeneral1",0x02070001u,true},{"CrateBreakableMedGeneral2",0x02070001u,true}}};
    for(const auto& control:controls) {
        Require(control.name&&*control.name,"Literal control lacks its diagnostic identity");
        for(const auto profile:{Client,Server,StandaloneClientServer}) {
            Require(QuestVr::OriginalLinkerCreateExportDecision(profile,control.flags,false)==
                (control.nonEditor ? Action::Construct : Action::Exclude),
                "First-Training literal positive/negative noneditor flag control was inverted");
            Require(QuestVr::OriginalLinkerCreateExportDecision(profile,control.flags,true)==Action::UseCached,
                "An existing cached editor-only object was incorrectly invalidated by context");
            ++cases;
        }
        Require(QuestVr::OriginalLinkerCreateExportDecision(Editor,control.flags,false)==Action::Construct,
            "Explicit editor context rejected an editor-admitted literal export");
        Require(QuestVr::OriginalLinkerCreateExportDecision(None,control.flags,false)==Action::Exclude,
            "Empty profile inferred a standalone/editor fallback");
    }
    Require(QuestVr::OriginalLinkerCreateExportDecision(StandaloneClientServer,0x00010000u,false)==Action::Construct &&
        QuestVr::OriginalLinkerCreateExportDecision(StandaloneClientServer,0x00020000u,false)==Action::Construct,
        "Combined client/server profile used an AND/all-context requirement instead of the original OR gate");
    Require(QuestVr::OriginalLinkerCreateExportDecision(All,0x00700000u,false)==Action::Exclude,
        "Upper NotForClient/Server/Edit bits were confused with positive load bits");
}
} // namespace

int main() {
    try {
        Exhaustive();FirstTrainingLiteralControls();
        std::cout<<"Pure original linker load-context decision: "<<checks<<" checks / "<<cases
            <<" generated cases; explicit eight-context OR masks, cached-first, unrelated flags and first-Training literal controls. NOT runtime loader/save/startup integration.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Linker load-context decision failed: "<<error.what()<<'\n';return 1;
    }
}
