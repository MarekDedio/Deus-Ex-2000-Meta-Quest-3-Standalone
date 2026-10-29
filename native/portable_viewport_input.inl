// Included after native viewport/session objects and configured Input defaults.
namespace {
void RetireRuntimeInputObject(RuntimeObject* object) noexcept {
    const auto canonical=persistentQualifiedObjects.find(object->reflection.objectPath);
    if(canonical!=persistentQualifiedObjects.end()&&canonical->second==object)persistentQualifiedObjects.erase(canonical);
    const auto alias=persistentVmObjects.find(object->birthVmKey);
    if(alias!=persistentVmObjects.end()&&alias->second==object)persistentVmObjects.erase(alias);
    persistentNativeInputs.erase(std::remove(persistentNativeInputs.begin(),persistentNativeInputs.end(),object),persistentNativeInputs.end());
}
constexpr std::array<const char*,52u> ExtensionInputNameSpellings{{
    "InitWindow","DestroyWindow","WindowReady","DrawWindow","PostDrawWindow",
    "ParentRequestedPreferredSize","ParentRequestedGranularity","ChildRequestedVisibilityChange","ChildRequestedReconfiguration",
    "ChildRequestedShowArea","ConfigurationChanged","VisibilityChanged","SensitivityChanged","ChildAdded","ChildRemoved",
    "DescendantAdded","DescendantRemoved","CursorRequested","MouseMoved","MouseEnteredWindow","MouseLeftWindow",
    "FocusEnteredWindow","FocusLeftWindow","FocusEnteredDescendant","FocusLeftDescendant","Tick","ButtonActivated",
    "ButtonActivatedRight","ToggleChanged","BoxOptionSelected","ScalePositionChanged","ScaleRangeChanged","ScaleAttributesChanged",
    "ClipAttributesChanged","ListRowActivated","ListSelectionChanged","ClipPositionChanged","FilterChar","TextChanged",
    "EditActivated","CalcView","RawMouseButtonPressed","MouseButtonPressed","MouseButtonReleased","RawKeyPressed",
    "VirtualKeyPressed","KeyPressed","AcceleratorKeyPressed","StyleChanged","ComputerStart","ComputerInputFinished","ComputerFadeOutCompleted"
}};
RuntimeObject* RequiredViewportInput(const std::uint64_t identity) {
    auto* viewport=NativeSessionViewport(identity);auto* input=viewport->nativeViewportInput;
    if(!input||!ActualNativeInputInstanceValid(input)||input->nativeInputInstance->ownerViewport!=viewport)
        throw std::runtime_error("Viewport has no current authenticated constructed Input");
    return input;
}
PortableViewportInputSnapshot NativeInputSnapshot(RuntimeObject* object,const RuntimeExtensionInputNames* stagedNames=nullptr,
    const std::int32_t* stagedCounter=nullptr,const bool staged=false) {
    if(!object||!object->nativeInputInstance||(!staged&&!ActualNativeInputInstanceValid(object)))
        throw std::runtime_error("Native Input constructor snapshot lost its actual instance authority");
    const auto& input=*object->nativeInputInstance;const auto& values=input.values;
    if(!object->cls||!object->outer||!input.ownerViewport||!values.keyEnum||!values.keyEnum->enumDescriptor||
        !values.execInstalled||values.viewport||input.initialized||input.retired||!object->lookupName||
        object->reflection.flags!=0x4000u||input.logging!=PortableInputLoggingProfile::Ordinary)
        throw std::runtime_error("Native Input constructor snapshot has invented Init/header/Enum state");
    PortableViewportInputSnapshot result;
    result.prepared=true;result.constructed=true;result.initialized=input.initialized;result.execInstalled=values.execInstalled;
    result.objectPath=object->reflection.objectPath;result.classPath=object->cls->reflection.objectPath;
    result.outerPath=object->outer->reflection.objectPath;result.ownerViewportPath=input.ownerViewport->reflection.objectPath;
    result.enumPath=values.keyEnum->reflection.objectPath;result.objectIndex=object->objectIndex.Index();
    result.classIndex=object->cls->objectIndex.Index();result.outerIndex=object->outer->objectIndex.Index();
    result.ownerViewportIndex=input.ownerViewport->objectIndex.Index();result.enumIndex=values.keyEnum->objectIndex.Index();
    result.nativeViewportIndex=values.viewport?values.viewport->objectIndex.Index():0u;result.objectFlags=object->reflection.flags;
    result.clientIdentity=input.clientIdentity;result.viewportIdentity=input.viewportIdentity;result.lookupGeneration=input.lookupGeneration;
    result.uniqueCounter=stagedCounter?*stagedCounter:object->cls->uniqueObjectCounter;
    result.retainedBytes=NativeSessionObjectBytes(object);result.configReloads=input.configReloads;result.configPasses=input.configPasses;
    result.configImports=input.configImports;result.configFailures=input.configFailures;result.configWarnings=input.configWarnings;result.configWork=input.configWork;
    for(std::size_t i=0u;i<values.aliases.size();++i) {
        const auto& alias=values.aliases[i];result.aliases[i]={alias.alias.ToString(),alias.command,alias.alias.GetCompareIndex()};
    }
    result.bindings=values.bindings;
    const auto* names=stagedNames?stagedNames:persistentInputSchema->get()->extensionNames.get();
    if(object->cls==persistentInputSchema->get()->inputExtClass) {
        if(!names||input.configReloads!=2u||input.configPasses!=4u)
            throw std::runtime_error("Native InputExt constructor lacks actual config reload/name bootstrap effects");
        result.extensionNamesPrepared=true;
        for(std::size_t i=0u;i<names->names.size();++i) {
            const auto& name=names->names[i];
            if(name.IsNone()||NameString::FindCompareIndex(ExtensionInputNameSpellings[i])!=std::optional<int>{name.GetCompareIndex()})
                throw std::runtime_error("Native Extension input bootstrap lost its ordered actual FName identity");
            result.extensionNames[i]=name.ToString();result.extensionNameIndices[i]=name.GetCompareIndex();
        }
    }else if(input.configReloads||input.configPasses)
        throw std::runtime_error("Base native Input ran invented InputExt config callbacks");
    return result;
}
std::size_t NativeInputSnapshotBytes(const PortableViewportInputSnapshot& value) {
    using QuestVr::DecalStateDetail::Add;auto bytes=sizeof(value);
    for(const auto* text:{&value.objectPath,&value.classPath,&value.outerPath,&value.ownerViewportPath,&value.enumPath})bytes=Add(bytes,text->capacity()+1u);
    for(const auto& alias:value.aliases) {bytes=Add(bytes,alias.alias.capacity()+1u);bytes=Add(bytes,alias.command.capacity()+1u);}
    for(const auto& text:value.bindings)bytes=Add(bytes,text.capacity()+1u);
    for(const auto& text:value.extensionNames)bytes=Add(bytes,text.capacity()+1u);
    return bytes;
}
struct NativeInputQueryOperations {
    using AliasName=int;
    struct Name {
        int compareIndex{};const NameString* spelling{};std::size_t slot{};
        bool operator==(const Name& other) const {return compareIndex==other.compareIndex;}
    };
    RuntimeObject* object;
    const std::function<void()>& charge;
    PortableInputQueryResult result;
    bool& AliasGuard() {
        // The original guard is shared by all UInput instances in this engine
        // process. All runtime entry points retain exclusive CPU ownership.
        static bool recursing{};return recursing;
    }
    AliasName FindAliasName(std::u16string_view value) {
        Validate();return NameString::FindCompareIndex(QuestVr::UnrealString::Encode(value)).value_or(0);
    }
    AliasName AliasAt(std::size_t index) {
        Validate();if(index>=40u)throw std::runtime_error("Native Input Alias index exceeds actual fixed storage");
        return object->nativeInputInstance->values.aliases[index].alias.GetCompareIndex();
    }
    void Admit(std::size_t bytes) {admit(QuestVr::DecalStateDetail::Add(scratchBytes,bytes));}
    void ExecInputCommands(std::size_t index) {
        Validate();if(index>=40u)throw std::runtime_error("Native Input Alias command index is invalid");
        throw std::runtime_error("Original Input alias requires its ExecInputCommands/GIsRunning/Action dispatch service: "+
            object->nativeInputInstance->values.aliases[index].alias.ToString());
    }
    const std::function<void(std::size_t)>& admit;
    std::size_t scratchBytes{};
    void Validate() const {
        if(!ActualNativeInputInstanceValid(object))throw std::runtime_error("Native Input query ownership was revoked or replaced");
    }
    void Step() {
        Validate();if(result.work==65536u)throw std::runtime_error("Native Input query work budget exceeded");
        ++result.work;charge();
    }
    std::size_t EnumSize() {Validate();return object->nativeInputInstance->values.keyEnum->enumDescriptor->names.size();}
    Name EnumName(std::size_t index) {
        Validate();const auto& names=object->nativeInputInstance->values.keyEnum->enumDescriptor->names;
        if(index>=names.size())throw std::runtime_error("Native Input query key exceeds actual Enum storage");
        return {names[index].GetCompareIndex(),&names[index],index};
    }
    std::u16string NameText(const Name& name) {
        Validate();const auto& names=object->nativeInputInstance->values.keyEnum->enumDescriptor->names;
        if(name.slot>=names.size()||name.spelling!=&names[name.slot]||name.compareIndex!=names[name.slot].GetCompareIndex())
            throw std::runtime_error("Native Input query spelling is not an actual retained Enum FName");
        return QuestVr::UnrealString::Decode(names[name.slot].ToString());
    }
    Name FindName(std::u16string_view value) {
        Validate();return {NameString::FindCompareIndex(QuestVr::UnrealString::Encode(value)).value_or(0),nullptr,0u};
    }
    std::u16string Binding(std::size_t index) {
        Validate();const auto& bindings=object->nativeInputInstance->values.bindings;
        if(index>=bindings.size())throw std::runtime_error("Native Input binding query exceeds actual fixed storage");
        return QuestVr::UnrealString::Decode(bindings[index]);
    }
    void Log(std::u16string_view value) {
        Validate();value=QuestVr::UnrealString::CString(value);
        if(value.size()>16384u)throw std::runtime_error("Native Input FString output exceeds its retained budget");
        result.output+=QuestVr::UnrealString::Encode(value); // Ordinary output device, no newline.
    }
};
PortableInputQueryResult ExecuteNativeInputQuery(RuntimeObject* input,const std::string& command,
    const std::function<void()>& charge,const std::function<void(std::size_t)>& admit) {
    if(command.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits*3u)
        throw std::runtime_error("Native Input command transport exceeds bound");
    if(!ActualNativeInputInstanceValid(input))throw std::runtime_error("Native Input key query requires a constructed current instance");
    // Meter concrete command/Enum/binding storage, not a fixed worst-case
    //112KiB reservation. Supported bundled GNU/libc++ strings grow by at most
    //2x: include transient old+new Decode buffers (6xcommand bytes), both
    // native name rereads/suffix/Encode copies (24xname bytes), binding Decode/
    // Encode/output copies (20xbinding bytes) and formatted lookup storage.
    // This is the explicit bundled-library growth profile, not an arbitrary
    // C++ implementation's capacity guarantee. No retained limit is raised.
    std::size_t nameBytes{},bindingBytes{};
    const auto& values=input->nativeInputInstance->values;
    for(const auto& name:values.keyEnum->enumDescriptor->names) {
        charge();nameBytes=std::max(nameBytes,name.ToString().size());
    }
    for(const auto& binding:values.bindings) {charge();bindingBytes=std::max(bindingBytes,binding.size());}
    using QuestVr::DecalStateDetail::Add;
    auto scratch=Add(sizeof(NativeInputQueryOperations)+sizeof(std::u16string)+sizeof(PortableInputQueryResult),
        6u*(command.size()+1u));
    scratch=Add(scratch,24u*(nameBytes+1u)+20u*(bindingBytes+1u)+4u*256u);
    admit(scratch);
    const auto units=QuestVr::UnrealString::Decode(command);
    if(units.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits)
        throw std::runtime_error("Native Input command unit limit exceeded");
    NativeInputQueryOperations operations{input,charge,{},admit,scratch};
    for(std::size_t i=0u;i<units.size();++i)operations.Step();
    operations.result.handled=QuestVr::NativeInputKeyQueries::Exec(operations,units);
    if(!operations.result.handled) {
        // These original branches precede key queries/aliases. A recognized
        // unimplemented action is a refusal, never an alias override/decline.
        for(const auto* action:{u"BUTTON",u"PULSE",u"TOGGLE",u"AXIS"}) {
            std::size_t cursor{};operations.Step();
            if(QuestVr::NativeInputKeyQueries::ParseCommand(units,cursor,action))
                throw std::runtime_error("Original native Input action command requires its initialized typed dispatch service");
        }
        operations.result.handled=QuestVr::NativeInputAliasTail::Exec(operations,units);
    }
    operations.Validate();return std::move(operations.result);
}
} // namespace

