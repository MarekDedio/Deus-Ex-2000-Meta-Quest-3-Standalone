PortableViewportConsoleSnapshot ReadPortableRuntimeViewportConsole(std::uint64_t viewportIdentity) {
    PortableActorVmHost host;auto* viewport=NativeSessionViewport(viewportIdentity);
    if(!viewport->nativeViewportConsole) {
        PortableViewportConsoleSnapshot result;result.inventoryCapacity=persistentNativeConsoles.capacity();
        result.inventoryBytes=NativeConsoleInventoryBytes();return result;
    }
    return host.CaptureNativeViewportConsole(viewport->nativeViewportConsole);
}
PortableViewportConsoleResult PreparePortableRuntimeViewportConsole(std::uint64_t viewportIdentity,
    const std::string& engineIni,bool commit,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableViewportConsoleResult result;
    try {
        PortableActorVmHost host;
        if(engineIni.empty()||engineIni.size()>8192u||engineIni.find('\0')!=std::string::npos)
            throw std::runtime_error("Console requires an explicit bounded original Engine INI path");
        host.ValidateAdditionalPackageBudget(QuestVr::EngineConfig::kMaxFileBytes+64u*1024u);
        const auto selected=QuestVr::EngineConfig::ReadConsoleClassFile(engineIni);
        if(!selected)throw std::runtime_error("Original Engine INI has no configured Console class");
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            result.console=host.ConstructNativeViewportConsole(viewportIdentity,*selected,execution);
            if(!commit)throw std::runtime_error("Non-publishing Console initialization probe completed; rollback required");
            return QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Object,result.console.objectPath);
        },limits);
    }catch(const std::exception& error) {
        result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();
    }
    return result;
}
