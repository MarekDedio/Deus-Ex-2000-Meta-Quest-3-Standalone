#pragma once

#include "persona_ui_canvas.h"
#include "quest_vr_input.h"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>

// Detached UI interaction only. These helpers do not allocate game actors,
// mutate inventory/equipment, execute scripts, or manufacture missing items.
namespace QuestVr {

enum class InventoryViewKind : std::uint8_t { Ready, Empty, NoVisibleItems, Overflow, Error };

struct InventoryNavigationState {
    InventoryViewKind kind{InventoryViewKind::Empty};
    std::size_t totalItems{}, visibleItems{}, hiddenItems{}, unavailableItems{}, overflowItems{};
    std::string error;
};

struct InventoryNavigationTarget {
    std::size_t inventoryIndex{};
    std::string identity;
    PersonaUiRect rect{};
};

class InventoryNavigationModel {
   public:
    // Layout placements, not the raw inventory list, are the selectable set.
    // Validate before publication; an invalid layout is distinct from empty
    // inventory and disables selection rather than guessing new coordinates.
    bool SetLayout(const std::vector<std::string>& identities, const PersonaInventoryLayout& layout) {
        const auto fail = [&](const char* reason) { SetError(reason, identities.size()); return false; };
        if (identities.size() > 4096u || layout.placements.size() > kPersonaVisibleItems ||
            layout.unavailable.size() > identities.size() || layout.unplaced.size() > identities.size())
            return fail("Inventory navigation metadata exceeds its bounds");
        std::vector<std::uint8_t> disposition(identities.size(), 0u);
        std::array<bool,kPersonaVisibleItems> occupied{};
        std::vector<InventoryNavigationTarget> targets;
        targets.reserve(layout.placements.size());
        std::size_t occupiedCells{}, generatedPositions{};
        for (const auto& placed : layout.placements) {
            if (placed.inventoryIndex >= identities.size() || disposition[placed.inventoryIndex] != 0u)
                return fail("Inventory layout contains a duplicate or missing identity index");
            const auto& identity = identities[placed.inventoryIndex];
            if (identity.empty() || identity.size() > 8192u || identity.find('\0') != std::string::npos ||
                std::any_of(targets.begin(),targets.end(),[&](const auto& item) { return item.identity == identity; }))
                return fail("Visible inventory identities are invalid or ambiguous");
            const auto& rect = placed.rect;
            if (rect.x < kPersonaGridX || rect.y < kPersonaGridY || rect.width <= 1u || rect.height <= 1u ||
                (rect.x-kPersonaGridX)%kPersonaGridStep != 0u || (rect.y-kPersonaGridY)%kPersonaGridStep != 0u ||
                (rect.width-1u)%kPersonaGridStep != 0u || (rect.height-1u)%kPersonaGridStep != 0u)
                return fail("Inventory target does not match the original grid coordinates");
            const auto x = (rect.x-kPersonaGridX)/kPersonaGridStep;
            const auto y = (rect.y-kPersonaGridY)/kPersonaGridStep;
            const auto width = (rect.width-1u)/kPersonaGridStep;
            const auto height = (rect.height-1u)/kPersonaGridStep;
            if (width > kPersonaGridColumns || height > kPersonaGridRows ||
                x > kPersonaGridColumns-width || y > kPersonaGridRows-height)
                return fail("Inventory target exceeds the original grid");
            for (std::uint32_t row = 0u; row < height; ++row) {
                for (std::uint32_t column = 0u; column < width; ++column) {
                    auto& cell = occupied[(y+row)*kPersonaGridColumns+x+column];
                    if (cell) return fail("Inventory targets overlap occupied grid cells");
                    cell = true;
                }
            }
            occupiedCells += width*height;
            generatedPositions += placed.generatedPosition ? 1u : 0u;
            disposition[placed.inventoryIndex] = 1u;
            targets.push_back({placed.inventoryIndex, identity, rect});
        }
        for (const auto index : layout.unavailable) {
            if (index >= identities.size() || disposition[index] != 0u)
                return fail("Unavailable inventory indices are invalid or duplicated");
            disposition[index] = 2u;
        }
        for (const auto index : layout.unplaced) {
            if (index >= identities.size() || disposition[index] != 0u)
                return fail("Overflow inventory indices are invalid or duplicated");
            disposition[index] = 3u;
        }
        if (occupiedCells != layout.occupiedCells || generatedPositions != layout.generatedPositions)
            return fail("Inventory layout accounting does not match its targets");
        std::sort(targets.begin(),targets.end(),[](const auto& left, const auto& right) {
            return left.inventoryIndex < right.inventoryIndex;
        });
        const std::string oldIdentity = SelectedIdentity();
        const std::size_t oldOrdinal = selectedOrdinal_.value_or(0u);
        targets_ = std::move(targets);
        state_ = {};
        state_.totalItems = identities.size();
        state_.visibleItems = targets_.size();
        state_.unavailableItems = layout.unavailable.size();
        state_.overflowItems = layout.unplaced.size();
        state_.hiddenItems = identities.size()-targets_.size()-layout.unavailable.size()-layout.unplaced.size();
        state_.kind = identities.empty() ? InventoryViewKind::Empty :
            !layout.unplaced.empty() ? InventoryViewKind::Overflow :
            targets_.empty() ? InventoryViewKind::NoVisibleItems : InventoryViewKind::Ready;
        selectedOrdinal_.reset();
        if (!targets_.empty()) {
            selectedOrdinal_ = std::min(oldOrdinal,targets_.size()-1u);
            if (!oldIdentity.empty()) SelectIdentity(oldIdentity);
        }
        return true;
    }

