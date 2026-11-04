#pragma once
#include "quest_audio_endpoint.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <exception>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace QuestVr {

// Platform output transport, NOT an AudioSubsystem UObject or original sound
// selection/spatial algorithm. One exclusive CPU producer reserves a batch;
// the audio owner consumes committed batches. The real-time callback only
// borrows immutable PCM and advances fixed voice cursors under the app's
// existing audio lock. No UObject, package or decoder reaches this interface.
class AudioPcmBudget final {
public:
    static constexpr std::size_t MaximumBytes=64u<<20u;
    std::size_t RetainedBytes() const noexcept {return retained_.load(std::memory_order_acquire);}
    void Acquire(std::size_t bytes) {
        auto current=retained_.load(std::memory_order_relaxed);
        do {
            if(bytes>MaximumBytes||current>MaximumBytes-bytes)
                throw std::runtime_error("Native audio prepared PCM budget exhausted");
        } while(!retained_.compare_exchange_weak(current,current+bytes,std::memory_order_acq_rel));
    }
    void Release(std::size_t bytes) noexcept {retained_.fetch_sub(bytes,std::memory_order_acq_rel);}
private:
    std::atomic<std::size_t> retained_{};
};

class AudioOutputTransport;
class AudioVoiceMixer;
// Unforgeable per-transport identity, not merely a reusable numeric stream
// generation. It also owns the exact PCM budget for that transport lifetime.
class AudioOutputIdentity final {
    friend class AudioOutputTransport;
    friend class AudioVoiceMixer;
    AudioOutputIdentity(std::uint64_t generation,std::uint32_t rate,std::shared_ptr<AudioPcmBudget> budget)
        :generation_(generation),rate_(rate),budget_(std::move(budget)) {}
    const std::uint64_t generation_;
    const std::uint32_t rate_;
    const std::shared_ptr<AudioPcmBudget> budget_;
};
class PreparedAudioPcm final {
    friend class AudioOutputTransport;
    friend class AudioVoiceMixer;
    PreparedAudioPcm(std::vector<std::int16_t>&& samples,std::shared_ptr<AudioPcmBudget> budget,
        std::uint64_t generation,std::uint32_t rate,std::size_t charge,std::shared_ptr<const AudioOutputIdentity> output)
        :samples_(std::move(samples)),budget_(std::move(budget)),generation_(generation),rate_(rate),charge_(charge),
         output_(std::move(output)) {}
public:
    PreparedAudioPcm(const PreparedAudioPcm&)=delete;
    PreparedAudioPcm& operator=(const PreparedAudioPcm&)=delete;
    ~PreparedAudioPcm() {budget_->Release(charge_);}
    std::span<const std::int16_t> Samples() const noexcept {return samples_;}
    std::size_t Frames() const noexcept {return samples_.size()/2u;}
    std::uint32_t Rate() const noexcept {return rate_;}
private:
    const std::vector<std::int16_t> samples_;
    const std::shared_ptr<AudioPcmBudget> budget_;
    const std::uint64_t generation_;
    const std::uint32_t rate_;
    const std::size_t charge_;
    const std::shared_ptr<const AudioOutputIdentity> output_;
};

inline constexpr std::size_t AudioVoiceCapacity=32u;
inline constexpr std::size_t AudioBatchCommandCapacity=64u;
inline constexpr std::uint64_t AudioVoiceIdentityMaximum=std::numeric_limits<std::uint64_t>::max()>>2u;
inline constexpr std::uint64_t AudioPhaseOne=std::uint64_t{1u}<<32u;
inline constexpr std::size_t AudioMaximumClipFrames=4u<<20u;
enum class AudioVoiceOperation : std::uint8_t {Play,Stop,Update};
struct AudioVoiceCommand {
    AudioVoiceOperation operation{AudioVoiceOperation::Stop};
    std::uint32_t slot{};
    std::uint64_t voice{},phaseStep{AudioPhaseOne};
    std::shared_ptr<const PreparedAudioPcm> pcm;
    float leftGain{1.0f},rightGain{1.0f};
    bool loop{};
};
struct AudioVoiceBatch {
    std::uint64_t generation{};
    std::size_t count{};
    std::array<AudioVoiceCommand,AudioBatchCommandCapacity> commands;
    std::shared_ptr<const AudioOutputIdentity> output;
};
enum class AudioVoicePlayback : std::uint8_t {Empty,Pending,Playing,Finished};
struct AudioVoiceFeedback {std::uint64_t voice{};AudioVoicePlayback playback{AudioVoicePlayback::Empty};};

