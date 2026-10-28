#pragma once
#include "quest_native_client.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

namespace QuestVr {
// Port-owned raw observation protocol, NOT WinDrv packets or Unreal key units.
// Producer: OpenXR/render owner. Consumer: exclusive runtime CPU owner. There
// are no XR/GL handles, UObject pointers or callbacks crossing this boundary.
struct QuestInputPose {std::array<float,3u> position{};std::array<float,4u> orientation{0,0,0,1};};
struct QuestInputHand {
    bool tracked{};float stickX{},stickY{},trigger{},grip{};QuestInputPose gripPose,aimPose;
};
struct QuestInputObservation {
    double displayTime{};float deltaSeconds{};
    bool focused{},headValid{},headTracked{},mounted{};
    // A,B,X,Y,Menu,left/right stick-click, in that order. Triggers stay per hand.
    std::uint8_t buttons{};QuestInputPose headPose;std::array<QuestInputHand,2u> hands{}; // left,right
};
struct QuestInputCursor {std::uint64_t source{},sequence{},rejections{};};
enum class QuestInputReadStatus {Empty,Ready,NeedsReset};
class QuestInputSource final {
public:
    static constexpr std::size_t Capacity=64u;
    struct Row {std::uint64_t sequence{};QuestInputObservation observation;};
    struct Batch {
        QuestInputReadStatus status{QuestInputReadStatus::Empty};bool reset{};
        QuestInputCursor before,after;std::size_t count{};std::array<Row,Capacity> rows{};
    };
    struct Snapshot {std::uint64_t source{},client{},viewport{},latest{},overwritten{},rejections{};std::size_t count{};bool alive{};};
    QuestInputSource(std::shared_ptr<NativeClientLifetime> lifetime,std::uint64_t client,std::uint64_t viewport)
        :lifetime_(std::move(lifetime)),client_(client),viewport_(viewport) {
        if(!lifetime_||!lifetime_->Alive()||!client_||!viewport_)Fail("missing current client/viewport lifetime");
        // Separate source identity prevents a cursor from another provider,
        // including one using the same viewport metadata, from being reused.
        auto identity=nextIdentity_.load();
        for(;;) {
            if(identity==std::numeric_limits<std::uint64_t>::max())Fail("source identity exhausted");
            if(nextIdentity_.compare_exchange_weak(identity,identity+1u)){identity_=identity+1u;break;}
        }
    }
    bool Matches(const std::shared_ptr<NativeClientLifetime>& lifetime,std::uint64_t client,std::uint64_t viewport) const noexcept {
        return lifetime_==lifetime&&client_==client&&viewport_==viewport&&lifetime_->Alive();
    }
    bool Matches(std::uint64_t client,std::uint64_t viewport) const noexcept {
        return client_==client&&viewport_==viewport&&lifetime_->Alive();
    }
    std::shared_ptr<NativeClientLifetime> Lifetime() const noexcept {return lifetime_;}
    void Publish(const QuestInputObservation& observation) {
        std::lock_guard lock(mutex_);RequireAlive();
        if(latest_==std::numeric_limits<std::uint64_t>::max())Fail("sample sequence exhausted");
        try {
            Validate(observation);
            if(latest_&&observation.displayTime<rows_[(latest_-1u)%Capacity].observation.displayTime)
                Fail("sample time moved backwards");
        }catch(...) {
            if(rejections_!=std::numeric_limits<std::uint64_t>::max())++rejections_;
            lastRejectedSequence_=latest_;throw;
        }
        const auto next=latest_+1u;rows_[(next-1u)%Capacity]={next,observation};latest_=next;
    }
    Snapshot Read() const {
        std::lock_guard lock(mutex_);RequireAlive();
        return {identity_,client_,viewport_,latest_,latest_>Capacity?latest_-Capacity:0u,rejections_,
            static_cast<std::size_t>(std::min<std::uint64_t>(latest_,Capacity)),true};
    }
    // Non-destructive snapshot: the caller stages `after` with its own world
    // transaction and publishes it only on commit. Rollback simply keeps before.
    // Reset explicitly accepts only the newest physical state as a baseline;
    // it is NOT a fabricated replay of input transitions lost to overflow.
    Batch Stage(const QuestInputCursor& cursor,bool reset=false) const {
        std::lock_guard lock(mutex_);RequireAlive();Batch batch;batch.before=cursor;batch.after=cursor;batch.reset=reset;
        if(cursor.source&&cursor.source!=identity_)Fail("cursor belongs to a different source");
        if(cursor.sequence>latest_||(!cursor.source&&cursor.sequence))Fail("cursor is ahead of its source");
        if(!latest_)return batch;
        if(rejections_&&latest_==lastRejectedSequence_) {batch.status=QuestInputReadStatus::NeedsReset;return batch;}
        if(reset) {
            batch.status=QuestInputReadStatus::Ready;batch.rows[0u]=rows_[(latest_-1u)%Capacity];batch.count=1u;
            batch.after={identity_,latest_,rejections_};return batch;
        }
        const auto first=latest_>Capacity?latest_-Capacity+1u:1u;
        if(!cursor.source||cursor.sequence<first-1u||cursor.rejections!=rejections_||rejections_==std::numeric_limits<std::uint64_t>::max()) {
            batch.status=QuestInputReadStatus::NeedsReset;return batch;
        }
        for(auto sequence=cursor.sequence;sequence<latest_;) {
            ++sequence;batch.rows[batch.count++]=rows_[(sequence-1u)%Capacity];
        }
        batch.status=batch.count?QuestInputReadStatus::Ready:QuestInputReadStatus::Empty;
        batch.after={identity_,latest_,rejections_};return batch;
    }
    static constexpr std::size_t RetainedBytes() noexcept {return sizeof(QuestInputSource)+8u*sizeof(void*);}
private:
    [[noreturn]] static void Fail(const char* reason){throw std::runtime_error(std::string("Quest input source: ")+reason);}
    void RequireAlive() const {if(!lifetime_->Alive())Fail("session revoked");}
    static void ValidatePose(const QuestInputPose& pose) {
        for(auto value:pose.position)if(!std::isfinite(value))Fail("nonfinite pose position");
        double norm{};for(auto value:pose.orientation){if(!std::isfinite(value))Fail("nonfinite orientation");norm+=double(value)*value;}
        if(norm<0.99||norm>1.01)Fail("orientation is not normalized");
    }
    static void Validate(const QuestInputObservation& observation) {
        if(!std::isfinite(observation.displayTime)||observation.displayTime<0||!std::isfinite(observation.deltaSeconds)||
            observation.deltaSeconds<0||observation.buttons>127u||(observation.headTracked&&!observation.headValid))Fail("invalid frame metadata");
        if(observation.headValid)ValidatePose(observation.headPose);
        for(const auto& hand:observation.hands) {
            // Inactive controls can legitimately retain SDK values. They are
            // observations only: focus/tracking gates belong to the consumer.
            if(!std::isfinite(hand.stickX)||!std::isfinite(hand.stickY)||std::fabs(hand.stickX)>1.001f||std::fabs(hand.stickY)>1.001f||
                !std::isfinite(hand.trigger)||hand.trigger<0||hand.trigger>1.001f||!std::isfinite(hand.grip)||hand.grip<0||hand.grip>1.001f)
                Fail("invalid controller axes");
            if(hand.tracked){ValidatePose(hand.gripPose);ValidatePose(hand.aimPose);}
        }
    }
    inline static std::atomic<std::uint64_t> nextIdentity_{};
    std::shared_ptr<NativeClientLifetime> lifetime_;
    const std::uint64_t client_,viewport_;std::uint64_t identity_{};
    mutable std::mutex mutex_;std::uint64_t latest_{},rejections_{},lastRejectedSequence_{};std::array<Row,Capacity> rows_{};
};
} // namespace QuestVr
