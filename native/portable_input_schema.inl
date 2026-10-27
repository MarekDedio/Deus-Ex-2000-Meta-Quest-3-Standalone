// Included after portable_engine_collision.inl. This is the real native
// StaticInitInput field/link/pre-config-default graph, not a serialized schema
// fixture or an Input instance. See ORIGINAL-INPUT-NATIVE-FIELDS.md.
namespace {
struct InputSchemaDefaultProfile {
    const char *path,*base;
    RuntimeNativeDefaultKind kind;
    std::uint32_t size,flags;
};
constexpr std::array<InputSchemaDefaultProfile,5u> InputSchemaDefaultProfiles{{
    {"Core.Struct","Core.Field",RuntimeNativeDefaultKind::Struct,0x6cu,0x12u},
    {"Core.NameProperty","Core.Property",RuntimeNativeDefaultKind::NameProperty,0x5cu,0x12u},
    {"Core.StructProperty","Core.Property",RuntimeNativeDefaultKind::StructProperty,0x64u,0x12u},
    {"Engine.Input","Core.Subsystem",RuntimeNativeDefaultKind::Input,0xfb0u,0x1eu},
    {"Extension.InputExt","Engine.Input",RuntimeNativeDefaultKind::InputExt,0xfb0u,0x1eu}
}};
struct InputSchemaLinkPlan {
    RuntimeObject *owner{},*children{};
    std::array<RuntimeObject*,3u> heads{};
};
using InputSchemaLinkPlans=std::array<InputSchemaLinkPlan,3u>;

// All five CDOs are new. Their typed payloads can be attached while detached;
// only Class.defaultObject/Children/heads and the unique-name counter need a
// staged view. Nothing calls a retail DLL or an instance InternalConstructor.
PortableInputSchemaSnapshot InputSchemaSnapshot(const RuntimeNativeInputDomain& domain,
    const InputSchemaLinkPlans* staged=nullptr,const std::int32_t* stagedCounter=nullptr) {
    if(domain.defaults.size()!=5u||domain.defaultClasses.size()!=5u||domain.fields.size()!=258u||
        !domain.inputClass||!domain.inputExtClass||!domain.keyEnum||!domain.aliasStruct||
        domain.defaultClasses[3u]!=domain.inputClass||domain.defaultClasses[4u]!=domain.inputExtClass)
        throw std::runtime_error("Native Input schema graph is incomplete");
    PortableActorVmHost host;
    const auto enumeration=ReadPortableRuntimeEnum("Engine.Actor.EInputKey");
    if(domain.keyEnum!=host.Object(enumeration.objectPath)||enumeration.outerPath!="Engine.Actor"||
        enumeration.names.size()!=255u||enumeration.names.front().ToString()!="IK_None"||
        enumeration.names.back().ToString()!="IK_OEMClear"||domain.keyEnum->objectIndex.Index()!=enumeration.objectIndex)
        throw std::runtime_error("Native Input schema lost its actual Actor EInputKey authority");
    const auto* alias=domain.aliasStruct;
    if(!alias->nativeStruct||alias->nativeProperty||alias->defaultOwner||alias->classDescriptor||alias->structDescriptor||
        alias->cls!=domain.defaultClasses[0u]||alias->outer!=domain.inputClass||alias->reflection.objectPath!="Engine.Input.Alias"||
        alias->reflection.flags||!alias->lookupName||alias->lookupName->ToString()!="Alias"||
        alias->nativeStruct->friendlyName!=*alias->lookupName||alias->nativeStruct->propertiesSize!=0x10u||alias->nativeStruct->super||
        !alias->sourcePath.empty()||alias->exportIndex!=std::numeric_limits<std::size_t>::max()||!alias->objectIndex)
        throw std::runtime_error("Native Input Alias is not the audited genuine native UStruct instance");
    std::set<std::uint32_t> indices;
    indices.insert(alias->objectIndex.Index());
    PortableInputSchemaSnapshot result;
    result.prepared=true;result.defaultsPrepared=true;result.configApplied=false;
    result.keyEnumPath=enumeration.objectPath;result.keyEnumIndex=enumeration.objectIndex;
    result.retainedBytes=InputSchemaRetainedBytes(domain);
    auto* stringClass=host.Object("Core.StrProperty");
    result.strPropertyUniqueCounter=stagedCounter?*stagedCounter:stringClass->uniqueObjectCounter;
    result.aliasStruct={alias->reflection.objectPath,alias->cls->reflection.objectPath,alias->outer->reflection.objectPath,
        alias->nativeStruct->friendlyName.ToString(),NativeObjectPath(alias->nativeStruct->super),alias->objectIndex.Index(),
        alias->cls->objectIndex.Index(),alias->outer->objectIndex.Index(),alias->reflection.flags,alias->nativeStruct->propertiesSize};
    result.defaults.reserve(5u);result.fields.reserve(258u);result.links.reserve(3u);
    for(std::size_t i=0u;i<InputSchemaDefaultProfiles.size();++i) {
        const auto& profile=InputSchemaDefaultProfiles[i];auto* cls=domain.defaultClasses[i];auto* object=domain.defaults[i];
        RuntimeObject* superDefault=cls&&cls->base?cls->base->defaultObject:nullptr;
        if(i==4u)superDefault=domain.defaults[3u];
        if(!cls||!object||!object->preparedNativeDefaults||host.Object(profile.path)!=cls||cls->reflection.objectPath!=profile.path||
            NativeObjectPath(cls->base)!=profile.base||cls->cls!=host.Object("Core.Class")||!cls->nativeMetadataClass||
            cls->nativeMetadataClass->nativeSize!=profile.size||cls->nativeMetadataClass->registrationClassFlags!=profile.flags||
            object->cls!=cls||object->defaultOwner!=cls||object->outer||object->lookupName||object->reflection.flags||
            object->runtimeBirth||object->nativeProperty||object->nativeStruct||!object->sourcePath.empty()||
            object->exportIndex!=std::numeric_limits<std::size_t>::max()||!object->objectIndex||
            !indices.insert(object->objectIndex.Index()).second||(!staged&&cls->defaultObject!=object)||
            (staged&&cls->defaultObject))
            throw std::runtime_error("Native Input pre-config default Class/header authority is invalid");
        const auto& payload=*object->preparedNativeDefaults;
        if(payload.kind!=profile.kind||payload.owner!=cls||payload.superDefault!=superDefault||!superDefault||
            payload.nativeSize!=profile.size||payload.classFlags!=profile.flags||payload.propertyDimension||payload.propertyElementSize||
            payload.propertyFlags||payload.propertyOffset||payload.structPropertiesSize||!payload.structFriendlyName.IsNone()||
            payload.primitiveBoundsValid||std::any_of(payload.primitiveBounds.begin(),payload.primitiveBounds.end(),[](float v){return v!=0.0f;})||
            bool(payload.input)!=(i>=3u))
            throw std::runtime_error("Native Input default violates its typed superclass-copy/zero-extension profile");
        PortableInputDefaultSnapshot snapshot;
        snapshot.classPath=profile.path;snapshot.superDefaultPath=superDefault->reflection.objectPath;
        snapshot.friendlyName=payload.structFriendlyName.ToString();snapshot.classIndex=cls->objectIndex.Index();
        snapshot.defaultIndex=object->objectIndex.Index();snapshot.superDefaultIndex=superDefault->objectIndex.Index();
        snapshot.nativeSize=payload.nativeSize;snapshot.classFlags=payload.classFlags;
        snapshot.propertyDimension=payload.propertyDimension;snapshot.propertyElementSize=payload.propertyElementSize;
        snapshot.propertyFlags=payload.propertyFlags;snapshot.propertyOffset=payload.propertyOffset;snapshot.structPropertiesSize=payload.structPropertiesSize;
        snapshot.hasInputValues=bool(payload.input);
        if(payload.input) {
            const auto& input=*payload.input;
            if(input.viewport||input.keyEnum||input.execInstalled)
                throw std::runtime_error("Pre-config Input defaults contain invented instance Enum/Viewport/FExec state");
            snapshot.execInstalled=input.execInstalled;snapshot.viewportIndex=input.viewport?input.viewport->objectIndex.Index():0u;
            snapshot.enumIndex=input.keyEnum?input.keyEnum->objectIndex.Index():0u;
            for(std::size_t slot=0u;slot<input.aliases.size();++slot) {
                const auto& value=input.aliases[slot];
                if(!value.alias.IsNone()||!value.command.empty())throw std::runtime_error("Pre-config Alias defaults were changed or imported");
                snapshot.aliases[slot]={value.alias.ToString(),value.command,value.alias.GetCompareIndex()};
            }
            for(std::size_t slot=0u;slot<input.bindings.size();++slot) {
                if(!input.bindings[slot].empty())throw std::runtime_error("Pre-config Input bindings were changed or imported");
                snapshot.bindings[slot]=input.bindings[slot];
            }
        }
        result.defaults.push_back(std::move(snapshot));
    }
    for(std::size_t i=0u;i<domain.fields.size();++i) {
        auto* field=domain.fields[i];
        if(!field||!field->nativeProperty||field->nativeStruct||field->property||field->defaultOwner||field->runtimeBirth||
            !field->cls||!field->outer||!field->lookupName||field->reflection.flags!=4u||!field->sourcePath.empty()||
            field->exportIndex!=std::numeric_limits<std::size_t>::max()||!field->objectIndex||
            !indices.insert(field->objectIndex.Index()).second||field->linkedClassOwner!=field->outer||
            !field->nativeConstructorLinksPrepared)
            throw std::runtime_error("Native Input field is not a distinct genuine native UProperty");
        const auto& p=*field->nativeProperty;
        auto* expectedClass=i==0u?domain.defaultClasses[1u]:i==2u?domain.defaultClasses[2u]:stringClass;
        auto* expectedOuter=i<2u?domain.aliasStruct:domain.inputClass;
        const std::uint32_t dimension=i==2u?40u:1u,element=i==0u?4u:i==2u?16u:12u;
        const std::uint32_t offset=i==0u?0u:i==1u?4u:i==2u?0x2cu:0x2acu+12u*static_cast<std::uint32_t>(i-3u);
        const std::uint32_t flags=i==0u?0x4000u:0x404000u;
        const std::string category=i<2u?"None":i==2u?"Aliases":"RawKeys";
        if(field->cls!=expectedClass||field->outer!=expectedOuter||p.dimension!=dimension||p.elementSize!=element||p.offset!=offset||
            p.flags!=flags||p.category!=category||p.inner||p.referencedStruct!=(i==2u?domain.aliasStruct:nullptr))
            throw std::runtime_error("Native Input linked property type/dimension/offset/flags/Struct reference differs from StaticInitInput");
        const auto categoryIdentity=NameString::FindCompareIndex(p.category);
        if(!categoryIdentity||((*categoryIdentity==0)!=(p.category=="None")))
            throw std::runtime_error("Native Input Property category lacks its actual previously constructed FName identity");
        if(i==0u&&field->lookupName->ToString()!="Alias")throw std::runtime_error("Alias NameProperty identity changed");
        if(i==1u&&field->lookupName->ToString()!="Command")throw std::runtime_error("Alias Command StrProperty identity changed");
        if(i==2u&&field->lookupName->ToString()!="Aliases")throw std::runtime_error("Aliases fixed StructProperty identity changed");
        if(i>=3u) {
            const auto units=QuestVr::UnrealString::Decode(enumeration.names[i-3u].ToString());
            if(enumeration.names[i-3u].IsNone()||units.size()<3u)throw std::runtime_error("Native Input source key FName is unsupported");
            const auto requested=NameString::FindCompareIndex(QuestVr::UnrealString::Encode(std::u16string_view(units).substr(3u)));
            if(!requested)throw std::runtime_error("Prepared raw-key suffix has no previously created actual FName identity");
            if(*requested!=0&&field->lookupName->GetCompareIndex()!=*requested)throw std::runtime_error("Native raw-key field lost the actual Enum suffix identity");
            if(*requested==0&&field->lookupName->IsNone())throw std::runtime_error("NAME_None raw-key allocation skipped actual unique naming");
        }
        if(field->reflection.objectPath!=expectedOuter->reflection.objectPath+'.'+field->lookupName->ToString()||
            field->reflection.outerPath!=expectedOuter->reflection.objectPath||
            p.propertyNext!=field->nativePropertyNext[0u]||p.configNext!=field->nativePropertyNext[1u]||
            p.constructorNext!=field->nativePropertyNext[2u])
            throw std::runtime_error("Native Input Property path/Outer/list mirrors disagree");
        PortableInputSchemaFieldSnapshot snapshot;
        snapshot.field={field->reflection.objectPath,field->cls->reflection.objectPath,field->outer->reflection.objectPath,
            NativeObjectPath(p.next),NativeObjectPath(p.inner),NativeObjectPath(p.propertyNext),NativeObjectPath(p.configNext),
            NativeObjectPath(p.constructorNext),p.category,field->objectIndex.Index(),field->cls->objectIndex.Index(),field->outer->objectIndex.Index(),
            field->reflection.flags,p.dimension,p.elementSize,p.flags,p.offset};
        snapshot.referencedStructPath=NativeObjectPath(p.referencedStruct);
        snapshot.referencedStructIndex=p.referencedStruct?p.referencedStruct->objectIndex.Index():0u;
        result.fields.push_back(std::move(snapshot));
    }
    const std::array<RuntimeObject*,3u> owners{domain.aliasStruct,domain.inputClass,domain.inputExtClass};
    for(std::size_t i=0u;i<owners.size();++i) {
        auto* owner=owners[i];auto* children=staged?(*staged)[i].children:owner->nativeChildren;
        const auto heads=staged?(*staged)[i].heads:owner->nativePropertyHeads;
        if((staged&&(*staged)[i].owner!=owner)||(!staged&&!owner->nativeConstructorLinksPrepared)||
            children!=(i==0u?domain.fields[1u]:i==1u?domain.fields.back():nullptr))
            throw std::runtime_error("Native Input Struct/Class Children ownership is invalid");
        PortableInputSchemaLinksSnapshot snapshot;
        snapshot.ownerPath=owner->reflection.objectPath;snapshot.ownerIndex=owner->objectIndex.Index();
        snapshot.childrenPath=NativeObjectPath(children);snapshot.constructorLinksPrepared=true;
        for(std::size_t list=0u;list<3u;++list) {
            snapshot.heads[list]=NativeObjectPath(heads[list]);
            std::vector<RuntimeObject*> expected;
            if(i==0u) {expected.push_back(domain.fields[1u]);if(list!=2u)expected.push_back(domain.fields[0u]);}
            else {
                for(std::size_t slot=domain.fields.size();slot>2u;--slot)expected.push_back(domain.fields[slot-1u]);
                auto* inherited=domain.inputClass->base->nativePropertyHeads[list];std::set<RuntimeObject*> seen;
                while(inherited) {
                    if(seen.size()>=8192u||!seen.insert(inherited).second)throw std::runtime_error("Input inherited native field lists cycle or exceed bounds");
                    expected.push_back(inherited);inherited=inherited->nativePropertyNext[list];
                }
            }
            auto* actual=heads[list];
            for(auto* expectedField:expected) {
                if(actual!=expectedField)throw std::runtime_error("Input native property/config/constructor list differs from original iterator order");
                actual=actual->nativePropertyNext[list];
            }
            if(actual)throw std::runtime_error("Input native linked list has extra fields");
        }
        result.links.push_back(std::move(snapshot));
    }
    if(domain.fields[1u]->nativeProperty->next!=domain.fields[0u]||domain.fields[0u]->nativeProperty->next||
        domain.fields[2u]->nativeProperty->next)
        throw std::runtime_error("Input Alias/native class own Next chains are invalid");
    for(std::size_t i=3u;i<domain.fields.size();++i)
        if(domain.fields[i]->nativeProperty->next!=domain.fields[i-1u])throw std::runtime_error("Input raw-key own Children/Next order is invalid");
    return result;
}
std::size_t InputSchemaSnapshotBytes(const PortableInputSchemaSnapshot& value) {
    using QuestVr::DecalStateDetail::Add;std::size_t bytes=sizeof(value)+value.keyEnumPath.capacity()+1u;
    bytes=Add(bytes,value.fields.capacity()*sizeof(PortableInputSchemaFieldSnapshot)+value.defaults.capacity()*sizeof(PortableInputDefaultSnapshot)+
        value.links.capacity()*sizeof(PortableInputSchemaLinksSnapshot));
    const auto text=[&](const std::string& v){bytes=Add(bytes,v.capacity()+1u);};
    for(const auto* v:{&value.aliasStruct.objectPath,&value.aliasStruct.classPath,&value.aliasStruct.outerPath,&value.aliasStruct.friendlyName,&value.aliasStruct.superPath})text(*v);
    for(const auto& row:value.fields) {
        for(const auto* v:{&row.field.objectPath,&row.field.classPath,&row.field.outerPath,&row.field.nextPath,&row.field.innerPath,
            &row.field.propertyNextPath,&row.field.configNextPath,&row.field.constructorNextPath,&row.field.category,&row.referencedStructPath})text(*v);
    }
    for(const auto& row:value.defaults) {
        text(row.classPath);text(row.superDefaultPath);text(row.friendlyName);
        for(const auto& alias:row.aliases) {text(alias.alias);text(alias.command);}for(const auto& binding:row.bindings)text(binding);
    }
    for(const auto& row:value.links) {text(row.ownerPath);text(row.childrenPath);for(const auto& head:row.heads)text(head);}
    return bytes;
}
} // namespace

