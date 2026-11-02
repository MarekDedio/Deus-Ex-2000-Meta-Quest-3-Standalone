// Actual XRootWindow.HandleVirtualKeyboard1003B820..1003B951.
// Root.Process's grab decision/key-map stores remain its caller's work.
// No char input, focus mutation, navigation or mouse-button provider is invented.
    void RequireKeyboardWindow(RuntimeObject* window) {
        LifecycleStep();RequireNativeWindow(window);
        if(Object(window->reflection.objectPath)!=window)
            throw std::runtime_error("Native keyboard event lost its actual Window identity");
        // Deliberately NO bBeingDestroyed test here. Retail tests it only
        // after an UNHANDLED callback. A handled callback may destroy itself.
    }
    Value KeyboardWindowScalar(RuntimeObject* window,const char* key,Kind kind,
        const char* owner,const char* type,const char* constraint="",std::uint64_t flags=2u) {
        RequireKeyboardWindow(window);
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if(property.zero.kind!=kind||property.arrayDimension!=1u||property.flags!=flags||
            !OwnsProperty(window->cls,property)||!field->property||
            field->outer!=Object(owner)||field->property->type!=type)
            throw std::runtime_error("Native keyboard field lost its exact original declaration");
        if(*constraint) {
            if(!field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!=LowerAscii(constraint))
                throw std::runtime_error("Native keyboard field lost its original referenced type");
        }else if(field->property->referencedType)
            throw std::runtime_error("Native keyboard scalar acquired a foreign referenced type");
        const auto value=Read(window,property,0u);
        if(value.kind!=kind||(kind==Kind::Byte&&(value.integer<0||value.integer>255))||
            (kind==Kind::Int&&(value.integer<std::numeric_limits<std::int32_t>::min()||
                value.integer>std::numeric_limits<std::int32_t>::max())))
            throw std::runtime_error("Native keyboard actual field has an invalid live Value kind/range");
        return value;
    }
    void CheckKeyboardWindowNotDestroyed(RuntimeObject* window) {
        // Original asserts1003B8B3 and1003B8E9 test native flag+30 bit10.
        if(KeyboardWindowScalar(window,"Extension.Window.bBeingDestroyed",Kind::Bool,
            "Extension.Window","BoolProperty").boolean)
            throw std::runtime_error("Native keyboard propagation reached a destroyed Window");
    }
    bool NativeKeyboardScriptEvent(RuntimeObject* window,bool raw,std::int32_t key,
        std::int32_t action,bool repeat,QuestVr::Vm::Execution& execution) {
        RequireKeyboardWindow(window);
        // Source-primary-vtable profile: all currently admitted native types
        // below have +BC10003750/+CC10003A40. XEditWindow, XListWindow and
        // XComputerWindow have distinct +CC implementations and MUST refuse.
        const auto native=NativeWindowAncestor(window->cls);
        if(native!="extension.window"&&native!="extension.tabgroupwindow"&&
            native!="extension.modalwindow"&&native!="extension.rootwindow"&&
            native!="extension.radioboxwindow"&&native!="extension.tilewindow"&&
            native!="extension.textwindow"&&native!="extension.textlogwindow"&&
            native!="extension.buttonwindow"&&native!="extension.togglewindow")
            throw std::runtime_error("Native keyboard virtual override is not implemented: "+native);
        // Selector/packet temporaries join the SAME enclosing scratch ledger.
        //8192 is the existing runtime metadata-path profile, not a traversal cap.
        const auto bytes=QuestVr::DecalStateDetail::Add(1024u+4u*sizeof(Evaluation),
            8u*(window->reflection.objectPath.size()+window->cls->reflection.objectPath.size()+8193u));
        QuestVr::NativeScratchScope packet(nativeQueryScratchBytes_,bytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        const char* name=raw?"RawKeyPressed":"VirtualKeyPressed";
        const auto identity=OptionalNativeWindowEvent(window,name);
        //10003778/10003A61 initialize ReturnValue=0, then invoke actual
        // ProcessScript1004DED0(...,checked=false). Genuine absent script is
        // therefore false; an unavailable virtual/metadata provider is NOT.
        if(!identity)return false;
        const auto selected=FunctionFor(Object(identity->path));
        const auto authored=FunctionFor(Object(std::string("Extension.Window.")+name));
        const auto validate=[&](const Function& function,bool base) {
            if(function.flags&0x400u)
                throw std::runtime_error("Native keyboard script callback has an unsupported native declaration");
            const std::size_t count=raw?4u:3u;
            std::array<const Property*,4u> parameters{};std::size_t found{};
            for(const auto& field:function.variables) {
                LifecycleStep();if(!(field.flags&0x80u))continue;
                if(found==count)throw std::runtime_error("Native keyboard callback has extra parameters");
                parameters[found++]=&field;
            }
            if(found!=count)throw std::runtime_error("Native keyboard callback lacks its actual packet");
            for(std::size_t slot=0u;slot<count;++slot) {
                LifecycleStep();const auto& field=*parameters[slot];auto* metadata=Object(field.key);
                const bool byte=slot==0u||(raw&&slot==1u),result=slot+1u==count;
                if(field.zero.kind!=(byte?Kind::Byte:Kind::Bool)||field.arrayDimension!=1u||
                    field.flags!=(result?0x580u:0x80u)||!metadata->property||
                    metadata->outer!=Object(function.path)||
                    metadata->property->type!=(byte?"ByteProperty":"BoolProperty"))
                    throw std::runtime_error("Native keyboard callback changed its copied Byte/Bool packet");
                const auto reference=metadata->property->referencedType;
                if(byte) {
                    const auto expected=slot==0u?"Extension.ExtensionObject.EInputKey":
                        "Extension.ExtensionObject.EInputState";
                    if(!reference||LowerAscii(Qualified(Table(metadata->sourcePath),reference))!=LowerAscii(expected))
                        throw std::runtime_error("Native keyboard callback changed its actual Enum identity");
                }else if(reference)
                    throw std::runtime_error("Native keyboard Bool packet has a foreign referenced type");
            }
            if(base&&(LowerAscii(function.path)!=LowerAscii(std::string("Extension.Window.")+name)||
                function.nativeIndex!=0u||function.flags!=0x802u))
                throw std::runtime_error("Native keyboard base event is not its original owned declaration");
        };
        validate(*authored,true);validate(*selected,false);
        if(rootInputObservation_) {
            if(selected->path.size()>8192u)throw std::runtime_error("Native keyboard observation path exceeds its admitted storage");
            if(raw){++rootInputObservation_->rawCallbacks;rootInputObservation_->lastRawFunction=selected->path;}
            else {++rootInputObservation_->virtualCallbacks;rootInputObservation_->lastVirtualFunction=selected->path;}
        }
        // The original native wrappers truncate Key/Action to Byte but repeat
        // is a script Bool, NOT an Int despite the native DWORD/FBOOL signature.
        const auto byteKey=Value::Byte(static_cast<std::uint8_t>(key));
        Value value;
        if(raw)value=execution.CallFunction(*selected,window->reflection.objectPath,
            {{byteKey,{}},{Value::Byte(static_cast<std::uint8_t>(action)),{}},{Value::Bool(false),{}}});
        else value=execution.CallFunction(*selected,window->reflection.objectPath,
            {{byteKey,{}},{Value::Bool(repeat),{}}});
        if(value.kind!=Kind::Bool)
            throw std::runtime_error("Native keyboard callback did not return its actual Bool");
        return value.boolean;
    }
    bool HandleNativeRootVirtualKeyboard(RuntimeObject* root,std::int32_t key,std::int32_t action,
        bool duplicatePress,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Native Root keyboard handling requires its actual outer VM root");
        if(key<0||key>=255)throw std::runtime_error("Native Root keyboard key exceeds its actual input domain");
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,
            512u+2u*sizeof(GCRootNode)+2u*sizeof(GCRoot<RuntimeObject>),
            [this](std::size_t extra){ValidateStateBudget(extra);});
        RequireKeyboardWindow(root);
        if(!root->nativeRootWindowObject||!IsDerivedFromPath(root->cls,"Extension.RootWindow")||
            NativeWindowRoot(root)!=root)
            throw std::runtime_error("Native keyboard handling requires the actual terminal constructed Root");
        GCRoot<RuntimeObject> rootPin(root),current(nullptr);
        const auto focus=KeyboardWindowScalar(root,"Extension.RootWindow.FocusWindow",Kind::Object,
            "Extension.RootWindow","ObjectProperty","Extension.Window");
        if(!focus.text.empty())current.set(Object(focus.text));
        else {
            NativeWindowNavigationOperations operations{*this};
            auto* candidate=QuestVr::QueryNativeWindowRelative(root,
                QuestVr::NativeWindowRelativeQuery::TopChild,true,operations);
            while(candidate) {
                const auto type=KeyboardWindowScalar(candidate,"Extension.Window.windowType",Kind::Byte,
                    "Extension.Window","ByteProperty","",0u);
                if(type.integer>=2)break;
                candidate=QuestVr::QueryNativeWindowRelative(candidate,
                    QuestVr::NativeWindowRelativeQuery::LowerSibling,true,operations);
            }
            current.set(candidate?candidate:root);
        }
        while(current) {
            LifecycleStep();auto* window=current.get();RequireKeyboardWindow(window);
            if(!duplicatePress&&NativeKeyboardScriptEvent(window,true,key,action,false,execution))return true;
            CheckKeyboardWindowNotDestroyed(window);
            if(action==1&&NativeKeyboardScriptEvent(window,false,key,action,duplicatePress,execution))return true;
            CheckKeyboardWindowNotDestroyed(window);
            // AFTER both callbacks/checks: do not snapshot parent before them.
            // WindowLink's added typed-value check below prevents malformed
            // object storage from silently becoming a genuine None parent.
            current.set(WindowLink(window,"winParent"));
        }
        return false;
    }

// Root.Process's selected grab consumes independently of this handled result.
// Navigation1442..1447, mouse handlers, character input, Quest reconciliation,
// completed Input.Init/publication and ReadInput remain separate real services.