PortableViewportInputSnapshot ReadPortableRuntimeViewportInput(const std::uint64_t viewportIdentity) {
    auto* viewport=NativeSessionViewport(viewportIdentity);
    if(!viewport->nativeViewportInput)return {};
    return NativeInputSnapshot(RequiredViewportInput(viewportIdentity));
}
PortableInputQueryResult ExecutePortableRuntimeInputQuery(const std::uint64_t viewportIdentity,const std::string& command) {
    PortableActorVmHost host;auto* input=RequiredViewportInput(viewportIdentity);
    return ExecuteNativeInputQuery(input,command,[]{},[&](std::size_t bytes){host.ValidateAdditionalPackageBudget(bytes);});
}
PortableViewportInputSnapshot PreparePortableRuntimeViewportInput(const std::uint64_t viewportIdentity,const PortableInputLoggingProfile logging) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(logging!=PortableInputLoggingProfile::Ordinary)throw std::runtime_error("Native Input output-device profile is not established");
    const auto loadProfile=ReadPortableRuntimeLoadProfile();
    if(loadProfile&&loadProfile->editor)
        throw std::runtime_error("Native Input construction is explicitly bound to standalone non-editor mode");
    auto* viewport=NativeSessionViewport(viewportIdentity);
    if(viewport->nativeViewportInput)return ReadPortableRuntimeViewportInput(viewportIdentity);
    if(!persistentInputSchema||!persistentInputSchema->get()||!persistentInputSchema->get()->config||
        !persistentInputClass||!ReadPortableRuntimeInputSchema().configApplied||!persistentEngineCollision||
        !persistentObjectLookup||!persistentObjectLookup->active)
        throw std::runtime_error("Native Input constructor requires configured defaults, real Transient and lookup domains");
    auto& domain=*persistentInputSchema->get();auto* cls=host.Object(*persistentInputClass);
    if((cls!=domain.inputClass&&cls!=domain.inputExtClass)||!InputSchemaDefaultValid(cls)||
        !cls->nativeMetadataClass||cls->nativeMetadataClass->registrationClassFlags!=0x1eu||
        !cls->defaultObject->preparedNativeDefaults->input)
        throw std::runtime_error("Native Input constructor selected Class/default authority is invalid");
    auto* outer=persistentEngineCollision->get()->transient;const auto client=persistentNativeClient.ClientIdentity();
    auto counter=cls->uniqueObjectCounter;
    const auto name=QuestVr::ObjectNames::MakeUnique(cls->lookupName->ToString(),counter,[&](std::string_view candidate) {
        const auto path=outer->reflection.objectPath+'.'+std::string(candidate);
        return persistentVmObjects.count(LowerAscii(path))||persistentQualifiedObjects.count(path)||IsContextExcludedPath(path);
    });
    const auto path=outer->reflection.objectPath+'.'+name;
    pendingBirthGarbage=true;
    GCRoot<RuntimeObject> staged(GC::Alloc<RuntimeObject>(PortableReflectionObject{
        path,cls->lookupName->ToString(),outer->reflection.objectPath,{},0x4000u,0},nullptr));
    auto* input=staged.get();input->cls=cls;input->outer=outer;input->lookupName=NameString(name);
    input->birthVmKey=LowerAscii(path); // Nonallocating weak-alias retirement; runtimeBirth remains false.
    input->exportIndex=std::numeric_limits<std::size_t>::max();input->nativeClientIdentity=client;
    input->nativeInputInstance=std::make_unique<RuntimeNativeInputInstance>();auto& state=*input->nativeInputInstance;
    state.values=*cls->defaultObject->preparedNativeDefaults->input; // BEFORE InternalConstructor.
    state.ownerViewport=viewport;state.clientIdentity=client;state.viewportIdentity=viewportIdentity;
    state.lookupGeneration=persistentObjectLookup->generation;state.logging=logging;
    // Alias/FStringNoInit constructors preserve copied default contents. Only
    // audited constructor effects are applied; Init's Viewport/reset stay absent.
    state.values.execInstalled=true;state.values.keyEnum=domain.keyEnum;
    const auto foundEnum=FindPortableRuntimeObject({"EInputKey",host.Object("Core.Enum")->objectIndex.Index(),
        host.Object("Engine.Actor")->objectIndex.Index(),false,false});
    if(!foundEnum||foundEnum->objectIndex!=domain.keyEnum->objectIndex.Index())
        throw std::runtime_error("Native Input constructor cannot resolve its actual Actor Enum");
    std::unique_ptr<RuntimePreparedNativeDefaults> reloadedDefault;
    std::unique_ptr<RuntimeExtensionInputNames> names;
    if(cls==domain.inputExtClass) {
        RuntimeNativeInputConfig invocation;
        std::size_t scratchLedger{};
        const auto stagedBytes=[&] {
            auto bytes=QuestVr::DecalStateDetail::Add(NativeSessionObjectBytes(input),sizeof(GCRootNode));
            if(reloadedDefault)bytes=QuestVr::DecalStateDetail::Add(bytes,
                QuestVr::DecalStateDetail::Add(sizeof(RuntimePreparedNativeDefaults),NativeInputValuesBytes(*reloadedDefault->input)));
            if(names)bytes=QuestVr::DecalStateDetail::Add(bytes,sizeof(RuntimeExtensionInputNames));
            return bytes;
        };
        const std::function<void(std::size_t)> scratchValidate=[&](std::size_t extra) {
            host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(stagedBytes(),
                QuestVr::DecalStateDetail::Add(scratchLedger,extra)));
        };
        invocation.scratchLedger=&scratchLedger;invocation.scratchValidate=&scratchValidate;
        host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(stagedBytes(),
            QuestVr::DecalStateDetail::Add(sizeof(RuntimePreparedNativeDefaults),
                NativeInputValuesBytes(*cls->defaultObject->preparedNativeDefaults->input))));
        reloadedDefault=CopyInputDefault(*cls->defaultObject->preparedNativeDefaults);
        LoadInputDefaultConfig(domain,invocation,cls,*reloadedDefault->input,0u,&domain.config->document);
        LoadInputDefaultConfig(domain,invocation,cls,state.values,0u,&domain.config->document);
        state.configReloads=2u;state.configPasses=invocation.passes;state.configImports=invocation.imports;
        state.configFailures=invocation.failures;state.configWarnings=invocation.warnings;state.configWork=invocation.work;
        if(!domain.extensionNames) {
            names=std::make_unique<RuntimeExtensionInputNames>();
            for(std::size_t i=0u;i<names->names.size();++i)names->names[i]=NameString(ExtensionInputNameSpellings[i]);
        }
    }
    std::vector<RuntimeObject*> publications{input};auto lookup=StageRuntimeLookupObjects(publications);
    if(!lookup)throw std::runtime_error("Native Input constructor lacks actual lookup publication authority");
    auto nextInputs=persistentNativeInputs;nextInputs.push_back(input);
    decltype(persistentQualifiedObjects) deltaQualified,replacementQualified;
    decltype(persistentVmObjects) deltaVm,replacementVm;
    if(!deltaQualified.emplace(path,input).second||!deltaVm.emplace(LowerAscii(path),input).second)
        throw std::runtime_error("Native Input constructor staged namespace collision");
    replacementQualified.max_load_factor(persistentQualifiedObjects.max_load_factor());
    replacementVm.max_load_factor(persistentVmObjects.max_load_factor());
    replacementQualified.reserve(persistentQualifiedObjects.size()+1u);replacementVm.reserve(persistentVmObjects.size()+1u);
    auto result=NativeInputSnapshot(input,names.get(),&counter,true);
    using QuestVr::DecalStateDetail::Add;
    auto extra=Add(NativeSessionObjectBytes(input),NativeInputSnapshotBytes(result));
    extra=Add(extra,lookup->RetainedBytes()+nextInputs.capacity()*sizeof(RuntimeObject*)+sizeof(GCRootNode)+
        publications.capacity()*sizeof(RuntimeObject*)+64u*1024u);
    if(reloadedDefault)extra=Add(extra,sizeof(RuntimePreparedNativeDefaults)+NativeInputValuesBytes(*reloadedDefault->input));
    if(names)extra=Add(extra,sizeof(RuntimeExtensionInputNames));
    extra=Add(extra,(persistentQualifiedObjects.bucket_count()+persistentVmObjects.bucket_count()+deltaQualified.bucket_count()+
        deltaVm.bucket_count()+replacementQualified.bucket_count()+replacementVm.bucket_count())*sizeof(void*));
    host.ValidateAdditionalPackageBudget(extra);
    if(client!=persistentNativeClient.ClientIdentity()||NativeSessionViewport(viewportIdentity)!=viewport||viewport->nativeViewportInput||
        state.lookupGeneration!=persistentObjectLookup->generation)
        throw std::runtime_error("Native Input owner/generation changed before publication");
    std::unique_ptr<const RuntimePreparedNativeDefaults> committedDefault(std::move(reloadedDefault));
    std::unique_ptr<const RuntimeExtensionInputNames> committedNames(std::move(names));
    // No allocating or throwing work follows. Inventory is weak; viewport.Input
    // is the genuine owning edge. Existing CDO UObject identities are retained.
    replacementQualified.merge(persistentQualifiedObjects);replacementQualified.merge(deltaQualified);
    replacementVm.merge(persistentVmObjects);replacementVm.merge(deltaVm);
    if(!persistentQualifiedObjects.empty()||!persistentVmObjects.empty()||!deltaQualified.empty()||!deltaVm.empty())std::terminate();
    persistentQualifiedObjects.swap(replacementQualified);persistentVmObjects.swap(replacementVm);
    PublishRuntimeLookupObjects(lookup,publications);persistentNativeInputs.swap(nextInputs);
    if(committedDefault)cls->defaultObject->preparedNativeDefaults.swap(committedDefault);
    if(committedNames)domain.extensionNames.swap(committedNames);
    cls->uniqueObjectCounter=counter;viewport->nativeViewportInput=input;
    return result;
}
