#include "quest_inventory_navigation.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checks{};
void Require(const bool value, const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}
void Near(const float actual, const float expected, const char* description) {
    Require(std::fabs(actual-expected) < 0.0001f,description);
}
void NoSteps(const QuestVr::MenuNavigationSteps& steps, const char* description) {
    Require(steps.itemSteps == 0 && steps.pageSteps == 0,description);
}
QuestVr::PersonaInventoryLayout SmallLayout(const std::size_t count) {
    return QuestVr::BuildPersonaInventoryLayout(std::vector<QuestVr::PersonaInventoryItem>(count));
}

void VisibleTargetsAndOverflow() {
    using namespace QuestVr;
    std::vector<PersonaInventoryItem> items(6u,{4u,2u,-1,-1,203u,77u});
    items[0].displayable = false;
    items[1].available = false;
    const auto layout = BuildPersonaInventoryLayout(items);
    const auto originalLayout = layout;
    InventoryNavigationModel model;
    Require(model.SetLayout({"Hidden","Unavailable","GEP0","GEP1","GEP2","Overflow"},layout),"Valid overflow layout rejected");
    const auto& state = model.State();
    Require(state.kind == InventoryViewKind::Overflow && state.totalItems == 6u && state.visibleItems == 3u &&
        state.hiddenItems == 1u && state.unavailableItems == 1u && state.overflowItems == 1u && state.error.empty(),
        "Overflow/hidden/unavailable accounting collapsed into a composition error");
    Require(model.SelectedIndex() == 2u && model.SelectedIdentity() == "GEP0","Initial selection did not choose first visible item");
    Require(!model.SelectIndex(0u) && !model.SelectIndex(1u) && !model.SelectIndex(5u),"Invisible item became selectable");
    Require(model.Move(1) && model.SelectedIndex() == 3u,"Next did not skip invisible indices");
    Require(model.Move(-1) && model.SelectedIndex() == 2u,"Previous did not preserve visible order");
    Require(model.Move(-1) && model.SelectedIndex() == 4u,"Previous failed to wrap visible items");
    Require(model.Move(1) && model.SelectedIndex() == 2u,"Next failed to wrap visible items");
    Require(!model.Move(3) && model.SelectedIndex() == 2u,"Whole visible-list cycle changed selection");
    Require(model.Move(std::numeric_limits<int>::min()),"Minimum signed step count was not safely handled");
    Require(layout.placements.size() == originalLayout.placements.size() && layout.occupiedCells == originalLayout.occupiedCells &&
        layout.placements[0].rect.x == originalLayout.placements[0].rect.x,"Navigation mutated detached layout coordinates");
}

void IdentityPreservationAndExplicitStates() {
    using namespace QuestVr;
    InventoryNavigationModel model;
    Require(model.State().kind == InventoryViewKind::Empty && !model.SelectedIndex(),"Default model is not explicitly empty");
    Require(model.SetLayout({"A","B","C"},SmallLayout(3u)) && model.SelectIdentity("B"),"Initial identity selection failed");
    Require(model.SetLayout({"C","A","B"},SmallLayout(3u)) && model.SelectedIdentity() == "B" && model.SelectedIndex() == 2u,
        "Reorder preserved raw index instead of stable identity");
    Require(model.SetLayout({"C","A"},SmallLayout(2u)) && model.SelectedIdentity() == "A", "Removal did not choose nearest remaining visible ordinal");
    Require(model.SetLayout({"C","A","New"},SmallLayout(3u)) && model.SelectedIdentity() == "A","Pickup changed an existing selected identity");
    Require(model.SetLayout({},SmallLayout(0u)) && model.State().kind == InventoryViewKind::Empty && !model.SelectedIndex() &&
        model.SelectedIdentity().empty() && !model.Move(1),"Empty inventory retained actionable selection");
    PersonaInventoryItem hidden; hidden.displayable = false;
    Require(model.SetLayout({"Hidden"},BuildPersonaInventoryLayout({hidden})) && model.State().kind == InventoryViewKind::NoVisibleItems &&
        model.State().hiddenItems == 1u && !model.SelectedIndex(),"All-hidden inventory was confused with empty or error");
    hidden.available = false;
    Require(model.SetLayout({"Unavailable"},BuildPersonaInventoryLayout({hidden})) && model.State().kind == InventoryViewKind::NoVisibleItems &&
        model.State().unavailableItems == 1u,"Unavailable metadata was represented as a hidden valid item");
    model.SetError("Composition rejected",7u);
    Require(model.State().kind == InventoryViewKind::Error && model.State().totalItems == 7u && model.State().error == "Composition rejected" &&
        model.Targets().empty() && !model.SelectIdentity("A"),"Composition error retained unsafe targets or claimed empty success");
    Require(model.SetLayout({"A"},SmallLayout(1u)) && model.State().kind == InventoryViewKind::Ready && model.State().error.empty(),
        "A later valid publication could not recover from an error");
}

