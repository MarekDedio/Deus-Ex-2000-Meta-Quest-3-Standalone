// PortableActorVmHost: actual USystem payload reads and FExec discriminator.
// appInit's later Suppress FName flag writes and command bodies are separate;
// preparing the constructor/root prefix does not claim those services exist.
    std::optional<Value> ReadNativeSystemValue(RuntimeObject* object,const Property& property,
        std::uint32_t index,bool defaults,bool immutable) {
        if(!persistentSystem||!persistentSystem->get())return std::nullopt;
        const auto& domain=*persistentSystem->get();
        const bool cls=object==domain.cls,instance=object==domain.instance,cdo=object==domain.defaults;
        if(!cls&&!instance&&!cdo)return std::nullopt;
        if(!NativeSystemDefaultValid(domain.cls)||(instance&&!ActualNativeSystemObjectValid(object))||immutable)
            throw std::runtime_error("Native System read lacks its actual rooted constructor/default authority");
        if(index||property.arrayDimension!=1u)
            throw std::runtime_error("Native System reflected slot is not scalar");
        const auto key=LowerAscii(property.key);
        if(key.rfind("core.object.",0u)==0u) {
            auto* owner=cls&&!defaults?object->cls:domain.cls;
            if(!OwnsProperty(owner,property))throw std::runtime_error("System header has no actual declaring field");
            const bool defaultHeader=cdo||(cls&&defaults);
            if(key=="core.object.class")return Value::Text(Kind::Object,defaultHeader?domain.cls->reflection.objectPath:object->cls->reflection.objectPath);
            if(key=="core.object.name")return Value::Text(Kind::Name,defaultHeader?"None":object->lookupName->ToString());
            if(key=="core.object.outer")return Value::Text(Kind::Object,defaultHeader||!object->outer?std::string{}:object->outer->reflection.objectPath);
            if(key=="core.object.objectflags")return Value::Integer(defaultHeader?0:std::bit_cast<std::int32_t>(object->reflection.flags));
            throw std::runtime_error("System raw UObject pointer/index storage is not script-visible");
        }
        if(cls&&!defaults)throw std::runtime_error("System Class object is not an instance/default value query");
        if(!OwnsProperty(domain.cls,property))throw std::runtime_error("System value has no authenticated native field");
        const auto* values=instance&&!defaults?&domain.instanceValues:&domain.defaultValues;
        if(key=="core.system.purgecachedays")return Value::Integer(values->purgeCacheDays);
        if(key=="core.system.savepath")return Value::Text(Kind::String,values->savePath);
        if(key=="core.system.cachepath")return Value::Text(Kind::String,values->cachePath);
        if(key=="core.system.cacheext")return Value::Text(Kind::String,values->cacheExt);
        throw std::runtime_error("System non-INT dynamic array/native storage has no generic VM value");
    }
    bool SystemCommandExec(const std::string& command,bool& handled) {
        if(!persistentSystem||!persistentSystem->get()||!persistentSystem->get()->execInstalled||
            !ActualNativeSystemObjectValid(persistentSystem->get()->instance))
            throw std::runtime_error("Original UEngine GSys command provider is not prepared: "+command);
        if(command.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits*3u)
            throw std::runtime_error("System command transport exceeds its bounded FString profile");
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,8u*(command.size()+1u)+4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto text=QuestVr::UnrealString::Decode(command);
        struct Operations {
            PortableActorVmHost& host;bool& handled;
            void Step(){host.LifecycleStep();}
            [[noreturn]] void Match(std::u16string_view token,std::size_t) {
                handled=true;
                throw std::runtime_error("Original System native command service is not implemented: "+QuestVr::UnrealString::Encode(token));
            }
        } operations{*this,handled};
        static constexpr std::array<std::u16string_view,6u> commands{{u"MEMSTAT",u"CONFIGHASH",u"EXIT",u"QUIT",u"RELAUNCH",u"DEBUG"}};
        std::size_t cursor{};
        return QuestVr::EngineExecCommands::Exec(operations,text,cursor,commands);
    }