    void SetError(std::string reason, const std::size_t totalItems = 0u) {
        targets_.clear();
        selectedOrdinal_.reset();
        state_ = {};
        state_.kind = InventoryViewKind::Error;
        state_.totalItems = totalItems;
        state_.error = reason.empty() ? "Inventory layout unavailable" : reason.substr(0u,1024u);
    }

    bool SelectIdentity(const std::string& identity) {
        for (std::size_t ordinal = 0u; ordinal < targets_.size(); ++ordinal) {
            if (targets_[ordinal].identity == identity) { selectedOrdinal_ = ordinal; return true; }
        }
        return false;
    }

    bool SelectIndex(const std::size_t inventoryIndex) {
        for (std::size_t ordinal = 0u; ordinal < targets_.size(); ++ordinal) {
            if (targets_[ordinal].inventoryIndex == inventoryIndex) { selectedOrdinal_ = ordinal; return true; }
        }
        return false;
    }

    // Positive steps mean next/down. Wrap across visible items only.
    bool Move(const int steps) {
        if (targets_.empty() || !selectedOrdinal_) return false;
        const auto count = static_cast<std::int64_t>(targets_.size());
        const auto old = *selectedOrdinal_;
        const auto next = (static_cast<std::int64_t>(old)+static_cast<std::int64_t>(steps)%count+count)%count;
        selectedOrdinal_ = static_cast<std::size_t>(next);
        return *selectedOrdinal_ != old;
    }

    std::optional<std::size_t> HitTestCanvas(const float x, const float y) const {
        if (!std::isfinite(x) || !std::isfinite(y)) return {};
        // Original buttons use 53*N+1 pixels. Adjacent border pixels overlap:
        // the logical cell's interior wins, then an exposed outer border.
        for (const bool interior : {true,false}) {
            for (const auto& item : targets_) {
                const auto width = item.rect.width-(interior ? 1u : 0u);
                const auto height = item.rect.height-(interior ? 1u : 0u);
                if (x >= static_cast<float>(item.rect.x) && y >= static_cast<float>(item.rect.y) &&
                    x < static_cast<float>(item.rect.x+width) && y < static_cast<float>(item.rect.y+height))
                    return item.inventoryIndex;
            }
        }
        return {};
    }

    bool SelectCanvasPoint(const float x, const float y) {
        const auto index = HitTestCanvas(x,y);
        return index && SelectIndex(*index);
    }

    std::optional<std::size_t> SelectedIndex() const {
        return selectedOrdinal_ ? std::optional<std::size_t>{targets_[*selectedOrdinal_].inventoryIndex} : std::nullopt;
    }
    std::string SelectedIdentity() const { return selectedOrdinal_ ? targets_[*selectedOrdinal_].identity : std::string{}; }
    const std::vector<InventoryNavigationTarget>& Targets() const { return targets_; }
    const InventoryNavigationState& State() const { return state_; }

   private:
    std::vector<InventoryNavigationTarget> targets_;
    std::optional<std::size_t> selectedOrdinal_;
    InventoryNavigationState state_;
};

struct MenuNavigationSteps {
    int pageSteps{}; // Positive: page right. Negative: page left.
    int itemSteps{}; // Positive: next/down. Negative: previous/up.
};

class MenuNavigationInput {
   public:
    static constexpr double InitialRepeatDelay = 0.40;
    static constexpr double RepeatDelay = 0.12;
    static constexpr double MaximumDeltaSeconds = 0.50;
    static constexpr int MaximumStepsPerUpdate = 4;