class AudioVoiceMixer final {
public:
    // Owner-side retirement storage. Destroy this AFTER releasing the existing
    // callback lock; replaced PCM can be large. Callback completion never drops
    // a shared_ptr, frees PCM or changes ownership of any decoder/resource.
    struct Retirement {
        std::array<std::shared_ptr<const PreparedAudioPcm>,AudioBatchCommandCapacity> pcm;
        std::size_t count{};
        bool Empty() const noexcept {
            return !count&&std::none_of(pcm.begin(),pcm.end(),[](const auto& value){return bool(value);});
        }
    };
    static bool ValidCommand(const AudioVoiceCommand& command) noexcept {
        if(command.slot>=AudioVoiceCapacity||command.voice>AudioVoiceIdentityMaximum)return false;
        if(command.operation==AudioVoiceOperation::Stop)return !command.pcm;
        if(command.operation!=AudioVoiceOperation::Play&&command.operation!=AudioVoiceOperation::Update)return false;
        if(!command.voice||!command.phaseStep||command.phaseStep>(AudioPhaseOne<<16u)||
            !std::isfinite(command.leftGain)||!std::isfinite(command.rightGain)||
            command.leftGain<0.0f||command.rightGain<0.0f)return false;
        return command.operation==AudioVoiceOperation::Play?
            command.pcm&&command.pcm->Frames()!=0u&&command.pcm->Frames()<=AudioMaximumClipFrames:!command.pcm;
    }
    // Caller owns the same lock used by MixAdd. Admission precedes every write.
    bool Apply(const AudioVoiceBatch& batch,Retirement& retired) noexcept {
        if(!output_||batch.output!=output_||!batch.generation||batch.generation!=output_->generation_||
            !batch.count||batch.count>AudioBatchCommandCapacity||!retired.Empty())
            return false;
        for(std::size_t index=0u;index<batch.count;++index) {
            const auto& command=batch.commands[index];
            if(!ValidCommand(command))return false;
            // Direct owner Apply is checked just as strictly as transport
            // Reserve: a numeric generation is never authority for foreign PCM.
            if(command.pcm&&(command.pcm->output_!=output_||command.pcm->budget_!=output_->budget_||
                command.pcm->generation_!=output_->generation_||command.pcm->Rate()!=output_->rate_||
                command.pcm->Samples().size()%2u))return false;
        }
        for(std::size_t index=0u;index<batch.count;++index) {
            const auto& command=batch.commands[index];auto& voice=voices_[command.slot];
            if(command.operation==AudioVoiceOperation::Update) {
                if(voice.identity==command.voice&&voice.active) {
                    voice.left=command.leftGain;voice.right=command.rightGain;voice.step=command.phaseStep;
                }
                continue;
            }
            if(command.operation==AudioVoiceOperation::Stop&&command.voice&&voice.identity!=command.voice)continue;
            // At most one retirement per admitted command, including repeated
            // replacements of the same slot. Fixed storage cannot overflow.
            if(voice.pcm)retired.pcm[retired.count++].swap(voice.pcm);
            voice.active=false;voice.cursor=0u;voice.identity=0u;
            activeMask_&=~(std::uint32_t{1u}<<command.slot);
            PublishFeedback(command.slot,0u,AudioVoicePlayback::Empty);
            if(command.operation==AudioVoiceOperation::Play) {
                voice.pcm=command.pcm;voice.identity=command.voice;voice.step=command.phaseStep;
                voice.left=command.leftGain;voice.right=command.rightGain;voice.loop=command.loop;voice.active=true;
                voice.reportedPlaying=false;activeMask_|=std::uint32_t{1u}<<command.slot;
                PublishFeedback(command.slot,voice.identity,AudioVoicePlayback::Pending);
            }
        }
        return true;
    }
    void ResetOnOwner(std::shared_ptr<const AudioOutputIdentity> output,Retirement& retired) noexcept {
        if(!retired.Empty())std::terminate();
        for(std::size_t index=0u;index<voices_.size();++index) {
            auto& voice=voices_[index];if(voice.pcm)retired.pcm[retired.count++].swap(voice.pcm);
            voice.active=false;voice.identity=voice.cursor=0u;PublishFeedback(index,0u,AudioVoicePlayback::Empty);
        }
        output_=std::move(output);activeMask_=0u;
    }
    AudioVoiceFeedback Feedback(std::size_t slot) const noexcept {
        if(slot>=feedback_.size())return {};
        const auto word=feedback_[slot].load(std::memory_order_acquire);
        return {word>>2u,static_cast<AudioVoicePlayback>(word&3u)};
    }
    // Stereo PCM backend profile: unsigned32.32 source-frame step and linear
    // interpolation; gains are PREPARED by the native spatial provider, not an
    // approximation of its source math here. All PCM/loop/resource choices are
    // explicit caller data. MixAdd never allocates, decodes, locks or releases.
    void MixAdd(std::int16_t* output,std::size_t frames,std::uint64_t generation) noexcept {
        if(!output||!output_||generation!=output_->generation_||!activeMask_)return;
        for(std::size_t frame=0u;frame<frames;++frame) {
            if(!activeMask_)return;
            double left=output[frame*2u],right=output[frame*2u+1u];
            auto remaining=activeMask_;
            while(remaining) {
                const auto slot=static_cast<std::size_t>(std::countr_zero(remaining));
                remaining&=remaining-1u;auto& voice=voices_[slot];
                const auto samples=voice.pcm->Samples();const auto length=voice.pcm->Frames();
                const auto at=static_cast<std::size_t>(voice.cursor>>32u);
                const auto next=at+1u<length?at+1u:voice.loop?0u:at;
                const double blend=static_cast<double>(voice.cursor&0xffffffffu)/4294967296.0;
                left+=(samples[at*2u]+(samples[next*2u]-samples[at*2u])*blend)*voice.left;
                right+=(samples[at*2u+1u]+(samples[next*2u+1u]-samples[at*2u+1u])*blend)*voice.right;
                if(!voice.reportedPlaying) {
                    PublishFeedback(slot,voice.identity,AudioVoicePlayback::Playing);voice.reportedPlaying=true;
                }
                voice.cursor+=voice.step;
                const auto end=static_cast<std::uint64_t>(length)<<32u;
                if(voice.cursor>=end) {
                    if(voice.loop)voice.cursor%=end;
                    else {
                        voice.active=false;activeMask_&=~(std::uint32_t{1u}<<slot);
                        PublishFeedback(slot,voice.identity,AudioVoicePlayback::Finished);
                    }
                }
            }
            output[frame*2u]=static_cast<std::int16_t>(std::clamp(left,-32768.0,32767.0));
            output[frame*2u+1u]=static_cast<std::int16_t>(std::clamp(right,-32768.0,32767.0));
        }
    }
private:
    struct Voice {
        std::shared_ptr<const PreparedAudioPcm> pcm;
        std::uint64_t identity{},cursor{},step{AudioPhaseOne};
        float left{},right{};bool active{},loop{},reportedPlaying{};
    };
    void PublishFeedback(std::size_t slot,std::uint64_t voice,AudioVoicePlayback playback) noexcept {
        feedback_[slot].store((voice<<2u)|static_cast<std::uint64_t>(playback),std::memory_order_release);
    }
    std::array<Voice,AudioVoiceCapacity> voices_;
    std::array<std::atomic<std::uint64_t>,AudioVoiceCapacity> feedback_{};
    std::shared_ptr<const AudioOutputIdentity> output_;
    std::uint32_t activeMask_{};
};

