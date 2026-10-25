// Private implementation included after the actual UObject lookup service.
// It shares the runtime's real GC/allocation/name/lookup domains, not a second
// compatibility object graph. See ORIGINAL-ENGINE-DEFAULT-LINK.md.
namespace {
constexpr std::array<QuestVr::NativeEngineClassRegistration,2u> EngineFieldClasses{{
    {"Core.FloatProperty","Core.Property","Core.Field",0x5cu,0x12u,0x04084004u,"Core"},
    {"Core.NameProperty","Core.Property","Core.Field",0x5cu,0x12u,0x04084004u,"Core"}
}};
struct NativeDefaultProfile {
    const char *path,*base;
    RuntimeNativeDefaultKind kind;
    std::uint32_t size,flags;
};
constexpr std::array<NativeDefaultProfile,11u> EngineDefaultProfiles{{
    {"Core.Object","",RuntimeNativeDefaultKind::Object,0x28u,0x112u},
    {"Core.Subsystem","Core.Object",RuntimeNativeDefaultKind::Subsystem,0x2cu,0x112u},
    {"Core.Field","Core.Object",RuntimeNativeDefaultKind::Field,0x34u,0x13u},
    {"Core.Property","Core.Field",RuntimeNativeDefaultKind::Property,0x5cu,0x13u},
    {"Core.StrProperty","Core.Property",RuntimeNativeDefaultKind::StrProperty,0x5cu,0x12u},
    {"Core.ArrayProperty","Core.Property",RuntimeNativeDefaultKind::ArrayProperty,0x60u,0x12u},
    {"Engine.Primitive","Core.Object",RuntimeNativeDefaultKind::Primitive,0x54u,0x12u},
    {"Engine.Engine","Core.Subsystem",RuntimeNativeDefaultKind::Engine,0x68u,0x11eu},
    {"Engine.GameEngine","Engine.Engine",RuntimeNativeDefaultKind::GameEngine,0xd0u,0x1eu},
    {"Extension.GameEngineExt","Engine.GameEngine",RuntimeNativeDefaultKind::GameEngineExt,0xd0u,0x1eu},
    {"DeusEx.DeusExGameEngine","Extension.GameEngineExt",RuntimeNativeDefaultKind::DeusExGameEngine,0xd0u,0x1eu}
}};
constexpr std::array<const char*,5u> EngineClassKeys{
    "GameRenderDevice","AudioDevice","Console","NetworkDevice","Language"};
constexpr std::array<const char*,5u> EngineClassConstraints{
    "Engine.RenderDevice","Engine.AudioSubsystem","Engine.Console","Engine.NetDriver","Core.Language"};
bool EngineDefaultKind(const RuntimeNativeDefaultKind kind) {
    return kind==RuntimeNativeDefaultKind::Engine || kind==RuntimeNativeDefaultKind::GameEngine ||
        kind==RuntimeNativeDefaultKind::GameEngineExt || kind==RuntimeNativeDefaultKind::DeusExGameEngine;
}
std::string NativeObjectPath(const RuntimeObject* object) {return object?object->reflection.objectPath:std::string{};}
std::array<std::string,5u> NativeConfiguredPaths(const RuntimeNativeEngineValues& values) {
    std::array<std::string,5u> result;
    for(std::size_t i=0u;i<result.size();++i) result[i]=NativeObjectPath(values.configuredClasses[i]);
    return result;
}
using NativeDefaultPayloads=std::vector<std::pair<RuntimeObject*,std::unique_ptr<const RuntimePreparedNativeDefaults>>>;
PortableEngineCollisionSnapshot EngineCollisionSnapshot(const RuntimeEngineCollisionDomain& domain,
    const NativeDefaultPayloads* stagedPayloads=nullptr) {
    if(!domain.engine || !domain.transient || !domain.engine->nativeEngineInstance ||
        !domain.transient->nativePackageInstance || domain.defaults.size()!=EngineDefaultProfiles.size() ||
        domain.fields.size()!=4u || domain.classes.size()!=EngineFieldClasses.size())
        throw std::runtime_error("Native collision Engine domain is incomplete");
    const auto& state=*domain.engine->nativeEngineInstance;auto* cylinder=state.values.cylinder;
    if(!cylinder || !cylinder->nativePrimitiveInstance || !domain.engine->cls || !cylinder->cls ||
        !domain.transient->cls || domain.engine->outer!=domain.transient || cylinder->outer!=domain.transient ||
        domain.engine->reflection.flags || cylinder->reflection.flags || domain.transient->reflection.flags ||
        !state.execInstalled || !state.values.urlValid || state.reservedLevels[0u] || state.reservedLevels[1u] ||
        (state.currentLevel && state.currentLevel->nativeLevelEngine!=domain.engine))
        throw std::runtime_error("Native collision Engine constructor/header/level state is invalid");
    PortableEngineCollisionSnapshot result;result.prepared=true;
    result.enginePath=domain.engine->reflection.objectPath;result.engineClassPath=domain.engine->cls->reflection.objectPath;
    result.transientPath=domain.transient->reflection.objectPath;result.cylinderPath=cylinder->reflection.objectPath;
    result.boundLevelPath=NativeObjectPath(state.currentLevel);
    result.engineIndex=domain.engine->objectIndex.Index();result.engineClassIndex=domain.engine->cls->objectIndex.Index();
    result.transientIndex=domain.transient->objectIndex.Index();result.cylinderIndex=cylinder->objectIndex.Index();
    result.cylinderClassIndex=cylinder->cls->objectIndex.Index();result.levelIndex=state.currentLevel?state.currentLevel->objectIndex.Index():0u;
    result.engineFlags=domain.engine->reflection.flags;result.transientFlags=domain.transient->reflection.flags;
    result.cylinderFlags=cylinder->reflection.flags;result.engineExecInstalled=state.execInstalled;
    result.instanceUrlValid=state.values.urlValid;result.cylinderBoundsValid=cylinder->nativePrimitiveInstance->boundsValid;
    result.engineUniqueCounter=domain.engine->cls->uniqueObjectCounter;result.primitiveUniqueCounter=cylinder->cls->uniqueObjectCounter;
    result.instanceCacheSizeMegs=state.values.cacheSizeMegs;result.instanceUseSound=state.values.useSound;
    result.instanceConfiguredClasses=NativeConfiguredPaths(state.values);
    result.instanceServerActors=state.values.serverActors;result.instanceServerPackages=state.values.serverPackages;
    for(std::size_t i=0u;i<domain.defaults.size();++i) {
        const auto* object=domain.defaults[i];const auto& profile=EngineDefaultProfiles[i];
        const RuntimePreparedNativeDefaults* payload=object?object->preparedNativeDefaults.get():nullptr;
        if(!payload && stagedPayloads) for(const auto& staged:*stagedPayloads)
            if(staged.first==object) payload=staged.second.get();
        if(!object || !payload || !object->defaultOwner || object->cls!=object->defaultOwner)
            throw std::runtime_error("Native collision defaults lack their actual owning Class");
        const auto& prepared=*payload;
        RuntimeObject* expectedSuper{};
        if(object->cls->base) for(auto* candidate:domain.defaults)
            if(candidate->defaultOwner==object->cls->base) expectedSuper=candidate;
        if(prepared.owner!=object->cls || prepared.kind!=profile.kind || prepared.nativeSize!=profile.size ||
            prepared.classFlags!=profile.flags || object->cls->reflection.objectPath!=profile.path ||
            prepared.superDefault!=expectedSuper ||
            prepared.propertyDimension || prepared.propertyElementSize || prepared.propertyFlags || prepared.propertyOffset ||
            prepared.primitiveBoundsValid ||
            std::any_of(prepared.primitiveBounds.begin(),prepared.primitiveBounds.end(),[](float value){return value!=0.0f;}))
            throw std::runtime_error("Native collision defaults violate the audited copy/extension profile");
        result.defaults.push_back({profile.path,object->cls->objectIndex.Index(),object->objectIndex.Index(),
            object->cls->base?object->cls->base->objectIndex.Index():0u,profile.size,profile.flags,
            NativeConfiguredPaths(prepared.engine),prepared.engine.cacheSizeMegs,prepared.engine.useSound,
            prepared.engine.urlValid,prepared.engine.currentTickRate,prepared.engine.serverActors,prepared.engine.serverPackages});
    }
    for(const auto* field:domain.fields) {
        if(!field || !field->nativeProperty || !field->cls || !field->outer || field->property || field->defaultOwner)
            throw std::runtime_error("Native Engine field is not a genuine constructed native Property");
        const auto& property=*field->nativeProperty;
        result.fields.push_back({field->reflection.objectPath,field->cls->reflection.objectPath,field->outer->reflection.objectPath,
            NativeObjectPath(property.next),NativeObjectPath(property.inner),NativeObjectPath(property.propertyNext),
            NativeObjectPath(property.configNext),NativeObjectPath(property.constructorNext),property.category,
            field->objectIndex.Index(),field->cls->objectIndex.Index(),field->outer->objectIndex.Index(),field->reflection.flags,
            property.dimension,property.elementSize,property.flags,property.offset});
    }
    return result;
}
} // namespace

