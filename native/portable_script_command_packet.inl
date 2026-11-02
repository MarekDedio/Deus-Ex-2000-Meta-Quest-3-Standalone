// Typed equivalent of ScriptConsoleExec's selected parameter storage. No raw
// PE32 ParmsSize/offset is guessed from property counts. Fixed-array packets
// and unreached native ImportText kinds remain explicit missing services.
    struct ScriptCommandCell {
        Property property;
        std::vector<std::shared_ptr<const Value>> versions;
        std::vector<GCRoot<RuntimeObject>> objectVersions;
        std::shared_ptr<const Value> current;
        bool destroyed{};
    };
    struct ScriptCommandPacket {
        GCRoot<RuntimeObject> receiver,executor,method;
        std::shared_ptr<const Function> function;
        std::vector<GCRoot<RuntimeObject>> properties;
        std::vector<std::shared_ptr<ScriptCommandCell>> cells;
        std::vector<Evaluation> arguments;
    };
    std::vector<std::shared_ptr<ScriptCommandPacket>> scriptCommandPackets_;
    std::size_t ScriptCommandPacketBytes() const {
        using QuestVr::DecalStateDetail::Add;
        auto bytes=scriptCommandPackets_.capacity()*sizeof(std::shared_ptr<ScriptCommandPacket>);
        for(const auto& packet:scriptCommandPackets_){
            bytes=Add(bytes,sizeof(ScriptCommandPacket)+3u*sizeof(GCRootNode)+
                packet->properties.capacity()*sizeof(GCRoot<RuntimeObject>)+packet->properties.size()*sizeof(GCRootNode)+
                packet->cells.capacity()*sizeof(std::shared_ptr<ScriptCommandCell>)+packet->arguments.capacity()*sizeof(Evaluation));
            for(const auto& cell:packet->cells){
                bytes=Add(bytes,sizeof(ScriptCommandCell)+sizeof(Reference)+128u+cell->property.key.capacity()+
                    cell->property.name.capacity()+DestroyRetainedValue(cell->property.zero)+
                    cell->versions.capacity()*sizeof(std::shared_ptr<const Value>)+
                    cell->objectVersions.capacity()*sizeof(GCRoot<RuntimeObject>)+cell->objectVersions.size()*sizeof(GCRootNode));
                for(const auto& version:cell->versions)bytes=Add(bytes,sizeof(Value)+32u+DestroyRetainedValue(*version));
            }
        }
        return bytes;
    }
    void ScriptCommandStore(const std::shared_ptr<ScriptCommandCell>& cell,const Value& incoming) {
        if(incoming.kind!=cell->property.zero.kind)throw std::runtime_error("Script command packet cell changed its native type");
        // Every previous immutable version remains owned until outer undo ends.
        // Reference rollback finds its existing version and cannot allocate or
        // reauthenticate now-revoked native/receiver objects.
        for(const auto& prior:cell->versions)if(incoming.kind==Kind::Float?
            std::bit_cast<std::uint32_t>(prior->floating)==std::bit_cast<std::uint32_t>(incoming.floating):
            QuestVr::Vm::Equal(*prior,incoming)){cell->current=prior;return;}
        const auto bytes=DestroyRetainedValue(incoming);
        ValidateStateBudget(bytes+sizeof(Value)+128u+(cell->versions.size()+1u)*(sizeof(std::shared_ptr<const Value>)+sizeof(GCRootNode)+sizeof(GCRoot<RuntimeObject>)));
        auto typed=TypedLiveValue(cell->property,incoming);
        GCRoot<RuntimeObject> pin(typed.kind==Kind::Object&&!typed.text.empty()?Object(typed.text):nullptr);
        auto next=std::make_shared<const Value>(std::move(typed));cell->objectVersions.push_back(std::move(pin));
        cell->versions.push_back(next);cell->current=std::move(next);
        ValidateStateBudget();
    }
    void ValidateScriptCommandPacket(const std::shared_ptr<ScriptCommandPacket>& packet) {
        LifecycleStep();
        if(!transaction_||!packet||!packet->receiver||!packet->method||
            Object(packet->receiver->reflection.objectPath)!=packet->receiver.get()||
            Object(packet->method->reflection.objectPath)!=packet->method.get()||!packet->method->script||
            packet->function->path!=packet->method->reflection.objectPath||
            packet->function->source!=packet->method->sourcePath||
            packet->function->flags!=packet->method->script->functionFlags||
            packet->function->nativeIndex!=packet->method->script->nativeIndex)
            throw std::runtime_error("Script command lost its retained actual receiver/function packet");
        if(packet->receiver->nativeConsoleObject)RequireNativeViewportConsole(packet->receiver.get());
        else RequireMapActor(packet->receiver.get());
    }
    std::shared_ptr<ScriptCommandPacket> MakeScriptCommandPacket(RuntimeObject* receiver,RuntimeObject* executor,RuntimeObject* method) {
        if(!transaction_||!method||!method->script)throw std::runtime_error("Script command packet lacks its actual function/root");
        ValidateStateBudget(sizeof(ScriptCommandPacket)+4096u);
        auto packet=std::make_shared<ScriptCommandPacket>();packet->receiver.set(receiver);packet->executor.set(executor);packet->method.set(method);
        packet->function=FunctionFor(method);
        std::size_t count{};for(const auto& field:packet->function->variables){LifecycleStep();if((field.flags&0x480u)!=0x80u)break;++count;}
        ValidateStateBudget(count*(sizeof(ScriptCommandCell)+sizeof(Reference)+sizeof(Value)+sizeof(Evaluation)+sizeof(GCRootNode)+256u));
        packet->properties.reserve(count);packet->cells.reserve(count);packet->arguments.reserve(count);
        scriptCommandPackets_.push_back(packet);
        for(std::size_t i=0u;i<count;++i){
            LifecycleStep();const auto& property=packet->function->variables[i];auto* field=Object(property.key);
            if(field->outer!=method||!field->property||field->property->baseField||property.arrayDimension!=1u||
                LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                    "core."+LowerAscii(field->property->type))
                throw std::runtime_error("Script command lacks its actual scalar-per-property parameter declaration");
            const auto& type=field->property->type;
            if(type!="ByteProperty"&&type!="IntProperty"&&type!="BoolProperty"&&type!="FloatProperty"&&type!="NameProperty"&&
                type!="StrProperty"&&type!="ObjectProperty"&&type!="ClassProperty")
                throw std::runtime_error("Script command packet layout/selected cleanup is not prepared: "+type);
            if(type=="StrProperty")RequireDestroyProperty(field,"StrProperty",9u);
            else if(field->property->flags&0x00400000u)
                throw std::runtime_error("Script command scalar unexpectedly selects dynamic destruction");
            packet->properties.emplace_back(field);auto cell=std::make_shared<ScriptCommandCell>();cell->property=property;
            cell->current=std::make_shared<const Value>(property.zero);cell->versions.push_back(cell->current);packet->cells.push_back(cell);
            auto reference=std::make_shared<Reference>();reference->zero=property.zero;reference->resultLayout=property.resultLayout;
            reference->read=[cell]{return *cell->current;};
            reference->write=[this,cell](const Value& next){ScriptCommandStore(cell,next);};
            packet->arguments.push_back({property.zero,reference});ValidateStateBudget();
        }
        ValidateScriptCommandPacket(packet);return packet;
    }
    float ImportScriptCommandFloat(std::u16string_view text) {
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        QuestVr::OriginalInputAxisQueries q;q.work=[this]{LifecycleStep();};
        q.enterScratch=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveScratch=[&](std::size_t bytes)noexcept{frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        QuestVr::InputAxisCommandDetail::Context context{q};return QuestVr::InputAxisCommandDetail::Atof(text,context);
    }
    std::optional<std::size_t> ImportScriptCommandCell(const std::shared_ptr<ScriptCommandPacket>& packet,std::size_t slot,
        std::u16string_view text,std::size_t cursor,std::uint32_t flags) {
        ValidateScriptCommandPacket(packet);
        if(flags!=1u||cursor>text.size()||slot>=packet->cells.size())throw std::runtime_error("Script command ImportText lost its actual packet/cursor/flags");
        auto* field=packet->properties[slot].get();auto cell=packet->cells[slot];const auto input=text.substr(cursor);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,16u*(input.size()+1u)+8192u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto& type=field->property->type;QuestVr::NativeConfigImport::Result imported;
        auto value=*cell->current;
        if(type=="FloatProperty"){
            value=Value::Float(ImportScriptCommandFloat(input));std::size_t end{};
            while(end<input.size()&&input[end]!=u','&&input[end]!=u')'&&input[end]!=u'\r'&&input[end]!=u'\n'){LifecycleStep();++end;}
            imported={true,end};
        }else if(type=="IntProperty"){
            auto integer=value.integer;imported=QuestVr::NativeConfigImport::Integer(input,integer);value=Value::Integer(integer);
        }else if(type=="BoolProperty"){
            std::uint32_t bits=value.boolean?1u:0u;imported=QuestVr::NativeConfigImport::Boolean(input,1u,bits);value=Value::Bool(bits!=0u);
        }else if(type=="StrProperty"){
            std::u16string string;imported=QuestVr::NativeConfigImport::String(input,flags,string);value=Value::Text(Kind::String,QuestVr::UnrealString::Encode(string));
        }else if(type=="NameProperty"){
            NameString name(value.text);imported=QuestVr::NativeConfigImport::Name(input,name,[](std::u16string_view spelling){return NameString(QuestVr::UnrealString::Encode(spelling));});
            value=Value::Text(Kind::Name,name.ToString());
        }else if(type=="ByteProperty"){
            auto numeric=input;std::size_t consumed{};
            if(field->property->referencedType){
                auto* enumeration=Object(Qualified(Table(field->sourcePath),field->property->referencedType));
                if(!enumeration->enumDescriptor)throw std::runtime_error("Script Byte import lacks its actual Enum");
                const auto token=QuestVr::NativeConfigImport::ReadToken(input);
                if(!token.result.returned)return {};
                const auto name=NameString::FindCompareIndex(QuestVr::UnrealString::Encode(token.value));
                if(name)for(std::size_t i=0u;i<enumeration->enumDescriptor->names.size();++i){LifecycleStep();
                    if(enumeration->enumDescriptor->names[i].GetCompareIndex()==*name){
                        if(i>255u)throw std::runtime_error("Script Byte Enum ordinal exceeds its native storage");
                        ScriptCommandStore(cell,Value::Byte(static_cast<std::uint8_t>(i)));return cursor+token.result.nextUnit;}}
                consumed=token.result.nextUnit;numeric=input.substr(consumed);
            }
            if(numeric.empty()||numeric.front()<u'0'||numeric.front()>u'9')return {};
            std::uint32_t integer{};std::size_t digits{};
            while(digits<numeric.size()&&numeric[digits]>=u'0'&&numeric[digits]<=u'9'){
                LifecycleStep();const auto digit=static_cast<std::uint32_t>(numeric[digits]-u'0');
                if(integer>(2147483647u-digit)/10u)
                    throw std::runtime_error("Script Byte ImportText appAtoi overflow profile is not established");
                integer=integer*10u+digit;++digits;}
            value=Value::Byte(static_cast<std::uint8_t>(integer));imported={true,consumed+digits};
        }else throw std::runtime_error("Original ScriptConsoleExec selected ImportText service is not prepared: "+type);
        if(imported.returned)ScriptCommandStore(cell,value);
        return imported.returned?std::optional<std::size_t>{cursor+imported.nextUnit}:std::nullopt;
    }
    bool OriginalScriptCommandEvent(const std::shared_ptr<ScriptCommandPacket>& packet,QuestVr::Vm::Execution& execution) {
        ValidateScriptCommandPacket(packet);auto* receiver=packet->receiver.get();const auto& function=*packet->function;
        auto* owner=packet->method->outer;if(owner&&owner->stateDescriptor)owner=owner->outer;
        if(!owner||!owner->classDescriptor||!IsDerivedFromPath(receiver->cls,owner->reflection.objectPath))
            throw std::runtime_error("Script command retained function does not belong to its actual receiver");
        const bool console=receiver->nativeConsoleObject;
        if(console){
            RequireNativeViewportConsole(receiver);
            // Exact Engine.Console104297C0: +10 Core.ProcessEvent,
            // +2C Core.IsPendingKill=false, +18 Core.ProcessRemoteFunction=false.
            if(receiver->cls!=Object("Engine.Console")||receiver->cls->base!=Object("Core.Object"))
                throw std::runtime_error("Script command Console native virtual profile is not prepared");
        }else{
            receiver=LifecycleActor(reinterpret_cast<std::uintptr_t>(receiver));OriginalAnimationActorVirtuals(receiver);
            const auto levelField=OriginalAnimationField(receiver,"Engine.Actor","Level",Kind::Object,2u,"ObjectProperty");
            const auto levelValue=OriginalAnimationRead(receiver,levelField,Kind::Object);
            ValidateObjectValue(levelValue,"Engine.LevelInfo",false);auto* level=Object(levelValue.text);
            const auto begun=OriginalAnimationField(level,"Engine.LevelInfo","bBegunPlay",Kind::Bool,0u,"BoolProperty");
            if(!OriginalAnimationRead(level,begun,Kind::Bool).boolean)return false;
        }
        if(!OriginalGameScriptsEnabled()||!CanCall(function,receiver->reflection.objectPath))return false;
        if(!console&&OriginalAnimationPendingKill(receiver))return false;
        if(function.nativeIndex)return false;
        if(!console&&(function.flags&0x400u))throw std::runtime_error("Original script command Actor RPC virtual branch is not represented");
        static_cast<void>(execution.CallProcessEventPacket(function,receiver->reflection.objectPath,packet->arguments));return true;
    }
    struct ScriptCommandCallOperations {
        PortableActorVmHost& host;RuntimeObject *receiver{},*executor{},*method{};
        QuestVr::Vm::Execution& execution;std::string& output;std::size_t base{},frames{};
        void Step(){host.LifecycleStep();}
        void EnterFrame(std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            host.nativeQueryScratchBytes_=base;host.ValidateStateBudget(next);frames=next;host.nativeQueryScratchBytes_=base+frames;}
        void LeaveFrame(std::size_t bytes)noexcept{frames-=bytes;host.nativeQueryScratchBytes_=base+frames;}
        auto ZeroPacket(){return host.MakeScriptCommandPacket(receiver,executor,method);}
        void Validate(const std::shared_ptr<ScriptCommandPacket>& packet){host.ValidateScriptCommandPacket(packet);}
        // Handles are1-based indexes into the retained immutable input plan.
        std::size_t FirstProperty(const std::shared_ptr<ScriptCommandPacket>& p){return p->cells.empty()?0u:1u;}
        std::size_t NextProperty(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i){return i<p->cells.size()?i+1u:0u;}
        std::uint32_t PropertyFlags(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i){return p->cells.at(i-1u)->property.flags;}
        bool IsObjectProperty(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i){
            return p->properties.at(i-1u)->property->type=="ObjectProperty";}
        bool HasExecutor(){return executor!=nullptr;}
        bool ExecutorCompatible(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i){auto* field=p->properties.at(i-1u).get();
            if(!field->property->referencedType)throw std::runtime_error("Script Object parameter lost its declared Class");
            return IsDerivedFromPath(executor->cls,host.Qualified(host.Table(field->sourcePath),field->property->referencedType));}
        void InjectExecutor(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i){
            host.ScriptCommandStore(p->cells.at(i-1u),Value::Text(Kind::Object,executor->reflection.objectPath));}
        auto ImportText(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i,std::u16string_view t,std::size_t c,std::uint32_t f){
            return host.ImportScriptCommandCell(p,i-1u,t,c,f);}
        void BadProperty(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i){static_cast<void>(output);
            throw std::runtime_error("Original ScriptConsoleExec localized BadProperty output service is not prepared: "+
                p->function->path+" / "+p->cells.at(i-1u)->property.name);}
        void ProcessEvent(const std::shared_ptr<ScriptCommandPacket>& p){static_cast<void>(host.OriginalScriptCommandEvent(p,execution));}
        void BeginCleanup(const std::shared_ptr<ScriptCommandPacket>&,bool)noexcept{}
        void EndCleanup(const std::shared_ptr<ScriptCommandPacket>&)noexcept{}
        void DestroyValue(const std::shared_ptr<ScriptCommandPacket>& p,std::size_t i,bool unwinding){
            auto& cell=*p->cells.at(i-1u);if(cell.destroyed)return;
            if(!unwinding)host.LifecycleStep();
            // Plan construction authenticated scalar UProperty no-op or actual
            // UStrProperty destructor. Release logical dynamic packet storage,
            // retaining immutable versions ONLY for safe enclosing VM undo.
            if(cell.property.zero.kind==Kind::String)cell.current=cell.versions.front();
            cell.destroyed=true;
        }
    };
    void ExecuteSelectedScriptCommand(RuntimeObject* receiver,RuntimeObject* executor,RuntimeObject* method,
        std::u16string_view command,std::size_t cursor,QuestVr::Vm::Execution& execution,std::string& output){
        const auto previous=nativeQueryScratchBytes_;
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        ScriptCommandCallOperations operations{*this,receiver,executor,method,execution,output,previous};
        static_cast<void>(QuestVr::ScriptCommandCall::Call(operations,command,cursor));
    }