class AudioOutputTransport final : public std::enable_shared_from_this<AudioOutputTransport> {
public:
    static constexpr std::size_t BatchCapacity=16u;
    static constexpr std::size_t MaximumClipFrames=AudioMaximumClipFrames;
    // Reservations belong exclusively to the CPU producer. Never transfer a
    // ticket into callback userdata or destroy/cancel/publish one in the real-
    // time callback: its final owner may release this transport and queued PCM.
    // The application owner retains the transport until producers are joined,
    // all tickets are gone and the real stream close barrier has completed.
    class Reservation final {
        friend class AudioOutputTransport;
        Reservation(std::shared_ptr<AudioOutputTransport> owner,std::uint64_t sequence)
            :owner_(std::move(owner)),sequence_(sequence) {}
    public:
        Reservation()=default;
        Reservation(const Reservation&)=delete;Reservation& operator=(const Reservation&)=delete;
        Reservation(Reservation&& other) noexcept:owner_(std::move(other.owner_)),sequence_(other.sequence_) {}
        Reservation& operator=(Reservation&& other) noexcept {
            if(this!=&other){Cancel();owner_=std::move(other.owner_);sequence_=other.sequence_;}return *this;
        }
        ~Reservation() {Cancel();}
        void RequirePublishable() const {
            if(!owner_)throw std::runtime_error("Native audio publication reservation is absent");
            owner_->RequireReady();
            if(owner_->write_.load(std::memory_order_relaxed)!=sequence_)
                throw std::runtime_error("Native audio publication reservation sequence changed");
        }
        // Only after all fallible world/config preparation. Already-owned slot,
        // fixed data and release publication: no allocation, decoder or throw.
        void PublishPrepared() noexcept {
            if(!owner_)std::terminate();
            owner_->write_.store(sequence_+1u,std::memory_order_release);
            owner_->producer_.clear(std::memory_order_release);owner_.reset();
        }
        void Cancel() noexcept {
            if(!owner_)return;
            owner_->batches_[sequence_%BatchCapacity]=AudioVoiceBatch{};
            owner_->producer_.clear(std::memory_order_release);owner_.reset();
        }
    private:
        std::shared_ptr<AudioOutputTransport> owner_;
        std::uint64_t sequence_{};
    };
    AudioOutputTransport(std::shared_ptr<AudioEndpointControl> endpoint,
        std::shared_ptr<NativeClientLifetime> client,std::uint64_t clientIdentity,std::uint64_t viewportIdentity)
        :endpoint_(std::move(endpoint)),client_(std::move(client)),clientIdentity_(clientIdentity),viewportIdentity_(viewportIdentity),
         pcmBudget_(std::make_shared<AudioPcmBudget>()) {
        if(!endpoint_||!client_||!client_->Alive()||!endpoint_->MatchesOwner(client_,clientIdentity_,viewportIdentity_))
            throw std::runtime_error("Native audio transport owner does not match the actual endpoint");
        identity_=std::shared_ptr<const AudioOutputIdentity>(
            new AudioOutputIdentity(Generation(),endpoint_->Format().sampleRate,pcmBudget_));
    }
    bool Ready() const noexcept {return alive_.load(std::memory_order_acquire)&&endpoint_->Ready();}
    void RequireReady() const {
        if(!alive_.load(std::memory_order_acquire))throw std::runtime_error("Native audio transport is retired");
        static_cast<void>(endpoint_->RequireReady(client_,clientIdentity_,viewportIdentity_));
    }
    void Revoke() noexcept {alive_.store(false,std::memory_order_release);}
    std::uint64_t Generation() const noexcept {return endpoint_->Generation();}
    std::uint64_t ClientIdentity() const noexcept {return clientIdentity_;}
    std::uint64_t ViewportIdentity() const noexcept {return viewportIdentity_;}
    const auto& ClientLifetime() const noexcept {return client_;}
    const auto& Endpoint() const noexcept {return endpoint_;}
    const auto& Identity() const noexcept {return identity_;}
    std::size_t PendingBatches() const noexcept {
        // Diagnostic only; neither count nor this snapshot authorizes publish.
        // Read consumption first for a conservative snapshot. Concurrent
        // owner/producers can advance through more than one queue lap between
        // loads, so clamp the transient upper estimate to fixed capacity.
        const auto consumed=read_.load(std::memory_order_acquire);
        const auto published=write_.load(std::memory_order_acquire);
        return published>=consumed?static_cast<std::size_t>(std::min<std::uint64_t>(published-consumed,BatchCapacity)):
            BatchCapacity;
    }
    std::size_t RetainedPcmBytes() const noexcept {return pcmBudget_->RetainedBytes();}
    std::shared_ptr<const PreparedAudioPcm> PreparePcm(std::vector<std::int16_t> samples,std::uint32_t rate) {
        RequireReady();
        if(samples.empty()||samples.size()%2u||samples.size()/2u>MaximumClipFrames||rate!=endpoint_->Format().sampleRate||
            samples.capacity()>(AudioPcmBudget::MaximumBytes-sizeof(PreparedAudioPcm)-8u*sizeof(void*))/sizeof(std::int16_t))
            throw std::runtime_error("Native audio PCM is empty, malformed, excessive or has the wrong output format");
        const auto charge=samples.capacity()*sizeof(std::int16_t)+sizeof(PreparedAudioPcm)+8u*sizeof(void*);
        pcmBudget_->Acquire(charge);
        // Allocate object separately before constructing its shared ownership:
        // allocation failure must release the charge; shared_ptr failure deletes
        // the already-constructed object, whose destructor releases it once.
        PreparedAudioPcm* pcm{};
        try {pcm=new PreparedAudioPcm(std::move(samples),pcmBudget_,Generation(),rate,charge,identity_);}
        catch(...){pcmBudget_->Release(charge);throw;}
        return std::shared_ptr<const PreparedAudioPcm>(pcm);
    }
    Reservation Reserve(AudioVoiceBatch batch) {
        RequireReady();
        if(batch.generation!=Generation()||!batch.count||batch.count>AudioBatchCommandCapacity||
            (batch.output&&batch.output!=identity_))
            throw std::runtime_error("Native audio batch has invalid generation/count");
        for(std::size_t index=0u;index<batch.count;++index) {
            const auto& command=batch.commands[index];
            if(!AudioVoiceMixer::ValidCommand(command))throw std::runtime_error("Native audio batch command is invalid");
            if(command.pcm&&(command.pcm->output_!=identity_||command.pcm->generation_!=Generation()||command.pcm->budget_!=pcmBudget_||
                command.pcm->Rate()!=endpoint_->Format().sampleRate))
                throw std::runtime_error("Native audio batch references another endpoint's prepared PCM");
        }
        batch.output=identity_;
        // Unused fixed slots must not retain hidden resources from another
        // transport (or extend a clip lifetime not represented by count).
        for(std::size_t index=batch.count;index<AudioBatchCommandCapacity;++index)
            batch.commands[index]=AudioVoiceCommand{};
        auto owner=shared_from_this();
        if(producer_.test_and_set(std::memory_order_acq_rel))
            throw std::runtime_error("Native audio transport already has an exclusive producer reservation");
        const auto sequence=write_.load(std::memory_order_relaxed);
        if(sequence==std::numeric_limits<std::uint64_t>::max()||
            sequence-read_.load(std::memory_order_acquire)>=BatchCapacity) {
            producer_.clear(std::memory_order_release);throw std::runtime_error("Native audio commit queue capacity exhausted");
        }
        batches_[sequence%BatchCapacity]=std::move(batch);
        return Reservation(std::move(owner),sequence);
    }
    // Audio OWNER only, never the callback. Move/destroy batch resources away
    // from the callback lock. The owner still checks Ready before mixer Apply.
    bool PopOnOwner(AudioVoiceBatch& output) noexcept {
        const auto sequence=read_.load(std::memory_order_relaxed);
        if(sequence==write_.load(std::memory_order_acquire))return false;
        output=std::move(batches_[sequence%BatchCapacity]);
        batches_[sequence%BatchCapacity].count=0u;
        read_.store(sequence+1u,std::memory_order_release);return true;
    }
private:
    const std::shared_ptr<AudioEndpointControl> endpoint_;
    const std::shared_ptr<NativeClientLifetime> client_;
    const std::uint64_t clientIdentity_,viewportIdentity_;
    const std::shared_ptr<AudioPcmBudget> pcmBudget_;
    std::shared_ptr<const AudioOutputIdentity> identity_;
    std::array<AudioVoiceBatch,BatchCapacity> batches_;
    alignas(64) std::atomic<std::uint64_t> write_{};
    alignas(64) std::atomic<std::uint64_t> read_{};
    std::atomic_flag producer_=ATOMIC_FLAG_INIT;
    std::atomic<bool> alive_{true};
};
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"Native audio feedback requires lock-free64-bit atomics");
} // namespace QuestVr