void LayoutValidation() {
    using namespace QuestVr;
    InventoryNavigationModel model;
    Require(!model.SetLayout({"Duplicate","Duplicate"},SmallLayout(2u)) && model.State().kind == InventoryViewKind::Error,
        "Ambiguous visible stable identities were accepted");
    auto layout = SmallLayout(1u); layout.placements[0].inventoryIndex = 1u;
    Require(!model.SetLayout({"A"},layout),"Out-of-range target index was accepted");
    layout = SmallLayout(1u); ++layout.placements[0].rect.x;
    Require(!model.SetLayout({"A"},layout),"Non-original target coordinates were accepted");
    layout = SmallLayout(2u); layout.placements[1].rect = layout.placements[0].rect;
    Require(!model.SetLayout({"A","B"},layout),"Occupied-cell overlap was accepted");
    layout = SmallLayout(1u); layout.unplaced.push_back(0u);
    Require(!model.SetLayout({"A"},layout),"Visible target also became an overflow entry");
    layout = SmallLayout(1u); layout.unavailable = {1u,1u};
    Require(!model.SetLayout({"A","B"},layout),"Duplicate unavailable metadata was accepted");
    layout = SmallLayout(1u); ++layout.occupiedCells;
    Require(!model.SetLayout({"A"},layout),"Corrupt occupancy accounting was accepted");
    layout = SmallLayout(1u); layout.placements[0].rect.width = std::numeric_limits<std::uint32_t>::max();
    Require(!model.SetLayout({"A"},layout),"Extreme rectangle dimensions were accepted");
    Require(!model.SetLayout({std::string("A\0B",3u)},SmallLayout(1u)),"Embedded-NUL stable identity was accepted");
    Require(model.State().kind == InventoryViewKind::Error && model.Targets().empty() && !model.SelectedIndex(),
        "Rejected publication retained actionable stale selection");
}

void CanvasEdgesAndMultiCellTargets() {
    using namespace QuestVr;
    InventoryNavigationModel model;
    const auto layout = BuildPersonaInventoryLayout({{4u,1u,-1,-1,159u,47u},{1u,1u,-1,-1,46u,28u},{2u,2u,-1,-1,94u,65u}});
    Require(model.SetLayout({"Rifle","Pistol","Assault"},layout),"Multi-cell hit-test layout rejected");
    Require(model.HitTestCanvas(42.0f,62.0f) == 0u,"Top/left target edges were excluded");
    Require(model.HitTestCanvas(253.999f,100.0f) == 0u,"Rifle logical interior was not one target");
    Require(model.HitTestCanvas(254.0f,100.0f) == 1u,"Shared border did not belong to adjacent logical cell");
    Require(model.HitTestCanvas(307.999f,100.0f) == 1u && !model.HitTestCanvas(308.0f,100.0f),
        "Outer right border was not half-open");
    Require(model.HitTestCanvas(70.0f,115.0f) == 2u,"Shared row border favored the preceding item");
    Require(model.HitTestCanvas(100.0f,170.0f) == 2u && model.HitTestCanvas(148.999f,221.999f) == 2u,
        "Multi-row footprint or exposed border was split into cells");
    Require(!model.HitTestCanvas(149.0f,170.0f) && !model.HitTestCanvas(70.0f,222.0f) && !model.HitTestCanvas(41.999f,62.0f),
        "Empty/outside canvas coordinates acquired a target");
    Require(model.SelectCanvasPoint(100.0f,170.0f) && model.SelectedIdentity() == "Assault","Point selection did not use stable item target");
    Require(!model.SelectCanvasPoint(0.0f,0.0f) && model.SelectedIdentity() == "Assault","Pointer miss destroyed existing selection");
    Require(!model.HitTestCanvas(std::numeric_limits<float>::quiet_NaN(),62.0f) &&
        !model.HitTestCanvas(42.0f,std::numeric_limits<float>::infinity()),"Nonfinite pointer acquired a target");
}

