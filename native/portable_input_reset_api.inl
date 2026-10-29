namespace {
PortableInputResetSourceSnapshot InputResetSourceSnapshot(const RuntimeNativeInputResetSource& reset) {
    return {true,reset.path,reset.sourceBytes,reset.document.SectionCount(),reset.document.EntryCount(),NativeInputResetSourceBytes(reset)};
}
}
PortableInputResetSourceSnapshot ReadPortableRuntimeInputResetConfigSource() {
    if(!persistentInputSchema||!persistentInputSchema->get()||!persistentInputSchema->get()->resetSource)return {};
    return InputResetSourceSnapshot(*persistentInputSchema->get()->resetSource);
}
PortableInputResetSourceSnapshot PreparePortableRuntimeInputResetConfigSource(const std::string& defUserIniPath) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    if(!persistentInputSchema||!persistentInputSchema->get()||!persistentInputSchema->get()->config)
        throw std::runtime_error("Input ResetConfig source requires the actual prepared Input schema/User cache");
    if(defUserIniPath.empty()||defUserIniPath.size()>8192u||defUserIniPath.find('\0')!=std::string::npos)
        throw std::runtime_error("Input ResetConfig requires an explicit bounded absolute DefUser source path");
    auto& domain=*persistentInputSchema->get();
    host.ValidateAdditionalPackageBudget(sizeof(RuntimeNativeInputResetSource)+16u*(defUserIniPath.size()+1u));
    const auto path=std::filesystem::path(defUserIniPath).lexically_normal();
    if(!path.is_absolute())throw std::runtime_error("Input ResetConfig source path is not absolute");
    if(domain.resetSource) {
        domain.resetSource->source.RevalidateAdmitted(path,[&](std::size_t bytes){host.ValidateAdditionalPackageBudget(bytes);});
        return ReadPortableRuntimeInputResetConfigSource();
    }
    auto reset=std::make_unique<RuntimeNativeInputResetSource>();reset->path=path.string();
    reset->source=QuestVr::ConfigFile::ReadOnlySource::BindAdmitted(path,[&](std::size_t bytes) {
        host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(sizeof(*reset)+reset->path.capacity()+1u,bytes));
    });
    const auto& bytes=reset->source.ReadAdmitted([&](std::size_t extra) {
        host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(NativeInputResetSourceBytes(*reset),extra));
    });
    reset->sourceBytes=bytes.size();
    // OrderedIni's parser owns decoded UTF16/transport/entry staging. Admit
    // those existing fixed caps BEFORE parsing while the pinned raw cache is
    // retained; the normal combined native-state cap is unchanged.
    host.ValidateAdditionalPackageBudget(QuestVr::DecalStateDetail::Add(NativeInputResetSourceBytes(*reset),
        QuestVr::DecalStateDetail::Add(16u*(bytes.size()+1u),QuestVr::NativeConfig::Limits{}.retainedBytes)));
    reset->document=QuestVr::NativeConfig::OrderedIni::Parse(std::string_view(
        bytes.empty()?"":reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    host.ValidateAdditionalPackageBudget(NativeInputResetSourceBytes(*reset)+sizeof(PortableInputResetSourceSnapshot)+2u*(reset->path.size()+1u));
    const auto result=InputResetSourceSnapshot(*reset);
    host.ValidateAdditionalPackageBudget(NativeInputResetSourceBytes(*reset)+sizeof(result)+result.sourcePath.capacity()+1u);
    domain.resetSource=std::move(reset);return result;
}
PortableInputResetResult ExecutePortableRuntimeInputResetConfig(const std::string& classPath,
    PortableCoreExecMode mode,const QuestVr::Vm::Limits& limits,const PortableInputResetCommands& commands) {
    CollectBirthGarbageAtBoundary collect;PortableInputResetResult result;
    try {
        if(mode!=PortableCoreExecMode::RollbackOnly&&mode!=PortableCoreExecMode::Commit)
            throw std::runtime_error("Input ResetConfig requires an explicit commit/rollback policy");
        if(classPath.empty()||classPath.size()>1024u||classPath.find('\0')!=std::string::npos)
            throw std::runtime_error("Input ResetConfig requires a bounded actual Class path");
        PortableActorVmHost host;
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            if(commands.before&&!host.CoreStaticExec(*commands.before,result.beforeCommand,execution))
                throw std::runtime_error("Input ResetConfig before-command is not an actual Core SET command");
            host.ValidateAdditionalPackageBudget(2u*(classPath.size()+1u));
            host.ResetInputClassConfig(host.Object(classPath),execution,result);
            if(commands.after&&!host.CoreStaticExec(*commands.after,result.afterCommand,execution))
                throw std::runtime_error("Input ResetConfig after-command is not an actual Core SET command");
            result.cacheDirty=persistentInputSchema->get()->config->cacheDirty;
            result.forwardNativeWork=host.ReadNativeWork()->used;
            if(mode==PortableCoreExecMode::RollbackOnly)
                throw std::runtime_error("Non-publishing Input ResetConfig probe completed; rollback required");
            return QuestVr::Vm::Value::Bool(result.completed);
        },limits);
    }catch(const std::exception& error) {
        result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();
    }
    return result;
}
