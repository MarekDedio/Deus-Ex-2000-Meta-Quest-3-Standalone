#include "quest_audio_transport.h"
#include <array>
#include <chrono>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace QuestVr;
std::size_t checks{},refusals{};

void Require(bool condition,const char* message) {
    ++checks;
    if(!condition)throw std::runtime_error(message);
}
template<class F>void Refused(F&& operation) {
    bool refused{};
    try {operation();}catch(const std::exception&){refused=true;}
    Require(refused,"Invalid audio transport operation accepted");
    ++refusals;
}
void Samples(const std::vector<std::int16_t>& actual,std::initializer_list<std::int16_t> expected,
    const char* message) {
    Require(actual==std::vector<std::int16_t>(expected),message);
}

// Synthetic host protocol evidence ONLY. ConfirmStartedOnOwner(true) here is
// not an AAudio stream, a headset callback or proof of playable game startup.
struct Fixture {
    std::shared_ptr<NativeClientLifetime> client=std::make_shared<NativeClientLifetime>();
    std::shared_ptr<AudioEndpointControl> endpoint;
    std::shared_ptr<AudioOutputTransport> transport;
    AudioVoiceMixer mixer;
    explicit Fixture(std::uint64_t generation=7u,bool start=true)
        :endpoint(std::make_shared<AudioEndpointControl>(generation,client,31u,47u,AudioOutputFormat{48'000u,2u})),
         transport(std::make_shared<AudioOutputTransport>(endpoint,client,31u,47u)) {
        if(start)Require(endpoint->ConfirmStartedOnOwner(true),"Synthetic endpoint start protocol refused");
        AudioVoiceMixer::Retirement retired;
        Require(mixer.ResetOnOwner(transport->Identity(),retired),"Fresh output refused exclusive mixer attachment");
        Require(retired.count==0u,"Fresh mixer unexpectedly retired PCM");
    }
    auto Pcm(std::initializer_list<std::int16_t> samples) {
        return transport->PreparePcm(std::vector<std::int16_t>(samples),48'000u);
    }
    AudioVoiceBatch Batch(std::initializer_list<AudioVoiceCommand> commands) const {
        AudioVoiceBatch result;
        result.generation=transport->Generation();result.output=transport->Identity();result.count=commands.size();
        std::copy(commands.begin(),commands.end(),result.commands.begin());
        return result;
    }
    void Apply(const AudioVoiceBatch& batch) {
        AudioVoiceMixer::Retirement retired;
        Require(mixer.Apply(batch,retired),"Valid owner audio batch refused");
        // A real owner releases its callback lock before this local retirement
        // is destroyed. No callback is active in this single-threaded helper.
    }
    void Commit(AudioVoiceBatch batch) const {
        auto reservation=transport->Reserve(std::move(batch));
        reservation.RequirePublishable();reservation.PublishPrepared();
    }
    void Drain() {
        AudioVoiceBatch batch;
        while(transport->PopOnOwner(batch)) {
            if(transport->Ready())Apply(batch);
            batch=AudioVoiceBatch{};
        }
    }
    std::vector<std::int16_t> Mix(std::size_t frames,std::int16_t seed=0) {
        std::vector<std::int16_t> result(frames*2u,seed);
        mixer.MixAdd(result.data(),frames,transport->Generation());return result;
    }
};
AudioVoiceCommand Play(std::shared_ptr<const PreparedAudioPcm> pcm,std::uint64_t voice=1u,
    std::uint32_t slot=0u,bool loop=false,float left=1.0f,float right=1.0f,
    std::uint64_t step=AudioPhaseOne) {
    AudioVoiceCommand result;
    result.operation=AudioVoiceOperation::Play;result.slot=slot;result.voice=voice;
    result.pcm=std::move(pcm);result.loop=loop;result.leftGain=left;result.rightGain=right;result.phaseStep=step;
    return result;
}
AudioVoiceCommand Stop(std::uint64_t voice=0u,std::uint32_t slot=0u) {
    AudioVoiceCommand result;result.operation=AudioVoiceOperation::Stop;result.voice=voice;result.slot=slot;return result;
}
AudioVoiceCommand Update(std::uint64_t voice,float left,float right,std::uint64_t step=AudioPhaseOne) {
    auto result=Stop(voice);result.operation=AudioVoiceOperation::Update;
    result.leftGain=left;result.rightGain=right;result.phaseStep=step;return result;
}
void Feedback(const AudioVoiceMixer& mixer,std::size_t slot,std::uint64_t voice,AudioVoicePlayback state) {
    const auto actual=mixer.Feedback(slot);
    Require(actual.voice==voice&&actual.playback==state,"Voice identity/playback feedback changed");
}

void ReservationPublication() {
    Fixture f;
    auto pcm=f.Pcm({1000,-2000,3000,-4000});
    auto batch=f.Batch({Play(pcm)});
    {
        auto reservation=f.transport->Reserve(batch);
        reservation.RequirePublishable();
        Require(f.transport->PendingBatches()==0u,"Uncommitted reservation was published");
        AudioVoiceBatch popped;
        Require(!f.transport->PopOnOwner(popped),"Consumer observed uncommitted PCM");
        Samples(f.Mix(2u),{0,0,0,0},"Reservation produced audible PCM before commit");
        Feedback(f.mixer,0u,0u,AudioVoicePlayback::Empty);
        Refused([&]{(void)f.transport->Reserve(batch);});
    }
    Require(f.transport->PendingBatches()==0u,"Rollback advanced queue publication");
    Samples(f.Mix(1u),{0,0},"Rolled-back audio reached the mixer");
    {
        auto reservation=f.transport->Reserve(batch);
        auto moved=std::move(reservation);
        Refused([&]{reservation.RequirePublishable();});
        moved.RequirePublishable();moved.Cancel();moved.Cancel();
        Refused([&]{moved.RequirePublishable();});
    }
    f.Commit(batch);
    Require(f.transport->PendingBatches()==1u,"Committed batch was not published exactly once");
    Samples(f.Mix(1u),{0,0},"Commit bypassed owner-side mixer admission");
    f.Drain();
    Feedback(f.mixer,0u,1u,AudioVoicePlayback::Pending);
    f.mixer.MixAdd(nullptr,1u,f.transport->Generation());
    (void)f.Mix(0u);
    std::array<std::int16_t,2u> wrongGeneration{19,-23};
    f.mixer.MixAdd(wrongGeneration.data(),1u,f.transport->Generation()+1u);
    Require(wrongGeneration==std::array<std::int16_t,2u>{19,-23},"Wrong generation mutated output");
    Feedback(f.mixer,0u,1u,AudioVoicePlayback::Pending);
    Samples(f.Mix(1u),{1000,-2000},"Committed PCM did not produce the first real stereo sample");
    Feedback(f.mixer,0u,1u,AudioVoicePlayback::Playing);
    Samples(f.Mix(2u),{3000,-4000,0,0},"One-shot mixed after its final source frame");
    Feedback(f.mixer,0u,1u,AudioVoicePlayback::Finished);

    Fixture other;
    auto first=f.transport->Reserve(f.Batch({Stop()}));
    auto second=other.transport->Reserve(other.Batch({Stop()}));
    first=std::move(second); // Cancels the first transport's occupied slot.
    auto reused=f.transport->Reserve(f.Batch({Stop()}));reused.Cancel();
    first.RequirePublishable();first.Cancel();
    auto otherReused=other.transport->Reserve(other.Batch({Stop()}));otherReused.Cancel();
    AudioOutputTransport::Reservation absent;
    Refused([&]{absent.RequirePublishable();});

    std::weak_ptr<AudioOutputTransport> lifetime;
    AudioOutputTransport::Reservation survivor;
    {
        auto transport=std::make_shared<AudioOutputTransport>(other.endpoint,other.client,31u,47u);
        lifetime=transport;auto owned=other.Batch({Stop()});owned.output=transport->Identity();
        survivor=transport->Reserve(std::move(owned));transport.reset();
        Require(!lifetime.expired(),"Outstanding reservation failed to retain transport owner");
    }
    survivor.RequirePublishable();survivor.Cancel();
    Require(lifetime.expired(),"Canceled reservation leaked its transport owner");
}

void QueueCapacityAndReuse() {
    Fixture f;
    for(std::size_t index=0u;index<AudioOutputTransport::BatchCapacity;++index)
        f.Commit(f.Batch({Stop(index+1u)}));
    Require(f.transport->PendingBatches()==AudioOutputTransport::BatchCapacity,"Queue capacity changed");
    Refused([&]{(void)f.transport->Reserve(f.Batch({Stop()}));});
    AudioVoiceBatch batch;
    Require(f.transport->PopOnOwner(batch)&&batch.commands[0u].voice==1u,"Full queue lost its oldest batch");
    {
        auto canceled=f.transport->Reserve(f.Batch({Stop(999u)}));canceled.Cancel();
    }
    Require(f.transport->PendingBatches()==AudioOutputTransport::BatchCapacity-1u,"Cancellation consumed ring capacity");
    f.Commit(f.Batch({Stop(AudioOutputTransport::BatchCapacity+1u)}));
    for(std::size_t expected=2u;expected<=AudioOutputTransport::BatchCapacity+1u;++expected) {
        Require(f.transport->PopOnOwner(batch),"Ring reuse dropped a committed batch");
        Require(batch.count==1u&&batch.generation==f.transport->Generation()&&batch.output==f.transport->Identity()&&
            batch.commands[0u].voice==expected,"Ring wrap changed deterministic command order/identity");
    }
    Require(!f.transport->PopOnOwner(batch)&&f.transport->PendingBatches()==0u,"Ring drained more than committed");
    for(unsigned cycle=0u;cycle<5u;++cycle) {
        auto canceled=f.transport->Reserve(f.Batch({Stop()}));canceled.Cancel();
        f.Commit(f.Batch({Stop(cycle+50u)}));
        Require(f.transport->PopOnOwner(batch)&&batch.commands[0u].voice==cycle+50u,"Repeated cancel/commit did not reuse slot");
    }
}

void OwnerAndResourceValidation() {
    Fixture pending(7u,false);
    Require(!pending.transport->Ready(),"Pending endpoint authorized PCM transport");
    Refused([&]{pending.transport->RequireReady();});
    Refused([&]{(void)pending.Pcm({1,2});});
    Refused([&]{(void)pending.transport->Reserve(pending.Batch({Stop()}));});
    pending.endpoint->ObserveDataCallback(0);
    Require(!pending.endpoint->ConfirmStartedOnOwner(false)&&!pending.transport->Ready(),"Zero-frame callback authorized transport");
    pending.endpoint->ObserveDataCallback(1);
    Require(pending.endpoint->ConfirmStartedOnOwner(false)&&pending.transport->Ready(),"Positive callback protocol did not ready transport");

    Fixture f;
    Refused([&]{(void)std::make_shared<AudioOutputTransport>(nullptr,f.client,31u,47u);});
    Refused([&]{(void)std::make_shared<AudioOutputTransport>(f.endpoint,nullptr,31u,47u);});
    Refused([&]{(void)std::make_shared<AudioOutputTransport>(f.endpoint,std::make_shared<NativeClientLifetime>(),31u,47u);});
    Refused([&]{(void)std::make_shared<AudioOutputTransport>(f.endpoint,f.client,32u,47u);});
    Refused([&]{(void)std::make_shared<AudioOutputTransport>(f.endpoint,f.client,31u,48u);});
    Require(f.transport->ClientIdentity()==31u&&f.transport->ViewportIdentity()==47u&&
        f.transport->ClientLifetime()==f.client&&f.transport->Endpoint()==f.endpoint,"Transport changed exact endpoint owner tuple");
    Refused([&]{(void)f.transport->PreparePcm({},48'000u);});
    Refused([&]{(void)f.transport->PreparePcm({1},48'000u);});
    Refused([&]{(void)f.transport->PreparePcm({1,2},44'100u);});
    Refused([&]{(void)f.transport->PreparePcm(std::vector<std::int16_t>((AudioOutputTransport::MaximumClipFrames+1u)*2u),48'000u);});
    Require(f.transport->RetainedPcmBytes()==0u,"Invalid PCM preparation leaked budget");
    auto pcm=f.Pcm({1,2});
    Require(pcm->Frames()==1u&&pcm->Rate()==48'000u&&pcm->Samples()[0u]==1&&pcm->Samples()[1u]==2,
        "Prepared immutable stereo PCM changed");
    const auto charged=f.transport->RetainedPcmBytes();
    Require(charged>=4u+sizeof(PreparedAudioPcm),"PCM capacity/object resource charge missing");

    auto equalGeneration=std::make_shared<AudioOutputTransport>(f.endpoint,f.client,31u,47u);
    Require(equalGeneration->Identity()!=f.transport->Identity(),"Separate outputs reused exact PCM identity");
    auto foreignClip=equalGeneration->PreparePcm({3,4},48'000u);
    Refused([&]{(void)f.transport->Reserve(f.Batch({Play(foreignClip)}));});
    Fixture foreignGeneration(f.transport->Generation()+1u);
    auto otherClip=foreignGeneration.Pcm({5,6});
    Refused([&]{(void)f.transport->Reserve(f.Batch({Play(otherClip)}));});
    auto wrong=f.Batch({Stop()});wrong.generation=0u;
    Refused([&]{(void)f.transport->Reserve(wrong);});
    wrong.generation=f.transport->Generation()+1u;
    Refused([&]{(void)f.transport->Reserve(wrong);});
    wrong=f.Batch({Stop()});wrong.output=equalGeneration->Identity();
    Refused([&]{(void)f.transport->Reserve(wrong);});
    wrong=f.Batch({Stop()});wrong.output.reset();
    auto stamped=f.transport->Reserve(std::move(wrong));stamped.RequirePublishable();stamped.PublishPrepared();
    AudioVoiceBatch popped;
    Require(f.transport->PopOnOwner(popped)&&popped.output==f.transport->Identity(),"Reserve failed to stamp exact output capability");
    wrong=f.Batch({Stop()});wrong.commands[AudioBatchCommandCapacity-1u]=Play(foreignClip);
    f.Commit(std::move(wrong));
    Require(f.transport->PopOnOwner(popped)&&!popped.commands[AudioBatchCommandCapacity-1u].pcm,
        "Uncounted batch slots retained hidden foreign PCM resources");

    Fixture revokedEndpoint;
    auto reserved=revokedEndpoint.transport->Reserve(revokedEndpoint.Batch({Stop()}));
    revokedEndpoint.endpoint->Revoke(-731);
    Require(!revokedEndpoint.transport->Ready(),"Revoked endpoint left output ready");
    Refused([&]{reserved.RequirePublishable();});reserved.Cancel();
    Refused([&]{(void)revokedEndpoint.Pcm({1,2});});
    Refused([&]{(void)revokedEndpoint.transport->Reserve(revokedEndpoint.Batch({Stop()}));});
    Fixture revokedClient;
    auto clientTicket=revokedClient.transport->Reserve(revokedClient.Batch({Stop()}));revokedClient.client->Revoke();
    Refused([&]{clientTicket.RequirePublishable();});clientTicket.Cancel();
    Require(!revokedClient.transport->Ready(),"Revoked native client retained transport readiness");
    Refused([&]{(void)revokedClient.Pcm({1,2});});
    Refused([&]{(void)std::make_shared<AudioOutputTransport>(revokedClient.endpoint,revokedClient.client,31u,47u);});
    Fixture revokedTransport;
    auto queued=revokedTransport.Pcm({900,-900});
    revokedTransport.Commit(revokedTransport.Batch({Play(queued)}));revokedTransport.transport->Revoke();
    Require(!revokedTransport.transport->Ready()&&revokedTransport.endpoint->Ready(),"Transport retirement revoked unrelated endpoint or stayed ready");
    Refused([&]{revokedTransport.transport->RequireReady();});
    revokedTransport.Drain(); // Owner discards stale queued PCM rather than applying it.
    Samples(revokedTransport.Mix(1u),{0,0},"Retired queued batch reached mixer");
    Require(revokedTransport.transport->PendingBatches()==0u,"Retired owner could not drain queued resources");
}

void PcmBudget() {
    AudioPcmBudget budget;
    budget.Acquire(AudioPcmBudget::MaximumBytes);
    Refused([&]{budget.Acquire(1u);});
    Refused([&]{budget.Acquire(std::numeric_limits<std::size_t>::max());});
    Require(budget.RetainedBytes()==AudioPcmBudget::MaximumBytes,"Rejected budget request changed charge");
    budget.Release(AudioPcmBudget::MaximumBytes);Require(budget.RetainedBytes()==0u,"Budget release did not retire charge");
    Fixture f;
    std::vector<std::shared_ptr<const PreparedAudioPcm>> clips;
    bool exhausted{};
    for(unsigned index=0u;index<5u&&!exhausted;++index) {
        std::vector<std::int16_t> samples; samples.reserve(AudioOutputTransport::MaximumClipFrames*2u);samples.resize(2u);
        const auto before=f.transport->RetainedPcmBytes();
        try {clips.push_back(f.transport->PreparePcm(std::move(samples),48'000u));}
        catch(const std::runtime_error&) {
            exhausted=true;
            Require(f.transport->RetainedPcmBytes()==before,"Failed aggregate PCM budget preparation leaked charge");
        }
        Require(f.transport->RetainedPcmBytes()<=AudioPcmBudget::MaximumBytes,"PCM aggregate resource limit exceeded");
    }
    Require(exhausted&&!clips.empty(),"Prepared vector capacity was not charged against aggregate PCM budget");
    clips.clear();Require(f.transport->RetainedPcmBytes()==0u,"Final prepared PCM release leaked aggregate budget");
    auto maximum=f.transport->PreparePcm(std::vector<std::int16_t>(AudioOutputTransport::MaximumClipFrames*2u),48'000u);
    Require(maximum->Frames()==AudioOutputTransport::MaximumClipFrames,"Maximum bounded PCM clip refused");
    maximum.reset();Require(f.transport->RetainedPcmBytes()==0u,"Maximum PCM charge did not retire");
}

void PcmMixingAndFeedback() {
    Fixture f;
    const auto pcm=f.Pcm({1000,-2000,3000,-4000});
    f.Apply(f.Batch({Play(pcm,1u,0u,true)}));
    Samples(f.Mix(5u),{1000,-2000,3000,-4000,1000,-2000,3000,-4000,1000,-2000},"Looping PCM changed stereo order/wrap");
    Feedback(f.mixer,0u,1u,AudioVoicePlayback::Playing);
    f.Apply(f.Batch({Play(pcm,2u,0u,false,1.0f,1.0f,AudioPhaseOne/2u)}));
    Samples(f.Mix(5u),{1000,-2000,2000,-3000,3000,-4000,3000,-4000,0,0},"One-shot interpolation/held final sample changed");
    Feedback(f.mixer,0u,2u,AudioVoicePlayback::Finished);
    f.Apply(f.Batch({Play(pcm,3u,0u,true,1.0f,1.0f,AudioPhaseOne/2u)}));
    Samples(f.Mix(6u),{1000,-2000,2000,-3000,3000,-4000,2000,-3000,1000,-2000,2000,-3000},"Loop interpolation failed across last/first frame");
    const auto three=f.Pcm({100,-100,200,-200,300,-300});
    f.Apply(f.Batch({Play(three,4u,0u,false,1.0f,1.0f,AudioPhaseOne*2u)}));
    Samples(f.Mix(3u),{100,-100,300,-300,0,0},"Prepared frame step failed to skip/finish source frames");
    Feedback(f.mixer,0u,4u,AudioVoicePlayback::Finished);
    f.Apply(f.Batch({Play(pcm,5u,0u,true,0.5f,0.25f)}));
    std::vector<std::int16_t> additive{10,-20};
    f.mixer.MixAdd(additive.data(),1u,f.transport->Generation());
    Samples(additive,{510,-520},"Stereo gains or addition to existing ambient output changed");
    const auto loud=f.Pcm({30000,-30000});
    f.Apply(f.Batch({Play(loud,6u,0u,true),Play(loud,7u,1u,true)}));
    Samples(f.Mix(1u),{32767,-32768},"Summed stereo voices did not saturate signed16 output");
    f.Apply(f.Batch({Stop(0u,0u),Stop(0u,1u)}));
    Samples(f.Mix(2u,37),{37,37,37,37},"Empty mixer changed existing output");
    const auto fractional=f.Pcm({3,-3});
    f.Apply(f.Batch({Play(fractional,8u,0u,true,0.5f,0.5f)}));
    Samples(f.Mix(1u),{1,-1},"Fractional sample conversion did not truncate toward zero");
    f.Apply(f.Batch({Play(pcm,AudioVoiceIdentityMaximum,0u,true,0.0f,0.0f)}));
    Feedback(f.mixer,0u,AudioVoiceIdentityMaximum,AudioVoicePlayback::Pending);
    Samples(f.Mix(1u),{0,0},"Zero prepared gains produced PCM");
    Feedback(f.mixer,0u,AudioVoiceIdentityMaximum,AudioVoicePlayback::Playing);
    Feedback(f.mixer,AudioVoiceCapacity,0u,AudioVoicePlayback::Empty);
}

void VoiceControlsAndRetirement() {
    Fixture f;
    auto pcm=f.Pcm({1000,-2000,3000,-4000});
    f.Apply(f.Batch({Play(pcm,100u,0u,true)}));
    f.Apply(f.Batch({Update(99u,0.0f,0.0f),Stop(99u)}));
    Feedback(f.mixer,0u,100u,AudioVoicePlayback::Pending);
    Samples(f.Mix(1u),{1000,-2000},"Stale update/stop changed current voice");
    f.Apply(f.Batch({Update(100u,0.5f,0.25f,AudioPhaseOne/2u)}));
    Samples(f.Mix(2u),{1500,-1000,1000,-750},"Matching update did not preserve cursor/apply prepared gain and step");
    f.Apply(f.Batch({Play(pcm,101u,0u,true)}));
    f.Apply(f.Batch({Update(100u,0.0f,0.0f),Stop(100u)}));
    Samples(f.Mix(1u),{1000,-2000},"Reused voice slot accepted old generation controls");
    f.Apply(f.Batch({Stop(101u)}));
    Feedback(f.mixer,0u,0u,AudioVoicePlayback::Empty);
    Samples(f.Mix(1u),{0,0},"Matching stop left active PCM");
    f.Apply(f.Batch({Play(pcm,102u,0u,true)}));f.Apply(f.Batch({Stop()}));
    Feedback(f.mixer,0u,0u,AudioVoicePlayback::Empty);

    Fixture retired;
    auto only=retired.Pcm({1234,-2345});
    std::weak_ptr<const PreparedAudioPcm> weak=only;
    retired.Apply(retired.Batch({Play(only,1u)}));only.reset();
    const auto charge=retired.transport->RetainedPcmBytes();
    Require(!weak.expired()&&charge!=0u,"Mixer did not retain immutable prepared resource");
    Samples(retired.Mix(1u),{1234,-2345},"One-frame clip did not actually mix");
    Feedback(retired.mixer,0u,1u,AudioVoicePlayback::Finished);
    Require(!weak.expired()&&retired.transport->RetainedPcmBytes()==charge,
        "Callback completion freed PCM instead of deferring owner retirement");
    Samples(retired.Mix(3u),{0,0,0,0,0,0},"Finished retained resource remained active");
    AudioVoiceMixer::Retirement ownerRetirement;
    Require(retired.mixer.Apply(retired.Batch({Stop(1u)}),ownerRetirement)&&ownerRetirement.count==1u,
        "Owner stop did not move finished clip into fixed retirement storage");
    Require(!weak.expired()&&retired.transport->RetainedPcmBytes()==charge,"Mixer Apply freed retired PCM under callback lock");
    ownerRetirement=AudioVoiceMixer::Retirement{}; // Simulate owner unlock, then release.
    Require(weak.expired()&&retired.transport->RetainedPcmBytes()==0u,"Off-callback retirement leaked PCM resource");

    AudioVoiceBatch all=retired.Batch({});all.count=AudioVoiceCapacity;
    auto constant=retired.Pcm({10,-10});
    for(std::size_t slot=0u;slot<AudioVoiceCapacity;++slot)
        all.commands[slot]=Play(constant,slot+1u,static_cast<std::uint32_t>(slot),true);
    retired.Apply(all);
    Samples(retired.Mix(1u),{320,-320},"Fixed mixer failed full voice capacity");
    AudioVoiceMixer::Retirement resetRetirement;
    Require(retired.mixer.ResetOnOwner(nullptr,resetRetirement),"Owner reset refused its exact mixer");
    Require(resetRetirement.count==AudioVoiceCapacity,"Owner reset failed to retire every occupied voice");
    Samples(retired.Mix(1u),{0,0},"Retired mixer generation remained audible");
    for(std::size_t slot=0u;slot<AudioVoiceCapacity;++slot)Feedback(retired.mixer,slot,0u,AudioVoicePlayback::Empty);
    resetRetirement=AudioVoiceMixer::Retirement{};
    Require(retired.mixer.ResetOnOwner(retired.transport->Identity(),resetRetirement),"Detached output could not reattach");
    retired.Apply(retired.Batch({Play(constant,1u,0u,true)}));
    AudioVoiceBatch repeated=retired.Batch({});repeated.count=AudioBatchCommandCapacity;
    for(std::size_t index=0u;index<repeated.count;++index)repeated.commands[index]=Play(constant,index+2u,0u,true);
    AudioVoiceMixer::Retirement maximumRetirement;
    Require(retired.mixer.Apply(repeated,maximumRetirement)&&maximumRetirement.count==AudioBatchCommandCapacity,
        "Repeated same-slot replacements overflowed/lost fixed retirement entries");
    Feedback(retired.mixer,0u,AudioBatchCommandCapacity+1u,AudioVoicePlayback::Pending);
}

void MalformedBatchAtomicity() {
    Fixture f;
    const auto pcm=f.Pcm({1000,-2000});
    f.Apply(f.Batch({Play(pcm,1u,0u,true)}));
    std::vector<AudioVoiceCommand> invalid;
    auto bad=Play(pcm);bad.slot=static_cast<std::uint32_t>(AudioVoiceCapacity);invalid.push_back(bad);
    bad=Play(pcm);bad.voice=0u;invalid.push_back(bad);
    bad=Play(pcm);bad.voice=AudioVoiceIdentityMaximum+1u;invalid.push_back(bad);
    bad=Play(pcm);bad.phaseStep=0u;invalid.push_back(bad);
    bad=Play(pcm);bad.phaseStep=(AudioPhaseOne<<16u)+1u;invalid.push_back(bad);
    bad=Play(pcm);bad.leftGain=-1.0f;invalid.push_back(bad);
    bad=Play(pcm);bad.rightGain=std::numeric_limits<float>::infinity();invalid.push_back(bad);
    bad=Play(pcm);bad.leftGain=std::numeric_limits<float>::quiet_NaN();invalid.push_back(bad);
    bad=Play(pcm);bad.pcm.reset();invalid.push_back(bad);
    bad=Stop();bad.pcm=pcm;invalid.push_back(bad);
    bad=Update(1u,1.0f,1.0f);bad.pcm=pcm;invalid.push_back(bad);
    bad=Stop();bad.operation=static_cast<AudioVoiceOperation>(255u);invalid.push_back(bad);
    for(const auto& command:invalid) {
        Require(!AudioVoiceMixer::ValidCommand(command),"Malformed command passed validation");
        const auto batch=f.Batch({Update(1u,0.0f,0.0f),command});
        AudioVoiceMixer::Retirement retired;
        Require(!f.mixer.Apply(batch,retired)&&retired.count==0u,"Malformed batch partially changed mixer/retirement");
        Refused([&]{(void)f.transport->Reserve(batch);});
        Samples(f.Mix(1u),{1000,-2000},"Late invalid command allowed earlier update mutation");
        Feedback(f.mixer,0u,1u,AudioVoicePlayback::Playing);
    }
    auto invalidBatch=f.Batch({Update(1u,0.0f,0.0f)});
    for(unsigned kind=0u;kind<5u;++kind) {
        auto malformed=invalidBatch;
        if(kind==0u)malformed.count=0u;
        if(kind==1u)malformed.count=AudioBatchCommandCapacity+1u;
        if(kind==2u)malformed.generation=0u;
        if(kind==3u)malformed.generation+=1u;
        if(kind==4u)malformed.output.reset();
        AudioVoiceMixer::Retirement retired;
        Require(!f.mixer.Apply(malformed,retired),"Malformed batch header reached mixer");
        if(kind!=4u)Refused([&]{(void)f.transport->Reserve(malformed);}); // Reserve stamps absent output.
        Samples(f.Mix(1u),{1000,-2000},"Invalid batch header changed existing voice");
    }
    Fixture sameNumbers(f.transport->Generation());
    const auto foreign=sameNumbers.Pcm({9000,-9000});
    for(const auto& operation:std::array<AudioVoiceCommand,3u>{Stop(),Update(1u,0.0f,0.0f),Play(foreign,2u)}) {
        auto other=sameNumbers.Batch({operation});
        AudioVoiceMixer::Retirement retired;
        Require(!f.mixer.Apply(other,retired),"Equal numeric generation bypassed exact output capability");
        Refused([&]{(void)f.transport->Reserve(other);});
        Samples(f.Mix(1u),{1000,-2000},"Foreign output Stop/Update/Play mutated local voice");
    }
    auto forged=f.Batch({Update(1u,0.0f,0.0f),Play(foreign,2u)});
    AudioVoiceMixer::Retirement retired;
    Require(!f.mixer.Apply(forged,retired),"Local batch token authorized foreign prepared PCM");
    Samples(f.Mix(1u),{1000,-2000},"Foreign late PCM caused partial local update");
    retired.count=1u;
    Require(!f.mixer.Apply(invalidBatch,retired),"Occupied retirement count accepted under mixer lock");
    retired.count=0u;retired.pcm[63u]=pcm;
    Require(!f.mixer.Apply(invalidBatch,retired),"Hidden retained resource accepted for callback-lock replacement");
    Samples(f.Mix(1u),{1000,-2000},"Malformed retirement storage changed voice");
}

void OutputFeedbackOwnership() {
    Fixture first,second;
    auto pcm=first.Pcm({100,-200,300,-400});
    first.Apply(first.Batch({Play(pcm,41u,0u,true)}));
    Require(first.transport->MixerAttached(),"Actual owner attachment was not visible through output capability");
    auto observed=first.transport->Feedback(0u);
    Require(observed.voice==41u&&observed.playback==AudioVoicePlayback::Pending,
        "Runtime-visible output feedback claimed PCM before a positive callback mix");
    Samples(first.Mix(1u),{100,-200},"Feedback path changed actual PCM");
    observed=first.transport->Feedback(0u);
    Require(observed.voice==41u&&observed.playback==AudioVoicePlayback::Playing,
        "Actual mixed PCM did not reach the runtime-visible feedback capability");
    observed=first.transport->Feedback(AudioVoiceCapacity);
    Require(!observed.voice&&observed.playback==AudioVoicePlayback::Empty,"Invalid feedback slot exposed another resource");
    const auto other=second.Pcm({75,-125});second.Apply(second.Batch({Play(other,51u,0u,true)}));
    AudioVoiceMixer::Retirement denied;
    Require(!second.mixer.ResetOnOwner(first.transport->Identity(),denied)&&denied.Empty(),
        "A second live mixer stole output feedback ownership");
    Feedback(first.mixer,0u,41u,AudioVoicePlayback::Playing);
    Feedback(second.mixer,0u,51u,AudioVoicePlayback::Pending);
    Samples(second.Mix(1u),{75,-125},"Denied consumer transfer mutated the existing mixer");
    AudioVoiceMixer::Retirement retired;
    Require(first.mixer.ResetOnOwner(nullptr,retired)&&!first.transport->MixerAttached()&&retired.count==1u,
        "Owner detach did not retire its claim and PCM together");
    observed=first.transport->Feedback(0u);
    Require(!observed.voice&&observed.playback==AudioVoicePlayback::Empty,
        "Retained runtime capability kept Playing after its consumer detached");
    retired=AudioVoiceMixer::Retirement{};
    Require(second.mixer.ResetOnOwner(first.transport->Identity(),retired)&&retired.count==1u&&
        first.transport->MixerAttached()&&!second.transport->MixerAttached(),"Exclusive consumer transfer was not atomic on owner");
    retired=AudioVoiceMixer::Retirement{};
    Require(second.mixer.Apply(first.Batch({Play(pcm,61u,0u,true)}),retired),"New exclusive consumer refused exact output PCM");
    std::array<std::int16_t,2u> actual{};second.mixer.MixAdd(actual.data(),1u,first.transport->Generation());
    observed=first.transport->Feedback(0u);
    Require(observed.voice==61u&&observed.playback==AudioVoicePlayback::Playing,
        "Transferred consumer wrote another output's feedback");
    auto equalNumbers=std::make_shared<AudioOutputTransport>(first.endpoint,first.client,31u,47u);
    Require(!equalNumbers->MixerAttached()&&!equalNumbers->Feedback(0u).voice,
        "A different exact transport inherited feedback from equal stream numbers");
    {
        AudioVoiceMixer temporary;AudioVoiceMixer::Retirement retirement;
        Require(temporary.ResetOnOwner(equalNumbers->Identity(),retirement),"Fresh distinct transport refused temporary consumer");
        auto batch=first.Batch({Play(equalNumbers->PreparePcm({19,-23},48'000u),71u,0u,true)});
        batch.output=equalNumbers->Identity();
        Require(temporary.Apply(batch,retirement),"Distinct feedback consumer refused own immutable PCM");
        std::array<std::int16_t,2u> output{};temporary.MixAdd(output.data(),1u,equalNumbers->Generation());
        Require(equalNumbers->Feedback(0u).voice==71u&&first.transport->Feedback(0u).voice==61u,
            "Equal generation aliased output feedback capabilities");
    }
    Require(!equalNumbers->MixerAttached()&&!equalNumbers->Feedback(0u).voice,
        "Destroyed owner left a dangling consumer claim or historical Playing state");
    AudioVoiceMixer replacement;AudioVoiceMixer::Retirement available;
    Require(replacement.ResetOnOwner(equalNumbers->Identity(),available),"Destroyed owner made output impossible to reattach");
}

void SpscPublication() {
    Fixture f;
    const auto pcm=f.Pcm({80,-160,80,-160});
    constexpr std::size_t total=24'000u;
    std::atomic<bool> failed{},producerDone{},consumerDone{},observerStarted{},observerSawPlaying{};
    std::size_t produced{},consumed{},canceled{},observedSamples{},feedbackReads{},playingFeedbackReads{};
    std::exception_ptr producerFailure,consumerFailure,observerFailure;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    auto Check=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
    // A third thread represents the CPU runtime. It must not inspect mixer
    // storage/output_ or borrow the owner's callback mutex.
    std::thread observer([&] {
        observerStarted.store(true,std::memory_order_release);
        std::uint64_t previous{};
        try {
            while(!consumerDone.load(std::memory_order_acquire)&&!failed.load(std::memory_order_acquire)) {
                Check(std::chrono::steady_clock::now()<deadline,"Feedback observer timed out");
                const auto feedback=f.transport->Feedback(0u);++feedbackReads;
                Check(f.transport->MixerAttached(),"Live SPSC consumer lost its exact output claim");
                if(feedback.playback==AudioVoicePlayback::Empty)Check(!feedback.voice,"Torn Empty/voice feedback word");
                else {
                    Check(feedback.voice>=previous&&feedback.voice<=total&&
                        (feedback.playback==AudioVoicePlayback::Pending||feedback.playback==AudioVoicePlayback::Playing),
                        "Feedback observer saw torn/reversed/invalid voice-state word");
                    previous=feedback.voice;
                    if(feedback.playback==AudioVoicePlayback::Playing) {
                        ++playingFeedbackReads;observerSawPlaying.store(true,std::memory_order_release);
                    }
                }
                std::this_thread::yield();
            }
        }catch(...){observerFailure=std::current_exception();failed.store(true,std::memory_order_release);}
    });
    std::thread producer([&] {
        try {
            for(std::size_t sequence=0u;sequence<total&&!failed.load(std::memory_order_acquire);) {
                Check(std::chrono::steady_clock::now()<deadline,"SPSC producer timed out");
                const float left=static_cast<float>(sequence%8u+1u)/8.0f;
                const float right=static_cast<float>(sequence%4u+1u)/4.0f;
                auto batch=f.Batch({Play(pcm,sequence+1u,0u,true,left,right)});
                try {
                    if(sequence%3u==0u) {
                        auto rollback=f.transport->Reserve(batch);rollback.RequirePublishable();rollback.Cancel();++canceled;
                    }
                    auto ticket=f.transport->Reserve(std::move(batch));ticket.RequirePublishable();ticket.PublishPrepared();
                    ++sequence;++produced;
                }catch(const std::runtime_error& error) {
                    if(std::string(error.what()).find("capacity exhausted")==std::string::npos)throw;
                    std::this_thread::yield();
                }
            }
        }catch(...){producerFailure=std::current_exception();failed.store(true,std::memory_order_release);}
        producerDone.store(true,std::memory_order_release);
    });
    std::thread consumer([&] {
        try {
            while(!observerStarted.load(std::memory_order_acquire))std::this_thread::yield();
            AudioVoiceBatch batch;
            for(std::size_t expected=0u;expected<total&&!failed.load(std::memory_order_acquire);) {
                Check(std::chrono::steady_clock::now()<deadline,"SPSC consumer timed out");
                if(!f.transport->PopOnOwner(batch)) {
                    if(!producerDone.load(std::memory_order_acquire)) {
                        std::this_thread::yield();continue;
                    }
                    // Final publish can occur between the empty observation
                    // and producerDone. Acquire done, then observe queue again.
                    Check(f.transport->PopOnOwner(batch),"SPSC producer ended before all sequences arrived");
                }
                Check(batch.count==1u&&batch.commands[0u].voice==expected+1u&&batch.output==f.transport->Identity()&&
                    batch.generation==f.transport->Generation()&&batch.commands[0u].pcm==pcm,
                    "SPSC consumer observed missing/torn/duplicated/reordered immutable batch");
                AudioVoiceMixer::Retirement retirement;
                Check(f.mixer.Apply(batch,retirement),"SPSC admitted batch failed mixer owner validation");
                const auto before=f.transport->Feedback(0u);
                Check(before.voice==expected+1u&&before.playback==AudioVoicePlayback::Pending,"SPSC batch claimed Playing before PCM mix");
                std::array<std::int16_t,2u> output{};
                f.mixer.MixAdd(output.data(),1u,f.transport->Generation());
                Check(output[0u]==static_cast<std::int16_t>((expected%8u+1u)*10u)&&
                    output[1u]==-static_cast<std::int16_t>((expected%4u+1u)*40u),"SPSC gain/PCM publication tore or lost a sequence");
                const auto after=f.transport->Feedback(0u);
                Check(after.voice==expected+1u&&after.playback==AudioVoicePlayback::Playing,"SPSC positive PCM frame failed feedback transition");
                if(expected==0u) {
                    // The stress reader must actually witness published PCM,
                    // not pass vacuously by sampling only Empty before/after.
                    while(!observerSawPlaying.load(std::memory_order_acquire)&&!failed.load(std::memory_order_acquire)) {
                        Check(std::chrono::steady_clock::now()<deadline,"Runtime reader never observed actual mixed Playing feedback");
                        std::this_thread::yield();
                    }
                }
                ++expected;++consumed;++observedSamples;
                batch=AudioVoiceBatch{};
                // Single consumer owns Apply/Mix. Real owner destroys this
                // retirement outside its callback lock, never in MixAdd.
            }
        }catch(...){consumerFailure=std::current_exception();failed.store(true,std::memory_order_release);}
        consumerDone.store(true,std::memory_order_release);
    });
    producer.join();consumer.join();observer.join();
    if(producerFailure)std::rethrow_exception(producerFailure);
    if(consumerFailure)std::rethrow_exception(consumerFailure);
    if(observerFailure)std::rethrow_exception(observerFailure);
    Require(produced==total&&consumed==total&&observedSamples==total&&canceled==total/3u,
        "SPSC final publication/cancel/PCM proof counts changed");
    Require(f.transport->PendingBatches()==0u,"SPSC final queue retained unconsumed publication");
    Require(feedbackReads!=0u&&playingFeedbackReads!=0u&&observerSawPlaying.load(std::memory_order_acquire),
        "Concurrent runtime never observed actual positive mixed feedback");
    Feedback(f.mixer,0u,total,AudioVoicePlayback::Playing);
}
} // namespace

int main() {
    try {
        ReservationPublication();QueueCapacityAndReuse();OwnerAndResourceValidation();PcmBudget();
        PcmMixingAndFeedback();VoiceControlsAndRetirement();MalformedBatchAtomicity();OutputFeedbackOwnership();SpscPublication();
        std::cout<<"PASS audio transport/real stereo PCM math: "<<checks<<" checks/"<<refusals
            <<" refusals; 24000 deterministic SPSC publications and PCM frames. Host protocol ONLY; "
              "not AAudio hardware readiness, original AudioSubsystem parity or playable startup.\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"FAIL "<<error.what()<<'\n';return 1;
    }
}
