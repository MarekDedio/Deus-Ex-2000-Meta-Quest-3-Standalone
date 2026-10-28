// Public explicit settings binding and original static command entry. Neither
// connects missing viewport providers nor manufactures successful possession.
namespace {
PortableUserConfigStorageSnapshot UserConfigStorageSnapshot(const RuntimeUserConfigStorage& storage) {
    return {true,storage.owned,storage.seedPath,storage.targetPath,storage.baseline.size(),
        UserConfigStorageBytes(),storage.generation,storage.publication};
}
}
PortableUserConfigStorageSnapshot ReadPortableRuntimeUserConfigStorage() {
    if(!persistentUserConfigStorage)return {};
    return UserConfigStorageSnapshot(*persistentUserConfigStorage);
}
PortableUserConfigStorageSnapshot BindPortableRuntimeUserConfigStorage(
    const std::string& applicationDataDirectory,const std::string& userIniSeed) {
    CollectBirthGarbageAtBoundary collect;PortableActorVmHost host;
    for(const auto* path:{&applicationDataDirectory,&userIniSeed})
        if(path->empty()||path->size()>8192u||path->find('\0')!=std::string::npos)
            throw std::runtime_error("User config binding requires explicit bounded directory/seed paths");
    if(!persistentInputSchema||!persistentInputSchema->get())
        throw std::runtime_error("User config backend requires the prepared actual Input schema");
    const auto application=std::filesystem::absolute(applicationDataDirectory).lexically_normal();
    const auto seed=std::filesystem::absolute(userIniSeed).lexically_normal().string();
    if(persistentUserConfigStorage) {
        if(persistentUserConfigStorage->seedPath!=seed||
            persistentUserConfigStorage->backend.Directory().parent_path()!=application)
            throw std::runtime_error("User config backend cannot replace another app-owned binding");
        return ReadPortableRuntimeUserConfigStorage();
    }
    if(persistentInputSchema->get()->config)
        throw std::runtime_error("Bind app-owned config before preparing the initial Input cache");
    host.ValidateAdditionalPackageBudget(3u*QuestVr::ConfigFile::MaximumBytes+applicationDataDirectory.size()*8u+userIniSeed.size()*8u);
    auto backend=QuestVr::ConfigFile::Backend::Bind(application,seed);auto snapshot=backend.Read();
    auto storage=std::make_unique<RuntimeUserConfigStorage>(RuntimeUserConfigStorage{
        std::move(backend),seed,(application/"Config"/"User.ini").string(),snapshot.Data(),snapshot.Generation(),snapshot.Owned(),
        PortableConfigPublication::None});
    auto bytes=QuestVr::DecalStateDetail::Add(sizeof(*storage),storage->backend.RetainedBytes());
    bytes=QuestVr::DecalStateDetail::Add(bytes,storage->baseline.capacity()+storage->seedPath.capacity()+storage->targetPath.capacity()+2u);
    host.ValidateAdditionalPackageBudget(bytes+sizeof(PortableUserConfigStorageSnapshot)+storage->seedPath.size()+storage->targetPath.size()+2u);
    const auto result=PortableUserConfigStorageSnapshot{true,storage->owned,storage->seedPath,storage->targetPath,
        storage->baseline.size(),bytes,storage->generation,storage->publication};
    persistentUserConfigStorage=std::move(storage);return result;
}
PortableCoreExecResult ExecutePortableRuntimeCoreStaticExec(const std::string& command,
    PortableCoreExecMode mode,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableCoreExecResult result;
    try {
        if(mode!=PortableCoreExecMode::RollbackOnly&&mode!=PortableCoreExecMode::Commit)
            throw std::runtime_error("Core static exec mode is not an admitted commit/rollback policy");
        PortableActorVmHost host;
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            const auto handled=host.CoreStaticExec(command,result,execution);
            if(mode==PortableCoreExecMode::RollbackOnly)
                throw std::runtime_error("Non-publishing Core static exec probe completed; rollback required");
            return QuestVr::Vm::Value::Bool(handled);
        },limits);
    }catch(const std::exception& error) {
        result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();
    }
    return result;
}
