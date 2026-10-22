// Included inside ScriptSaveSchema's public section. Source identities are
// authoritative here; a currently live actor with a matching name is not a
// substitute for a registered saved birth or a serialized ULevel member.
void StageActorOrderRetain(const std::size_t bytes) { Retain(bytes); }

bool HasOriginalNativeLevel() {
    const auto& map=Table(mapPath_);
    std::size_t levels{};
    for (const auto& entry:map.exports)
        if (LowerAscii(Qualified(map,entry.ObjClass))=="engine.level") ++levels;
    if (levels>1u) throw std::runtime_error("Script save map has multiple native Levels");
    return levels!=0u;
}

std::vector<std::int32_t> OriginalActorOrderReferences() {
    const auto& map=Table(mapPath_);
    std::vector<std::int32_t> order;
    if (HasOriginalNativeLevel()) {
        // This is the exact source order, never nativeActorOrder_'s separately
        // sorted membership cache and never the current compacted live array.
        order=ReadPortableLevel68ActorOrder(map,SchemaScratchBytes);
        Retain(QuestVr::DecalStateDetail::Multiply(order.capacity(),sizeof(std::int32_t)));
    } else {
        // Explicit metadata-only fixture compatibility, identical to loading:
        // export-order membership only for an actual loaded Actor UClass.
        std::size_t count{};
        for (std::size_t index=0u;index<map.exports.size();++index) {
            if (!SourceExportAdmitted(index)) continue;
            const auto& entry=map.exports[index];
            const auto found=persistentVmObjects.find(LowerAscii(Qualified(map,entry.ObjClass)));
            if (found!=persistentVmObjects.end() && found->second &&
                IsDerivedFromPath(found->second,"Engine.Actor")) ++count;
        }
        Retain(QuestVr::DecalStateDetail::Multiply(count,sizeof(std::int32_t)));
        order.reserve(count);
        if (order.capacity()>count)
            Retain(QuestVr::DecalStateDetail::Multiply(order.capacity()-count,sizeof(std::int32_t)));
        for (std::size_t i=0u;i<map.exports.size();++i) {
            if (!SourceExportAdmitted(i)) continue;
            const auto found=persistentVmObjects.find(LowerAscii(Qualified(map,map.exports[i].ObjClass)));
            if (found!=persistentVmObjects.end() && found->second &&
                IsDerivedFromPath(found->second,"Engine.Actor")) order.push_back(static_cast<std::int32_t>(i+1u));
        }
    }
    if (order.size()>QuestVr::ScriptStateDetail::MaximumWorldActorSlots)
        throw std::runtime_error("Script save original native actor order exceeds its fixed slot cap");
    Retain(map.exports.size());
    std::vector<std::uint8_t> seen(map.exports.size(),0u);
    if (seen.capacity()>map.exports.size()) Retain(seen.capacity()-map.exports.size());
    for (auto& reference:order) {
        if (reference==0) continue;
        if (reference<0 || static_cast<std::size_t>(reference)>map.exports.size())
            throw std::runtime_error("Script save original Level slot is not a local export");
        const auto index=static_cast<std::size_t>(reference-1);
        if (seen[index]++) throw std::runtime_error("Script save original Level has duplicate actor membership");
        // ULevel source references remain unchanged on disk. The explicit
        // loader profile's excluded exports resolve to native None at their
        // original slots, without allocating or indexing a placeholder UObject.
        if (!SourceExportAdmitted(index)) reference=0;
    }
    return order;
}

