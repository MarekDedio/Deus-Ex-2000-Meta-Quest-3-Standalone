// Real native Input defaults and an explicit immutable User.ini provider.
// Included after portable_input_schema.inl. Constructor/Init and live input
// events are separate services; this never fabricates them from a key query.
namespace {
constexpr std::size_t InputConfigWorkLimit=16u<<20u;
void InputConfigStep(RuntimeNativeInputConfig& config,const std::size_t amount=1u) {
    if(amount>InputConfigWorkLimit-config.work)
        throw std::runtime_error("Native Input config exceeds its shared import work budget");
    if(config.charge)(*config.charge)(amount);
    config.work+=amount;
}

std::unique_ptr<RuntimePreparedNativeDefaults> CopyInputDefault(const RuntimePreparedNativeDefaults& source) {
    if(!source.input||(source.kind!=RuntimeNativeDefaultKind::Input&&source.kind!=RuntimeNativeDefaultKind::InputExt))
        throw std::runtime_error("Native Input config cannot copy an unauthenticated default payload");
    auto result=std::make_unique<RuntimePreparedNativeDefaults>();
    result->kind=source.kind;result->owner=source.owner;result->superDefault=source.superDefault;
    result->nativeSize=source.nativeSize;result->classFlags=source.classFlags;result->engine=source.engine;
    result->propertyDimension=source.propertyDimension;result->propertyElementSize=source.propertyElementSize;
    result->propertyFlags=source.propertyFlags;result->propertyOffset=source.propertyOffset;
    result->structPropertiesSize=source.structPropertiesSize;result->structFriendlyName=source.structFriendlyName;
    result->primitiveBounds=source.primitiveBounds;result->primitiveBoundsValid=source.primitiveBoundsValid;
    result->input=std::make_unique<RuntimeNativeInputValues>(*source.input);
    return result;
}

struct InputAliasConfigImport {
    using Name=int;
    using StructureType=RuntimeObject*;
    using Field=RuntimeObject*;
    const RuntimeNativeInputDomain& domain;
    RuntimeNativeInputConfig& config;
    RuntimeNativeInputAlias& destination;
    std::size_t arrayIndex{};
    RuntimeObject* containingProperty{};
    void Store(Field member,const QuestVr::Vm::Value& value) {
        if(config.store) {
            if(!containingProperty)throw std::runtime_error("Native Alias write has no containing property authority");
            (*config.store)(containingProperty,arrayIndex,member,value);
        }else if(member==domain.fields[0u])destination.alias=NameString(value.text);
        else if(member==domain.fields[1u])destination.command=value.text;
        else throw std::runtime_error("Native Alias write has no actual member authority");
    }
    void Step() {InputConfigStep(config);}
    StructureType Structure() const {return domain.aliasStruct;}
    Field Child(StructureType structure) const {
        if(structure!=domain.aliasStruct||!ActualNativeInputSchemaObjectValid(structure)||!structure->nativeStruct)
            throw std::runtime_error("Native Input Alias import lost its actual UStruct authority");
        return structure->nativeChildren;
    }
    StructureType Super(StructureType structure) const {
        if(structure!=domain.aliasStruct||!structure->nativeStruct)
            throw std::runtime_error("Native Input Alias import has a foreign Super receiver");
        return structure->nativeStruct->super;
    }
    bool IsProperty(Field field) const {
        if((field!=domain.fields[0u]&&field!=domain.fields[1u])||!ActualNativeInputSchemaObjectValid(field)||!field->nativeProperty)
            throw std::runtime_error("Native Input Alias import encountered a foreign member");
        return true;
    }
    Field Next(Field field) const {static_cast<void>(IsProperty(field));return field->nativeProperty->next;}
    Name FieldName(Field field) const {static_cast<void>(IsProperty(field));return field->lookupName->GetCompareIndex();}
    Name FindName(std::u16string_view spelling) {
        InputConfigStep(config,spelling.size());
        return NameString::FindCompareIndex(QuestVr::UnrealString::Encode(spelling)).value_or(0);
    }
    std::uint32_t Dimension(Field field) const {static_cast<void>(IsProperty(field));return field->nativeProperty->dimension;}
    std::uint32_t ElementSize(Field field) const {static_cast<void>(IsProperty(field));return field->nativeProperty->elementSize;}
    bool Port(Field field) const {
        static_cast<void>(IsProperty(field));const auto& property=*field->nativeProperty;
        if(static_cast<std::uint32_t>(property.elementSize*property.dimension)==0u)return false;
        const auto category=NameString::FindCompareIndex(property.category);
        if(!category)throw std::runtime_error("Native Input Alias category has no actual FName identity");
        if((property.flags&0x3000u)&&*category==0)return false;
        const auto className=NameString::FindCompareIndex("Class");
        if(!className)throw std::runtime_error("Native Input Alias Port lacks actual Class FName identity");
        return field->lookupName->GetCompareIndex()!=*className;
    }
    QuestVr::NativeConfigImport::Result Import(Field field,const std::size_t index,
        std::u16string_view value,const std::uint32_t flags) {
        static_cast<void>(IsProperty(field));InputConfigStep(config,value.size());
        if(index||(flags!=2u&&flags!=3u))throw std::runtime_error("Native Input Alias member import has an unaudited index/flags profile");
        // Command ImportText first decodes the PREVIOUS destination, before
        // tokenizing/replacing it. That storage is independent of a short SET
        // command or the1023-unit scalar source and cannot hide in its budget.
        const auto oldCommand=field==domain.fields[1u]?destination.command.size():0u;
        InputConfigScratch importScratch(config,QuestVr::DecalStateDetail::Add(8192u,
            QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(
                QuestVr::DecalStateDetail::Add(value.size(),oldCommand),1u),16u)));
        if(field==domain.fields[0u]) {
            auto name=destination.alias;
            const auto result=QuestVr::NativeConfigImport::Name(value,name,[](std::u16string_view spelling) {
                return NameString(QuestVr::UnrealString::Encode(spelling));
            });
            if(result.returned)Store(field,QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Name,name.ToString()));
            return result;
        }
        std::u16string command=QuestVr::UnrealString::Decode(destination.command);
        const auto result=QuestVr::NativeConfigImport::String(value,flags,command);
        if(result.returned)Store(field,QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::String,QuestVr::UnrealString::Encode(command)));
        return result;
    }
    void Warning(QuestVr::NativeConfigImport::StructImportWarning) {InputConfigStep(config);++config.warnings;}
};