    // Opening a menu/tracking loss requires a fresh tracked neutral sample.
    void Reset() { needsNeutral_ = true; direction_ = 0; vertical_ = false; repeatRemaining_ = 0.0; }

    MenuNavigationSteps Update(const float x, const float y, const bool tracked, const double deltaSeconds) {
        MenuNavigationSteps result;
        if (!tracked || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
            Reset(); return result;
        }
        const float magnitude = std::max(std::fabs(x),std::fabs(y));
        if (magnitude <= AxisReleaseThreshold) {
            needsNeutral_ = false; direction_ = 0; repeatRemaining_ = 0.0; return result;
        }
        if (needsNeutral_) return result;
        const auto emit = [&](const int count) {
            if (vertical_) result.itemSteps = -direction_*count;
            else result.pageSteps = direction_*count;
        };
        if (direction_ == 0) {
            if (magnitude < AxisEngageThreshold) return result;
            // One axis per centered excursion. Vertical wins exact ties, so
            // diagonal browsing cannot both change page and skip selection.
            vertical_ = std::fabs(y) >= std::fabs(x);
            direction_ = (vertical_ ? y : x) > 0.0f ? 1 : -1;
            repeatRemaining_ = InitialRepeatDelay;
            emit(1); return result;
        }
        const float axis = vertical_ ? y : x;
        if (std::fabs(axis) <= AxisReleaseThreshold) return result;
        if ((axis > 0.0f ? 1 : -1) != direction_) { Reset(); return result; }
        repeatRemaining_ -= std::min(deltaSeconds,MaximumDeltaSeconds);
        int count{};
        while (repeatRemaining_ <= 0.000000001 && count < MaximumStepsPerUpdate) {
            ++count; repeatRemaining_ += RepeatDelay;
        }
        if (count == MaximumStepsPerUpdate && repeatRemaining_ <= 0.0) repeatRemaining_ = RepeatDelay;
        emit(count);
        return result;
    }

   private:
    bool needsNeutral_{true}, vertical_{};
    int direction_{};
    double repeatRemaining_{};
};

struct InventoryRayVector { float x{}, y{}, z{}; };
struct InventoryCanvasPoint { float x{}, y{}; };

// Root transforms the actual controller ray into the actual rendered panel's
// local pose first. Dimensions are explicit: no SDK-dependent pose or hidden
// scaling assumption. Front face only, non-unit rays accepted, half-open canvas.
inline std::optional<InventoryCanvasPoint> IntersectPersonaPanelRay(
    const InventoryRayVector& origin, const InventoryRayVector& direction,
    const float physicalWidth, const float physicalHeight,
    const std::uint32_t canvasWidth, const std::uint32_t canvasHeight) {
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z) ||
        !std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z) ||
        !std::isfinite(physicalWidth) || !std::isfinite(physicalHeight) ||
        physicalWidth <= 0.0f || physicalHeight <= 0.0f || canvasWidth == 0u || canvasHeight == 0u) return {};
    const double rayLength = std::hypot(std::hypot(static_cast<double>(direction.x),
        static_cast<double>(direction.y)),static_cast<double>(direction.z));
    if (rayLength == 0.0 || static_cast<double>(direction.z) >= -rayLength*0.000001) return {};
    const double distance = -static_cast<double>(origin.z)/direction.z;
    if (distance < 0.0 || !std::isfinite(distance)) return {};
    const double x = origin.x+distance*direction.x;
    const double y = origin.y+distance*direction.y;
    const double halfWidth = static_cast<double>(physicalWidth)*0.5;
    const double halfHeight = static_cast<double>(physicalHeight)*0.5;
    if (!std::isfinite(x) || !std::isfinite(y) || x < -halfWidth || x >= halfWidth || y <= -halfHeight || y > halfHeight)
        return {};
    const double canvasX = (x+halfWidth)/physicalWidth*canvasWidth;
    const double canvasY = (halfHeight-y)/physicalHeight*canvasHeight;
    // Float conversion at an almost-right/bottom edge must not round a valid
    // point outside the half-open canvas.
    return InventoryCanvasPoint{
        std::min(static_cast<float>(canvasX),std::nextafter(static_cast<float>(canvasWidth),0.0f)),
        std::min(static_cast<float>(canvasY),std::nextafter(static_cast<float>(canvasHeight),0.0f))};
}

} // namespace QuestVr
