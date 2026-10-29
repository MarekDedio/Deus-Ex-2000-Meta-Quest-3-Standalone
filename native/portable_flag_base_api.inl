PortableFlagBaseClassSnapshot PreparePortableRuntimeFlagBaseClasses() {
    if(!persistentRuntime||!persistentRuntime->get()||!persistentObjectLookup)
        throw std::runtime_error("FlagBase classes require original runtime/Class/default/lookup phases");
    PortableActorVmHost host;auto* cls=host.Object("Extension.FlagBase");host.AuthenticateFlagBaseClass(cls);
    host.AuthenticatePlayerFlagBaseDefault();
    auto* cdo=cls->defaultObject;
    if(cdo->nativeFlagBase) {
        if(!cdo->nativeFlagBase->defaultCopyVerified||cdo->nativeFlagBase->constructorCompleted||
            cdo->nativeFlagBase->defaultExpiration||std::any_of(cdo->nativeFlagBase->buckets.begin(),cdo->nativeFlagBase->buckets.end(),[](auto* p){return p!=nullptr;}))
            throw std::runtime_error("Prepared FlagBase CDO changed its original zero-extension baseline");
        return {true,0x12cu,64u};
    }
    host.ValidateAdditionalPackageBudget(sizeof(RuntimeNativeFlagBaseState));
    auto payload=std::make_unique<RuntimeNativeFlagBaseState>();payload->defaultCopyVerified=true;
    cdo->nativeFlagBase=std::move(payload);return {true,0x12cu,64u};
}
std::vector<PortableFlagBaseSnapshot> ReadPortableRuntimeFlagBases() {
    std::vector<PortableFlagBaseSnapshot> result;if(!persistentRuntime||!persistentRuntime->get())return result;
    PortableActorVmHost host;
    auto bytes=QuestVr::DecalStateDetail::Multiply(persistentFlagBases.size(),sizeof(PortableFlagBaseSnapshot));
    for(const auto* object:persistentFlagBases) {
        if(!object||!object->cls||!object->outer)throw std::runtime_error("FlagBase snapshot lost native object/header authority");
        bytes=QuestVr::DecalStateDetail::Add(bytes,4u*(object->reflection.objectPath.size()+
            object->cls->reflection.objectPath.size()+object->outer->reflection.objectPath.size()+3u));
    }
    auto scratch=host.ReserveStringToNameDiagnosticScratch(bytes);result.reserve(persistentFlagBases.size());
    for(auto* object:persistentFlagBases)result.push_back(host.FlagBaseSnapshot(object));
    return result;
}
PortableFlagBaseProbeResult ExecutePortableRuntimeFlagBaseProbe(const std::string& outerPath,
    const std::vector<std::pair<std::string,std::uint8_t>>& names,PortableCoreExecMode mode,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableFlagBaseProbeResult result;
    try {
        if(mode!=PortableCoreExecMode::RollbackOnly&&mode!=PortableCoreExecMode::Commit)
            throw std::runtime_error("FlagBase probe requires an explicit root commit/rollback mode");
        if(outerPath.empty()||outerPath.size()>1024u||outerPath.find('\0')!=std::string::npos||names.size()>1024u)
            throw std::runtime_error("FlagBase probe needs bounded actual Outer/query inputs");
        std::size_t bytes=16384u+16u*(outerPath.size()+1u)+names.size()*sizeof(bool);
        for(const auto& query:names){if(query.first.size()>limits.stringBytes)throw std::runtime_error("FlagBase query Name exceeds VM text budget");bytes=QuestVr::DecalStateDetail::Add(bytes,16u*(query.first.size()+1u));}
        PortableActorVmHost host;auto scratch=host.ReserveStringToNameDiagnosticScratch(bytes);
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            auto* object=host.ConstructFlagBase(host.Object(outerPath));result.object=host.FlagBaseSnapshot(object);
            for(const auto& query:names) {
                const auto name=execution.CallFunction(*host.OriginalStringToNameDeclaration(nullptr),
                    "@ClassDefault:Extension.ExtensionObject",{{QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::String,query.first),{}}});
                const auto value=execution.CallFunction(*host.FlagBaseCheckDeclaration(),object->reflection.objectPath,
                    {{name,{}},{QuestVr::Vm::Value::Byte(query.second),{}}});
                if(value.kind!=QuestVr::Vm::Kind::Bool)throw std::runtime_error("CheckFlag did not return Bool");
                result.matches.push_back(value.boolean);
            }
            result.completed=true;
            if(mode==PortableCoreExecMode::RollbackOnly)throw std::runtime_error("Original FlagBase construction/query completed; diagnostic root rollback required");
            host.ValidateAdditionalPackageBudget(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode)+8u*sizeof(void*));
            result.diagnosticLifetime=std::make_shared<GCRoot<RuntimeObject>>(object);
            return QuestVr::Vm::Value::Bool(true);
        },limits);
        if(!result.execution.passed())result.diagnosticLifetime.reset();
    }catch(const std::exception& error){result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();}
    return result;
}