std::vector<std::string> ValidateNativeActorOrder(const QuestVr::ScriptSavedState& saved) {
    if (!saved.nativeActorOrder) return {};
    const auto& order=*saved.nativeActorOrder;
    const auto& map=Table(mapPath_);
    if (!HasOriginalNativeLevel() || saved.mapName!=mapName_ || order.levelPath!=NativeLevelPath())
        throw std::runtime_error("Script save native actor order has no exact source ULevel identity");
    QuestVr::ScriptStateDetail::NativeActorOrderBounds(order.firstDynamic,order.slots.size(),{});
    const auto original=OriginalActorOrderReferences();
    if (original.size()<2u || original[0]==0)
        throw std::runtime_error("Script save original Level has no reserved native actor slots");
    Retain(QuestVr::DecalStateDetail::Multiply(map.exports.size(),sizeof(std::uint8_t)));
    std::vector<std::uint8_t> membership(map.exports.size(),0u);
    if (membership.capacity()>map.exports.size()) Retain(membership.capacity()-map.exports.size());
    for (const auto reference:original) if (reference!=0) {
        const auto index=static_cast<std::size_t>(reference-1);
        auto* cls=host_.Object(Qualified(map,map.exports[index].ObjClass));
        if (!cls || cls->reflection.metaClass!="Class" || !IsDerivedFromPath(cls,"Engine.Actor"))
            throw std::runtime_error("Script save original native slot has no actual loaded Actor class");
        membership[index]=1u;
    }

    using BirthRecord=std::pair<const QuestVr::ScriptSavedBirth*,bool>;
    std::unordered_map<std::string,BirthRecord> births;
    std::unordered_map<std::string,const QuestVr::ScriptSavedObject*> objects;
    const auto prepareMap=[&](auto& values,const std::size_t count) {
        Retain(QuestVr::DecalStateDetail::Multiply(count,sizeof(typename std::decay_t<decltype(values)>::value_type)+6u*sizeof(void*)));
        values.reserve(count);
        Retain(QuestVr::DecalStateDetail::Multiply(values.bucket_count(),sizeof(void*)));
    };
    prepareMap(births,saved.births.size());prepareMap(objects,saved.objects.size());
    for (const auto& birth:saved.births) {
        const auto key=LowerAscii(birth.path);
        Retain(key.capacity()+1u);
        const auto registered=birthClasses_.find(key);
        if (registered==birthClasses_.end() || !registered->second ||
            LowerAscii(registered->second->reflection.objectPath)!=LowerAscii(birth.classPath) ||
            !IsDerivedFromPath(registered->second,"Engine.Actor") ||
            !births.emplace(key,BirthRecord{&birth,false}).second)
            throw std::runtime_error("Script save native actor birth is not its registered manifest class/identity");
    }
    for (const auto& object:saved.objects) {
        const auto key=LowerAscii(object.path);Retain(key.capacity()+1u);
        if (!objects.emplace(key,&object).second)
            throw std::runtime_error("Script save native actor overlay has duplicate identities");
    }
    const auto removed=[&](const std::string& path) {
        const auto found=objects.find(LowerAscii(path));
        return found!=objects.end() && found->second->path==path && found->second->lifecycle &&
            found->second->lifecycle->worldRemoved;
    };
    Retain(QuestVr::DecalStateDetail::Multiply(order.slots.size(),sizeof(std::string)));
    std::vector<std::string> slots;slots.reserve(order.slots.size());
    if (slots.capacity()>order.slots.size())
        Retain(QuestVr::DecalStateDetail::Multiply(slots.capacity()-order.slots.size(),sizeof(std::string)));
    for (std::size_t i=0u;i<order.slots.size();++i) {
        const auto& slot=order.slots[i];std::string path;
        switch (slot.kind) {
        case QuestVr::ScriptSavedNativeActorSlotKind::None:
            if (slot.authoredExport!=0u || !slot.birthPath.empty())
                throw std::runtime_error("Script save null native slot has inactive payload");
            break;
        case QuestVr::ScriptSavedNativeActorSlotKind::AuthoredExport: {
            const auto index=static_cast<std::size_t>(slot.authoredExport);
            if (!slot.birthPath.empty() || index>=map.exports.size() ||
                !SourceExportAdmitted(index) || membership[index]!=1u)
                throw std::runtime_error("Script save native slot is duplicate, foreign or not an original Level member");
            auto* cls=host_.Object(Qualified(map,map.exports[index].ObjClass));
            if (!cls || cls->reflection.metaClass!="Class" || !IsDerivedFromPath(cls,"Engine.Actor"))
                throw std::runtime_error("Script save native authored slot has no actual Actor class");
            membership[index]=3u;path=Qualified(map,static_cast<std::int32_t>(index+1u));
            break;
        }
        case QuestVr::ScriptSavedNativeActorSlotKind::Birth: {
            const auto born=births.find(LowerAscii(slot.birthPath));
            if (slot.authoredExport!=0u || born==births.end() || born->second.second ||
                born->second.first->path!=slot.birthPath || born->second.first->worldActorIndex!=i)
                throw std::runtime_error("Script save native birth slot disagrees with its registered manifest");
            born->second.second=true;path=slot.birthPath;
            break;
        }
        default: throw std::runtime_error("Script save native actor slot tag is invalid");
        }
        Retain(path.capacity()+1u);slots.push_back(std::move(path));
    }
    const auto originalPath=[&](const std::size_t index) {
        return original[index]==0 ? std::string{} : Qualified(map,original[index]);
    };
    if (slots[0]!=originalPath(0u) || (!slots[1].empty() && slots[1]!=originalPath(1u)))
        throw std::runtime_error("Script save rewrites reserved native Level actor identities");
    for (const auto reference:original) {
        if (reference==0) continue;
        const auto index=static_cast<std::size_t>(reference-1);
        if (membership[index]==1u && !removed(Qualified(map,reference)))
            throw std::runtime_error("Script save omitted authored native actor lacks explicit removed lifecycle");
    }
    for (const auto& [key,born]:births) {
        static_cast<void>(key);
        if (!born.second && (born.first->worldActorIndex!=std::numeric_limits<std::uint32_t>::max() ||
            !removed(born.first->path)))
            throw std::runtime_error("Script save nonmember birth lacks explicit removed lifecycle/sentinel");
    }
    return slots;
}