void PanelRayUsesActualDimensions() {
    using namespace QuestVr;
    const auto hit = [](const InventoryRayVector& origin, const InventoryRayVector& direction) {
        return IntersectPersonaPanelRay(origin,direction,1.2f,0.9f,640u,480u);
    };
    const auto center = hit({0.0f,0.0f,1.0f},{0.0f,0.0f,-2.0f});
    Require(center.has_value(),"Non-unit forward ray missed panel");
    Near(center->x,320.0f,"Panel center x did not map to actual canvas");
    Near(center->y,240.0f,"Panel center y did not map to actual canvas");
    Require(hit({0.0f,0.0f,1.0f},{0.0f,0.0f,-0.00000001f}).has_value(),
        "Equivalent tiny non-unit forward ray was mistaken for a parallel ray");
    const auto topLeft = hit({-0.6f,0.45f,1.0f},{0.0f,0.0f,-1.0f});
    Require(topLeft.has_value(),"Top-left panel edge was excluded");
    Near(topLeft->x,0.0f,"Panel left edge mapped incorrectly");
    Near(topLeft->y,0.0f,"Panel top edge mapped incorrectly");
    Require(!hit({0.6f,0.0f,1.0f},{0.0f,0.0f,-1.0f}) && !hit({0.0f,-0.45f,1.0f},{0.0f,0.0f,-1.0f}),
        "Panel right/bottom edge escaped half-open canvas");
    Require(!hit({0.0f,0.0f,1.0f},{1.0f,0.0f,0.0f}) && !hit({0.0f,0.0f,-1.0f},{0.0f,0.0f,-1.0f}) &&
        !hit({0.0f,0.0f,-1.0f},{0.0f,0.0f,1.0f}),"Parallel/behind/back-face ray acquired a panel hit");
    const auto different = IntersectPersonaPanelRay({0.5f,0.25f,1.0f},{0.0f,0.0f,-1.0f},2.0f,1.0f,1000u,400u);
    Require(different.has_value(),"Alternate actual panel dimensions were ignored");
    Near(different->x,750.0f,"Alternate physical width or canvas width was ignored");
    Near(different->y,100.0f,"Alternate physical height or canvas height was ignored");
    Require(!IntersectPersonaPanelRay({0,0,1},{0,0,-1},0.0f,1.0f,640u,480u) &&
        !IntersectPersonaPanelRay({0,0,1},{0,0,-1},1.0f,1.0f,0u,480u) &&
        !hit({std::numeric_limits<float>::quiet_NaN(),0,1},{0,0,-1}),"Invalid panel dimensions/ray were accepted");
}

void DominantAxisHeldRepeatAndTracking() {
    using namespace QuestVr;
    MenuNavigationInput input;
    NoSteps(input.Update(0.0f,-1.0f,true,0.0),"Opening with a held stick navigated without neutral");
    NoSteps(input.Update(0.0f,0.0f,true,0.0),"Neutral sample produced navigation");
    auto steps = input.Update(0.8f,-0.8f,true,0.0);
    Require(steps.itemSteps == 1 && steps.pageSteps == 0,"Diagonal tie paged away instead of browsing one item");
    NoSteps(input.Update(0.8f,-0.8f,true,0.39),"Held stick repeated before initial delay");
    steps = input.Update(0.8f,-0.8f,true,0.01);
    Require(steps.itemSteps == 1 && steps.pageSteps == 0,"Held stick did not repeat at initial delay");
    steps = input.Update(0.9f,-0.7f,true,0.24);
    Require(steps.itemSteps == 2 && steps.pageSteps == 0,"Held excursion changed dominant axis or lost repeat interval");
    steps = input.Update(0.8f,-0.8f,true,1000.0);
    Require(steps.itemSteps > 0 && steps.itemSteps <= MenuNavigationInput::MaximumStepsPerUpdate && steps.pageSteps == 0,
        "Long frame caused unbounded navigation catch-up");
    NoSteps(input.Update(0.8f,0.8f,true,1.0),"Opposite input rearmed without neutral");
    NoSteps(input.Update(0.8f,-0.8f,true,1.0),"Blocked excursion resumed without neutral");
    NoSteps(input.Update(0.0f,0.0f,true,0.0),"Centered excursion navigated");
    steps = input.Update(0.7f,0.2f,true,0.0);
    Require(steps.pageSteps == 1 && steps.itemSteps == 0,"Right dominant threshold did not page right");
    NoSteps(input.Update(1.0f,0.0f,false,1.0),"Untracked controller navigated");
    NoSteps(input.Update(1.0f,0.0f,true,1.0),"Tracking recovery reused stale held input");
    NoSteps(input.Update(0.35f,-0.35f,true,0.0),"Release-band edge produced navigation");
    steps = input.Update(0.2f,0.7f,true,0.0);
    Require(steps.itemSteps == -1 && steps.pageSteps == 0,"Up dominant threshold did not browse previous item");
    NoSteps(input.Update(std::numeric_limits<float>::quiet_NaN(),1.0f,true,0.01),"Invalid axis navigated");
    NoSteps(input.Update(0.0f,1.0f,true,1.0),"Invalid-data recovery reused held input");
    input.Reset();
    NoSteps(input.Update(0.0f,0.0f,true,0.0),"Explicit reset/neutral navigated");
    steps = input.Update(-1.0f,0.4f,true,0.0);
    Require(steps.pageSteps == -1 && steps.itemSteps == 0,"Left dominant input did not page left");
    NoSteps(input.Update(-1.0f,0.4f,true,-0.1),"Negative time produced a navigation repeat");
}
} // namespace

int main() {
    try {
        VisibleTargetsAndOverflow();
        IdentityPreservationAndExplicitStates();
        LayoutValidation();
        CanvasEdgesAndMultiCellTargets();
        PanelRayUsesActualDimensions();
        DominantAxisHeldRepeatAndTracking();
        std::cout << "PASS: 6 inventory navigation regression groups; " << checks
                  << " checks. Synthetic detached UI only; no equipment, game data, headset or SDK required.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