PortableInputSchemaSnapshot ReadPortableRuntimeInputSchema() {
    if(!persistentInputSchema)return {};
    if(!persistentInputSchema->get()||!persistentEngineCollision||!persistentInputClasses||!persistentObjectLookup||!persistentObjectLookup->active)
        throw std::runtime_error("Native Input schema lost its actual prerequisite ownership");
    const auto& domain=*persistentInputSchema->get();PortableActorVmHost host;
    for(auto* cls:domain.defaultClasses)if(!InputSchemaDefaultValid(cls))throw std::runtime_error("Native Input schema lost an actual owning Class/default identity");
    for(auto* object:domain.fields)if(host.Object(object->reflection.objectPath)!=object||!ActualNativeInputSchemaObjectValid(object))
        throw std::runtime_error("Native Input schema Property lost actual lookup/Class/Outer lifetime");
    if(host.Object(domain.aliasStruct->reflection.objectPath)!=domain.aliasStruct||!ActualNativeInputSchemaObjectValid(domain.aliasStruct))
        throw std::runtime_error("Native Input Alias Struct lost actual lookup/Class/Outer lifetime");
    return InputSchemaSnapshot(domain);
}

PortableInputSchemaSnapshot PreparePortableRuntimeInputSchema() {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(persistentInputSchema)return ReadPortableRuntimeInputSchema();
    if(!persistentEngineCollision||!ReadPortableRuntimeEngineCollision().prepared||!ReadPortableRuntimeNativeInputClasses().prepared||
        !ReadPortableRuntimeObjectLookup().prepared)
        throw std::runtime_error("Native Input schema requires actual Input classes, lookup and Engine collision/default domains");
    auto* inputClass=host.Object("Engine.Input");auto* inputExtClass=host.Object("Extension.InputExt");auto* meta=host.Object("Core.Class");
    auto* structClass=host.Object("Core.Struct");auto* nameClass=host.Object("Core.NameProperty");auto* stringClass=host.Object("Core.StrProperty");
    auto* structPropertyClass=host.Object("Core.StructProperty");auto* propertyClass=host.Object("Core.Property");
    auto* fieldClass=host.Object("Core.Field");auto* subsystemClass=host.Object("Core.Subsystem");
    for(auto* cls:{fieldClass,propertyClass,subsystemClass,stringClass})
        if(!PreparedNativeClassDefaultValid(cls)||!cls->defaultObject->preparedNativeDefaults||!ActualLookupClass(*persistentObjectLookup,cls))
            throw std::runtime_error("Native Input schema lacks an authenticated prepared superclass/Property default");
    if(inputClass->nativeMetadataClass!=&QuestVr::NativeInputClasses[0u]||inputExtClass->nativeMetadataClass!=&QuestVr::NativeInputClasses[1u]||
        inputClass->base!=subsystemClass||inputExtClass->base!=inputClass||structClass->base!=fieldClass||
        nameClass->base!=propertyClass||structPropertyClass->base!=propertyClass||
        nameClass->nativeMetadataClass!=&EngineFieldClasses[1u]||nameClass->nativeClassWithin!=fieldClass)
        throw std::runtime_error("Native Input schema actual native class identities/Super/Within are invalid");
    const auto enumeration=ReadPortableRuntimeEnum("Engine.Actor.EInputKey");
    auto* keyEnum=host.Object("Engine.Actor.EInputKey");auto* actorClass=host.Object("Engine.Actor");auto* enumClass=host.Object("Core.Enum");
    if(enumeration.names.size()!=255u||enumeration.outerPath!="Engine.Actor"||keyEnum->outer!=actorClass||keyEnum->cls!=enumClass||
        enumeration.names.front().ToString()!="IK_None"||enumeration.names.back().ToString()!="IK_OEMClear")
        throw std::runtime_error("Native Input schema requires the complete actual Actor EInputKey enum");
    const auto foundEnum=FindPortableRuntimeObject({"EInputKey",enumClass->objectIndex.Index(),actorClass->objectIndex.Index(),false,false});
    if(!foundEnum||foundEnum->objectIndex!=keyEnum->objectIndex.Index())throw std::runtime_error("Input constructor Enum lookup has no actual UEnum/Actor-Outer identity");
    std::set<int> enumNames;
    for(const auto& name:enumeration.names) {
        if(name.IsNone()||!enumNames.insert(name.GetCompareIndex()).second||QuestVr::UnrealString::Decode(name.ToString()).size()<3u)
            throw std::runtime_error("Owned Input enum has missing/duplicate/short source Names outside its audited profile");
    }
    const std::array<RuntimeObject*,5u> defaultClasses{structClass,nameClass,structPropertyClass,inputClass,inputExtClass};
    for(std::size_t i=0u;i<defaultClasses.size();++i) {
        auto* cls=defaultClasses[i];const auto& profile=InputSchemaDefaultProfiles[i];
        if(!ActualLookupClass(*persistentObjectLookup,cls)||cls->cls!=meta||!cls->nativeMetadataClass||cls->classDescriptor||cls->defaultObject||
            cls->nativeMetadataClass->path!=profile.path||cls->nativeMetadataClass->nativeSize!=profile.size||
            cls->nativeMetadataClass->registrationClassFlags!=profile.flags||NativeObjectPath(cls->base)!=profile.base||
            !cls->classDefaultValues.empty()||cls->nativeChildren||cls->nativeConstructorLinksPrepared||
            std::any_of(cls->nativePropertyHeads.begin(),cls->nativePropertyHeads.end(),[](auto* p){return p!=nullptr;}))
            throw std::runtime_error("Native Input default/field Class baseline is already occupied or unaudited");
    }
    if(persistentQualifiedObjects.size()>1'000'000u-259u||persistentVmObjects.size()>1'000'000u-259u)
        throw std::runtime_error("Native Input schema exceeds its actual object namespace bound");
    pendingBirthGarbage=true;
    GCRoot<RuntimeNativeInputDomain> staged(GC::Alloc<RuntimeNativeInputDomain>());auto* domain=staged.get();
    domain->inputClass=inputClass;domain->inputExtClass=inputExtClass;domain->keyEnum=keyEnum;
    domain->defaultClasses.assign(defaultClasses.begin(),defaultClasses.end());domain->defaults.reserve(5u);domain->fields.reserve(258u);
    std::vector<GCRoot<RuntimeObject>> allocations;allocations.reserve(264u);
    std::vector<RuntimeObject*> publications;publications.reserve(259u);
    decltype(persistentQualifiedObjects) nextQualified;decltype(persistentVmObjects) nextVm;
    const auto indexed=[&](const std::string& path)->RuntimeObject* {
        const auto key=LowerAscii(path);const auto stagedObject=nextVm.find(key);
        if(stagedObject!=nextVm.end())return stagedObject->second;
        const auto live=persistentVmObjects.find(key);return live==persistentVmObjects.end()?nullptr:live->second;
    };
    const auto allocate=[&](const std::string& path,const std::string& type,RuntimeObject* cls,RuntimeObject* outer,const std::uint32_t flags) {
        if(path.empty()||indexed(path)||persistentQualifiedObjects.count(path)||nextQualified.count(path)||IsContextExcludedPath(path))
            throw std::runtime_error("Native Input schema name collides with an actual/context-excluded UObject");
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{path,type,NativeObjectPath(outer),{},flags,0},nullptr));
        auto* object=allocations.back().get();object->cls=cls;object->outer=outer;
        object->exportIndex=std::numeric_limits<std::size_t>::max();object->lookupName=NameString(path.substr(path.find_last_of('.')+1u));
        if(!nextQualified.emplace(path,object).second||!nextVm.emplace(LowerAscii(path),object).second)
            throw std::runtime_error("Native Input staged allocation has conflicting canonical/folded identity");
        publications.push_back(object);return object;
    };
    for(std::size_t i=0u;i<defaultClasses.size();++i) {
        auto* cls=defaultClasses[i];const auto& profile=InputSchemaDefaultProfiles[i];
        auto* superDefault=i==4u?domain->defaults[3u]:cls->base->defaultObject;
        if(!superDefault||!superDefault->preparedNativeDefaults)throw std::runtime_error("Native Input defaults are not in authenticated Super copy order");
        const auto& inherited=*superDefault->preparedNativeDefaults;
        if(inherited.owner!=cls->base||superDefault->cls!=cls->base||superDefault->defaultOwner!=cls->base||
            inherited.nativeSize>profile.size||inherited.propertyDimension||inherited.propertyElementSize||inherited.propertyFlags||inherited.propertyOffset||
            inherited.structPropertiesSize||!inherited.structFriendlyName.IsNone())
            throw std::runtime_error("Native Input superclass default has a nonzero or foreign typed baseline");
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            std::string("@ClassDefault:")+profile.path,std::string(profile.path).substr(std::string(profile.path).find_last_of('.')+1u),{},{},0u,0},nullptr));
        auto* object=allocations.back().get();object->cls=cls;object->defaultOwner=cls;object->exportIndex=std::numeric_limits<std::size_t>::max();
        auto payload=std::make_unique<RuntimePreparedNativeDefaults>();payload->kind=profile.kind;payload->owner=cls;payload->superDefault=superDefault;
        payload->nativeSize=profile.size;payload->classFlags=profile.flags;
        payload->propertyDimension=inherited.propertyDimension;payload->propertyElementSize=inherited.propertyElementSize;
        payload->propertyFlags=inherited.propertyFlags;payload->propertyOffset=inherited.propertyOffset;
        payload->structPropertiesSize=inherited.structPropertiesSize;payload->structFriendlyName=inherited.structFriendlyName;
        if(i==3u)payload->input=std::make_unique<RuntimeNativeInputValues>();
        else if(i==4u) {
            if(!inherited.input)throw std::runtime_error("InputExt default lacks the actual prepared Input payload to deep-copy");
            payload->input=std::make_unique<RuntimeNativeInputValues>(*inherited.input);
        }else if(inherited.input)throw std::runtime_error("Metadata default unexpectedly inherited Input instance storage");
        object->preparedNativeDefaults=std::move(payload);domain->defaults.push_back(object);
    }
    domain->aliasStruct=allocate("Engine.Input.Alias","Struct",structClass,inputClass,0u);
    domain->aliasStruct->nativeStruct=std::make_unique<RuntimeNativeStruct>();
    domain->aliasStruct->nativeStruct->friendlyName=*domain->aliasStruct->lookupName;
    domain->aliasStruct->nativeStruct->propertiesSize=0x10u;
    const auto makeField=[&](const std::string& name,RuntimeObject* cls,RuntimeObject* outer,const std::uint32_t dimension,
        const std::uint32_t element,const std::uint32_t offset,const std::uint32_t flags,const char* category) {
        auto* field=allocate(outer->reflection.objectPath+'.'+name,cls->reflection.objectPath.substr(5u),cls,outer,4u);
        field->nativeProperty=std::make_unique<RuntimeNativeProperty>();field->linkedClassOwner=outer;
        auto& p=*field->nativeProperty;p.dimension=dimension;p.elementSize=element;p.offset=offset;p.flags=flags;
        // UProperty's Cpp constructor performs FNAME_Add1 on Category before
        // AddCppProperty. RawKeys is absent from the owned package name tables;
        // retaining just its spelling would omit this native arena side effect.
        const NameString categoryName(category);p.category=categoryName.ToString();
        domain->fields.push_back(field);return field;
    };
    auto* aliasName=makeField("Alias",nameClass,domain->aliasStruct,1u,4u,0u,0x4000u,"None");
    auto* command=makeField("Command",stringClass,domain->aliasStruct,1u,12u,4u,0x404000u,"None");
    command->nativeProperty->next=aliasName;
    auto* aliases=makeField("Aliases",structPropertyClass,inputClass,40u,16u,0x2cu,0x404000u,"Aliases");
    aliases->nativeProperty->referencedStruct=domain->aliasStruct;
    auto counter=stringClass->uniqueObjectCounter;
    RuntimeObject* child=aliases;
    for(std::size_t key=0u;key<enumeration.names.size();++key) {
        const auto units=QuestVr::UnrealString::Decode(enumeration.names[key].ToString());
        const auto requested=NameString(QuestVr::UnrealString::Encode(std::u16string_view(units).substr(3u)));
        auto name=requested.ToString();
        if(requested.IsNone())name=QuestVr::ObjectNames::MakeUnique("StrProperty",counter,[&](const std::string_view candidate) {
            return indexed(inputClass->reflection.objectPath+'.'+std::string(candidate))!=nullptr;
        });
        auto* field=makeField(name,stringClass,inputClass,1u,12u,0x2acu+12u*static_cast<std::uint32_t>(key),0x404000u,"RawKeys");
        field->nativeProperty->next=child;child=field;
    }
    InputSchemaLinkPlans links{{
        {domain->aliasStruct,command,{}},{inputClass,child,{}},{inputExtClass,nullptr,{}}
    }};
    for(std::size_t list=0u;list<3u;++list) {
        std::vector<RuntimeObject*> aliasFields{command};if(list!=2u)aliasFields.push_back(aliasName);
        links[0u].heads[list]=command;
        for(std::size_t i=0u;i<aliasFields.size();++i)aliasFields[i]->nativePropertyNext[list]=i+1u<aliasFields.size()?aliasFields[i+1u]:nullptr;
        std::vector<RuntimeObject*> own;
        for(std::size_t slot=domain->fields.size();slot>2u;--slot)own.push_back(domain->fields[slot-1u]);
        links[1u].heads[list]=own.front();links[2u].heads[list]=own.front();
        for(std::size_t i=0u;i<own.size();++i)own[i]->nativePropertyNext[list]=i+1u<own.size()?own[i+1u]:subsystemClass->nativePropertyHeads[list];
    }
    for(auto* field:domain->fields) {
        field->nativeConstructorLinksPrepared=true;auto& p=*field->nativeProperty;
        p.propertyNext=field->nativePropertyNext[0u];p.configNext=field->nativePropertyNext[1u];p.constructorNext=field->nativePropertyNext[2u];
    }
    domain->aliasStruct->nativeChildren=command;domain->aliasStruct->nativePropertyHeads=links[0u].heads;
    domain->aliasStruct->nativeConstructorLinksPrepared=true;
    auto owner=std::make_unique<GCRoot<RuntimeNativeInputDomain>>(domain);
    auto lookup=StageRuntimeLookupObjects(publications);
    if(!lookup)throw std::runtime_error("Native Input schema lost its required actual lookup staging service");
    auto result=InputSchemaSnapshot(*domain,&links,&counter);
    using QuestVr::DecalStateDetail::Add;
    std::size_t stagingBytes=InputSchemaSnapshotBytes(result);
    stagingBytes=Add(stagingBytes,lookup->RetainedBytes()+allocations.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+
        publications.capacity()*sizeof(RuntimeObject*)+enumeration.names.capacity()*sizeof(NameString));
    // Reserve EMPTY replacement bucket arrays, never the live namespaces.
    // Failed allocation/admission must preserve live retained capacity as well
    // as lookup contents, Class links and counters. Existing nodes/strings are
    // not copied: default-allocator node transfers below reuse their ownership.
    decltype(persistentQualifiedObjects) replacementQualified;
    decltype(persistentVmObjects) replacementVm;
    replacementQualified.max_load_factor(persistentQualifiedObjects.max_load_factor());
    replacementVm.max_load_factor(persistentVmObjects.max_load_factor());
    replacementQualified.reserve(persistentQualifiedObjects.size()+nextQualified.size());
    replacementVm.reserve(persistentVmObjects.size()+nextVm.size());
    // Peak holds old-live + empty-replacement + delta bucket arrays together.
    // Existing live nodes are in the runtime's retained budget; new alias
    // nodes/strings are already counted ONCE by InputSchemaRetainedBytes.
    // Merge transfers them, so no second node/string copy is charged here.
    stagingBytes=Add(stagingBytes,(persistentQualifiedObjects.bucket_count()+persistentVmObjects.bucket_count()+
        replacementQualified.bucket_count()+replacementVm.bucket_count()+nextQualified.bucket_count()+nextVm.bucket_count())*sizeof(void*));
    host.ValidateInputSchemaBudget(*domain,stagingBytes);
    // All allocations, snapshots, provenance/list checks and budgets passed.
    // Existing Class/default/Children and the shared concrete-class name
    // counter are changed only in this nonthrowing publication suffix.
    for(std::size_t i=0u;i<domain->defaultClasses.size();++i)domain->defaultClasses[i]->defaultObject=domain->defaults[i];
    for(const auto& plan:links) {
        plan.owner->nativeChildren=plan.children;plan.owner->nativePropertyHeads=plan.heads;plan.owner->nativeConstructorLinksPrepared=true;
    }
    stringClass->uniqueObjectCounter=counter;
    // The replacements have capacity for both complete node sets. With the
    // same default allocators and string hash/equality, these transfers need
    // neither node/string allocation nor rehash. Old empty bucket arrays are
    // released only after the nonthrowing commit via the final swaps.
    replacementQualified.merge(persistentQualifiedObjects);replacementQualified.merge(nextQualified);
    replacementVm.merge(persistentVmObjects);replacementVm.merge(nextVm);
    if(!persistentQualifiedObjects.empty()||!persistentVmObjects.empty()||!nextQualified.empty()||!nextVm.empty())std::terminate();
    persistentQualifiedObjects.swap(replacementQualified);persistentVmObjects.swap(replacementVm);
    PublishRuntimeLookupObjects(lookup,publications);persistentInputSchema=std::move(owner);
    return result;
}
