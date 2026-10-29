// Native USystem Class/default/constructor/root prefix. No retail module is
// loaded. Typed configuration belongs to this GC domain, not per-UObject
// overlays. Unknown commands decline only after the actual provider exists.
namespace {
bool NativeSystemDefaultValid(const RuntimeObject* cls) {
    if(!persistentSystem||!persistentSystem->get()||!cls)return false;
    const auto& domain=*persistentSystem->get();
    const auto* expected=cls==domain.cls?domain.defaults:
        domain.integerDefaults&&cls==domain.integerDefaults->cls?domain.integerDefaults:nullptr;
    if(!expected||cls->defaultObject!=expected||!expected->preparedNativeDefaults||
        expected->cls!=cls||expected->defaultOwner!=cls||expected->outer||expected->lookupName||
        expected->reflection.flags||expected->runtimeBirth||!expected->objectIndex)return false;
    const auto& payload=*expected->preparedNativeDefaults;
    return payload.owner==cls&&cls->base&&payload.superDefault==cls->base->defaultObject&&
        payload.kind==(cls==domain.cls?RuntimeNativeDefaultKind::System:RuntimeNativeDefaultKind::IntProperty)&&
        payload.nativeSize==(cls==domain.cls?0x6cu:0x5cu)&&payload.classFlags==(cls==domain.cls?0x16u:0x12u);
}
bool ActualNativeSystemObjectValid(const RuntimeObject* object) {
    if(!persistentSystem||!persistentSystem->get()||!persistentObjectLookup||!persistentObjectLookup->active||!object||
        !object->cls||!object->objectIndex||!object->lookupName||object->lookupControl!=persistentObjectLookup||
        object->defaultOwner||object->runtimeBirth||!object->sourcePath.empty()||
        object->exportIndex!=std::numeric_limits<std::size_t>::max())return false;
    const auto& domain=*persistentSystem->get();
    if(object==domain.instance)return persistentEngineCollision&&persistentEngineCollision->get()&&domain.rooted&&domain.execInstalled&&object->cls==domain.cls&&
        object->outer==persistentEngineCollision->get()->transient&&!object->reflection.flags&&NativeSystemDefaultValid(domain.cls);
    if(std::find(domain.fields.begin(),domain.fields.end(),object)==domain.fields.end()||!object->nativeProperty||
        !PreparedNativeClassDefaultValid(object->cls)||object->reflection.flags!=4u)return false;
    return object->outer==domain.cls||object->outer==domain.fields[4u]||object->outer==domain.fields[6u];
}
bool ActualNativeSystemFieldValid(const RuntimeObject* object) {
    return ActualNativeSystemObjectValid(object)&&object->nativeProperty;
}
std::size_t NativeSystemRetainedBytes(const RuntimeNativeSystemDomain& domain) {
    using QuestVr::DecalStateDetail::Add;
    auto bytes=sizeof(domain)+sizeof(GCAllocation)+sizeof(GCRoot<RuntimeNativeSystemDomain>)+sizeof(GCRootNode)+domain.iniPath.capacity()+1u;
    // The two typed payload headers are inline in sizeof(domain), not two
    // additional allocations. Their owned vector/string storage is separate.
    bytes=Add(bytes,QuestVr::SystemConfig::RetainedBytes(domain.defaultValues)-sizeof(domain.defaultValues));
    bytes=Add(bytes,QuestVr::SystemConfig::RetainedBytes(domain.instanceValues)-sizeof(domain.instanceValues));
    for(auto* object:{domain.cls,domain.defaults,domain.integerDefaults,domain.instance}) {
        if(!object)continue; // Detached constructor staging has no instance yet.
        auto storage=NativeSessionObjectBytes(object);
        // Session helper includes two named namespace nodes. These actual
        // unnamed CDOs own no canonical/folded lookup aliases.
        if(object->defaultOwner)storage-=2u*(sizeof(std::string)+6u*sizeof(void*)+object->reflection.objectPath.capacity()+1u);
        bytes=Add(bytes,storage);
        if(object->preparedNativeDefaults)bytes=Add(bytes,sizeof(RuntimePreparedNativeDefaults));
    }
    for(auto* field:domain.fields) {
        if(!field)continue; // Detached field construction is metered incrementally.
        bytes=Add(bytes,NativeSessionObjectBytes(field));
        if(!field->nativeProperty)throw std::runtime_error("System accounting lacks typed native property storage");
        bytes=Add(bytes,sizeof(RuntimeNativeProperty)+field->nativeProperty->category.capacity()+1u);
    }
    return bytes;
}
PortableSystemServicesSnapshot SystemServicesSnapshot(const RuntimeNativeSystemDomain& domain,const NameStringArenaStage* arena=nullptr) {
    if(!domain.cls||!domain.defaults||!domain.integerDefaults||!domain.instance||!domain.cls->base||!domain.cls->nativeClassWithin||
        !domain.instance->outer||!domain.rooted||!domain.execInstalled||domain.iniPath.empty())
        throw std::runtime_error("System constructor/root domain is incomplete");
    PortableSystemServicesSnapshot result;
    const std::array<RuntimeObject*,6u> own{{domain.fields[6u],domain.fields[4u],domain.fields[3u],domain.fields[2u],domain.fields[1u],domain.fields[0u]}};
    if(domain.cls->nativeChildren!=own.front()||!domain.cls->nativeConstructorLinksPrepared)
        throw std::runtime_error("System Class Children/linked state differs from StaticConstructor");
    for(std::size_t list=0u;list<3u;++list) {
        auto* cursor=domain.cls->nativePropertyHeads[list];
        for(auto* field:own) {
            if(!field||!field->nativeProperty)throw std::runtime_error("System linked property is missing");
            if(list==2u&&!(field->nativeProperty->flags&0x400000u))continue;
            if(cursor!=field)throw std::runtime_error("System native Property/Config/Constructor list order differs from original Link");
            const auto& p=*field->nativeProperty;
            const std::array<RuntimeObject*,3u> mirrors{{p.propertyNext,p.configNext,p.constructorNext}};
            if(mirrors[list]!=cursor->nativePropertyNext[list])throw std::runtime_error("System native linked-property mirrors differ");
            cursor=cursor->nativePropertyNext[list];
        }
        if(cursor!=domain.cls->base->nativePropertyHeads[list])throw std::runtime_error("System property list lost its actual inherited tail");
    }
    result.prepared=true;result.rooted=domain.rooted;result.execInstalled=domain.execInstalled;result.constructorLinksVerified=true;
    result.classPath=domain.cls->reflection.objectPath;result.objectPath=domain.instance->reflection.objectPath;
    result.outerPath=domain.instance->outer->reflection.objectPath;result.configPath=domain.iniPath;
    result.classIndex=domain.cls->objectIndex.Index();result.objectIndex=domain.instance->objectIndex.Index();
    result.outerIndex=domain.instance->outer->objectIndex.Index();result.objectFlags=domain.instance->reflection.flags;
    result.classFlags=NativeSystemClass.registrationClassFlags;result.nativeSize=NativeSystemClass.nativeSize;
    result.superIndex=domain.cls->base->objectIndex.Index();result.withinIndex=domain.cls->nativeClassWithin->objectIndex.Index();
    const auto& values=domain.instanceValues;
    result.purgeCacheDays=values.purgeCacheDays;result.savePath=values.savePath;result.cachePath=values.cachePath;result.cacheExt=values.cacheExt;
    const auto spelling=[&](const NameString& name)->const std::string& {return arena?arena->Spelling(name):name.ToString();};
    result.paths=values.paths;for(const auto& name:values.suppress)result.suppress.push_back(spelling(name));
    result.defaultPaths=domain.defaultValues.paths;
    for(const auto& name:domain.defaultValues.suppress)result.defaultSuppress.push_back(spelling(name));
    result.fields.reserve(domain.fields.size());
    for(auto* field:domain.fields) {
        if(!field||!field->nativeProperty||!field->cls||!field->outer||!field->lookupName||!field->objectIndex)
            throw std::runtime_error("System native field graph is incomplete");
        const auto& p=*field->nativeProperty;
        result.fields.push_back({field->reflection.objectPath,field->cls->reflection.objectPath,field->outer->reflection.objectPath,
            NativeObjectPath(p.next),NativeObjectPath(p.inner),p.category,field->objectIndex.Index(),field->cls->objectIndex.Index(),
            field->outer->objectIndex.Index(),field->reflection.flags,p.flags,p.dimension,p.elementSize,p.offset});
    }
    return result;
}
}
PortableSystemServicesSnapshot ReadPortableRuntimeSystemServices() {
    if(!persistentSystem)return {};
    const auto* domain=persistentSystem->get();
    if(!domain||!domain->integerDefaults||!NativeSystemDefaultValid(domain->cls)||!NativeSystemDefaultValid(domain->integerDefaults->cls)||
        !ActualNativeSystemObjectValid(domain->instance))throw std::runtime_error("System rooted Class/default/instance authority was lost");
    PortableActorVmHost host;
    if(host.Object(domain->cls->reflection.objectPath)!=domain->cls)throw std::runtime_error("System Class namespace authority was lost");
    for(auto* field:domain->fields)if(!ActualNativeSystemFieldValid(field)||host.Object(field->reflection.objectPath)!=field)
        throw std::runtime_error("System field lookup/lifetime authority was lost");
    return SystemServicesSnapshot(*domain);
}
PortableSystemServicesSnapshot PreparePortableRuntimeSystemServices(const std::string& engineIni) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(engineIni.empty()||engineIni.size()>8192u||engineIni.find('\0')!=std::string::npos)
        throw std::runtime_error("System requires an explicit bounded engine INI source");
    const auto supplied=std::filesystem::absolute(engineIni).lexically_normal().string();
    if(persistentSystem) {
        if(supplied!=persistentSystem->get()->iniPath)throw std::runtime_error("System cannot replace its committed config source");
        return ReadPortableRuntimeSystemServices();
    }
    if(!persistentRuntime||!persistentRuntime->get()||!persistentEngineCollision||!persistentInputSchema||
        !ReadPortableRuntimeInputSchema().prepared||!ReadPortableRuntimeObjectLookup().prepared)
        throw std::runtime_error("System requires actual Engine collision, Input schema/default and lookup prerequisites");
    if(supplied!=persistentEngineCollision->get()->iniPath)throw std::runtime_error("System source differs from the engine's bound config source");
    host.ValidateAdditionalPackageBudget(3u*QuestVr::NativeConfig::Limits{}.fileBytes+QuestVr::NativeConfig::Limits{}.retainedBytes);
    const auto ini=QuestVr::NativeConfig::ReadOrderedIni(supplied);
    if(!ini)throw std::runtime_error("System supplied config source is missing");
    std::size_t nameReserve{};
    {
        host.ValidateAdditionalPackageBudget(2u*ini->RetainedBytes()+4096u);
        const auto names=ini->StringArray("Core.System","Suppress");
        // Only the actual Suppress entries can add config FNames. The32-row
        // spelling allowance covers every own object/category/unique name.
        // Unrelated renderer/audio/driver INI entries need no Names capacity.
        nameReserve=2u*((names?names->size():0u)+32u);
    }
    auto* meta=host.Object("Core.Class");auto* subsystem=host.Object("Core.Subsystem");auto* objectClass=host.Object("Core.Object");
    auto* propertyClass=host.Object("Core.Property");auto* intClass=host.Object("Core.IntProperty");
    auto* strClass=host.Object("Core.StrProperty");auto* arrayClass=host.Object("Core.ArrayProperty");auto* nameClass=host.Object("Core.NameProperty");
    for(auto* cls:{subsystem,propertyClass,strClass,arrayClass,nameClass})if(!PreparedNativeClassDefaultValid(cls))
        throw std::runtime_error("System shared native default prerequisite is not prepared");
    if(intClass->base!=propertyClass||intClass->defaultObject||!intClass->nativeMetadataClass||
        intClass->nativeMetadataClass->nativeSize!=0x5cu||intClass->nativeMetadataClass->registrationClassFlags!=0x12u)
        throw std::runtime_error("System IntProperty CDO baseline is occupied or unauthenticated");
    if(persistentQualifiedObjects.size()>1'000'000u-10u||persistentVmObjects.size()>1'000'000u-10u)
        throw std::runtime_error("System exceeds actual object namespace bounds");
    pendingBirthGarbage=true;
    GCRoot<RuntimeNativeSystemDomain> staged(GC::Alloc<RuntimeNativeSystemDomain>());auto* domain=staged.get();domain->iniPath=supplied;
    std::vector<GCRoot<RuntimeObject>> allocations;allocations.reserve(12u);
    std::vector<RuntimeObject*> publications;publications.reserve(10u);
    decltype(persistentQualifiedObjects) deltaQualified;decltype(persistentVmObjects) deltaVm;
    std::size_t payloadPeak{},nameWork{};
    NameStringArenaStage arena(NameStringArenaStage::Work{
        [&](std::size_t units){if(units>(16u<<20u)-nameWork)throw std::runtime_error("System FName staging work budget exhausted");nameWork+=units;},
        [&](std::size_t bytes){host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(bytes,
            NativeSystemRetainedBytes(*domain)+ini->RetainedBytes()+payloadPeak+
            allocations.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+publications.capacity()*sizeof(RuntimeObject*)+
            (deltaQualified.bucket_count()+deltaVm.bucket_count())*sizeof(void*)));}
    },nameReserve);
    const auto occupied=[&](const std::string& path){return persistentVmObjects.count(LowerAscii(path))||deltaVm.count(LowerAscii(path));};
    const auto admitConstruction=[&](std::size_t extra) {
        host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(extra,
            NativeSystemRetainedBytes(*domain)+ini->RetainedBytes()+payloadPeak+arena.RetainedBytes()+
            allocations.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+publications.capacity()*sizeof(RuntimeObject*)+
            (deltaQualified.bucket_count()+deltaVm.bucket_count())*sizeof(void*)));
    };
    const auto allocate=[&](const std::string& path,const std::string& type,RuntimeObject* cls,RuntimeObject* outer,std::uint32_t flags) {
        if(path.empty()||occupied(path)||persistentQualifiedObjects.count(path)||IsContextExcludedPath(path))
            throw std::runtime_error("System allocation collides with actual/context-excluded object identity");
        admitConstruction(sizeof(RuntimeObject)+sizeof(GCAllocation)+sizeof(GCRootNode)+sizeof(RuntimeNativeProperty)+
            16u*(path.size()+type.size()+NativeObjectPath(outer).size()+256u));
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{path,type,NativeObjectPath(outer),{},flags,0},nullptr));
        auto* result=allocations.back().get();result->cls=cls;result->outer=outer;result->exportIndex=std::numeric_limits<std::size_t>::max();
        result->lookupName=arena.Add(path.substr(path.find_last_of('.')+1u));
        if(!deltaQualified.emplace(path,result).second||!deltaVm.emplace(LowerAscii(path),result).second)
            throw std::runtime_error("System delta namespace has conflicting canonical/folded identity");
        publications.push_back(result);return result;
    };
    domain->cls=allocate("Core.System","Class",meta,RegisteredNativePackage("Core"),NativeSystemClass.registrationObjectFlags);
    auto* cls=domain->cls;cls->nativeMetadataClass=&NativeSystemClass;cls->base=subsystem;cls->nativeClassWithin=objectClass;
    cls->reflection.basePath="Core.Subsystem";
    const auto defaults=[&](RuntimeObject* owner,RuntimeNativeDefaultKind kind,std::uint32_t size,std::uint32_t flags) {
        auto* parent=owner->base->defaultObject;
        if(!parent||!parent->preparedNativeDefaults)throw std::runtime_error("System CDO lost actual superclass-copy authority");
        const auto& inherited=*parent->preparedNativeDefaults;
        if(inherited.nativeSize>size||inherited.propertyDimension||inherited.propertyElementSize||inherited.propertyFlags||
            inherited.propertyOffset||inherited.structPropertiesSize||inherited.input)
            throw std::runtime_error("System superclass CDO has a foreign native extension baseline");
        admitConstruction(sizeof(RuntimeObject)+sizeof(GCAllocation)+sizeof(GCRootNode)+sizeof(RuntimePreparedNativeDefaults)+
            16u*(owner->reflection.objectPath.size()+256u));
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{"@ClassDefault:"+owner->reflection.objectPath,
            arena.Spelling(*owner->lookupName),{},{},0u,0},nullptr));
        auto* result=allocations.back().get();result->cls=owner;result->defaultOwner=owner;result->exportIndex=std::numeric_limits<std::size_t>::max();
        auto payload=std::make_unique<RuntimePreparedNativeDefaults>();payload->kind=kind;payload->owner=owner;payload->superDefault=parent;
        payload->nativeSize=size;payload->classFlags=flags;result->preparedNativeDefaults=std::move(payload);return result;
    };
    domain->integerDefaults=defaults(intClass,RuntimeNativeDefaultKind::IntProperty,0x5cu,0x12u);
    domain->defaults=defaults(cls,RuntimeNativeDefaultKind::System,0x6cu,0x16u);
    const auto field=[&](const char* name,RuntimeObject* type,RuntimeObject* outer,std::uint32_t offset,std::uint32_t element,std::uint32_t flags,const char* category) {
        auto* value=allocate(outer->reflection.objectPath+'.'+name,arena.Spelling(*type->lookupName),type,outer,4u);
        value->nativeProperty=std::make_unique<RuntimeNativeProperty>();value->linkedClassOwner=outer;value->nativeConstructorLinksPrepared=true;
        auto& p=*value->nativeProperty;p.dimension=1u;p.elementSize=element;p.offset=offset;p.flags=flags;p.category=arena.Spelling(arena.Add(category));return value;
    };
    auto& f=domain->fields;
    f[0u]=field("PurgeCacheDays",intClass,cls,0x2cu,4u,0x4000u,"Options");
    f[1u]=field("SavePath",strClass,cls,0x30u,12u,0x404000u,"Options");
    f[2u]=field("CachePath",strClass,cls,0x3cu,12u,0x404000u,"Options");
    f[3u]=field("CacheExt",strClass,cls,0x48u,12u,0x404000u,"Options");
    f[4u]=field("Paths",arrayClass,cls,0x54u,12u,0x404000u,"Options");
    f[5u]=field("StrProperty0",strClass,f[4u],0u,12u,0x400000u,"None");
    f[6u]=field("Suppress",arrayClass,cls,0x60u,12u,0x404000u,"Options");
    f[7u]=field("NameProperty0",nameClass,f[6u],0u,4u,0u,"None");
    f[4u]->nativeProperty->inner=f[5u];f[6u]->nativeProperty->inner=f[7u];
    const std::array<RuntimeObject*,6u> own{{f[6u],f[4u],f[3u],f[2u],f[1u],f[0u]}};
    cls->nativeChildren=own.front();cls->nativeConstructorLinksPrepared=true;
    for(std::size_t i=0u;i<own.size();++i)own[i]->nativeProperty->next=i+1u<own.size()?own[i+1u]:nullptr;
    for(std::size_t list=0u;list<3u;++list) {
        RuntimeObject* previous{};
        for(auto* value:own) {
            if(list==2u&&!(value->nativeProperty->flags&0x400000u))continue;
            if(previous)previous->nativePropertyNext[list]=value;else cls->nativePropertyHeads[list]=value;
            previous=value;
        }
        if(previous)previous->nativePropertyNext[list]=subsystem->nativePropertyHeads[list];
        else cls->nativePropertyHeads[list]=subsystem->nativePropertyHeads[list];
    }
    for(auto* value:f) {
        auto& p=*value->nativeProperty;p.propertyNext=value->nativePropertyNext[0u];p.configNext=value->nativePropertyNext[1u];p.constructorNext=value->nativePropertyNext[2u];
    }
    struct Import {
        using Name=NameString;PortableActorVmHost& host;RuntimeNativeSystemDomain& domain;NameStringArenaStage& arena;
        std::size_t& payloadPeak;std::size_t& work;std::size_t sourceBytes{};
        void Step(){if(++work>(16u<<20u))throw std::runtime_error("System config work budget exhausted");}
        void Admit(std::size_t bytes){payloadPeak=std::max(payloadPeak,bytes);host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(payloadPeak,
            NativeSystemRetainedBytes(domain)+sourceBytes+arena.RetainedBytes()));}
        Name AddName(std::u16string_view text){return arena.Add(QuestVr::UnrealString::Encode(text));}
    } import{host,*domain,arena,payloadPeak,nameWork,ini->RetainedBytes()};
    domain->defaultValues=QuestVr::SystemConfig::Load(*ini,import);
    host.ValidateAdditionalPackageBudget(NativeSystemRetainedBytes(*domain)+ini->RetainedBytes()+QuestVr::SystemConfig::RetainedBytes(domain->defaultValues)+arena.RetainedBytes());
    domain->instanceValues=domain->defaultValues;
    auto counter=cls->uniqueObjectCounter;
    const auto name=QuestVr::ObjectNames::MakeUnique("System",counter,[&](std::string_view candidate){
        return occupied(persistentEngineCollision->get()->transient->reflection.objectPath+'.'+std::string(candidate));});
    domain->instance=allocate(persistentEngineCollision->get()->transient->reflection.objectPath+'.'+name,"System",cls,
        persistentEngineCollision->get()->transient,0u);
    domain->rooted=true;domain->execInstalled=true;
    auto owner=std::make_unique<GCRoot<RuntimeNativeSystemDomain>>(domain);
    auto lookup=StageRuntimeLookupObjects(publications);
    if(!lookup)throw std::runtime_error("System actual lookup staging is unavailable");
    // Admit snapshot and replacement buckets BEFORE their allocations. The
    // post-allocation exact check below is an additional verification, not the
    // first permission to allocate a whole-namespace replacement.
    const auto namespaceBuckets=4u*(persistentQualifiedObjects.size()+persistentVmObjects.size()+publications.size()+2u)*sizeof(void*);
    const auto snapshotPeak=4u*NativeSystemRetainedBytes(*domain)+namespaceBuckets+ini->RetainedBytes()+arena.RetainedBytes();
    host.ValidateAdditionalPackageBudget(snapshotPeak+lookup->RetainedBytes());
    auto result=SystemServicesSnapshot(*domain,&arena);
    decltype(persistentQualifiedObjects) nextQualified;decltype(persistentVmObjects) nextVm;
    nextQualified.max_load_factor(persistentQualifiedObjects.max_load_factor());nextVm.max_load_factor(persistentVmObjects.max_load_factor());
    nextQualified.reserve(persistentQualifiedObjects.size()+deltaQualified.size());nextVm.reserve(persistentVmObjects.size()+deltaVm.size());
    using QuestVr::DecalStateDetail::Add;
    auto peak=Add(NativeSystemRetainedBytes(*domain),lookup->RetainedBytes());
    peak=Add(peak,allocations.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+publications.capacity()*sizeof(RuntimeObject*));
    peak=Add(peak,(persistentQualifiedObjects.bucket_count()+persistentVmObjects.bucket_count()+nextQualified.bucket_count()+nextVm.bucket_count()+
        deltaQualified.bucket_count()+deltaVm.bucket_count())*sizeof(void*));
    peak=Add(peak,ini->RetainedBytes()+sizeof(result)+arena.RetainedBytes());
    for(const auto* text:{&result.classPath,&result.objectPath,&result.outerPath,&result.configPath,&result.savePath,&result.cachePath,&result.cacheExt})
        peak=Add(peak,text->capacity()+1u);
    for(const auto* strings:{&result.paths,&result.suppress,&result.defaultPaths,&result.defaultSuppress}) {
        peak=Add(peak,strings->capacity()*sizeof(std::string));
        for(const auto& text:*strings)peak=Add(peak,text.capacity()+1u);
    }
    for(const auto& row:result.fields)for(const auto* text:{&row.path,&row.classPath,&row.outerPath,&row.nextPath,&row.innerPath,&row.category})
        peak=Add(peak,text->capacity()+1u);
    peak=Add(peak,result.fields.capacity()*sizeof(PortableSystemField));
    arena.Validate();
    const auto nameGrowth=arena.CommittedGrowthBytes();
    const auto nextCredit=Add(systemNameArenaCredit,nameGrowth);
    host.ValidateAdditionalPackageBudget(Add(peak,nameGrowth));
    // No allocation/throwing callback after publishing pre-existing Class state.
    arena.Commit();systemNameArenaCredit=nextCredit;domain->nameBytes=nameGrowth;
    intClass->defaultObject=domain->integerDefaults;cls->defaultObject=domain->defaults;cls->uniqueObjectCounter=counter;
    nextQualified.merge(persistentQualifiedObjects);nextQualified.merge(deltaQualified);nextVm.merge(persistentVmObjects);nextVm.merge(deltaVm);
    if(!persistentQualifiedObjects.empty()||!deltaQualified.empty()||!persistentVmObjects.empty()||!deltaVm.empty())std::terminate();
    persistentQualifiedObjects.swap(nextQualified);persistentVmObjects.swap(nextVm);
    PublishRuntimeLookupObjects(lookup,publications);persistentSystem=std::move(owner);return result;
}
PortableSystemCommandResult ExecutePortableRuntimeSystemCommand(const std::string& command,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableSystemCommandResult result;
    try {
        if(!persistentSystem||!persistentSystem->get())throw std::runtime_error("System command requires its prepared actual native provider");
        PortableActorVmHost host;
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution&) {
            return QuestVr::Vm::Value::Bool(host.SystemCommandExec(command,result.handled));
        },limits);
    }catch(const std::exception& error){result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();}
    return result;
}
