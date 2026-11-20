// Private runtime API implementation, after actual Engine/default/Class services.
namespace {
std::size_t NativeAudioSnapshotPeakBytes(const RuntimeNativeAudioDomain& domain) {
    using QuestVr::DecalStateDetail::Add;
    using QuestVr::DecalStateDetail::Multiply;
    auto bytes=sizeof(PortableNativeAudioSnapshot);
    const auto text=[&](std::size_t size) {
        bytes=Add(bytes,Multiply(Add(size,sizeof(std::string)+1u),4u));
    };
    for(const auto* object:{domain.cls,domain.instance,domain.engine,domain.state?domain.state->viewport:nullptr})
        text(object?object->reflection.objectPath.size():0u);
    text(domain.iniPath.size());
    if(domain.state) {
        const auto& channels=domain.state->channels;
        if(channels.liveCount>channels.slots.size())throw std::runtime_error("Quest Audio snapshot has invalid channel bounds");
        bytes=Add(bytes,Multiply(channels.liveCount,sizeof(PortableNativeAudioChannelSnapshot)));
        for(std::size_t slot=0u;slot<channels.liveCount;++slot) {
            const auto& channel=channels.slots[slot];
            for(const auto* object:{reinterpret_cast<const RuntimeObject*>(channel.actor),reinterpret_cast<const RuntimeObject*>(channel.sound)})
                text(object?object->reflection.objectPath.size():0u);
        }
    }
    return bytes;
}
std::size_t NativeAudioSnapshotRetainedBytes(const PortableNativeAudioSnapshot& snapshot) {
    using QuestVr::DecalStateDetail::Add;
    auto bytes=Add(sizeof(snapshot),QuestVr::DecalStateDetail::Multiply(snapshot.channels.capacity(),sizeof(PortableNativeAudioChannelSnapshot)));
    for(const auto* text:{&snapshot.classPath,&snapshot.audioPath,&snapshot.enginePath,&snapshot.viewportPath,&snapshot.configSource})
        bytes=Add(bytes,text->capacity()+1u);
    for(const auto& channel:snapshot.channels)
        for(const auto* text:{&channel.actorPath,&channel.soundPath})bytes=Add(bytes,text->capacity()+1u);
    return bytes;
}
PortableNativeAudioSnapshot NativeAudioSnapshot(const RuntimeNativeAudioDomain& domain) {
    PortableNativeAudioSnapshot out;out.prepared=true;out.initialized=domain.initialized;out.retired=domain.retired;
    out.ready=!domain.retired&&domain.output&&domain.output->Ready()&&domain.output->MixerAttached()&&
        domain.output->ConsumerEpoch()==domain.consumerEpoch;
    out.classPath=NativeObjectPath(domain.cls);out.audioPath=NativeObjectPath(domain.instance);
    out.enginePath=NativeObjectPath(domain.engine);out.configSource=domain.iniPath;
    out.classIndex=domain.cls?domain.cls->objectIndex.Index():0u;out.audioIndex=domain.instance?domain.instance->objectIndex.Index():0u;
    out.baseDefaultIndex=domain.baseDefault?domain.baseDefault->objectIndex.Index():0u;
    out.defaultIndex=domain.defaults?domain.defaults->objectIndex.Index():0u;
    out.lookupGeneration=domain.lookupGeneration;out.outputGeneration=domain.output?domain.output->Generation():0u;
    out.consumerEpoch=domain.consumerEpoch;
    if(domain.state) {
        const auto& state=*domain.state;out.viewportPath=NativeObjectPath(state.viewport);
        out.liveChannels=state.channels.liveCount;out.freeCounter=state.channels.freeCounter;
        out.voiceSerial=state.channels.voiceSerial;out.previousAudioTime=state.previousTime;out.updates=state.updates;
        out.resourceCount=state.resources.size();
        if(state.channels.liveCount>state.channels.slots.size())throw std::runtime_error("Quest Audio snapshot has invalid channel bounds");
        out.channels.reserve(state.channels.liveCount);
        for(std::size_t slot=0u;slot<state.channels.liveCount;++slot) {
            const auto& channel=state.channels.slots[slot];
            out.channels.push_back({static_cast<std::uint32_t>(slot),channel.id,channel.voice,
                NativeObjectPath(reinterpret_cast<RuntimeObject*>(channel.actor)),
                NativeObjectPath(reinterpret_cast<RuntimeObject*>(channel.sound)),channel.priority,channel.elapsed});
        }
    }
    out.referenceBytes=NativeAudioRetainedBytes(domain);out.pcmBytes=domain.output?domain.output->RetainedPcmBytes():0u;
    return out;
}
}
PortableNativeAudioSnapshot ReadPortableRuntimeQuestAudioSubsystem() {
    if(!persistentNativeAudio)return {};
    const auto& domain=*persistentNativeAudio->get();
    if(!domain.retired&&!ActualNativeAudioObjectValid(domain.instance))
        throw std::runtime_error("Quest Audio lost its actual native Class/default/instance/Engine ownership");
    PortableActorVmHost host;host.ValidateAdditionalPackageBudget(NativeAudioSnapshotPeakBytes(domain));
    auto result=NativeAudioSnapshot(domain);
    host.ValidateAdditionalPackageBudget(NativeAudioSnapshotRetainedBytes(result));
    return result;
}
PortableNativeAudioSnapshot PreparePortableRuntimeQuestAudioSubsystem(const std::string& path) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(persistentVmRootCount)throw std::runtime_error("Quest Audio initialization requires an exclusive CPU boundary");
    if(path.empty()||path.size()>8192u||path.find('\0')!=std::string::npos)
        throw std::runtime_error("Quest Audio requires an explicit bounded engine INI source");
    const auto supplied=std::filesystem::absolute(path).lexically_normal().string();
    if(!persistentEngineCollision||
        supplied!=persistentEngineCollision->get()->iniPath||!persistentAudioOutput)
        throw std::runtime_error("Quest Audio requires actual Engine/read-only INI/output binding");
    const auto binding=ReadPortableRuntimeAudioOutput();
    if(!binding.bound||!binding.ready||!binding.mixerAttached)
        throw std::runtime_error("Quest Audio initialization requires actual started output and exclusive PCM consumer");
    if(persistentNativeAudio) {
        const auto& current=*persistentNativeAudio->get();
        if(current.retired||current.output!=persistentAudioOutput->transport||current.iniPath!=supplied)
            throw std::runtime_error("Retired/replaced Quest Audio requires complete native runtime teardown");
        return ReadPortableRuntimeQuestAudioSubsystem();
    }
    auto* engine=persistentEngineCollision->get()->engine;
    if(!ActualNativeCollisionObjectValid(engine)||!engine->nativeEngineInstance||engine->nativeEngineInstance->values.audio||
        !engine->nativeEngineInstance->values.useSound)
        throw std::runtime_error("Quest Audio requires its genuine sound-enabled Engine with an unoccupied Audio slot");
    host.ValidateAdditionalPackageBudget(3u*QuestVr::NativeConfig::Limits{}.fileBytes+QuestVr::NativeConfig::Limits{}.retainedBytes);
    const auto ini=QuestVr::NativeConfig::ReadOrderedIni(supplied);
    if(!ini)throw std::runtime_error("Quest Audio config source is missing");
    const auto selected=ini->Scalar("Engine.Engine","AudioDevice");
    if(!selected||LowerAscii(std::string(*selected))!="galaxy.galaxyaudiosubsystem")
        throw std::runtime_error("Quest Audio has no explicit platform mapping for the selected native AudioDevice");
    const auto integer=[&](const char* key,int lower,int upper) {
        const auto text=ini->Scalar("Galaxy.GalaxyAudioSubsystem",key);std::int32_t value{};
        if(!text||!QuestVr::NativeConfigImport::Integer(QuestVr::UnrealString::Decode(*text),value).returned||
            value<lower||value>upper)throw std::runtime_error(std::string("Quest Audio requires bounded explicit ")+key);
        return value;
    };
    const auto channels=integer("EffectsChannels",0,32),soundVolume=integer("SoundVolume",0,255),speechVolume=integer("SpeechVolume",0,255);
    const auto real=[&](const char* key) {
        const auto text=ini->Scalar("Galaxy.GalaxyAudioSubsystem",key);
        if(!text||text->empty()||text->size()>128u)throw std::runtime_error(std::string("Quest Audio requires explicit ")+key);
        const std::string input(*text);char* end{};const float value=std::strtof(input.c_str(),&end);
        if(end!=input.c_str()+input.size()||!std::isfinite(value)||value<0.0f)
            throw std::runtime_error(std::string("Quest Audio finite config is invalid: ")+key);
        return value;
    };
    const float ambientFactor=real("AmbientFactor"),dopplerSpeed=real("DopplerSpeed");
    if(dopplerSpeed==0.0f)throw std::runtime_error("Quest Audio DopplerSpeed must be positive");
    const auto reverse=ini->Scalar("Galaxy.GalaxyAudioSubsystem","ReverseStereo");
    if(!reverse)throw std::runtime_error("Quest Audio ReverseStereo source is missing");
    std::uint32_t reverseBits{};
    if(!QuestVr::NativeConfigImport::Boolean(QuestVr::UnrealString::Decode(*reverse),1u,reverseBits).returned)
        throw std::runtime_error("Quest Audio ReverseStereo is invalid");
    auto* base=host.Object("Engine.AudioSubsystem");auto* meta=host.Object("Core.Class");auto* objectClass=host.Object("Core.Object");
    auto* transient=persistentEngineCollision->get()->transient;
    if(base->nativeMetadataClass!=&QuestVr::NativeConfigClasses[1u]||base->defaultObject||
        !PreparedNativeClassDefaultValid(base->base)||base->nativeClassWithin!=objectClass||base->cls!=meta)
        throw std::runtime_error("Quest Audio abstract base lacks real superclass/default/Class provenance");
    if(persistentQualifiedObjects.size()>1'000'000u-2u||persistentVmObjects.size()>1'000'000u-2u)
        throw std::runtime_error("Quest Audio exceeds actual object namespace bounds");
    host.ValidateAdditionalPackageBudget(ini->RetainedBytes()+sizeof(RuntimeNativeAudioDomain)+sizeof(RuntimeNativeAudioState)+
        sizeof(GCAllocation)+2u*sizeof(GCRootNode)+4u*sizeof(GCRoot<RuntimeObject>)+2u*sizeof(RuntimeObject*)+4096u);
    pendingBirthGarbage=true;GCRoot<RuntimeNativeAudioDomain> staged(GC::Alloc<RuntimeNativeAudioDomain>());
    auto* domain=staged.get();domain->engine=engine;domain->iniPath=supplied;
    domain->lookupGeneration=persistentObjectLookup->generation;domain->output=persistentAudioOutput->transport;
    domain->consumerEpoch=domain->output->ConsumerEpoch();
    domain->soundVolume=static_cast<std::uint8_t>(soundVolume);domain->speechVolume=static_cast<std::uint8_t>(speechVolume);
    domain->reverseStereo=(reverseBits&1u)!=0u;
    domain->ambientFactor=ambientFactor;domain->dopplerSpeed=dopplerSpeed;
    domain->state=std::make_unique<RuntimeNativeAudioState>();domain->state->channels.liveCount=static_cast<std::uint32_t>(channels);
    // Per-transport initial counter/voice identity and zero active channel
    // default state. No copied/template Galaxy object is being reset here.
    decltype(persistentQualifiedObjects) deltaQualified;decltype(persistentVmObjects) deltaVm;
    std::vector<GCRoot<RuntimeObject>> allocations;allocations.reserve(4u);
    std::vector<RuntimeObject*> publications;publications.reserve(2u);
    std::size_t nameWork{};
    using QuestVr::DecalStateDetail::Add;
    const auto constructionBytes=[&]() {
        auto bytes=Add(NativeAudioRetainedBytes(*domain),ini->RetainedBytes());
        // An arena callback can run inside allocate(), before its caller has
        // installed the returned pointer into the domain. Keep that rooted
        // in-progress object's real payload in the admission peak as well.
        for(const auto& allocation:allocations) {
            const auto* object=allocation.get();
            if(object==domain->cls||object==domain->baseDefault||object==domain->defaults||object==domain->instance)continue;
            bytes=Add(bytes,sizeof(RuntimeObject)+sizeof(GCAllocation)+object->references.capacity()*sizeof(RuntimeObject*));
            if(object->preparedNativeDefaults)bytes=Add(bytes,sizeof(RuntimePreparedNativeDefaults));
            for(const auto* text:{&object->reflection.objectPath,&object->reflection.metaClass,&object->reflection.outerPath,
                &object->reflection.basePath,&object->sourcePath})bytes=Add(bytes,text->capacity()+1u);
            bytes=Add(bytes,2u*(sizeof(std::string)+6u*sizeof(void*)+object->reflection.objectPath.capacity()+1u));
        }
        bytes=Add(bytes,allocations.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+publications.capacity()*sizeof(RuntimeObject*));
        return Add(bytes,(deltaQualified.bucket_count()+deltaVm.bucket_count())*sizeof(void*));
    };
    NameStringArenaStage arena(NameStringArenaStage::Work{
        [&](std::size_t units) {
            if(units>(16u<<20u)-nameWork)throw std::runtime_error("Quest Audio FName staging work budget exhausted");
            nameWork+=units;
        },
        [&](std::size_t bytes){host.ValidateAdditionalPackageBudget(Add(bytes,constructionBytes()));}},16u);
    const auto admitConstruction=[&](std::size_t extra) {
        host.ValidateAdditionalPackageBudget(Add(extra,Add(constructionBytes(),arena.RetainedBytes())));
    };
    const auto allocate=[&](const std::string& objectPath,const std::string& type,RuntimeObject* cls,RuntimeObject* outer,std::uint32_t flags) {
        if(persistentVmObjects.count(LowerAscii(objectPath))||deltaVm.count(LowerAscii(objectPath))||
            persistentQualifiedObjects.count(objectPath)||IsContextExcludedPath(objectPath))
            throw std::runtime_error("Quest Audio allocation collides with actual source namespace");
        admitConstruction(sizeof(RuntimeObject)+sizeof(GCAllocation)+sizeof(GCRootNode)+
            16u*(objectPath.size()+type.size()+NativeObjectPath(outer).size()+256u));
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{objectPath,type,NativeObjectPath(outer),{},flags,0},nullptr));
        auto* object=allocations.back().get();object->cls=cls;object->outer=outer;object->exportIndex=std::numeric_limits<std::size_t>::max();
        object->lookupName=arena.Add(objectPath.substr(objectPath.find_last_of('.')+1u));
        if(!deltaQualified.emplace(objectPath,object).second||!deltaVm.emplace(LowerAscii(objectPath),object).second)
            throw std::runtime_error("Quest Audio staged namespace collision");
        publications.push_back(object);return object;
    };
    domain->cls=allocate(std::string(QuestAudioClass.path),"Class",meta,RegisteredNativePackage("Engine"),QuestAudioClass.registrationObjectFlags);
    domain->cls->nativeMetadataClass=&QuestAudioClass;domain->cls->base=base;domain->cls->nativeClassWithin=objectClass;
    domain->cls->reflection.basePath="Engine.AudioSubsystem";
    const auto defaults=[&](RuntimeObject* cls,RuntimeObject* parent,RuntimeNativeDefaultKind kind,std::uint32_t size,std::uint32_t flags) {
        if(!cls->base||!parent||parent->cls!=cls->base||parent->defaultOwner!=cls->base||!parent->preparedNativeDefaults||
            parent->preparedNativeDefaults->owner!=cls->base)
            throw std::runtime_error("Quest Audio CDO lost actual superclass-copy authority");
        const auto& inherited=*parent->preparedNativeDefaults;
        if(inherited.nativeSize>size||inherited.propertyDimension||inherited.propertyElementSize||inherited.propertyFlags||
            inherited.propertyOffset||inherited.structPropertiesSize||!inherited.structFriendlyName.IsNone()||inherited.input||inherited.primitiveBoundsValid)
            throw std::runtime_error("Quest Audio superclass CDO has a foreign native extension baseline");
        admitConstruction(sizeof(RuntimeObject)+sizeof(GCAllocation)+sizeof(GCRootNode)+sizeof(RuntimePreparedNativeDefaults)+
            16u*(cls->reflection.objectPath.size()+256u));
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{"@ClassDefault:"+cls->reflection.objectPath,
            arena.Spelling(*cls->lookupName),{},{},0u,0},nullptr));
        auto* object=allocations.back().get();object->cls=cls;object->defaultOwner=cls;
        object->exportIndex=std::numeric_limits<std::size_t>::max();
        auto data=std::make_unique<RuntimePreparedNativeDefaults>();data->kind=kind;data->owner=cls;data->superDefault=parent;
        data->nativeSize=size;data->classFlags=flags;object->preparedNativeDefaults=std::move(data);return object;
    };
    domain->baseDefault=defaults(base,base->base->defaultObject,RuntimeNativeDefaultKind::AudioSubsystem,0x2cu,0x17u);
    domain->defaults=defaults(domain->cls,domain->baseDefault,RuntimeNativeDefaultKind::QuestAudioSubsystem,QuestAudioClass.nativeSize,0x16u);
    domain->instance=allocate(transient->reflection.objectPath+".QuestAudioSubsystem0","QuestAudioSubsystem",domain->cls,transient,0x4000u);
    domain->initialized=true;
    auto owner=std::make_unique<GCRoot<RuntimeNativeAudioDomain>>(domain);
    auto lookup=StageRuntimeLookupObjects(publications);
    if(!lookup)throw std::runtime_error("Quest Audio actual lookup staging is unavailable");
    // Admit the whole-namespace replacement buckets and result strings before
    // allocating them. Live namespace storage remains unchanged on refusal.
    const auto namespaceBuckets=4u*(persistentQualifiedObjects.size()+persistentVmObjects.size()+publications.size()+2u)*sizeof(void*);
    host.ValidateAdditionalPackageBudget(Add(Add(constructionBytes(),arena.RetainedBytes()),
        Add(Add(namespaceBuckets,lookup->RetainedBytes()),NativeAudioSnapshotPeakBytes(*domain))));
    auto result=NativeAudioSnapshot(*domain);
    decltype(persistentQualifiedObjects) nextQualified;decltype(persistentVmObjects) nextVm;
    nextQualified.max_load_factor(persistentQualifiedObjects.max_load_factor());nextVm.max_load_factor(persistentVmObjects.max_load_factor());
    nextQualified.reserve(persistentQualifiedObjects.size()+deltaQualified.size());nextVm.reserve(persistentVmObjects.size()+deltaVm.size());
    auto peak=Add(constructionBytes(),lookup->RetainedBytes());
    peak=Add(peak,(persistentQualifiedObjects.bucket_count()+persistentVmObjects.bucket_count()+
        nextQualified.bucket_count()+nextVm.bucket_count())*sizeof(void*));
    peak=Add(peak,Add(arena.RetainedBytes(),NativeAudioSnapshotRetainedBytes(result)));
    domain->output->RequireReady();
    if(!domain->output->MixerAttached()||domain->output->ConsumerEpoch()!=domain->consumerEpoch||
        !persistentNativeClient.MatchesLifetime(domain->output->ClientLifetime()))
        throw std::runtime_error("Quest Audio actual owner retired before provider initialization publication");
    arena.Validate();const auto nameGrowth=arena.CommittedGrowthBytes();
    const auto nextCredit=Add(nativeAudioNameArenaCredit,nameGrowth);
    host.ValidateAdditionalPackageBudget(Add(peak,nameGrowth));
    // All throwing preparation is finished; same-allocator node transfers use
    // the fully reserved empty maps, exactly as the System sibling provider.
    arena.Commit();nativeAudioNameArenaCredit=nextCredit;
    base->defaultObject=domain->baseDefault;domain->cls->defaultObject=domain->defaults;
    nextQualified.merge(persistentQualifiedObjects);nextQualified.merge(deltaQualified);
    nextVm.merge(persistentVmObjects);nextVm.merge(deltaVm);
    if(!persistentQualifiedObjects.empty()||!deltaQualified.empty()||!persistentVmObjects.empty()||!deltaVm.empty())std::terminate();
    persistentQualifiedObjects.swap(nextQualified);persistentVmObjects.swap(nextVm);
    PublishRuntimeLookupObjects(lookup,publications);engine->nativeEngineInstance->values.audio=domain->instance;
    persistentNativeAudio=std::move(owner);return result;
}
QuestVr::Vm::Result SetPortableRuntimeQuestAudioViewport(std::uint64_t identity,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    return QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
        host.NativeAudioSetViewport(identity?NativeSessionViewport(identity):nullptr,execution);return QuestVr::Vm::Value::Bool(true);
    },limits);
}
QuestVr::Vm::Result UpdatePortableRuntimeQuestAudio(const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    return QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution&) {
        host.NativeAudioUpdate();return QuestVr::Vm::Value::Bool(true);
    },limits);
}
