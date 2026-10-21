// Included inside PortableActorVmHost. Live portable-loader attachment, not
// immutable source provenance, Package Outer, UObject lookup or a destructor.
    void RequireNativeLinkerStorage(RuntimeObject* object) {
        if(!object||!object->objectIndex||Object(object->reflection.objectPath)!=object||!object->nativeLinkerBound)
            throw std::runtime_error("UObject linker header has no bound live loader authority");
        if(object->nativeLinker&&!object->nativeLinker->active)
            throw std::runtime_error("UObject linker attachment belongs to a retired loader");
    }
    PortableObjectLinkerSnapshot NativeLinkerSnapshot(RuntimeObject* object) {
        PortableObjectLinkerSnapshot result;
        result.bound=object->nativeLinkerBound;result.attached=bool(object->nativeLinker);
        result.linkerIdentity=reinterpret_cast<std::uintptr_t>(object->nativeLinker.get());
        result.nativeIndex=object->nativeLinkerIndex;result.sourcePath=object->sourcePath;
        result.sourceExportIndex=object->exportIndex;result.objectIndex=object->objectIndex.Index();
        result.outerIndex=object->outer?object->outer->objectIndex.Index():0u;
        result.flags=DestroyObjectFlags(object);
        if(result.bound)RequireNativeLinkerStorage(object);
        std::shared_ptr<RuntimeNativeLinker> source;
        const auto select=[&](const auto& linker) {
            if(linker&&linker->active&&linker->sourcePath==object->sourcePath) {
                if(source&&source!=linker)throw std::runtime_error("Native source linker authority is ambiguous");
                source=linker;
            }
        };
        for(const auto& linker:persistentRuntime->get()->sourceLinkers)select(linker.live);
        select(persistentRuntime->get()->mapLinker);
        if(source&&object->exportIndex<source->count) {
            result.sourceExport=true;result.sourceLinkerIdentity=reinterpret_cast<std::uintptr_t>(source.get());
            auto* exported=source->Read(object->exportIndex);
            if(exported) {
                if(Object(exported->reflection.objectPath)!=exported||exported->sourcePath!=source->sourcePath||
                    exported->exportIndex!=object->exportIndex)
                    throw std::runtime_error("Native source export diagnostic has a different live identity");
                result.exportObjectPath=exported->reflection.objectPath;
            }
        }
        return result;
    }
    void JournalNativeExportSlot(const std::shared_ptr<RuntimeNativeLinker>& linker,std::size_t index) {
        if(!transaction_||!linker||!linker->active||index>=linker->count)
            throw std::runtime_error("Native export-slot mutation lacks its caller-owned live loader");
        if(std::none_of(beforeLinkerSlots_.begin(),beforeLinkerSlots_.end(),[&](const auto& row) {
            return row.linker==linker&&row.index==index;
        })) {
            if(beforeLinkerSlots_.size()>=4096u)throw std::runtime_error("Native export-slot journal exceeds caller bound");
            if(beforeLinkerSlots_.size()==beforeLinkerSlots_.capacity()) {
                const auto next=std::min<std::size_t>(4096u,std::max<std::size_t>(8u,beforeLinkerSlots_.capacity()*2u));
                ValidateStateBudget(next*sizeof(BeforeLinkerSlot));beforeLinkerSlots_.reserve(next);
            }
            beforeLinkerSlots_.push_back({linker,index,linker->Read(index),linker->changed.contains(index)});
        }
    }
    void NativeLinkerIndexStore(RuntimeObject* object,std::int32_t index) {
        RequireNativeLinkerStorage(object);Touch(object);
        object->nativeLinkerIndex=index;object->nativeLinkerChanged=true;ValidateStateBudget();
    }
    void SetNativeObjectLinker(RuntimeObject* object,std::uintptr_t newLinker,std::int32_t newIndex) {
        RequireNativeLinkerStorage(object);
        if(newLinker)throw std::runtime_error("Native SetLinker nonnull reassignment requires the loader attachment service");
        const auto retained=object->nativeLinker;
        const auto receiver=[this,object](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(object))
                throw std::runtime_error("Native SetLinker changed its retained UObject");
            RequireNativeLinkerStorage(object);return object;
        };
        const auto slot=[retained](std::uintptr_t identity,std::int32_t index)->RuntimeObject* {
            if(!retained||!retained->active||identity!=reinterpret_cast<std::uintptr_t>(retained.get())||
                index<0||static_cast<std::size_t>(index)>=retained->count)
                throw std::runtime_error("Native SetLinker export is outside its retained live loader");
            return retained->Read(static_cast<std::size_t>(index));
        };
        QuestVr::OriginalSetLinkerQueries query;
        query.linker=[receiver](std::uintptr_t identity){return reinterpret_cast<std::uintptr_t>(receiver(identity)->nativeLinker.get());};
        query.linkerIndex=[receiver](std::uintptr_t identity){return receiver(identity)->nativeLinkerIndex;};
        query.exportObject=[slot](std::uintptr_t identity,std::int32_t index){return reinterpret_cast<std::uintptr_t>(slot(identity,index));};
        query.clearExportObject=[this,retained,slot,object](std::uintptr_t identity,std::int32_t index) {
            static_cast<void>(slot(identity,index));JournalNativeExportSlot(retained,static_cast<std::size_t>(index));Touch(object);
            if(!retained->changed.contains(static_cast<std::size_t>(index)))
                ValidateStateBudget(sizeof(std::pair<const std::size_t,RuntimeObject*>)+6u*sizeof(void*));
            retained->changed[static_cast<std::size_t>(index)]=nullptr;
            object->nativeLinkerChanged=true;ValidateStateBudget();
        };
        query.writeLinker=[this,receiver](std::uintptr_t identity,std::uintptr_t linker) {
            if(linker)throw std::runtime_error("Native SetLinker unexpected nonnull target");
            auto* target=receiver(identity);Touch(target);target->nativeLinker.reset();
            target->nativeLinkerChanged=true;ValidateStateBudget();
        };
        query.writeLinkerIndex=[this,receiver](std::uintptr_t identity,std::int32_t index){NativeLinkerIndexStore(receiver(identity),index);};
        query.work=[this]{LifecycleStep();};
        QuestVr::SetOriginalObjectLinker(reinterpret_cast<std::uintptr_t>(object),newLinker,newIndex,query);
    }