QuestVr::NativeConfigImport::Result ImportNativeInputAlias(const RuntimeNativeInputDomain& domain,
    RuntimeNativeInputConfig& config,RuntimeNativeInputAlias& destination,std::size_t index,
    RuntimeObject* property,std::u16string_view value,std::uint32_t flags) {
    if(property!=domain.fields[2u]||(flags!=0u&&flags!=1u))
        throw std::runtime_error("Native Alias import lacks its actual Config/SET StructProperty authority");
    InputAliasConfigImport operations{domain,config,destination,index,property};
    return QuestVr::NativeConfigImport::Struct(value,flags,operations);
}

// Native LoadConfig(Propagate=1) recursively visits Super on the SAME receiver.
// InputExt inherits Input's Config list; ordinary Config uses each supplied
// class's section, while GlobalConfig uses the actual declaring class.
void LoadInputDefaultConfig(const RuntimeNativeInputDomain& domain,RuntimeNativeInputConfig& config,
    RuntimeObject* suppliedClass,RuntimeNativeInputValues& destination,const std::size_t depth=0u,
    const QuestVr::NativeConfig::OrderedIni* suppliedDocument=nullptr) {
    const auto& document=suppliedDocument?*suppliedDocument:config.document;
    InputConfigStep(config);
    if(!suppliedClass||depth>=128u)
        throw std::runtime_error("Native Input config lacks bounded actual Class/Super authority");
    const auto flags=suppliedClass->classDescriptor?suppliedClass->classDescriptor->classFlags:
        suppliedClass->nativeMetadataClass?suppliedClass->nativeMetadataClass->registrationClassFlags:0u;
    if(!suppliedClass->classDescriptor&&!suppliedClass->nativeMetadataClass)
        throw std::runtime_error("Native Input config Class flags have no actual source/registration authority");
    if(!(flags&4u))return;
    if(suppliedClass!=domain.inputClass&&suppliedClass!=domain.inputExtClass)
        throw std::runtime_error("Native Input config encountered an unaudited Config-class ancestor");
    LoadInputDefaultConfig(domain,config,suppliedClass->base,destination,depth+1u,&document);
    if(!suppliedClass->nativeConstructorLinksPrepared)
        throw std::runtime_error("Native Input config requires actual linked Config properties");
    ++config.passes;std::size_t fields{};
    for(auto* field=suppliedClass->nativePropertyHeads[1u];field;field=field->nativeProperty->configNext) {
        InputConfigStep(config);
        if(++fields>8192u||!ActualNativeInputSchemaObjectValid(field)||!field->nativeProperty||!field->lookupName)
            throw std::runtime_error("Native Input Config list has a foreign/cyclic/unprepared property");
        const auto& property=*field->nativeProperty;
        if(!(property.flags&0x4000u)||property.configNext!=field->nativePropertyNext[1u]||
            field->outer!=domain.inputClass||field->linkedClassOwner!=field->outer)
            throw std::runtime_error("Native Input Config property lost its declaring Class/list authority");
        const auto* section=(property.flags&0x40000u)?field->outer:suppliedClass;
        for(std::size_t index=0u;index<property.dimension;++index) {
            InputConfigStep(config,document.EntryCount()+document.SectionCount()+1u);
            // Literal native section/key names and dimension suffixes are
            // small, but Scalar's name normalization allocates before it
            // returns its BORROWED value view. Meter that query first.
            InputConfigScratch queryScratch(config,1024u);
            auto key=field->lookupName->ToString();
            if(property.dimension>1u)key+='['+std::to_string(index)+']';
            const auto raw=document.Scalar(section->reflection.objectPath,key);
            if(!raw)continue;
            // PropertyScalar decodes the complete original value BEFORE its
            // GetString1024 truncation. Raw transport can be much larger than
            // both the resulting scalar and the current SET command.
            InputConfigScratch valueScratch(config,QuestVr::DecalStateDetail::Add(8192u,
                QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(raw->size(),1u),16u)));
            InputConfigStep(config,document.EntryCount()+document.SectionCount()+1u);
            const auto value=document.PropertyScalar(section->reflection.objectPath,key);
            if(!value)throw std::runtime_error("Native Input config immutable scalar changed between view/import");
            const auto units=QuestVr::UnrealString::Decode(*value);InputConfigStep(config,units.size());++config.imports;
            QuestVr::NativeConfigImport::Result imported;
            if(field==domain.fields[2u]&&property.dimension==40u&&property.elementSize==0x10u&&
                property.offset==0x2cu&&property.referencedStruct==domain.aliasStruct&&field->cls==domain.defaultClasses[2u]) {
                imported=ImportNativeInputAlias(domain,config,destination.aliases[index],index,field,units,0u);
            }else if(field->cls->reflection.objectPath=="Core.StrProperty"&&property.dimension==1u&&property.elementSize==12u&&
                property.offset>=0x2acu&&(property.offset-0x2acu)%12u==0u&&
                (property.offset-0x2acu)/12u<destination.bindings.size()&&!property.referencedStruct) {
                const auto slot=(property.offset-0x2acu)/12u;std::u16string text;
                imported=QuestVr::NativeConfigImport::String(units,0u,text);
                if(imported.returned) {
                    auto value=QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::String,QuestVr::UnrealString::Encode(text));
                    if(config.store)(*config.store)(field,index,nullptr,value);
                    else destination.bindings[slot]=std::move(value.text);
                }
            }else throw std::runtime_error("Native Input Config property has no audited typed storage slot");
            // Retail ignores ImportText's pointer. Normal malformed structs
            // retain their member-prefix writes and do not abort later fields.
            if(!imported.returned)++config.failures;
        }
    }
}
} // namespace

