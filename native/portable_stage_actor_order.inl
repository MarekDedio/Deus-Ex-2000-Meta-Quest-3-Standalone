// Included globally immediately after the complete ScriptSaveSchema. All
// containers and reverse-index assignments are candidates until publication;
// neither native actor slots nor a live actor header are edited here.
void StageRuntimeBirthReplacement(PreparedScriptState& prepared,const QuestVr::ScriptSavedState* saved) {
    if (!persistentRuntime || !persistentRuntime->get() || persistentMapPackageName.empty())
        throw std::runtime_error("Native actor restore has no loaded map runtime");
    if (saved && LowerAscii(saved->mapName)!=LowerAscii(persistentMapPackageName))
        throw std::runtime_error("Native actor restore cannot stage into another map");
    ScriptSaveSchema schema(saved ? saved->mapName : persistentMapPackageName);
    if (saved) schema.RegisterBirths(saved->births,saved->nativeActorOrder ? &*saved->nativeActorOrder : nullptr);
    else schema.RegisterBirths({});
    const auto original=schema.OriginalActorOrderReferences();
    const bool explicitOrder=saved && saved->nativeActorOrder.has_value();
    const auto symbolic=explicitOrder ? schema.ValidateNativeActorOrder(*saved) : std::vector<std::string>{};
    const auto retain=[&](const std::size_t bytes) {schema.StageActorOrderRetain(bytes);};
    const auto multiply=[](const std::size_t count,const std::size_t size) {
        return QuestVr::DecalStateDetail::Multiply(count,size);
    };
    const auto reserveVector=[&](auto& values,const std::size_t count) {
        if (!values.empty()) throw std::runtime_error("Native actor restore staging container is already populated");
        const auto previous=values.capacity();
        if (count>previous) {
            // Admit the requested array before reserve, then charge any actual
            // implementation capacity in excess of that request as well.
            retain(multiply(count,sizeof(typename std::decay_t<decltype(values)>::value_type)));
            values.reserve(count);
            if (values.capacity()>count)
                retain(multiply(values.capacity()-count,sizeof(typename std::decay_t<decltype(values)>::value_type)));
        } else retain(multiply(previous,sizeof(typename std::decay_t<decltype(values)>::value_type)));
    };
    const auto prepareMap=[&](auto& values,const std::size_t count) {
        if (!values.empty()) throw std::runtime_error("Native actor restore name registry is already populated");
        retain(multiply(count,sizeof(typename std::decay_t<decltype(values)>::value_type)+6u*sizeof(void*)));
        values.reserve(count);
        retain(multiply(values.bucket_count(),sizeof(void*)));
    };
    const auto insertMap=[&](auto& values,const std::string& key,RuntimeObject* object) {
        retain(key.capacity()+1u);
        const auto inserted=values.emplace(key,object);
        if (!inserted.second) throw std::runtime_error("Restored native actor name publication collision");
        if (inserted.first->first.capacity()>key.capacity())
            retain(inserted.first->first.capacity()-key.capacity());
    };
    std::size_t authoredExports{},qualified{},vm{};bool birthTail{};
    for (auto* object:persistentRuntime->get()->exports) {
        if (!object) throw std::runtime_error("Native actor restore encountered a missing retained export object");
        if (object->runtimeBirth) {pendingBirthGarbage=true;birthTail=true;}
        else {
            if (birthTail) throw std::runtime_error("Native actor restore cannot shift authored export storage past interleaved births");
            ++authoredExports;
        }
    }
    for (const auto& [key,object]:persistentQualifiedObjects) {
        static_cast<void>(key);if (!object || !object->runtimeBirth) ++qualified;
    }
    for (const auto& [key,object]:persistentVmObjects) {
        static_cast<void>(key);if (!object || !object->runtimeBirth) ++vm;
    }
    const auto births=prepared.births.size();
    if ((!saved && births!=0u) || (saved && births!=saved->births.size()))
        throw std::runtime_error("Native actor restore staged birth count disagrees with its manifest");
    reserveVector(prepared.exports,QuestVr::DecalStateDetail::Add(authoredExports,births));
    prepareMap(prepared.qualified,QuestVr::DecalStateDetail::Add(qualified,births));
    prepareMap(prepared.vm,QuestVr::DecalStateDetail::Add(vm,births));
    for (const auto& [key,object]:persistentQualifiedObjects)
        if (!object || !object->runtimeBirth) insertMap(prepared.qualified,key,object);
    for (const auto& [key,object]:persistentVmObjects)
        if (!object || !object->runtimeBirth) insertMap(prepared.vm,key,object);
    for (auto* object:persistentRuntime->get()->exports)
        if (!object->runtimeBirth) prepared.exports.push_back(object);

    std::vector<RuntimeObject*> orderedBirths;
    reserveVector(orderedBirths,births);
    for (const auto& root:prepared.births) {
        auto* actor=root.get();
        if (!actor || !actor->runtimeBirth || !actor->cls || !IsDerivedFromPath(actor->cls,"Engine.Actor") ||
            actor->sourcePath!=schema.MapSource() || actor->birthVmKey!=LowerAscii(actor->reflection.objectPath) ||
            !schema.IsBirth(actor->reflection.objectPath))
            throw std::runtime_error("Native actor restore birth has no registered staged Actor identity");
        const auto manifest=std::find_if(saved->births.begin(),saved->births.end(),[&](const auto& birth) {
            return birth.path==actor->reflection.objectPath;
        });
        if (manifest==saved->births.end() || LowerAscii(manifest->classPath)!=LowerAscii(actor->cls->reflection.objectPath))
            throw std::runtime_error("Native actor restore staged birth class does not match manifest");
        orderedBirths.push_back(actor);
    }
    std::sort(orderedBirths.begin(),orderedBirths.end(),[explicitOrder](const auto* a,const auto* b) {
        // Legacy actor snapshots expose the original appended birth order,
        // independently of canonical manifest sorting. Those unique tail
        // indices were authenticated before staging. Explicit native order
        // instead owns membership separately from this retained export tail.
        if(!explicitOrder)return a->worldActorIndex<b->worldActorIndex;
        return QuestVr::ScriptStateDetail::Compare(a->reflection.objectPath,b->reflection.objectPath)<0;
    });
    for (auto* actor:orderedBirths) {
        insertMap(prepared.qualified,actor->reflection.objectPath,actor);
        insertMap(prepared.vm,actor->birthVmKey,actor);
        // RuntimeNativeLinker addresses the combined export vector's original
        // authored segments. Only births append; slot partition never moves it.
        prepared.exports.push_back(actor);
    }
    const auto staged=[&](const std::string& path) -> RuntimeObject* {
        if (path.empty()) return nullptr;
        const auto canonical=prepared.qualified.find(path);
        const auto folded=prepared.vm.find(LowerAscii(path));
        if (canonical==prepared.qualified.end() || folded==prepared.vm.end() || !canonical->second ||
            canonical->second!=folded->second || canonical->second->reflection.objectPath!=path)
            throw std::runtime_error("Native actor restore slot is absent from its staged identity graph");
        return canonical->second;
    };
    RuntimeObject* level{};
    if (schema.HasOriginalNativeLevel()) {
        level=staged(schema.NativeLevelPath());
        if (level->sourcePath!=schema.MapSource() || !level->nativeStandaloneLevelServices || !level->nativeTimeSeconds)
            throw std::runtime_error("Native actor restore ULevel has no actual loader-owned native binding");
    }
    const auto validateActor=[&](RuntimeObject* actor,const bool born) {
        if (!actor || actor->runtimeBirth!=born || actor->sourcePath!=schema.MapSource() || !actor->cls ||
            !IsDerivedFromPath(actor->cls,"Engine.Actor") || (level && actor->nativeXLevel!=level))
            throw std::runtime_error("Native actor restore slot has a foreign or absent native Actor binding");
    };
    for (auto* actor:orderedBirths) validateActor(actor,true);
    std::size_t sourceCount{};
    for (auto* object:prepared.exports)
        if (!object->runtimeBirth && object->sourcePath==schema.MapSource()) ++sourceCount;
    std::vector<RuntimeObject*> sourceExports;reserveVector(sourceExports,sourceCount);
    for (auto* object:prepared.exports)
        if (!object->runtimeBirth && object->sourcePath==schema.MapSource()) sourceExports.push_back(object);
    std::sort(sourceExports.begin(),sourceExports.end(),[](const auto* a,const auto* b) {return a->exportIndex<b->exportIndex;});
    for (std::size_t i=1u;i<sourceExports.size();++i)
        if (sourceExports[i-1u]->exportIndex==sourceExports[i]->exportIndex)
            throw std::runtime_error("Native actor restore source export storage has duplicate identities");
    const auto sourceExport=[&](const std::size_t index) -> RuntimeObject* {
        const auto found=std::lower_bound(sourceExports.begin(),sourceExports.end(),index,
            [](const auto* actor,const auto value) {return actor->exportIndex<value;});
        if (found==sourceExports.end() || (*found)->exportIndex!=index)
            throw std::runtime_error("Native actor restore source export is unavailable");
        return *found;
    };
    for (const auto reference:original) {
        if (reference==0) continue;
        auto* actor=sourceExport(static_cast<std::size_t>(reference-1));
        if (!actor->cls && !explicitOrder) continue; // The real loader's unresolved-class null slot.
        validateActor(actor,false);
    }
    const auto slotCount=explicitOrder ? symbolic.size() : QuestVr::DecalStateDetail::Add(original.size(),births);
    if (slotCount>QuestVr::ScriptStateDetail::MaximumWorldActorSlots)
        throw std::runtime_error("Native actor restore slot array exceeds fixed slot budget");
    reserveVector(prepared.worldActors,slotCount);prepared.worldActors.resize(slotCount,nullptr);
    if (explicitOrder) {
        for (std::size_t i=0u;i<symbolic.size();++i) {
            if (symbolic[i].empty()) continue;
            auto* actor=staged(symbolic[i]);
            const auto& identity=saved->nativeActorOrder->slots[i];
            const bool born=identity.kind==QuestVr::ScriptSavedNativeActorSlotKind::Birth;
            validateActor(actor,born);
            if (!born && (actor->exportIndex!=identity.authoredExport || sourceExport(identity.authoredExport)!=actor))
                throw std::runtime_error("Native actor restore source export identity does not match its staged object");
            prepared.worldActors[i]=actor;
        }
    } else {
        // Legacy migration starts from the immutable package, never a live
        // prefix that may already have been partitioned or contain null holes.
        for (std::size_t i=0u;i<original.size();++i) {
            const auto reference=original[i];if (reference==0) continue;
            const auto index=static_cast<std::size_t>(reference-1);
            auto* actor=sourceExport(index);
            // The real loader explicitly leaves an unresolved source-class
            // slot null. Preserve that fact, rather than inventing an Actor.
            if (!actor->cls) continue;
            validateActor(actor,false);prepared.worldActors[i]=actor;
        }
        for (auto* actor:orderedBirths) {
            validateActor(actor,true);
            if (actor->worldActorIndex<0 || static_cast<std::size_t>(actor->worldActorIndex)<original.size() ||
                static_cast<std::size_t>(actor->worldActorIndex)>=slotCount ||
                prepared.worldActors[static_cast<std::size_t>(actor->worldActorIndex)])
                throw std::runtime_error("Legacy birth native slot is not its unique appended source slot");
            prepared.worldActors[static_cast<std::size_t>(actor->worldActorIndex)]=actor;
        }
    }
    std::size_t actors{};
    for (auto* object:prepared.exports)
        if (object->sourcePath==schema.MapSource() && object->cls && IsDerivedFromPath(object->cls,"Engine.Actor")) ++actors;
    reserveVector(prepared.worldIndices,actors);
    for (auto* object:prepared.exports)
        if (object->sourcePath==schema.MapSource() && object->cls && IsDerivedFromPath(object->cls,"Engine.Actor"))
            prepared.worldIndices.emplace_back(object,-1);
    const auto less=[](const auto& a,const auto& b) {return std::less<RuntimeObject*>{}(a.first,b.first);};
    std::sort(prepared.worldIndices.begin(),prepared.worldIndices.end(),less);
    for (std::size_t i=1u;i<prepared.worldIndices.size();++i)
        if (prepared.worldIndices[i-1u].first==prepared.worldIndices[i].first)
            throw std::runtime_error("Native actor restore has duplicate retained Actor objects");
    for (std::size_t i=0u;i<prepared.worldActors.size();++i) {
        auto* actor=prepared.worldActors[i];if (!actor) continue;
        const auto found=std::lower_bound(prepared.worldIndices.begin(),prepared.worldIndices.end(),
            std::pair<RuntimeObject*,std::int32_t>{actor,-1},less);
        if (found==prepared.worldIndices.end() || found->first!=actor || found->second!=-1)
            throw std::runtime_error("Native actor restore slot membership is missing or duplicated");
        found->second=static_cast<std::int32_t>(i);
    }
    prepared.actorOrder.reset();
    if (level) prepared.actorOrder=std::pair{level,explicitOrder ? saved->nativeActorOrder->firstDynamic : std::optional<std::uint32_t>{}};
    prepared.actorOrderExplicit=explicitOrder;
    prepared.replaceBirths=true;
}
