// Included inside the actual VM host; process state is external app-loop state,
// not actor data and not an initialized Input or platform dispatch capability.
    bool ObserveOriginalProcessRunning() {
        if(!transaction_)throw std::runtime_error("Process running observation requires its active outer VM root");
        LifecycleStep();
        const auto& binding=RequireProcessLoopBinding();
        if(!processLoopControl_) {
            ValidateStateBudget(sizeof(processLoopControl_)+sizeof(processLoopGeneration_)+sizeof(processLoopRuntime_));
            processLoopControl_=binding.control;processLoopGeneration_=binding.lookupGeneration;
            processLoopRuntime_=binding.runtime;
        }
        ValidateProcessLoopLease();
        // This is the selected invocation's one actual external observation.
        // Nested invocations may read again. Commit authenticates the lease,
        // NOT equality of this Boolean with a later external source state.
        return processLoopControl_->Running();
    }
    void ValidateProcessLoopLease() const {
        if(!processLoopControl_)return;
        const auto& binding=RequireProcessLoopBinding();
        if(binding.control!=processLoopControl_||binding.lookupGeneration!=processLoopGeneration_||
            binding.runtime!=processLoopRuntime_||!processLoopControl_->Alive())
            throw std::runtime_error("Process running root lease is revoked, stale or replaced");
    }
    Value ProbeProcessLoop(const std::function<void()>& afterFirst,const std::function<void()>& beforeCommit,bool lateFailure,
        PortableProcessLoopProbeResult& result) {
        result.first=ObserveOriginalProcessRunning();
        const auto& binding=RequireProcessLoopBinding();
        result.identity=binding.control->Identity();result.lookupGeneration=binding.lookupGeneration;
        if(afterFirst)afterFirst();
        result.second=ObserveOriginalProcessRunning();result.completed=true;
        if(lateFailure)throw std::runtime_error("Process loop diagnostic late failure; outer rollback required");
        if(beforeCommit)beforeCommit();
        return Value::Bool(result.second);
    }
    void RetireProcessLoopRoot() noexcept {
        processLoopControl_.reset();processLoopGeneration_=0u;processLoopRuntime_=nullptr;
        if(!persistentVmRootCount)std::terminate();
        --persistentVmRootCount;
    }
