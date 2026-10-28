PortableViewportCanvasSnapshot ReadPortableRuntimeViewportCanvas(std::uint64_t viewportIdentity) {
    PortableActorVmHost host;auto* viewport=NativeSessionViewport(viewportIdentity);
    if(!viewport->nativeCanvasPeer) {
        PortableViewportCanvasSnapshot result;result.inventoryCapacity=persistentNativeCanvases.capacity();
        result.inventoryBytes=NativeCanvasInventoryBytes();return result;
    }
    return host.CaptureNativeViewportCanvas(viewport->nativeCanvasPeer);
}
PortableViewportCanvasResult PreparePortableRuntimeViewportCanvas(std::uint64_t viewportIdentity,
    const std::string& engineIni,bool commit,const QuestVr::Vm::Limits& limits) {
    CollectBirthGarbageAtBoundary collect;PortableViewportCanvasResult result;
    try {
        PortableActorVmHost host;
        if(engineIni.empty()||engineIni.size()>8192u||engineIni.find('\0')!=std::string::npos)
            throw std::runtime_error("Canvas requires an explicit bounded original Engine INI path");
        host.ValidateAdditionalPackageBudget(QuestVr::EngineConfig::kMaxFileBytes+64u*1024u);
        const auto selected=QuestVr::EngineConfig::ReadCanvasClassFile(engineIni);
        if(!selected)throw std::runtime_error("Original Engine INI has no configured Canvas class");
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            result.canvas=host.ConstructNativeViewportCanvas(viewportIdentity,*selected,execution);
            if(!commit)throw std::runtime_error("Non-publishing Canvas initialization probe completed; rollback required");
            return QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Object,result.canvas.objectPath);
        },limits);
    }catch(const std::exception& error) {
        result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();
    }
    return result;
}
