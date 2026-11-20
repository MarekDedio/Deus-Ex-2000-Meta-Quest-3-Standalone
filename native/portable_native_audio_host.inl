// Included in PortableActorVmHost: same VM root, no nested public transaction.
    struct NativeAudioJournal {
        GCRoot<RuntimeNativeAudioDomain> domain;
        std::unique_ptr<RuntimeNativeAudioState> before;
        std::vector<GCRoot<RuntimeObject>> roots;
        std::array<std::optional<QuestVr::AudioVoiceCommand>,QuestVr::AudioVoiceCapacity> commands;
        std::optional<QuestVr::AudioOutputTransport::Reservation> reservation;
        explicit NativeAudioJournal(RuntimeNativeAudioDomain* value):domain(value){}
    };
    std::unique_ptr<NativeAudioJournal> nativeAudioJournal_;
    std::size_t NativeAudioJournalBytes() const {
        if(!nativeAudioJournal_)return 0u;
        return sizeof(NativeAudioJournal)+sizeof(RuntimeNativeAudioState)+
            nativeAudioJournal_->roots.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+
            (nativeAudioJournal_->before?nativeAudioJournal_->before->resources.capacity()*sizeof(RuntimeNativeAudioResource):0u);
    }
    RuntimeNativeAudioDomain& RequireNativeAudio() {
        if(!transaction_||!persistentNativeAudio||!persistentNativeAudio->get())
            throw std::runtime_error("Quest native audio operation has no actual provider/root");
        auto& domain=*persistentNativeAudio->get();LifecycleStep();
        if(!ActualNativeAudioObjectValid(domain.instance)||!domain.output||!domain.output->Ready()||!domain.output->MixerAttached()||
            domain.output->ConsumerEpoch()!=domain.consumerEpoch||
            !persistentAudioOutput||persistentAudioOutput->transport!=domain.output||
            Object(domain.instance->reflection.objectPath)!=domain.instance)
            throw std::runtime_error("Quest native audio provider/output ownership is retired or stale");
        auto* client=RequireActualEngineClient(CurrentClockLevel());
        if(!client||domain.engine!=CurrentClockLevel()->nativeLevelEngine||
            persistentAudioOutput->engine!=domain.engine||persistentAudioOutput->client!=client||
            persistentAudioOutput->lookupGeneration!=domain.lookupGeneration)
            throw std::runtime_error("Quest native audio lost its actual reciprocal Engine/Client/output tuple");
        domain.output->RequireReady();return domain;
    }
    RuntimeNativeAudioState& TouchNativeAudio() {
        auto& domain=RequireNativeAudio();
        if(nativeAudioJournal_) {
            if(nativeAudioJournal_->domain.get()!=&domain)throw std::runtime_error("Native audio changed provider inside its VM root");
            return *domain.state;
        }
        ValidateStateBudget(sizeof(NativeAudioJournal)+sizeof(RuntimeNativeAudioState)+
            domain.state->resources.capacity()*sizeof(RuntimeNativeAudioResource)+
            (2u*QuestVr::AudioVoiceCapacity+domain.state->resources.size()+1u)*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode)));
        auto staged=std::make_unique<NativeAudioJournal>(&domain);
        staged->before=std::make_unique<RuntimeNativeAudioState>(*domain.state);
        staged->roots.reserve(2u*QuestVr::AudioVoiceCapacity+domain.state->resources.size()+1u);
        if(domain.state->viewport)staged->roots.emplace_back(domain.state->viewport);
        for(const auto& channel:domain.state->channels.slots) {
            if(channel.actor)staged->roots.emplace_back(reinterpret_cast<RuntimeObject*>(channel.actor));
            if(channel.sound)staged->roots.emplace_back(reinterpret_cast<RuntimeObject*>(channel.sound));
        }
        for(const auto& resource:domain.state->resources)staged->roots.emplace_back(resource.sound);
        nativeAudioJournal_=std::move(staged);return *domain.state;
    }
    void StageNativeAudioStop(std::size_t slot,std::uint64_t) {
        nativeAudioJournal_->domain->state->occlusion[slot]=0.0f;
        QuestVr::AudioVoiceCommand command;command.slot=static_cast<std::uint32_t>(slot);
        // Final state for an exclusively owned slot supersedes any uncommitted
        // Play/Update of that slot. The exact transport capability fences it.
        nativeAudioJournal_->commands[slot]=std::move(command);
    }
    RuntimeObject* NativeAudioListener(RuntimeNativeAudioState& state) {
        if(!state.viewport)return nullptr;
        if(NativeSessionViewport(state.viewport->nativeViewportIdentity)!=state.viewport)
            throw std::runtime_error("Native audio viewport is not its actual Client array member");
        auto* pawn=state.viewport->nativeViewportActor;
        if(!pawn)throw std::runtime_error("Native audio viewport has no actual possessed PlayerPawn listener");
        RequireMapActor(pawn);
        auto* target=SoundObjectField(pawn,"Engine.PlayerPawn.ViewTarget","Engine.Actor");
        if(target)RequireMapActor(target);
        return target?target:pawn;
    }
    bool NativeAudioPlay(RuntimeObject* actor,std::uint32_t id,RuntimeObject* sound,
        const std::array<float,3u>& location,float volume,float radius,float pitch) {
        auto& state=TouchNativeAudio();
        if(actor&&(!actor->cls||!IsDerivedFromPath(actor->cls,"Engine.Actor")||
            ActualLookupObject(*persistentObjectLookup,reinterpret_cast<std::uintptr_t>(actor))!=actor))
            throw std::runtime_error("Native sound source is not an actual Actor");
        if(sound)ValidateObjectValue(Value::Text(Kind::Object,sound->reflection.objectPath),"Engine.Sound",false);
        const auto* listener=(state.viewport&&sound)?NativeAudioListener(state):nullptr;
        const auto position=listener?SoundLocation(const_cast<RuntimeObject*>(listener)):PortableModelVec3{};
        return QuestVr::PlayNativeAudioChannel(state.channels,state.viewport!=nullptr,
            reinterpret_cast<std::uintptr_t>(actor),id,reinterpret_cast<std::uintptr_t>(sound),location,
            volume,radius,pitch,{position.x,position.y,position.z},[this](auto slot,auto voice){StageNativeAudioStop(slot,voice);});
    }
    void NativeAudioNoteDestroy(RuntimeObject* audio,RuntimeObject* actor) {
        auto& domain=RequireNativeAudio();
        if(audio!=domain.instance||!actor||!actor->cls||!IsDerivedFromPath(actor->cls,"Engine.Actor")||
            !actor->lookupControl||actor->lookupControl!=persistentObjectLookup||
            ActualLookupObject(*persistentObjectLookup,reinterpret_cast<std::uintptr_t>(actor))!=actor)
            throw std::runtime_error("Native NoteDestroy requires valid actual Audio/Actor identities");
        // World slot removal and bDeleteMe already happened at this call site;
        // they must not invalidate this still-live UObject identity.
        auto& state=TouchNativeAudio();
        QuestVr::NoteNativeAudioActorDestroyed(state.channels,reinterpret_cast<std::uintptr_t>(actor),
            [this](auto slot,auto voice){StageNativeAudioStop(slot,voice);});
    }
    void NativeAudioSetViewport(RuntimeObject* viewport,QuestVr::Vm::Execution& execution) {
        auto& domain=RequireNativeAudio();auto& state=TouchNativeAudio();
        for(std::size_t slot=0u;slot<state.channels.liveCount;++slot) {
            LifecycleStep();QuestVr::StopNativeAudioChannel(state.channels,slot,
                [this](auto index,auto voice){StageNativeAudioStop(index,voice);});
        }
        if(state.viewport==viewport)return; // Stop loop MUST precede equality.
        if(viewport&&(viewport!=persistentAudioOutput->viewport||
            NativeSessionViewport(domain.output->ViewportIdentity())!=viewport))
            throw std::runtime_error("Quest Audio viewport change lacks its real initialized output/target");
        state.viewport=viewport;
        if(viewport&&viewport->nativeViewportActor) {
            auto* pawn=viewport->nativeViewportActor;RequireMapActor(pawn);
            auto* song=SoundObjectField(pawn,"Engine.PlayerPawn.Song","Engine.Music");
            const auto transition=SoundField(pawn,"Engine.PlayerPawn.Transition",Kind::Byte,"Engine.Actor.EMusicTransition");
            if(song&&transition.integer==0) {
                const auto field=DescribeProperty("Engine.PlayerPawn.Transition");
                execution.WriteReference(ReferenceFor(pawn,field,0u,false),Value::Byte(1u));
            }
        }
        // Changed-viewport backend adaptation: retain the already-open AAudio
        // stream, retire old resource inventory and bind its authenticated XR
        // viewport. Never pass fake Win32 handles or restart accepted music.
        state.resources.clear();
    }
    const RuntimeNativeAudioResource& PrepareNativeSound(RuntimeObject* sound) {
        auto& domain=RequireNativeAudio();auto& state=TouchNativeAudio();
        for(const auto& resource:state.resources)if(resource.sound==sound)return resource;
        if(!sound||Object(sound->reflection.objectPath)!=sound||state.resources.size()>=256u)
            throw std::runtime_error("Native sound resource identity/inventory is invalid");
        ValidateObjectValue(Value::Text(Kind::Object,sound->reflection.objectPath),"Engine.Sound",false);
        LifecycleStep();
        PortableSound encoded;
        if(!sound->sourcePath.empty()&&sound->exportIndex<Table(sound->sourcePath).exports.size()) {
            const auto& table=Table(sound->sourcePath);const auto& entry=table.exports[sound->exportIndex];
            if(LowerAscii(Qualified(table,entry.ObjClass))!="engine.sound")
                throw std::runtime_error("Native Sound source export is not its authored Engine.Sound");
            encoded=LoadPortableSound(table,sound->exportIndex,RemainingNativeStateBytes());
        }else throw std::runtime_error("Native sound requires its actual admitted Sound package export");
        using QuestVr::DecalStateDetail::Add;
        const auto encodedBytes=Add(sizeof(encoded),encoded.data.capacity());
        ValidateStateBudget(Add(encodedBytes,sizeof(PortableNativeSoundDecodeWork)));
        PortableNativeSoundDecodeWork work;
        work.step=[this](std::size_t units){nativeWork_.Step(units);};
        work.admit=[this,encodedBytes](std::size_t bytes){
            ValidateStateBudget(Add(Add(encodedBytes,sizeof(PortableNativeSoundDecodeWork)),bytes));
        };
        auto decoded=DecodePortableNativeSoundPcm(encoded.format.ToString(),encoded.data,work);
        ValidateStateBudget(Add(Add(encodedBytes,sizeof(work)),sizeof(decoded)+
            decoded.stereo.capacity()*sizeof(std::int16_t)));
        auto pcm=domain.output->PrepareSourcePcm(std::move(decoded.stereo),decoded.rate);
        const auto scratch=Add(Add(encodedBytes,sizeof(work)),sizeof(decoded));
        ValidateStateBudget(Add(scratch,sizeof(RuntimeNativeAudioResource)));
        if(state.resources.size()==state.resources.capacity()) {
            const auto next=std::min<std::size_t>(256u,std::max<std::size_t>(8u,state.resources.size()*2u));
            ValidateStateBudget(Add(scratch,next*sizeof(RuntimeNativeAudioResource))); // Old/new storage and encoded scratch coexist.
            state.resources.reserve(next);
        }
        state.resources.push_back({sound,std::move(pcm),decoded.loopBegin,decoded.loopEnd,decoded.loop});
        ValidateStateBudget(scratch);
        return state.resources.back();
    }
    void NativeAudioAmbient(RuntimeNativeAudioDomain& domain,RuntimeNativeAudioState& state,RuntimeObject* listener) {
        auto* pawn=state.viewport->nativeViewportActor;
        const auto flags=static_cast<std::uint32_t>(SoundField(pawn,"Engine.PlayerPawn.ShowFlags",Kind::Int).integer);
        auto* info=SoundObjectField(pawn,"Engine.Actor.Level","Engine.LevelInfo");
        if(!info)throw std::runtime_error("Native ambient audio has no actual listener LevelInfo");
        const auto pauser=SoundField(info,"Engine.LevelInfo.Pauser",Kind::String).text;
        const bool eligible=(flags&0x4800u)!=0u&&pauser.empty();
        const auto target=SoundLocation(listener);
        const auto radius=[&](RuntimeObject* actor) {
            return QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Multiply(QuestVr::NativeScalar::Load(
                static_cast<double>(SoundField(actor,"Engine.Actor.SoundRadius",Kind::Byte).integer+1)),QuestVr::NativeScalar::Load(25.0)));
        };
        const auto within=[&](RuntimeObject* actor,bool maintenance) {
            const auto at=SoundLocation(actor);const auto r=radius(actor);
            using namespace QuestVr::NativeScalar;
            const auto x=Subtract(Load(at.x),Load(target.x)),y=Subtract(Load(at.y),Load(target.y)),z=Subtract(Load(at.z),Load(target.z));
            const auto squared=maintenance?Add(Add(Multiply(z,z),Multiply(y,y)),Multiply(x,x)):
                Add(Add(Multiply(x,x),Multiply(z,z)),Multiply(y,y));
            return Compare(179u,Multiply(Load(r),Load(r)),squared);
        };
        const auto volume=[&](RuntimeObject* actor,bool maintenance) {
            using namespace QuestVr::NativeScalar;
            auto value=Multiply(Multiply(Load(static_cast<double>(SoundField(actor,"Engine.Actor.SoundVolume",Kind::Byte).integer)),
                Load(domain.ambientFactor)),Load(1.0/255.0));
            if(maintenance)value=Add(value,value);
            return StoreFloat(value);
        };
        const auto pitch=[&](RuntimeObject* actor) {return QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Multiply(
            QuestVr::NativeScalar::Load(static_cast<double>(SoundField(actor,"Engine.Actor.SoundPitch",Kind::Byte).integer)),
            QuestVr::NativeScalar::Load(1.0/64.0)));};
        if(eligible)for(auto* actor:persistentWorldActors) {
            LifecycleStep();if(!actor||actor->worldRemoved)continue;
            auto* sound=SoundObjectField(actor,"Engine.Actor.AmbientSound","Engine.Sound");
            if(!sound||!within(actor,false))continue;
            const auto id=(actor->objectIndex.Index()<<4u)+8u;
            bool present{};for(std::size_t slot=0u;slot<state.channels.liveCount;++slot)
                present=present||state.channels.slots[slot].id==id;
            if(!present) {
                const auto at=SoundLocation(actor);
                NativeAudioPlay(actor,id,sound,{at.x,at.y,at.z},volume(actor,false),radius(actor),pitch(actor));
            }
        }
        for(std::size_t slot=0u;slot<state.channels.liveCount;++slot) {
            auto& channel=state.channels.slots[slot];if((channel.id&0xEu)!=8u)continue;
            auto* actor=reinterpret_cast<RuntimeObject*>(channel.actor);
            if(!actor||ActualLookupObject(*persistentObjectLookup,channel.actor)!=actor)
                throw std::runtime_error("Native ambient channel has no valid retained Actor");
            if(!eligible||!within(actor,true)||SoundObjectField(actor,"Engine.Actor.AmbientSound","Engine.Sound")!=
                reinterpret_cast<RuntimeObject*>(channel.sound)) {
                QuestVr::StopNativeAudioChannel(state.channels,slot,[this](auto index,auto voice){StageNativeAudioStop(index,voice);});continue;
            }
            if(SoundField(actor,"Engine.Actor.LightType",Kind::Byte).integer!=0)
                throw std::runtime_error("Original ambient light modulation requires the authenticated native renderer service");
            channel.volume=volume(actor,true);channel.radius=radius(actor);channel.pitch=pitch(actor);
        }
    }
    void NativeAudioUpdate() {
        auto& domain=RequireNativeAudio();auto& state=TouchNativeAudio();
        if(!state.viewport)return;
        auto* listener=NativeAudioListener(state);const auto position=SoundLocation(listener);
        const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto difference=QuestVr::NativeScalar::Subtract(QuestVr::NativeScalar::Load(now),QuestVr::NativeScalar::Load(state.previousTime));
        const double delta=std::clamp(QuestVr::NativeScalar::StoreDouble(difference),0.0,1.0);
        state.previousTime=QuestVr::NativeScalar::StoreDouble(QuestVr::NativeScalar::Add(difference,
            QuestVr::NativeScalar::Load(state.previousTime)));
        if(state.updates==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Native audio update serial exhausted");
        ++state.updates;
        NativeAudioAmbient(domain,state,listener);
        const auto rotation=SoundField(listener,"Engine.Actor.Rotation",Kind::Rotator).rotation;
        const double yaw=static_cast<double>(rotation[1])*6.2831853071795864769/65536.0;
        // Quest stereo adapter: source-authenticated radius/volume/priority,
        // equal-power L/R output using actual listener yaw. This is a concrete
        // software backend profile, NOT Galaxy's pan quantizer/HRTF/Win32 mix.
        for(std::size_t slot=0u;slot<state.channels.liveCount;++slot) {
            LifecycleStep();auto& channel=state.channels.slots[slot];
            if(channel.actor&&ActualLookupObject(*persistentObjectLookup,channel.actor)!=reinterpret_cast<RuntimeObject*>(channel.actor))
                throw std::runtime_error("Native audio source UObject is no longer valid");
            if(!channel.id)continue;
            if(channel.voice) {
                const auto feedback=domain.output->Feedback(slot);
                if(feedback.voice==channel.voice&&feedback.playback==QuestVr::AudioVoicePlayback::Finished) {
                    QuestVr::StopNativeAudioChannel(state.channels,slot,[this](auto index,auto voice){StageNativeAudioStop(index,voice);});
                    continue; // Original completed-handle branch does no elapsed/spatial work.
                }
                if(feedback.voice&&feedback.voice!=channel.voice) {
                    // Old backend feedback may lag an already committed
                    // replacement. Never call it completion of the new voice.
                    const auto& pending=nativeAudioJournal_->commands[slot];
                    if(pending&&pending->operation==QuestVr::AudioVoiceOperation::Stop)
                        throw std::runtime_error("Native audio stale completion conflicts with its slot journal");
                }
            }
            channel.elapsed=QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Add(
                QuestVr::NativeScalar::Load(channel.elapsed),QuestVr::NativeScalar::Load(delta)));
            if(channel.actor) {
                auto* source=reinterpret_cast<RuntimeObject*>(channel.actor);
                if(ActualLookupObject(*persistentObjectLookup,channel.actor)!=source)
                    throw std::runtime_error("Native audio source UObject is no longer valid");
                const auto current=SoundLocation(source);channel.location={current.x,current.y,current.z};
            }
            channel.priority=QuestVr::OriginalAudioPriority(channel.location,{position.x,position.y,position.z},channel.volume,channel.radius).stored;
            auto* sound=reinterpret_cast<RuntimeObject*>(channel.sound);const auto& resource=PrepareNativeSound(sound);
            const double dx=static_cast<double>(channel.location[0])-position.x;
            const double dy=static_cast<double>(channel.location[1])-position.y;
            const double dz=static_cast<double>(channel.location[2])-position.z;
            const double distance=std::sqrt(dx*dx+dy*dy+dz*dz);
            const double side=-dx*std::sin(yaw)+dy*std::cos(yaw);
            double pan=distance>0.0?std::clamp(side/distance,-1.0,1.0):0.0;
            if(domain.reverseStereo)pan=-pan;
            const bool speech=(channel.id&15u)==10u||(channel.id&15u)==11u;
            bool blocked{};
            if(channel.actor&&!speech) {
                auto* pawn=state.viewport->nativeViewportActor;auto to=SoundLocation(pawn);
                to.z=QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Add(QuestVr::NativeScalar::Load(to.z),
                    QuestVr::NativeScalar::Load(SoundField(pawn,"Engine.Pawn.BaseEyeHeight",Kind::Float).floating)));
                auto* source=reinterpret_cast<RuntimeObject*>(channel.actor);
                blocked=!NativeFastLine(source,to,SoundLocation(source));
            }
            const auto fade=QuestVr::NativeScalar::Add(QuestVr::NativeScalar::Load(state.occlusion[slot]),
                QuestVr::NativeScalar::Load(blocked?delta:-delta));
            state.occlusion[slot]=static_cast<float>(std::clamp(QuestVr::NativeScalar::StoreDouble(fade),0.0,0.5));
            const double obstruction=std::max(1.0-2.0*state.occlusion[slot],0.33);
            const double gain=std::clamp(1.0-distance/channel.radius,0.0,1.0)*
                std::max(0.0f,channel.volume)*(speech?domain.speechVolume:domain.soundVolume)/255.0*obstruction;
            double doppler=1.0;
            if(channel.actor&&(channel.id&0xEu)==8u&&distance>0.0) {
                const auto velocity=SoundField(reinterpret_cast<RuntimeObject*>(channel.actor),"Engine.Actor.Velocity",Kind::Vector).vector;
                doppler=std::clamp(1.0-(dx*velocity[0u]+dy*velocity[1u]+dz*velocity[2u])/distance/domain.dopplerSpeed,0.5,2.0);
            }
            const double step=static_cast<double>(resource.pcm->Rate())*channel.pitch*doppler/domain.output->Endpoint()->Format().sampleRate;
            if(!std::isfinite(step)||step<=0.0||step>65536.0)
                throw std::runtime_error("Quest native sound frequency is outside supported positive mixer phase range");
            QuestVr::AudioVoiceCommand command;command.slot=static_cast<std::uint32_t>(slot);
            command.phaseStep=std::max<std::uint64_t>(1u,static_cast<std::uint64_t>(step*QuestVr::AudioPhaseOne));
            command.leftGain=static_cast<float>(gain*std::sqrt((1.0-pan)*0.5));
            command.rightGain=static_cast<float>(gain*std::sqrt((1.0+pan)*0.5));
            if(!channel.voice) {
                channel.voice=QuestVr::NextNativeAudioVoice(state.channels);command.operation=QuestVr::AudioVoiceOperation::Play;
                command.pcm=resource.pcm;command.loop=resource.loop;
                command.loopBegin=resource.loopBegin;command.loopEnd=resource.loopEnd;
            }else if(const auto& pending=nativeAudioJournal_->commands[slot];pending&&
                pending->operation==QuestVr::AudioVoiceOperation::Play&&pending->voice==channel.voice) {
                command.operation=QuestVr::AudioVoiceOperation::Play;command.pcm=pending->pcm;
                command.loop=pending->loop;command.loopBegin=pending->loopBegin;command.loopEnd=pending->loopEnd;
            }else command.operation=QuestVr::AudioVoiceOperation::Update;
            command.voice=channel.voice;nativeAudioJournal_->commands[slot]=std::move(command);
        }
    }
    void PrepareNativeAudioPublication() {
        if(!nativeAudioJournal_)return;
        auto& domain=RequireNativeAudio();QuestVr::AudioVoiceBatch batch;
        batch.generation=domain.output->Generation();
        for(const auto& command:nativeAudioJournal_->commands)if(command)batch.commands[batch.count++]=*command;
        if(batch.count) {
            nativeAudioJournal_->reservation.emplace(domain.output->Reserve(std::move(batch)));
            nativeAudioJournal_->reservation->RequirePublishable();
        }
        ValidateStateBudget();domain.output->RequireReady();
        if(!domain.output->MixerAttached()||domain.output->ConsumerEpoch()!=domain.consumerEpoch)
            throw std::runtime_error("Native audio lost/replaced its consumer before commit");
    }
    void ValidateNativeAudioPublicationLease() {
        if(!nativeAudioJournal_)return;
        auto& domain=RequireNativeAudio();
        if(nativeAudioJournal_->domain.get()!=&domain)throw std::runtime_error("Native audio publication changed provider");
        if(nativeAudioJournal_->reservation)nativeAudioJournal_->reservation->RequirePublishable();
    }
    void CommitNativeAudioPublication() noexcept {
        if(!nativeAudioJournal_)return;
        if(nativeAudioJournal_->reservation)nativeAudioJournal_->reservation->PublishPrepared();
        nativeAudioJournal_.reset();
    }
    void RollbackNativeAudio() noexcept {
        if(!nativeAudioJournal_)return;
        nativeAudioJournal_->reservation.reset();
        auto* domain=nativeAudioJournal_->domain.get();
        if(domain&&nativeAudioJournal_->before)domain->state.swap(nativeAudioJournal_->before);
        nativeAudioJournal_.reset();
    }