PortableEngineCollisionSnapshot ReadPortableRuntimeEngineCollision() {
    if(!persistentEngineCollision) return {};
    const auto& domain=*persistentEngineCollision->get();
    auto result=EngineCollisionSnapshot(domain);PortableActorVmHost host;
    for(const auto* object:{domain.engine,domain.transient,domain.engine->nativeEngineInstance->values.cylinder})
        if(host.Object(object->reflection.objectPath)!=object || !object->lookupControl ||
            object->lookupControl!=persistentObjectLookup)
            throw std::runtime_error("Native collision Engine object lost its actual lookup generation");
    for(const auto* object:domain.defaults)
        if(object->cls->defaultObject!=object || !PreparedNativeClassDefaultValid(object->cls))
            throw std::runtime_error("Native collision Engine default owner changed");
    return result;
}

PortableEngineCollisionSnapshot PreparePortableRuntimeEngineCollision(const std::string& iniPath) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(iniPath.empty() || iniPath.size()>8192u || iniPath.find('\0')!=std::string::npos)
        throw std::runtime_error("Native collision Engine requires an explicit bounded INI path");
    const auto suppliedPath=std::filesystem::absolute(iniPath).lexically_normal().string();
    if(persistentEngineCollision) {
        if(suppliedPath!=persistentEngineCollision->get()->iniPath)
            throw std::runtime_error("Native collision Engine is already bound to another explicit INI source");
        return ReadPortableRuntimeEngineCollision();
    }
    if(!ReadPortableRuntimeNativeEngineClasses().prepared || !ReadPortableRuntimeNativeMetadataClasses().prepared ||
        !ReadPortableRuntimeNativeConfigClasses().prepared || !ReadPortableRuntimeNativePackages().prepared ||
        !ReadPortableRuntimeObjectLookup().prepared)
        throw std::runtime_error("Native collision Engine requires explicit class/metadata/config/Package/lookup phases");
    const auto ini=QuestVr::NativeConfig::ReadOrderedIni(suppliedPath);
    if(!ini) throw std::runtime_error("Native collision Engine supplied INI is missing");
    auto* objectClass=host.Object("Core.Object");auto* meta=host.Object("Core.Class");
    auto* propertyClass=host.Object("Core.Property");auto* fieldClass=host.Object("Core.Field");
    auto* gameEngineClass=host.Object("Engine.GameEngine");auto* primitiveClass=host.Object("Engine.Primitive");
    // Delta dictionaries own only the new nodes. Cloning the entire loaded
    // namespace exceeded the existing32MiB admission budget on real Training.
    decltype(persistentQualifiedObjects) nextQualified;
    decltype(persistentVmObjects) nextVm;
    const auto indexed=[&](const std::string& key)->RuntimeObject* {
        const auto staged=nextVm.find(key);if(staged!=nextVm.end()) return staged->second;
        const auto live=persistentVmObjects.find(key);return live==persistentVmObjects.end()?nullptr:live->second;
    };
    pendingBirthGarbage=true;
    GCRoot<RuntimeEngineCollisionDomain> staged(GC::Alloc<RuntimeEngineCollisionDomain>());
    auto* domain=staged.get();domain->iniPath=suppliedPath;domain->defaults.reserve(11u);domain->fields.reserve(4u);domain->classes.reserve(2u);
    std::vector<GCRoot<RuntimeObject>> allocations;allocations.reserve(16u);
    std::vector<RuntimeObject*> publications;publications.reserve(9u);
    const auto allocate=[&](const std::string& path,const std::string& type,RuntimeObject* cls,RuntimeObject* outer,const std::uint32_t flags) {
        if(nextQualified.size()+persistentQualifiedObjects.size()>=1'000'000u ||
            nextVm.size()+persistentVmObjects.size()>=1'000'000u || nextQualified.count(path) ||
            persistentQualifiedObjects.count(path) || indexed(LowerAscii(path)))
            throw std::runtime_error("Native collision Engine allocation name collides or exceeds its lookup bound");
        allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{path,type,NativeObjectPath(outer),{},flags,0},nullptr));
        auto* object=allocations.back().get();object->cls=cls;object->outer=outer;
        object->exportIndex=std::numeric_limits<std::size_t>::max();
        object->lookupName=NameString(path.substr(path.find_last_of('.')+1u));
        nextQualified.emplace(path,object);nextVm.emplace(LowerAscii(path),object);publications.push_back(object);
        return object;
    };
    for(const auto& entry:EngineFieldClasses) {
        auto* cls=allocate(std::string(entry.path),"Class",meta,RegisteredNativePackage(entry.package),entry.registrationObjectFlags);
        cls->nativeMetadataClass=&entry;cls->base=propertyClass;cls->nativeClassWithin=fieldClass;
        cls->reflection.basePath=std::string(entry.base);domain->classes.push_back(cls);
    }
    // Bind only authenticated Class imports to the two new actual native
    // UClasses. Merely matching a display path/leaf never gains this authority.
    std::unordered_map<std::string,PortablePackageTables> tables;
    const auto table=[&](const std::string& source)->const PortablePackageTables& {
        auto found=tables.find(source);
        if(found==tables.end()) {
            if(tables.size()>=64u) throw std::runtime_error("Native Engine source table budget exceeded");
            found=tables.emplace(source,LoadPortablePackageTables(source)).first;
        }
        return found->second;
    };
    const auto referencePath=[&](const PortablePackageTables& source,const std::int32_t reference) {
        if(!reference) return std::string{};
        auto path=GetPortableObjectPath(source,reference);
        if(reference>0) path=PackageStem(source.sourcePath)+'.'+path;
        return path;
    };
    const auto classReference=[&](const PortablePackageTables& source,const std::int32_t reference) {
        if(!reference) throw std::runtime_error("Native Engine actual Class reference is null");
        if(reference<0) {
            const auto index=static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference)-1);
            if(index>=source.imports.size()) throw std::runtime_error("Native Engine Class import is outside its source");
            const auto& imported=source.imports[static_cast<std::size_t>(index)];
            if(imported.ClassName<0 || imported.ClassPackage<0 ||
                static_cast<std::size_t>(imported.ClassName)>=source.names.size() ||
                static_cast<std::size_t>(imported.ClassPackage)>=source.names.size() ||
                !SameInventoryIdentity(source.names[static_cast<std::size_t>(imported.ClassName)].Name.ToString(),"Class") ||
                !SameInventoryIdentity(source.names[static_cast<std::size_t>(imported.ClassPackage)].Name.ToString(),"Core"))
                throw std::runtime_error("Native Engine Class import has the wrong actual metaclass package");
        }else if(static_cast<std::size_t>(reference)>source.exports.size() || source.exports[reference-1u].ObjClass!=0)
            throw std::runtime_error("Native Engine Class reference is not an actual source UClass");
        auto* found=indexed(LowerAscii(referencePath(source,reference)));
        if(!found || found->cls!=meta ||
            (!found->classDescriptor && !found->nativeEngineClass && !found->nativeMetadataClass && !found->nativeClientClass))
            throw std::runtime_error("Native Engine Class reference has no actual registered UClass");
        return found;
    };
    std::vector<std::pair<RuntimeObject*,RuntimeObject*>> classBindings;
    for(auto* object:persistentRuntime->get()->exports) {
        if(!object || !object->inventoryTableMetadata || object->inventorySerializedClass || object->sourcePath.empty() ||
            object->runtimeBirth || object->nativeWindowObject) continue;
        const auto& source=table(object->sourcePath);const auto& entry=source.exports.at(object->exportIndex);
        if(entry.ObjClass>=0) continue;
        const auto path=LowerAscii(referencePath(source,entry.ObjClass));
        for(auto* cls:domain->classes) if(path==LowerAscii(cls->reflection.objectPath)) {
            if(classReference(source,entry.ObjClass)!=cls || (object->cls && object->cls!=cls))
                throw std::runtime_error("Native Engine source metadata Class binding conflicts");
            classBindings.emplace_back(object,cls);
        }
    }
    const auto actualClass=[&](RuntimeObject* object) {
        for(const auto& binding:classBindings) if(binding.first==object) return binding.second;
        return object->cls;
    };
    const auto sourceFields=[&](RuntimeObject* cls) {
        std::vector<RuntimeObject*> result;
        if(!cls->classDescriptor) return result;
        if(!cls->classFieldLinksValid) throw std::runtime_error("Native Engine class source Children links are invalid");
        const auto& source=table(cls->sourcePath);auto reference=cls->classDescriptor->state.children;
        std::unordered_set<RuntimeObject*> seen;
        while(reference) {
            auto* found=indexed(LowerAscii(referencePath(source,reference)));
            if(!found || found->outer!=cls || seen.size()>=8192u || !seen.insert(found).second)
                throw std::runtime_error("Native Engine source Children/Next ownership is invalid");
            auto* field=found;
            if(!field->property) {
                if(field->script) reference=field->script->nextField;
                else if(field->stateDescriptor) reference=field->stateDescriptor->nextField;
                else if(field->commonFieldLinks) reference=field->commonFieldLinks->nextField;
                else reference=LoadPortableFieldLinks(table(field->sourcePath),field->exportIndex).nextField;
                continue;
            }
            if(field->linkedClassOwner!=cls || field->property->baseField ||
                !actualClass(field) || !IsDerivedFromPath(actualClass(field),"Core.Property"))
                throw std::runtime_error("Native Engine source field lacks actual Property ancestry");
            const auto& fieldSource=table(field->sourcePath);
            if(classReference(fieldSource,fieldSource.exports.at(field->exportIndex).ObjClass)!=actualClass(field))
                throw std::runtime_error("Native Engine field Class disagrees with its authenticated source");
            result.push_back(field);reference=field->property->nextField;
        }
        return result;
    };
    const auto ownedObjectFields=sourceFields(objectClass);
    const auto ownedSubsystemFields=sourceFields(host.Object("Core.Subsystem"));
    const auto ownedEngineFields=sourceFields(host.Object("Engine.Engine"));
    if(ownedObjectFields.size()!=5u || ownedSubsystemFields.size()!=1u || ownedEngineFields.size()!=15u)
        throw std::runtime_error("Native Engine source field/default profile is not the audited original shape");
    struct SourceFieldProfile {const char *path,*type;std::int32_t dimension;std::uint32_t flags;const char *primary,*secondary;};
    static constexpr std::array<SourceFieldProfile,21u> schema{{
        {"Core.Object.ObjectInternal","IntProperty",6,0x1002u,"",""},
        {"Core.Object.Outer","ObjectProperty",1,0x1002u,"Core.Object",""},
        {"Core.Object.ObjectFlags","IntProperty",1,0x1002u,"",""},
        {"Core.Object.Name","NameProperty",1,0x21003u,"",""},
        {"Core.Object.Class","ClassProperty",1,0x21003u,"Core.Class","Core.Object"},
        {"Core.Subsystem.ExecVtbl","IntProperty",1,0x1002u,"",""},
        {"Engine.Engine.GameRenderDevice","ClassProperty",1,0x4000u,"Core.Class","Engine.RenderDevice"},
        {"Engine.Engine.AudioDevice","ClassProperty",1,0x4001u,"Core.Class","Engine.AudioSubsystem"},
        {"Engine.Engine.Console","ClassProperty",1,0x4001u,"Core.Class","Engine.Console"},
        {"Engine.Engine.NetworkDevice","ClassProperty",1,0x4001u,"Core.Class","Engine.NetDriver"},
        {"Engine.Engine.Language","ClassProperty",1,0x4001u,"Core.Class","Core.Language"},
        {"Engine.Engine.Cylinder","ObjectProperty",1,0u,"Engine.Primitive",""},
        {"Engine.Engine.Client","ObjectProperty",1,2u,"Engine.Client",""},
        {"Engine.Engine.Render","ObjectProperty",1,2u,"Engine.RenderBase",""},
        {"Engine.Engine.Audio","ObjectProperty",1,2u,"Engine.AudioSubsystem",""},
        {"Engine.Engine.TickCycles","IntProperty",1,0u,"",""},
        {"Engine.Engine.GameCycles","IntProperty",1,0u,"",""},
        {"Engine.Engine.ClientCycles","IntProperty",1,0u,"",""},
        {"Engine.Engine.CacheSizeMegs","IntProperty",1,0x4001u,"",""},
        {"Engine.Engine.UseSound","BoolProperty",1,0x4001u,"",""},
        {"Engine.Engine.CurrentTickRate","FloatProperty",1,1u,"",""}
    }};
    for(const auto& profile:schema) {
        auto* field=host.Object(profile.path);const auto& source=table(field->sourcePath);
        const auto constraint=[&](const std::int32_t reference,const std::string_view expected) {
            if(expected.empty()) return reference==0;
            if(reference==0) return false;
            if(reference>0) return static_cast<std::size_t>(reference)<=source.exports.size() &&
                source.exports[static_cast<std::size_t>(reference-1)].ObjClass==0 &&
                SameInventoryIdentity(referencePath(source,reference),expected);
            const auto index=static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference)-1);
            if(index>=source.imports.size()) return false;
            const auto& imported=source.imports[static_cast<std::size_t>(index)];
            return imported.ClassName>=0 && imported.ClassPackage>=0 &&
                static_cast<std::size_t>(imported.ClassName)<source.names.size() &&
                static_cast<std::size_t>(imported.ClassPackage)<source.names.size() &&
                SameInventoryIdentity(source.names[static_cast<std::size_t>(imported.ClassName)].Name.ToString(),"Class") &&
                SameInventoryIdentity(source.names[static_cast<std::size_t>(imported.ClassPackage)].Name.ToString(),"Core") &&
                SameInventoryIdentity(referencePath(source,reference),expected);
        };
        if(!field->property || field->property->type!=profile.type || field->property->arrayDimension!=profile.dimension ||
            field->property->flags!=profile.flags || field->property->baseField ||
            !constraint(field->property->referencedType,profile.primary) || !constraint(field->property->secondaryType,profile.secondary) ||
            actualClass(field)->reflection.objectPath!=std::string("Core.")+profile.type)
            throw std::runtime_error(std::string("Native Engine source field violates its closed typed schema: ")+profile.path);
        // Category and source Children/Next remain the actual loaded data;
        // neither confers typed slot authority and neither is reconstructed.
    }
    for(std::size_t i=0u;i<EngineClassKeys.size();++i) {
        auto* field=host.Object(std::string("Engine.Engine.")+EngineClassKeys[i]);const auto& source=table(field->sourcePath);
        if(!field->property || field->outer!=host.Object("Engine.Engine") ||
            field->property->type!="ClassProperty" || field->property->arrayDimension!=1u ||
            field->property->flags!=(i?0x4001u:0x4000u) ||
            classReference(source,field->property->referencedType)!=meta ||
            classReference(source,field->property->secondaryType)!=host.Object(EngineClassConstraints[i]))
            throw std::runtime_error("Native Engine config ClassProperty lost its actual original constraint");
    }
    NativeDefaultPayloads payloads;
    payloads.reserve(11u);
    std::unordered_map<RuntimeObject*,RuntimeObject*> defaultsByClass;
    const auto actualDerives=[&](RuntimeObject* cls,RuntimeObject* required) {
        std::array<RuntimeObject*,256u> seen{};std::size_t depth{};
        while(cls) {
            if(depth==seen.size() || std::find(seen.begin(),seen.begin()+depth,cls)!=seen.begin()+depth ||
                !ActualLookupClass(*persistentObjectLookup,cls))
                throw std::runtime_error("Native Engine config import has no authenticated actual Super chain");
            seen[depth++]=cls;if(cls==required) return true;
            if(cls->inventoryTableMetadata && (!cls->inventoryBaseReferenceValid || (!cls->base && !cls->serializedBaseZero)))
                throw std::runtime_error("Native Engine config import has unresolved actual Super metadata");
            cls=cls->base;
        }
        return false;
    };
    for(const auto& profile:EngineDefaultProfiles) {
        auto* cls=host.Object(profile.path);
        const auto flags=cls->classDescriptor?cls->classDescriptor->classFlags:
            cls->nativeEngineClass?cls->nativeEngineClass->registrationClassFlags:cls->nativeMetadataClass->registrationClassFlags;
        if(cls->cls!=meta || NativeObjectPath(cls->base)!=profile.base || flags!=profile.flags ||
            !cls->classDefaultValues.empty() || cls->nativeChildren ||
            std::any_of(cls->nativePropertyHeads.begin(),cls->nativePropertyHeads.end(),[](auto* value){return value!=nullptr;}))
            throw std::runtime_error("Native Engine default profile/Class/flags/field baseline is invalid");
        if(cls->classDescriptor) {
            if((std::string_view(profile.path)!="Core.Object" && std::string_view(profile.path)!="Core.Subsystem" &&
                std::string_view(profile.path)!="Engine.Engine") || !cls->defaultObject || !cls->classFieldLinksValid)
                throw std::runtime_error("Native Engine source default owner is unavailable");
        }else if(cls->defaultObject || (cls->nativeEngineClass?cls->nativeEngineClass->nativeSize:cls->nativeMetadataClass->nativeSize)!=profile.size)
            throw std::runtime_error("Native Engine native default owner/size is not the audited registration");
        auto* defaults=cls->defaultObject;
        if(!defaults) {
            allocations.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
                std::string("@ClassDefault:")+profile.path,std::string(profile.path).substr(std::string(profile.path).find_last_of('.')+1u),{},{},0u,0},nullptr));
            defaults=allocations.back().get();defaults->cls=cls;defaults->defaultOwner=cls;
            defaults->exportIndex=std::numeric_limits<std::size_t>::max();
        }
        if(defaults->defaultOwner!=cls || defaults->cls!=cls || defaults->preparedNativeDefaults || defaults->outer || defaults->lookupName)
            throw std::runtime_error("Native Engine existing default identity/header is invalid");
        auto prepared=std::make_unique<RuntimePreparedNativeDefaults>();prepared->kind=profile.kind;
        prepared->owner=cls;prepared->nativeSize=profile.size;prepared->classFlags=profile.flags;
        if(cls->base) {
            const auto inherited=defaultsByClass.find(cls->base);
            if(inherited==defaultsByClass.end()) throw std::runtime_error("Native Engine defaults are not prepared in actual Super order");
            prepared->superDefault=inherited->second;
            for(const auto& payload:payloads) if(payload.first==inherited->second) {
                prepared->engine=payload.second->engine;
                prepared->propertyDimension=payload.second->propertyDimension;
                prepared->propertyElementSize=payload.second->propertyElementSize;
                prepared->propertyFlags=payload.second->propertyFlags;prepared->propertyOffset=payload.second->propertyOffset;
            }
        }
        if(profile.kind==RuntimeNativeDefaultKind::Engine) {
            if(cls->classDescriptor->defaults.size()!=2u)
                throw std::runtime_error("Native Engine original authored default tags changed");
            const auto cache=host.Read(cls,host.DescribeProperty("Engine.Engine.CacheSizeMegs"),0u,true);
            const auto sound=host.Read(cls,host.DescribeProperty("Engine.Engine.UseSound"),0u,true);
            if(cache.kind!=QuestVr::Vm::Kind::Int || cache.integer!=2 || sound.kind!=QuestVr::Vm::Kind::Bool || !sound.boolean)
                throw std::runtime_error("Native Engine original scalar default profile changed");
            prepared->engine.cacheSizeMegs=cache.integer;prepared->engine.useSound=true;
        }else if(cls->classDescriptor && !cls->classDescriptor->defaults.empty())
            throw std::runtime_error("Native Engine Object/Subsystem default tags are not empty");
        if(EngineDefaultKind(profile.kind)) {
            auto& values=prepared->engine;
            for(std::size_t i=0u;i<EngineClassKeys.size();++i) if(const auto input=ini->PropertyScalar(profile.path,EngineClassKeys[i])) {
                auto destination=reinterpret_cast<std::uintptr_t>(values.configuredClasses[i]);
                static_cast<void>(QuestVr::NativeConfigImport::Class(QuestVr::UnrealString::Decode(*input),
                    reinterpret_cast<std::uintptr_t>(meta),reinterpret_cast<std::uintptr_t>(meta),
                    reinterpret_cast<std::uintptr_t>(host.Object(EngineClassConstraints[i])),destination,
                    [&](const std::uintptr_t requested,const std::u16string_view name)->std::uintptr_t {
                        auto* requestedClass=ActualLookupObject(*persistentObjectLookup,requested);
                        const auto found=FindPortableRuntimeObject({QuestVr::UnrealString::Encode(name),requestedClass->objectIndex.Index(),0u,true,false});
                        if(!found) return 0u;
                        return persistentObjectLookup->index.RecordAt(found->objectIndex)->identity;
                    },[&](const std::uintptr_t value){return ActualLookupObject(*persistentObjectLookup,value)->cls==meta;},
                    [&](const std::uintptr_t value,const std::uintptr_t required) {
                        return actualDerives(ActualLookupObject(*persistentObjectLookup,value),ActualLookupObject(*persistentObjectLookup,required));
                    }));
                values.configuredClasses[i]=destination?ActualLookupObject(*persistentObjectLookup,destination):nullptr;
            }
            if(const auto value=ini->PropertyScalar(profile.path,"CacheSizeMegs"))
                static_cast<void>(QuestVr::NativeConfigImport::Integer(QuestVr::UnrealString::Decode(*value),values.cacheSizeMegs));
            if(const auto value=ini->PropertyScalar(profile.path,"UseSound")) {
                std::uint32_t bits=values.useSound?1u:0u;
                static_cast<void>(QuestVr::NativeConfigImport::Boolean(QuestVr::UnrealString::Decode(*value),1u,bits));values.useSound=(bits&1u)!=0u;
            }
            if(profile.kind!=RuntimeNativeDefaultKind::Engine) {
                const auto importStrings=[&](const char* key,std::vector<std::string>& destination) {
                    if(auto configured=ini->StringArray(profile.path,key)) {
                        // Original array LoadConfig calls the actual StrProperty
                        // inner ImportText with flags0 for every source value.
                        for(auto& text:*configured) {
                            std::u16string imported;
                            const auto result=QuestVr::NativeConfigImport::String(QuestVr::UnrealString::Decode(text),0u,imported);
                            if(!result.returned || result.nextUnit)
                                throw std::runtime_error("Native Engine StrProperty config import lost its raw CString contract");
                            text=QuestVr::UnrealString::Encode(imported);
                        }
                        destination=std::move(*configured);
                    }
                };
                importStrings("ServerActors",values.serverActors);
                importStrings("ServerPackages",values.serverPackages);
            }
        }
        domain->defaults.push_back(defaults);defaultsByClass.emplace(cls,defaults);
        payloads.emplace_back(defaults,std::move(prepared));
    }
    // Native-only array declarations are real indexed Field/Property UObjects.
    // Source Engine fields above are retained; no destructive Cpp ctor replay.
    const auto makeField=[&](const std::string& path,RuntimeObject* cls,RuntimeObject* outer,const std::uint32_t flags,const std::uint32_t offset) {
        auto* field=allocate(path,cls->reflection.objectPath.substr(5u),cls,outer,4u);
        const auto inherited=defaultsByClass.at(cls);const RuntimePreparedNativeDefaults* copied{};
        for(const auto& payload:payloads) if(payload.first==inherited) copied=payload.second.get();
        if(!copied || copied->propertyDimension || copied->propertyFlags || copied->propertyOffset || copied->propertyElementSize)
            throw std::runtime_error("Native Engine Property instance lacks its prepared native zero defaults");
        field->nativeProperty=std::make_unique<RuntimeNativeProperty>();auto& property=*field->nativeProperty;
        property.dimension=copied->propertyDimension;property.flags=copied->propertyFlags;
        property.offset=copied->propertyOffset;property.elementSize=copied->propertyElementSize;
        property.dimension=1u;property.flags=flags;property.offset=offset;
        property.elementSize=12u;property.flags|=0x400000u;property.category=outer==gameEngineClass?"Settings":"None";
        domain->fields.push_back(field);return field;
    };
    auto* serverActors=makeField("Engine.GameEngine.ServerActors",host.Object("Core.ArrayProperty"),gameEngineClass,0x4000u,0xb8u);
    auto* actorInner=makeField("Engine.GameEngine.ServerActors.StrProperty0",host.Object("Core.StrProperty"),serverActors,0u,0u);
    auto* serverPackages=makeField("Engine.GameEngine.ServerPackages",host.Object("Core.ArrayProperty"),gameEngineClass,0x4000u,0xc4u);
    auto* packageInner=makeField("Engine.GameEngine.ServerPackages.StrProperty0",host.Object("Core.StrProperty"),serverPackages,0u,0u);
    serverActors->nativeProperty->inner=actorInner;serverPackages->nativeProperty->inner=packageInner;
    serverPackages->nativeProperty->next=serverActors;
    std::unordered_map<RuntimeObject*,std::vector<RuntimeObject*>> ownFields;
    ownFields.emplace(objectClass,ownedObjectFields);ownFields.emplace(host.Object("Core.Subsystem"),ownedSubsystemFields);
    ownFields.emplace(host.Object("Engine.Engine"),ownedEngineFields);ownFields.emplace(gameEngineClass,std::vector<RuntimeObject*>{serverPackages,serverActors});
    std::unordered_map<RuntimeObject*,std::array<std::vector<RuntimeObject*>,3u>> linked;
    std::unordered_map<RuntimeObject*,std::array<RuntimeObject*,3u>> nextLinks,heads;
    for(const auto& profile:EngineDefaultProfiles) {
        auto* cls=host.Object(profile.path);auto& lists=linked[cls];
        for(auto* field:ownFields[cls]) {
            const auto flags=field->nativeProperty?field->nativeProperty->flags:field->property->flags;
            lists[0u].push_back(field);if(flags&0x4000u) lists[1u].push_back(field);if(flags&0x400000u) lists[2u].push_back(field);
        }
        if(cls->base) for(std::size_t list=0u;list<3u;++list) {
            const auto& inherited=linked.at(cls->base)[list];lists[list].insert(lists[list].end(),inherited.begin(),inherited.end());
        }
        for(std::size_t list=0u;list<3u;++list) {
            heads[cls][list]=lists[list].empty()?nullptr:lists[list].front();
            for(std::size_t i=0u;i<lists[list].size();++i) nextLinks[lists[list][i]][list]=i+1u<lists[list].size()?lists[list][i+1u]:nullptr;
        }
    }
    if(indexed("transient") || persistentNativePackages->objects.count("transient"))
        throw std::runtime_error("Native collision Engine Transient package name is already occupied");
    domain->transient=allocate("Transient","Package",persistentNativePackages->packageClass,nullptr,0u);
    domain->transient->nativePackageKey="transient";domain->transient->nativePackageIndex=persistentNativePackages;
    domain->transient->nativePackageInstance=std::make_unique<QuestVr::NativePackage::Instance>(QuestVr::NativePackage::Construct(
        *persistentNativePackages->defaults->nativePackageDefaults,0u,QuestVr::NativePackage::AllocationRequest::Transient,QuestVr::NativePackage::NoLegacyModuleProvider{}));
    decltype(persistentNativePackages->objects) nextPackages;
    if(persistentNativePackages->objects.size()>=4096u) throw std::runtime_error("Native collision Package registry bound exceeded");
    nextPackages.emplace("transient",domain->transient);
    auto* engineClass=host.Object(*persistentGameEngineClass);
    if((engineClass->nativeEngineClass!=&QuestVr::NativeEngineClasses[2u] && engineClass->nativeEngineClass!=&QuestVr::NativeEngineClasses[3u] &&
        engineClass->nativeEngineClass!=&QuestVr::NativeEngineClasses[4u]) || engineClass->nativeClassWithin!=objectClass ||
        primitiveClass->nativeClassWithin!=objectClass || primitiveClass->base!=objectClass)
        throw std::runtime_error("Native collision Engine instance lacks actual concrete class/Within authority");
    auto engineCounter=engineClass->uniqueObjectCounter;auto primitiveCounter=primitiveClass->uniqueObjectCounter;
    const auto unique=[&](RuntimeObject* cls,std::int32_t& counter) {
        return QuestVr::ObjectNames::MakeUnique(cls->lookupName->ToString(),counter,[&](const std::string_view candidate) {
            // ANY-class collision in actual Transient Outer, including staged
            // objects. Existing publications use actual names, not path aliases.
            const auto index=NameString::FindCompareIndex(candidate);
            if(index) for(auto* object=persistentPublishedHead;object;object=object->lookupNext)
                if(object->outer==domain->transient && object->lookupName && object->lookupName->GetCompareIndex()==*index) return true;
            for(auto* object:publications) if(object->outer==domain->transient && object->lookupName &&
                LowerAscii(object->lookupName->ToString())==LowerAscii(std::string(candidate))) return true;
            return false;
        });
    };
    domain->engine=allocate("Transient."+unique(engineClass,engineCounter),engineClass->lookupName->ToString(),engineClass,domain->transient,0u);
    domain->engine->nativeEngineInstance=std::make_unique<RuntimeNativeEngineInstance>();
    auto& engine=*domain->engine->nativeEngineInstance;
    for(const auto& payload:payloads) if(payload.first==defaultsByClass.at(engineClass)) engine.values=payload.second->engine;
    // Genuine fresh Engine/GameEngine constructor sequence, before later
    // FURL.StaticInit globals. Deep string/array copies must not alias the CDO.
    engine.execInstalled=true;engine.values.protocol.clear();engine.values.host.clear();engine.values.map.clear();engine.values.portal.clear();
    engine.values.port=0;engine.values.options.clear();engine.values.urlValid=true;
    auto* cylinder=allocate("Transient."+unique(primitiveClass,primitiveCounter),"Primitive",primitiveClass,domain->transient,0u);
    cylinder->nativePrimitiveInstance=std::make_unique<RuntimeNativePrimitiveInstance>();
    const RuntimePreparedNativeDefaults* primitiveDefaults{};
    for(const auto& payload:payloads) if(payload.first==defaultsByClass.at(primitiveClass)) primitiveDefaults=payload.second.get();
    if(!primitiveDefaults || primitiveDefaults->nativeSize!=0x54u || primitiveDefaults->primitiveBoundsValid)
        throw std::runtime_error("Native collision Primitive instance lacks its prepared actual defaults");
    cylinder->nativePrimitiveInstance->bounds=primitiveDefaults->primitiveBounds;
    cylinder->nativePrimitiveInstance->boundsValid=primitiveDefaults->primitiveBoundsValid;
    // Actual Primitive constructor writes min/max/Valid/sphere zero.
    cylinder->nativePrimitiveInstance->bounds.fill(0.0f);cylinder->nativePrimitiveInstance->boundsValid=false;
    engine.values.cylinder=cylinder;
    // Engine may be prepared before OR after the embedding's native client.
    // Join its actual UObject edge, never a registry descriptor or saved patch.
    if(persistentPlayerSession&&persistentPlayerSession->get()&&persistentPlayerSession->get()->client) {
        auto* client=persistentPlayerSession->get()->client;
        if(!persistentNativeClient.HasClient()||client->nativeClientIdentity!=persistentNativeClient.ClientIdentity()||
            host.Object(client->reflection.objectPath)!=client)
            throw std::runtime_error("Native Engine construction encountered a revoked or stale client session");
        engine.values.client=client;
    }
    RuntimeObject* loadedLevel{};
    if(!persistentWorldActors.empty() && persistentWorldActors.front()) {
        loadedLevel=persistentWorldActors.front()->nativeXLevel;
        if(loadedLevel && (!loadedLevel->nativeLevelModel || !loadedLevel->nativeMapGeneration || loadedLevel->nativeLevelEngine ||
            loadedLevel->nativeMapGeneration!=persistentMapGenerationCounter))
            throw std::runtime_error("Native collision Engine cannot bind an incomplete or foreign native Level");
    }
    // Attach payloads only to staged new defaults now. Existing original CDO
    // identities are untouched until every allocating validation has succeeded.
    std::size_t stagedPayloadBytes{};
    for(auto& payload:payloads) {
        if(payload.first->cls->defaultObject==payload.first) {
            stagedPayloadBytes+=sizeof(RuntimePreparedNativeDefaults);
            const auto& values=payload.second->engine;
            for(const auto* text:{&values.protocol,&values.host,&values.map,&values.portal}) stagedPayloadBytes+=text->capacity()+1u;
            for(const auto* list:{&values.options,&values.serverActors,&values.serverPackages}) {
                stagedPayloadBytes+=list->capacity()*sizeof(std::string);for(const auto& text:*list) stagedPayloadBytes+=text.capacity()+1u;
            }
        }else payload.first->preparedNativeDefaults=std::move(payload.second);
    }
    auto owner=std::make_unique<GCRoot<RuntimeEngineCollisionDomain>>(domain);
    auto lookup=StageRuntimeLookupObjects(publications);
    // Reuse spare buckets instead of asking reserve(size+9) to shrink a large
    // already-loaded namespace. Only an actual capacity shortfall needs a new
    // bucket array before the nonallocating node transfer. A failed reserve
    // can retain spare capacity, never a new logical publication.
    const auto oldQualifiedBuckets=persistentQualifiedObjects.bucket_count();
    const auto oldVmBuckets=persistentVmObjects.bucket_count();
    const auto ensureMergeCapacity=[](auto& destination,const std::size_t incoming) {
        if(!incoming) return std::size_t{};
        const auto required=destination.size()+incoming;
        const auto capacity=static_cast<long double>(destination.bucket_count())*
            static_cast<long double>(destination.max_load_factor());
        // An untouched empty standard container may expose only its dummy
        // singleton bucket, without an initialized first-insertion threshold.
        if(!destination.empty() && static_cast<long double>(required)<=capacity) return std::size_t{};
        const auto previous=destination.bucket_count();
        destination.reserve(required);return previous;
    };
    const auto replacedQualifiedBuckets=ensureMergeCapacity(persistentQualifiedObjects,nextQualified.size());
    const auto replacedVmBuckets=ensureMergeCapacity(persistentVmObjects,nextVm.size());
    const auto replacedPackageBuckets=ensureMergeCapacity(persistentNativePackages->objects,nextPackages.size());
    // Conservative peak charge for detached publication dictionaries, source
    // rebindings and temporary GC roots as well as new payloads/index nodes.
    auto stagingBytes=QuestVr::DecalStateDetail::Add(stagedPayloadBytes,ini->RetainedBytes());
    stagingBytes=QuestVr::DecalStateDetail::Add(stagingBytes,lookup?lookup->RetainedBytes():0u);
    stagingBytes=QuestVr::DecalStateDetail::Add(stagingBytes,sizeof(QuestVr::NativePackage::Instance)+
        2u*(sizeof(std::string)+6u*sizeof(void*)+std::string("transient").capacity()+1u));
    stagingBytes=QuestVr::DecalStateDetail::Add(stagingBytes,allocations.capacity()*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+
        classBindings.capacity()*sizeof(decltype(classBindings)::value_type)+publications.capacity()*sizeof(RuntimeObject*));
    for(const auto* dictionary:{&nextQualified,&nextVm,&nextPackages}) {
        stagingBytes=QuestVr::DecalStateDetail::Add(stagingBytes,dictionary->bucket_count()*sizeof(void*));
        for(const auto& [key,value]:*dictionary) {
            static_cast<void>(value);
            stagingBytes=QuestVr::DecalStateDetail::Add(stagingBytes,sizeof(std::string)+6u*sizeof(void*)+key.capacity()+1u);
        }
    }
    // Charge each retained bucket array. Only a performed capacity expansion
    // can also hold an old array during rehash; reusing an unchanged array
    // cannot create a second copy. Node/string ownership itself transfers.
    stagingBytes=QuestVr::DecalStateDetail::Add(stagingBytes,(replacedQualifiedBuckets+replacedVmBuckets+replacedPackageBuckets+
        persistentQualifiedObjects.bucket_count()+persistentVmObjects.bucket_count()+persistentNativePackages->objects.bucket_count())*sizeof(void*));
    try {host.ValidateEngineCollisionBudget(*domain,stagingBytes);}
    catch(const std::exception& error) {
        throw std::runtime_error("Native collision Engine admission (domain="+
            std::to_string(EngineCollisionRetainedBytes(*domain))+", staging="+std::to_string(stagingBytes)+
            ", liveLookup="+std::to_string(persistentObjectLookup->index.RetainedBytes())+
            ", qualifiedBuckets="+std::to_string(oldQualifiedBuckets)+"/"+
            std::to_string(persistentQualifiedObjects.bucket_count())+", foldedBuckets="+
            std::to_string(oldVmBuckets)+"/"+std::to_string(persistentVmObjects.bucket_count())+
            "): "+error.what());
    }
    for(const auto& [field,next]:nextLinks) if(field->nativeProperty) {
        field->nativeProperty->propertyNext=next[0u];field->nativeProperty->configNext=next[1u];field->nativeProperty->constructorNext=next[2u];
    }
    auto result=EngineCollisionSnapshot(*domain,&payloads);
    result.engineUniqueCounter=engineCounter;result.primitiveUniqueCounter=primitiveCounter;
    result.boundLevelPath=NativeObjectPath(loadedLevel);result.levelIndex=loadedLevel?loadedLevel->objectIndex.Index():0u;
    // Nonthrowing publication. No partial class/default/field/Engine/Level
    // capability is exposed on failed source/config/name/budget admission.
    for(auto& payload:payloads) {
        if(payload.second) payload.first->preparedNativeDefaults=std::move(payload.second);
        payload.first->cls->defaultObject=payload.first;
    }
    for(const auto& binding:classBindings) {
        binding.first->cls=binding.second;binding.first->inventoryClassReferenceValid=true;
    }
    for(const auto& [cls,head]:heads) {cls->nativePropertyHeads=head;cls->nativeConstructorLinksPrepared=true;}
    for(const auto& [field,next]:nextLinks) {
        field->nativePropertyNext=next;
        field->nativeConstructorLinksPrepared=true;
        if(field->nativeProperty) {
            field->nativeProperty->propertyNext=next[0u];field->nativeProperty->configNext=next[1u];field->nativeProperty->constructorNext=next[2u];
        }
    }
    gameEngineClass->nativeChildren=serverPackages;
    engineClass->uniqueObjectCounter=engineCounter;primitiveClass->uniqueObjectCounter=primitiveCounter;
    persistentQualifiedObjects.merge(nextQualified);persistentVmObjects.merge(nextVm);
    persistentNativePackages->objects.merge(nextPackages);
    if(!nextQualified.empty() || !nextVm.empty() || !nextPackages.empty()) std::terminate();
    PublishRuntimeLookupObjects(lookup,publications);persistentEngineCollision=std::move(owner);
    if(loadedLevel) {loadedLevel->nativeLevelEngine=domain->engine;engine.currentLevel=loadedLevel;}
    return result;
}
