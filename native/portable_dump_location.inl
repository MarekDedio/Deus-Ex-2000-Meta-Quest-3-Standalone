// Included inside PortableActorVmHost. Native3015 is not Actor.Spawn and does
// not borrow window/birth/export ownership. Original addresses are recorded in
// ORIGINAL-TRAINING-DUMP-LOCATION.md. All mutation belongs to the caller root.
    void RequireDumpLocation(RuntimeObject* object) {
        if(!object || !object->nativeDump || !object->nativeDump->helper || object->runtimeBirth ||
            object->nativeWindowObject || object->defaultOwner || object->cls!=Object("DeusEx.DumpLocation") ||
            !object->outer || !object->lookupName || !object->objectIndex ||
            object->exportIndex!=std::numeric_limits<std::size_t>::max() || object->worldActorIndex!=-1)
            throw std::runtime_error("DumpLocation service requires a genuine constructed non-Actor UObject");
    }
    void DumpLocationStructSchema(RuntimeObject* field) {
        auto* structure=Object("DeusEx.DumpLocation.DumpLocationStruct");
        if(!field->property || field->property->type!="StructProperty" ||
            Object(Qualified(Table(field->sourcePath),field->property->referencedType))!=structure ||
            structure->outer!=Object("DeusEx.DumpLocation") ||
            LowerAscii(Qualified(Table(structure->sourcePath),Table(structure->sourcePath).exports.at(structure->exportIndex).ObjClass))!="core.struct")
            throw std::runtime_error("DumpLocation pose lacks its canonical DumpLocationStruct declaration");
        const auto& declaration=StructDescriptor(structure);const auto schema=StructSchema(structure);
        if(declaration.baseField || declaration.logicalSize || !declaration.rawBytes.empty() || !declaration.bytecode.empty() ||
            schema->kind!=Kind::Struct || schema->fields.size()!=8u)
            throw std::runtime_error("DumpLocationStruct has a different base/script/member shape");
        struct Member {const char* name;const char* type;Kind kind;std::uint32_t flags;};
        constexpr Member members[]{
            {"MapName","StrProperty",Kind::String,0x400000u},{"Title","StrProperty",Kind::String,0x400000u},
            {"desc","StrProperty",Kind::String,0x400000u},{"GameVersion","StrProperty",Kind::String,0x400000u},
            {"Location","StructProperty",Kind::Vector,0u},{"ViewRotation","StructProperty",Kind::Rotator,0u},
            {"locationid","IntProperty",Kind::Int,0u},{"bDeleted","BoolProperty",Kind::Bool,0u}};
        for(const auto& expected:members) {
            const auto path=structure->reflection.objectPath+'.'+expected.name;auto* member=Object(path);
            const auto found=std::find_if(schema->fields.begin(),schema->fields.end(),[&](const auto& entry){
                return LowerAscii(entry.key)==LowerAscii(path);});
            if(found==schema->fields.end() || found->zero.kind!=expected.kind || member->outer!=structure ||
                !member->property || member->property->type!=expected.type || member->property->arrayDimension!=1 ||
                member->property->baseField || member->property->flags!=expected.flags ||
                LowerAscii(Qualified(Table(member->sourcePath),Table(member->sourcePath).exports.at(member->exportIndex).ObjClass))!=
                    "core."+LowerAscii(expected.type))
                throw std::runtime_error("DumpLocationStruct member lacks its exact original declaration: "+path);
            if(expected.kind==Kind::Vector)NativeCoordsVectorSchema(member);
            else if(expected.kind==Kind::Rotator)NativeCoordsRotatorSchema(member);
            else if(found->nested || member->property->referencedType)
                throw std::runtime_error("DumpLocationStruct scalar member has an unexpected referenced type");
        }
    }
    Property DumpLocationField(RuntimeObject* object,const char* name,Kind kind,const char* type) {
        const auto key=std::string("DeusEx.DumpLocation.")+name;
        const auto field=DescribeProperty(key);auto* metadata=Object(field.key);
        if(field.zero.kind!=kind || field.arrayDimension!=1u || field.flags!=0x1000u ||
            !OwnsProperty(object->cls,field) || !metadata->property || metadata->property->type!=type ||
            metadata->outer!=Object("DeusEx.DumpLocation") || metadata->property->baseField ||
            LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!=
                "core."+LowerAscii(type))
            throw std::runtime_error("DumpLocation field lacks its original typed declaration: "+key);
        if(kind==Kind::Struct)DumpLocationStructSchema(metadata);
        return field;
    }
    std::vector<std::string> DumpLocationFiles(const std::string& pattern,const bool files,const bool directories) {
        if(!persistentDumpServices || pattern!="*.DMP" || !files || directories)
            throw std::runtime_error("DumpLocation file query lacks its explicit original profile");
        std::vector<std::string> names;std::size_t retained{},visited{};
        const auto append=[&](std::string name,const bool directory) {
            LifecycleStep();if(++visited>65536u)throw std::runtime_error("DumpLocation file enumeration work exceeds bound");
            if(directory || name=="." || name=="..")return;
            if(names.size()==QuestVr::DumpLocation::MaximumFiles || name.size()>QuestVr::DumpLocation::MaximumNameBytes ||
                retained>QuestVr::DumpLocation::MaximumDirectoryBytes ||
                name.size()+1u>QuestVr::DumpLocation::MaximumDirectoryBytes-retained)
                throw std::runtime_error("DumpLocation file enumeration storage exceeds bound");
            ValidateStateBudget(retained+name.size()+1u+(names.size()+1u)*sizeof(std::string));
            retained+=name.size()+1u;names.push_back(std::move(name));
        };
#if defined(_WIN32)
        // Audited Editor FFileManagerWindows profile: real Win32 wildcard and
        // OS enumeration order, filenames only. No process CWD mutation.
        WIN32_FIND_DATAW entry{};
        const auto search=std::filesystem::path(persistentDumpServices->searchDirectory)/L"*.DMP";
        const auto handle=FindFirstFileW(search.c_str(),&entry);
        if(handle==INVALID_HANDLE_VALUE)return names; // Original FindFiles void/empty result.
        struct Close {HANDLE handle;~Close(){FindClose(handle);}} close{handle};
        do {
            std::u16string units;for(const auto c:entry.cFileName){if(!c)break;units.push_back(static_cast<char16_t>(c));}
            append(QuestVr::UnrealString::Encode(units),(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0u);
        }while(FindNextFileW(handle,&entry));
#else
        // Explicit Android file-manager embedding: provider iteration order,
        // case-insensitive DMP extension, no recursion, file-only filenames.
        // Not a claim of the packed Windows launcher's factory or CWD policy.
        for(const auto& entry:std::filesystem::directory_iterator(persistentDumpServices->searchDirectory)) {
            LifecycleStep();if(++visited>65536u)throw std::runtime_error("DumpLocation directory scan exceeds work bound");
            const auto text=entry.path().filename().u8string();std::string name(text.begin(),text.end());
            if(LowerAscii(entry.path().extension().string())==".dmp")append(std::move(name),entry.is_directory());
        }
#endif
        return names;
    }
    Value ReadDumpLocationValue(RuntimeObject* object,const Property& property,const std::uint32_t index) {
        RequireDumpLocation(object);
        if(index || property.arrayDimension!=1u || !OwnsProperty(object->cls,property))
            throw std::runtime_error("DumpLocation read lacks scalar ownership");
        const auto key=LowerAscii(property.key);
        if(key=="core.object.class")return Value::Text(Kind::Object,object->cls->reflection.objectPath);
        if(key=="core.object.name")return Value::Text(Kind::Name,object->lookupName->ToString());
        if(key=="core.object.outer")return Value::Text(Kind::Object,object->outer->reflection.objectPath);
        if(key=="core.object.objectflags")return Value::Integer(std::bit_cast<std::int32_t>(object->reflection.flags));
        if(key=="deusex.dumplocation.currentuser") {
            static_cast<void>(DumpLocationField(object,"currentUser",Kind::String,"StrProperty"));
            return Value::Text(Kind::String,object->nativeDump->data.currentUser);
        }
        if(key=="deusex.dumplocation.currentdumplocation") {
            static_cast<void>(DumpLocationField(object,"currentDumpLocation",Kind::Struct,"StructProperty"));
            if(const auto* value=FindScriptOverlay(object,"currentDumpLocation"))return *value;
            return object->nativeDump->initialLocation;
        }
        for(const auto* name:{"currentDumpFileIndex","currentDumpLocationIndex","currentDumpFileLocation","dumpLocationCount"})
            if(key=="deusex.dumplocation."+LowerAscii(name)) {
                static_cast<void>(DumpLocationField(object,name,Kind::Int,"IntProperty"));
                return Value::Integer(std::string_view(name)=="currentDumpFileIndex"?object->nativeDump->data.fileIndex:
                    std::string_view(name)=="currentDumpLocationIndex"?object->nativeDump->data.locationIndex:0);
            }
        if(key=="deusex.dumplocation.player") {
            static_cast<void>(DumpLocationField(object,"Player",Kind::Object,"ObjectProperty"));return Value::Text(Kind::Object,{});
        }
        throw std::runtime_error("DumpLocation raw pointer/directory field needs its typed file service");
    }
    Evaluation DumpLocationNative(const std::uint16_t index,RuntimeObject* receiver,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution& execution) {
        const std::string path=index==3015u?"DeusEx.DeusExPlayer.CreateDumpLocationObject":index==3037u?
            "DeusEx.DumpLocation.SaveLocation":index==3038u?"DeusEx.DumpLocation.LoadLocation":"DeusEx.DumpLocation.HasLocationBeenSaved";
        const auto authored=FunctionFor(Object(path));const bool result=index==3015u || index==3039u;
        const auto valid=[&](const Function& f) {
            if(LowerAscii(f.path)!=LowerAscii(path) || f.nativeIndex!=index || f.flags!=0x401u ||
                !f.bytecode.empty() || f.variables.size()!=(result?1u:0u))
                throw std::runtime_error("DumpLocation native declaration differs from the original");
            if(result) {
                const auto& field=f.variables.front();auto* metadata=Object(field.key);
                if(LowerAscii(field.key)!=LowerAscii(path)+".returnvalue" || field.arrayDimension!=1u || field.flags!=0x580u ||
                    field.zero.kind!=(index==3015u?Kind::Object:Kind::Bool) || !metadata->property ||
                    metadata->property->type!=(index==3015u?"ObjectProperty":"BoolProperty") ||
                    (index==3015u && LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="deusex.dumplocation"))
                    throw std::runtime_error("DumpLocation return declaration is malformed");
            }
        };
        valid(*authored);if(supplied){valid(*supplied);if(supplied->source!=authored->source)throw std::runtime_error("DumpLocation source differs");}
        if(!arguments.empty() || !transaction_)throw std::runtime_error("DumpLocation natives require no arguments and an outer root");
        if(index!=3015u) {
            RequireDumpLocation(receiver);
            if(index==3039u)return {Value::Bool(persistentDumpSession.saved),{}};
            const auto field=DumpLocationField(receiver,"currentDumpLocation",Kind::Struct,"StructProperty");
            auto value=ReadDumpLocationValue(receiver,field,0u);
            auto& location=value.fields.at("location");auto& view=value.fields.at("viewrotation");
            if(location.kind!=Kind::Vector || view.kind!=Kind::Rotator)throw std::runtime_error("DumpLocation pose shape is invalid");
            std::array<std::uint32_t,3u> position{},rotation{};
            for(std::size_t i=0u;i<3u;++i){position[i]=std::bit_cast<std::uint32_t>(location.vector[i]);rotation[i]=std::bit_cast<std::uint32_t>(view.rotation[i]);}
            if(index==3037u)persistentDumpSession.Save(position,rotation);
            else if(persistentDumpSession.Load(position,rotation)) {
                for(std::size_t i=0u;i<3u;++i){location.vector[i]=std::bit_cast<float>(position[i]);view.rotation[i]=std::bit_cast<std::int32_t>(rotation[i]);}
                execution.WriteReference(ReferenceFor(receiver,field,0u,false),value);
            }
            ValidateStateBudget();return {};
        }
        if(!persistentDumpServices)throw std::runtime_error("CreateDumpLocationObject requires explicit config/file-manager bindings");
        if(!IsDerivedFromPath(receiver->cls,"DeusEx.DeusExPlayer") || !receiver->outer || Object(receiver->outer->reflection.objectPath)!=receiver->outer)
            throw std::runtime_error("CreateDumpLocationObject lacks actual DeusExPlayer.GetOuter authority");
        auto* cls=Object("DeusEx.DumpLocation");auto* outer=receiver->outer;
        if(!cls->classDescriptor || cls->classDescriptor->classFlags!=0x112u || cls->base!=Object("Core.Object") ||
            !(cls->reflection.flags&0x04000000u) || !cls->classDescriptor->classWithinReference ||
            Object(Qualified(Table(cls->sourcePath),*cls->classDescriptor->classWithinReference))!=Object("Core.Object"))
            throw std::runtime_error("DumpLocation class/base/Within/native metadata is not original");
        if(persistentDumpObjects.size()>=4096u)throw std::runtime_error("DumpLocation live object count exceeds bound");
        const auto field=DumpLocationField(cls->defaultObject,"currentDumpLocation",Kind::Struct,"StructProperty");
        auto initial=Read(cls,field,0u,true);
        // Native member constructors replace these four FString fields after
        // StaticAllocateObject's default copy. Numeric pose/id/bit storage is
        // retained; never interpret the uninitialized file-helper bytes.
        for(const auto* key:{"mapname","title","desc","gameversion"}) {
            auto& text=initial.fields.at(key);
            if(text.kind!=Kind::String)throw std::runtime_error("DumpLocation native string member shape is invalid");
            text.text.clear();
        }
        auto data=QuestVr::DumpLocation::Construct(persistentDumpServices->config,
            [this](const auto& pattern,bool files,bool directories){return DumpLocationFiles(pattern,files,directories);});
        Touch(cls);const auto name=QuestVr::ObjectNames::MakeUnique("DumpLocation",cls->uniqueObjectCounter,[&](std::string_view candidate){
            LifecycleStep();return persistentVmObjects.count(LowerAscii(outer->reflection.objectPath+'.'+std::string(candidate)))!=0u;});
        const auto objectPath=outer->reflection.objectPath+'.'+name;
        PortableReflectionObject reflection{objectPath,"DumpLocation",outer->reflection.objectPath,{},0u,0};
        auto sourcePath=cls->sourcePath;auto alias=LowerAscii(objectPath);
        EnsureConversationIndexTransaction();conversationIndices_->PrepareNextEntryCapacity();
        const QuestVr::AuthoredStructLimits valueLimits;
        QuestVr::AuthoredStructDetail::Budget valueBytes{valueLimits};
        QuestVr::AuthoredStructDetail::RetainedValue(initial,valueBytes);
        std::size_t directoryBytes=data.currentUser.capacity()+1u+data.directory.capacity()*sizeof(std::string);
        for(const auto& file:data.directory)directoryBytes+=file.capacity()+1u;
        ValidateStateBudget(sizeof(RuntimeObject)+sizeof(RuntimeNativeDumpState)+sizeof(GCAllocation)+
            QuestVr::DumpLocation::OriginalHelperBytes+directoryBytes+valueBytes.retained+
            reflection.objectPath.capacity()+reflection.metaClass.capacity()+reflection.outerPath.capacity()+
            reflection.basePath.capacity()+sourcePath.capacity()+alias.capacity()+6u+2u*sizeof(RuntimeObject*)+
            2u*(sizeof(decltype(persistentVmObjects)::value_type)+6u*sizeof(void*)+objectPath.capacity()+1u),cls);
        pendingBirthGarbage=true;provisionalDumps_.emplace_back(GC::Alloc<RuntimeObject>(std::move(reflection),nullptr));
        auto* object=provisionalDumps_.back().get();object->cls=cls;object->outer=outer;object->lookupName=NameString(name);
        object->sourcePath=std::move(sourcePath);object->exportIndex=std::numeric_limits<std::size_t>::max();
        object->birthVmKey=std::move(alias);object->references={cls,outer};object->frozenBirthDefaults=cls->classDefaultValues;
        object->nativeDump=std::make_unique<RuntimeNativeDumpState>();object->nativeDump->data=std::move(data);
        object->nativeDump->initialLocation=std::move(initial);
        object->nativeDump->helper.reset(new std::byte[QuestVr::DumpLocation::OriginalHelperBytes]);
        persistentDumpObjects.push_back(object);
        if(!persistentQualifiedObjects.emplace(objectPath,object).second || !persistentVmObjects.emplace(object->birthVmKey,object).second)
            throw std::runtime_error("DumpLocation construction collided with an actual UObject");
        PublishTransactionalLookupObject(object);ValidateStateBudget();
        if(worldStartupResult_) {
            const auto& state=object->nativeDump->data;
            std::size_t bytes=sizeof(PortableDumpLocationObservation)+objectPath.size()+outer->reflection.objectPath.size()+
                cls->reflection.objectPath.size()+state.currentUser.size()+4u+state.directory.capacity()*sizeof(std::string);
            for(const auto& file:state.directory)bytes+=file.size()+1u;
            ValidateStateBudget(bytes);
            auto& output=worldStartupResult_->dumpLocations;
            if(output.size()>=4096u)throw std::runtime_error("DumpLocation observation count exceeds caller policy");
            if(output.size()==output.capacity()) {
                const auto previous=output.capacity();const auto requested=std::max<std::size_t>(1u,previous*2u);
                ValidateStateBudget(requested*sizeof(PortableDumpLocationObservation));output.reserve(requested);
                nativeActorOrderObservationBytes_+=(output.capacity()-previous)*sizeof(PortableDumpLocationObservation);
            }
            PortableDumpLocationObservation observation{objectPath,outer->reflection.objectPath,cls->reflection.objectPath,
                state.currentUser,state.directory,object->objectIndex.Index(),object->reflection.flags,state.fileIndex,state.locationIndex,
                state.locationCount,persistentDumpSession.saved,!IsDerivedFromPath(object->cls,"Engine.Actor"),
                object->exportIndex==std::numeric_limits<std::size_t>::max(),object->worldActorIndex==-1};
            bytes=observation.path.capacity()+observation.outerPath.capacity()+observation.classPath.capacity()+
                observation.currentUser.capacity()+4u+observation.directory.capacity()*sizeof(std::string);
            for(const auto& file:observation.directory)bytes+=file.capacity()+1u;
            ValidateStateBudget(bytes);nativeActorOrderObservationBytes_+=bytes;
            output.push_back(std::move(observation));
        }
        return {Value::Text(Kind::Object,objectPath),{}};
    }
    Value ProbeDumpLocationSession(const PortableDumpLocationProbeOptions& options,
        PortableDumpLocationProbeResult& result,QuestVr::Vm::Execution& execution) {
        dumpLocationPreview_=options.rollbackOnly;
        if(options.action!=PortableDumpLocationAction::Inspect && options.action!=PortableDumpLocationAction::Save &&
            options.action!=PortableDumpLocationAction::Load)
            throw std::runtime_error("Unknown DumpLocation diagnostic action");
        auto* player=Object(options.playerPath);
        if(!IsDerivedFromPath(player->cls,"DeusEx.DeusExPlayer"))
            throw std::runtime_error("DumpLocation diagnostic requires an actual DeusExPlayer");
        const auto created=execution.CallFunction(*Member(player,"DeusEx.DeusExPlayer.CreateDumpLocationObject"),
            player->reflection.objectPath);
        if(created.kind!=Kind::Object || created.text.empty())throw std::runtime_error("DumpLocation constructor did not return an object");
        auto* object=Object(created.text);RequireDumpLocation(object);
        const auto field=DumpLocationField(object,"currentDumpLocation",Kind::Struct,"StructProperty");
        const auto& state=object->nativeDump->data;
        const QuestVr::AuthoredStructLimits valueLimits;QuestVr::AuthoredStructDetail::Budget bytes{valueLimits};
        // Admission precedes the detached constructor/typed-value copies.
        // Keep the reservation through Commit, including native3038 writes.
        bytes.Retain(sizeof(result)+object->reflection.objectPath.capacity()+object->outer->reflection.objectPath.capacity()+
            object->cls->reflection.objectPath.capacity()+state.currentUser.capacity()+4u+
            state.directory.capacity()*sizeof(std::string));
        for(const auto& file:state.directory)bytes.Retain(file.capacity()+1u);
        QuestVr::AuthoredStructDetail::RetainedValue(object->nativeDump->initialLocation,bytes);
        QuestVr::AuthoredStructDetail::RetainedValue(options.currentLocation?*options.currentLocation:object->nativeDump->initialLocation,bytes);
        ValidateStateBudget(bytes.retained);nativeQueryScratchBytes_+=bytes.retained;
        result.before=ReadDumpLocationValue(object,field,0u);
        const auto has=[&] {
            const auto saved=execution.CallFunction(*Member(object,"DeusEx.DumpLocation.HasLocationBeenSaved"),created.text);
            if(saved.kind!=Kind::Bool)throw std::runtime_error("HasLocationBeenSaved returned a non-Bool");
            return saved.boolean;
        };
        result.savedBefore=has();
        result.object={created.text,object->outer->reflection.objectPath,object->cls->reflection.objectPath,
            state.currentUser,state.directory,object->objectIndex.Index(),object->reflection.flags,state.fileIndex,state.locationIndex,
            state.locationCount,result.savedBefore,!IsDerivedFromPath(object->cls,"Engine.Actor"),
            object->exportIndex==std::numeric_limits<std::size_t>::max(),object->worldActorIndex==-1};
        if(options.currentLocation)execution.WriteReference(ReferenceFor(object,field,0u,false),*options.currentLocation);
        if(options.action!=PortableDumpLocationAction::Inspect)
            static_cast<void>(execution.CallFunction(*Member(object,options.action==PortableDumpLocationAction::Save?
                "DeusEx.DumpLocation.SaveLocation":"DeusEx.DumpLocation.LoadLocation"),created.text));
        result.savedAfter=has();result.after=ReadDumpLocationValue(object,field,0u);
        ValidateStateBudget();result.available=true;return {};
    }