PortableInputSchemaSnapshot PreparePortableRuntimeInputConfig(const std::string& userIniPath) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(userIniPath.empty()||userIniPath.size()>8192u||userIniPath.find('\0')!=std::string::npos)
        throw std::runtime_error("Native Input config requires an explicit bounded User.ini path");
    const auto supplied=std::filesystem::absolute(userIniPath).lexically_normal().string();
    if(!persistentInputSchema||!persistentInputSchema->get())
        throw std::runtime_error("Native Input config requires the actual prepared native Input schema/default graph");
    auto& domain=*persistentInputSchema->get();
    if(domain.config) {
        const auto& bound=domain.config->seedPath.empty()?domain.config->sourcePath:domain.config->seedPath;
        if(bound!=supplied)
            throw std::runtime_error("Native Input config is already bound to another explicit User.ini source");
        return ReadPortableRuntimeInputSchema();
    }
    static_cast<void>(ReadPortableRuntimeInputSchema());
    std::optional<QuestVr::NativeConfig::OrderedIni> document;
    std::string effective=supplied;
    if(persistentUserConfigStorage) {
        const auto& storage=*persistentUserConfigStorage;
        if(storage.seedPath!=supplied)
            throw std::runtime_error("Input config seed differs from its explicit app-owned backend binding");
        // Backend read, raw snapshot, decoded file, parser vectors/strings and
        // retained document can coexist. Admit the same fixed portable caps
        // BEFORE reading/decoding, not after parser allocations have happened.
        host.ValidateAdditionalPackageBudget(3u*QuestVr::ConfigFile::MaximumBytes+
            QuestVr::NativeConfig::Limits{}.retainedBytes);
        const auto snapshot=storage.backend.Read();
        if(snapshot.Data()!=storage.baseline)
            throw std::runtime_error("Input config backing changed before initial cache preparation");
        const auto& bytes=snapshot.Data();
        document=QuestVr::NativeConfig::OrderedIni::Parse(std::string_view(
            bytes.empty()?"":reinterpret_cast<const char*>(bytes.data()),bytes.size()));
        if(snapshot.Owned())effective=storage.targetPath;
    }else document=QuestVr::NativeConfig::ReadOrderedIni(supplied);
    if(!document)throw std::runtime_error("Native Input supplied User.ini is missing");
    auto config=std::make_unique<RuntimeNativeInputConfig>();config->sourcePath=effective;config->document=std::move(*document);
    if(persistentUserConfigStorage)config->seedPath=supplied;
    auto input=CopyInputDefault(*domain.defaults[3u]->preparedNativeDefaults);
    auto extension=CopyInputDefault(*domain.defaults[4u]->preparedNativeDefaults);
    const auto stagedBytes=[&] {
        auto bytes=QuestVr::DecalStateDetail::Add(NativeInputConfigBytes(*config),2u*sizeof(RuntimePreparedNativeDefaults));
        bytes=QuestVr::DecalStateDetail::Add(bytes,NativeInputValuesBytes(*input->input));
        return QuestVr::DecalStateDetail::Add(bytes,NativeInputValuesBytes(*extension->input));
    };
    host.ValidateAdditionalPackageBudget(stagedBytes());
    {
        std::size_t scratchLedger{};
        const std::function<void(std::size_t)> scratchValidate=[&](std::size_t extra) {
            host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(stagedBytes(),
                QuestVr::DecalStateDetail::Add(scratchLedger,extra)));
        };
        struct RestoreContext {
            RuntimeNativeInputConfig& config;
            std::size_t* ledger;
            const std::function<void(std::size_t)>* validate;
            ~RestoreContext(){config.scratchLedger=ledger;config.scratchValidate=validate;}
        } restore{*config,config->scratchLedger,config->scratchValidate};
        config->scratchLedger=&scratchLedger;config->scratchValidate=&scratchValidate;
        LoadInputDefaultConfig(domain,*config,domain.inputClass,*input->input);
        // StaticInitInput config precedes subclass default copying. Admit the
        // new copy while the old descendant payload still coexists with it.
        host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(stagedBytes(),
            QuestVr::DecalStateDetail::Add(NativeInputValuesBytes(*input->input),scratchLedger)));
        extension->input=std::make_unique<RuntimeNativeInputValues>(*input->input);
        LoadInputDefaultConfig(domain,*config,domain.inputExtClass,*extension->input);
    } // Borrowed scratch context is cleared before snapshot/publication.
    const InputConfigPayloads payloads{input.get(),extension.get()};
    auto result=InputSchemaSnapshot(domain,nullptr,nullptr,&payloads,config.get());
    // Live+staged payloads coexist. The result and bounded import scratch are
    // admitted before any live default/config pointer changes. Originals and
    // the process's preexisting default object/index identities stay untouched.
    auto peak=QuestVr::DecalStateDetail::Add(stagedBytes(),InputSchemaSnapshotBytes(result));
    peak=QuestVr::DecalStateDetail::Add(peak,64u<<10u);
    host.ValidateAdditionalPackageBudget(peak);
    std::unique_ptr<const RuntimePreparedNativeDefaults> committedInput(std::move(input)),committedExtension(std::move(extension));
    std::unique_ptr<const RuntimeNativeInputConfig> committedConfig(std::move(config));
    domain.defaults[3u]->preparedNativeDefaults.swap(committedInput);
    domain.defaults[4u]->preparedNativeDefaults.swap(committedExtension);
    domain.config.swap(committedConfig);
    return result;
}
