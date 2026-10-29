PortableStringToNameResult ExecutePortableRuntimeStringToName(const std::string& receiverPath,
    const QuestVr::Vm::Value& argument,PortableCoreExecMode mode,const QuestVr::Vm::Limits& limits,bool indexedNative) {
    CollectBirthGarbageAtBoundary collect;PortableStringToNameResult result;
    try {
        if(mode!=PortableCoreExecMode::RollbackOnly&&mode!=PortableCoreExecMode::Commit)
            throw std::runtime_error("StringToName invocation requires an explicit world commit/rollback policy");
        if(receiverPath.empty()||receiverPath.size()>1024u||receiverPath.find('\0')!=std::string::npos)
            throw std::runtime_error("StringToName invocation requires an actual bounded receiver path");
        if(argument.kind!=QuestVr::Vm::Kind::String)
            throw std::runtime_error("StringToName invocation requires a String input");
        // Value's tagged payload can still physically own unrelated containers.
        // Reject those tails BEFORE copying the input into the VM argument
        // vector; budgeting text alone cannot admit an arbitrary nested tree.
        if(!argument.fields.empty()||argument.intArray.ContainerBytes()!=0u)
            throw std::runtime_error("StringToName invocation requires a canonical String without container tails");
        if(argument.text.size()>limits.stringBytes)
            throw std::runtime_error("StringToName invocation exceeds its VM String byte limit");
        if(!persistentRuntime||!persistentRuntime->get())
            throw std::runtime_error("StringToName invocation requires an initialized original runtime");
        PortableActorVmHost host;const auto creditBefore=runtimeNameArenaCredit;
        using QuestVr::DecalStateDetail::Add;using QuestVr::DecalStateDetail::Multiply;
        // Keep API-owned argument/vector/result copies visible through nested
        // native work AND the outer Commit validation, not just at allocation.
        auto scratch=host.ReserveStringToNameDiagnosticScratch(Add(4096u,
            Multiply(Add(Add(argument.text.size(),receiverPath.size()),2u),16u)));
        result.execution=QuestVr::Vm::ExecuteTransaction(host,[&](QuestVr::Vm::Execution& execution) {
            const std::vector<QuestVr::Vm::Evaluation> arguments{{argument,{}}};
            const auto value=indexedNative?
                host.NativeWithExecution(1024u,receiverPath,arguments,nullptr,execution).Load():
                execution.CallFunction(*host.OriginalStringToNameDeclaration(nullptr),receiverPath,arguments);
            if(value.kind!=QuestVr::Vm::Kind::Name)throw std::runtime_error("StringToName did not return its genuine Name result");
            const auto identity=NameString::FindCompareIndex(value.text);
            if(!identity)throw std::runtime_error("StringToName returned text without an actual name-arena identity");
            result.canonicalName=value.text;result.compareIndex=*identity;result.completed=true;
            result.arenaGrowthBytes=runtimeNameArenaCredit-creditBefore;
            result.processNameCreditBytes=runtimeNameArenaCredit;result.forwardNativeWork=host.ReadNativeWork()->used;
            if(mode==PortableCoreExecMode::RollbackOnly)
                throw std::runtime_error("Original StringToName completed; world rollback required, process name identity retained");
            return value;
        },limits);
    }catch(const std::exception& error) {
        result.execution.status=QuestVr::Vm::Status::Unsupported;result.execution.error=error.what();
    }
    return result;
}
