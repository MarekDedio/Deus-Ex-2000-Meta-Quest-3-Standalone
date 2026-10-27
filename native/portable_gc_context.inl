// Included inside PortableActorVmHost. Actual transient GC UObjects, original
// declarations and existing outer journal; no presentation context substitute.
    void RequireNativeGcContext(RuntimeObject* object) {
        if(!object || !object->nativeWindowObject || !object->cls ||
            !IsDerivedFromPath(object->cls,"Extension.GC") ||
            NativeWindowAncestor(object->cls)!="extension.gc" ||
            !persistentWindowDomain || !persistentWindowDomain->get() ||
            std::find(persistentWindowDomain->get()->objects.begin(),
                persistentWindowDomain->get()->objects.end(),object)==persistentWindowDomain->get()->objects.end() ||
            Object(object->reflection.objectPath)!=object)
            throw std::runtime_error("Native GC requires an actual live constructor-owned graphics context");
    }
    void ValidateNativeGcContextFields(RuntimeObject* object) {
        RequireNativeGcContext(object);
        const auto scalar=[&](const char* name,Kind kind,const char* type,const char* constraint="",
            std::uint32_t flags=0u) {
            LifecycleStep();
            ValidateNativeWindowScalar(object,("Extension.GC."+std::string(name)).c_str(),kind,type,constraint,flags);
        };
        scalar("Canvas",Kind::Object,"ObjectProperty","Engine.Canvas",0x1000u);
        scalar("gcClipRect",Kind::Struct,"StructProperty","Extension.ExtensionObject.ClipRect");
        scalar("Style",Kind::Byte,"ByteProperty");
        for(const auto* name:{"bSmoothed","bDrawEnabled","bMasked","bTranslucent","bModulated",
            "bTextTranslucent","bWordWrap","bParseMetachars","bFree"}) scalar(name,Kind::Bool,"BoolProperty");
        for(const auto* name:{"PolyFlags","textPolyFlags","hMultiplier","vMultiplier","gcCount"})
            scalar(name,Kind::Int,"IntProperty");
        for(const auto* name:{"tileColor","TextColor"}) scalar(name,Kind::Struct,"StructProperty","Core.Object.Color");
        for(const auto* name:{"tilePlane","textPlane"}) scalar(name,Kind::Struct,"StructProperty","Core.Object.Plane");
        for(const auto* name:{"normalFont","boldFont"}) scalar(name,Kind::Object,"ObjectProperty","Engine.Font");
        scalar("underlineTexture",Kind::Object,"ObjectProperty","Engine.Texture");
        for(const auto* name:{"underlineHeight","baselineOffset","textVSpacing"}) scalar(name,Kind::Float,"FloatProperty");
        scalar("HAlign",Kind::Byte,"ByteProperty","Extension.ExtensionObject.EHAlign");
        scalar("VAlign",Kind::Byte,"ByteProperty","Extension.ExtensionObject.EVAlign");
        for(const auto* name:{"gcOwner","gcStack","gcFree"}) scalar(name,Kind::Object,"ObjectProperty","Extension.GC");
    }
    Value NativeGcRead(RuntimeObject* object,const char* name) {
        RequireNativeGcContext(object);
        std::string key{"Extension.GC."};key.append(name);
        return Read(object,DescribeProperty(key),0u);
    }
    RuntimeObject* NativeGcLink(RuntimeObject* object,const char* name) {
        const auto value=NativeGcRead(object,name);
        if(value.kind!=Kind::Object) throw std::runtime_error("Native GC link is not its original Object field");
        auto* linked=value.text.empty()?nullptr:Object(value.text);
        if(linked) RequireNativeGcContext(linked);
        return linked;
    }
    void NativeGcWrite(RuntimeObject* object,const char* name,const Value& value,QuestVr::Vm::Execution& execution) {
        RequireNativeGcContext(object);
        NativeWindowWrite(object,("Extension.GC."+std::string(name)).c_str(),value,execution);
    }
    void ValidateNativeGcLists(RuntimeObject* receiver) {
        // Validate current owned edges before any stack mutation. A cycle is an
        // explicit charged failure, never an endless pop or guessed lost node.
        LifecycleStep();
        if(NativeGcLink(receiver,"gcOwner")) return; // Original owner-bound no-op path.
        // Every link is authenticated against this finite owned domain. More
        // visits than domain objects proves repetition; stack/free sharing is
        // also impossible under their opposite required bFree values. This
        // needs constant scratch, not a32KiB array for an empty/one-node list.
        const auto maximum=persistentWindowDomain->get()->objects.size();std::size_t count{};
        for(const bool free:{false,true}) {
            auto* node=NativeGcLink(receiver,free?"gcFree":"gcStack");
            while(node) {
                LifecycleStep();
                if(count++==maximum)throw std::runtime_error("Native GC lists are cyclic or exceed their actual owned domain");
                const auto flag=NativeGcRead(node,"bFree");
                if(node==receiver || node->outer!=receiver || NativeGcLink(node,"gcOwner")!=receiver ||
                    NativeGcLink(node,"gcFree") || flag.kind!=Kind::Bool || flag.boolean!=free)
                    throw std::runtime_error("Native GC list node has malformed ownership or membership");
                node=NativeGcLink(node,"gcStack");
            }
        }
    }
    struct NativeGcOperations {
        using Node=RuntimeObject*;
        PortableActorVmHost& host;QuestVr::Vm::Execution& execution;
        void Step() {host.LifecycleStep();}
        Value ReadContextField(Node source,const char* name) {return host.NativeGcRead(source,name);}
        void WriteContextField(Node destination,const char* name,const Value& value) {
            host.NativeGcWrite(destination,name,value,execution);
        }
        Node ReadOwner(Node node) {return host.NativeGcLink(node,"gcOwner");}
        Node ReadStack(Node node) {return host.NativeGcLink(node,"gcStack");}
        Node ReadFree(Node node) {return host.NativeGcLink(node,"gcFree");}
        std::int32_t ReadCount(Node node) {
            const auto value=host.NativeGcRead(node,"gcCount");
            if(value.kind!=Kind::Int)throw std::runtime_error("Native GC count is not its original signed Int");
            return value.integer;
        }
        void Link(Node node,const char* name,Node value) {
            if(value)host.RequireNativeGcContext(value);
            host.NativeGcWrite(node,name,Value::Text(Kind::Object,value?value->reflection.objectPath:std::string{}),execution);
        }
        void WriteOwner(Node node,Node value) {Link(node,"gcOwner",value);}
        void WriteStack(Node node,Node value) {Link(node,"gcStack",value);}
        void WriteFree(Node node,Node value) {Link(node,"gcFree",value);}
        void WriteCount(Node node,std::int32_t value) {host.NativeGcWrite(node,"gcCount",Value::Integer(value),execution);}
        void WriteFreeFlag(Node node,bool value) {host.NativeGcWrite(node,"bFree",Value::Bool(value),execution);}
        void CopyContext(Node destination,Node source) {
            host.ValidateNativeGcContextFields(destination);host.ValidateNativeGcContextFields(source);
            QuestVr::NativeGcContext::Copy(*this,destination,source);
        }
        Node AllocateCopy(Node source) {
            host.ValidateNativeGcContextFields(source);
            auto* node=host.ConstructWindowObject(host.DefaultClass(host.Object("Extension.GC")),source,execution);
            host.ValidateNativeGcContextFields(node);
            // Original copying ctor10026C70, not InitializeWindowGC's default
            // ctor: base identity, zero clip, Copy, then five protocol stores.
            host.ZeroNativeWindowFloatStruct(node,"Extension.GC.gcClipRect",execution);
            CopyContext(node,source);
            WriteFreeFlag(node,false);WriteStack(node,nullptr);WriteFree(node,nullptr);
            WriteOwner(node,nullptr);WriteCount(node,1);
            return node;
        }
    };
    RuntimeObject* NativeWindowGetGc(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);
        for(const auto* name:{"gGc","wGc"})
            ValidateNativeWindowScalar(window,("Extension.Window."+std::string(name)).c_str(),
                Kind::Object,"ObjectProperty","Extension.GC");
        LifecycleStep();const auto general=Read(window,DescribeProperty("Extension.Window.gGc"),0u);
        if(general.kind!=Kind::Object)throw std::runtime_error("Window.GetGC general context is not Object");
        if(general.text.empty())return nullptr;
        auto* gc=Object(general.text);ValidateNativeGcContextFields(gc);ValidateNativeGcLists(gc);
        NativeGcOperations operations{*this,execution};
        static_cast<void>(QuestVr::NativeGcContext::Push(operations,gc));
        // Original rereads wGc after Push, not before its allocation/copy.
        LifecycleStep();const auto drawing=Read(window,DescribeProperty("Extension.Window.wGc"),0u);
        if(drawing.kind!=Kind::Object || drawing.text.empty())
            throw std::runtime_error("Window.GetGC cannot copy a None or invalid drawing context");
        operations.CopyContext(gc,Object(drawing.text));
        return gc;
    }
    void NativeWindowReleaseGc(RuntimeObject* gc,QuestVr::Vm::Execution& execution) {
        if(!gc)return;
        ValidateNativeGcContextFields(gc);ValidateNativeGcLists(gc);
        NativeGcOperations operations{*this,execution};QuestVr::NativeGcContext::Pop(operations,gc);
    }
    std::shared_ptr<const Function> NativeGcDeclaration(std::uint16_t index,const Function* declaration) {
        struct Parameter {const char* name;Kind kind;const char* type;std::uint32_t flags;const char* constraint;};
        const char* path{};std::vector<Parameter> parameters;
        switch(index) {
        case 1270u:path="Extension.GC.CopyGC";parameters={{"Copy",Kind::Object,"ObjectProperty",0x80u,"Extension.GC"}};break;
        case 1271u:path="Extension.GC.PushGC";parameters={{"ReturnValue",Kind::Int,"IntProperty",0x580u,""}};break;
        case 1272u:path="Extension.GC.PopGC";parameters={{"gcNum",Kind::Int,"IntProperty",0x90u,""}};break;
        case 1485u:path="Extension.Window.GetGC";parameters={{"ReturnValue",Kind::Object,"ObjectProperty",0x580u,"Extension.GC"}};break;
        case 1486u:path="Extension.Window.ReleaseGC";parameters={{"GC",Kind::Object,"ObjectProperty",0x80u,"Extension.GC"}};break;
        default:throw std::runtime_error("Unregistered original GC declaration");
        }
        const auto authored=FunctionFor(Object(path));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!=LowerAscii(path) || function.nativeIndex!=index || function.flags!=0x401u ||
                function.variables.size()!=parameters.size())throw std::runtime_error("GC method lacks its original native declaration");
            for(std::size_t slot=0u;slot<parameters.size();++slot) {
                const auto& expected=parameters[slot];const auto& value=function.variables[slot];auto* field=Object(value.key);
                if(LowerAscii(value.key)!=LowerAscii(std::string(path)+'.'+expected.name) || value.zero.kind!=expected.kind ||
                    value.arrayDimension!=1u || value.flags!=expected.flags || !field->property || field->property->type!=expected.type ||
                    (*expected.constraint && LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!=
                        LowerAscii(expected.constraint)))throw std::runtime_error("GC native packet differs from its actual property schema");
            }
        };
        validate(*authored);
        if(declaration) {validate(*declaration);if(declaration->source!=authored->source)
            throw std::runtime_error("GC method declaration is not its original owned source");}
        return authored;
    }
    Evaluation NativeGcContextMethod(std::uint16_t index,RuntimeObject* receiver,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        const auto authored=NativeGcDeclaration(index,declaration);
        if(index==1485u || index==1486u)RequireNativeWindow(receiver);
        else ValidateNativeGcContextFields(receiver);
        if(index==1485u || index==1271u) {
            if(!arguments.empty())throw std::runtime_error("GC query takes no input arguments");
            if(index==1485u) {auto* gc=NativeWindowGetGc(receiver,execution);
                return {Value::Text(Kind::Object,gc?gc->reflection.objectPath:std::string{}),{}};}
            ValidateNativeGcLists(receiver);NativeGcOperations operations{*this,execution};
            return {Value::Integer(QuestVr::NativeGcContext::Push(operations,receiver)),{}};
        }
        if(index==1272u) {
            if(arguments.size()>1u)throw std::runtime_error("GC.PopGC accepts one optional Int");
            auto value=arguments.empty()?Value::Integer(-1):arguments[0u].Load();
            if(value.kind==Kind::Nothing)value=Value::Integer(-1);
            if(value.kind!=Kind::Int)throw std::runtime_error("GC.PopGC requires its original Int argument");
            ValidateNativeGcLists(receiver);NativeGcOperations operations{*this,execution};
            QuestVr::NativeGcContext::PopScript(operations,receiver,value.integer);return {};
        }
        if(arguments.size()!=1u)throw std::runtime_error("GC copy/release requires one Object argument");
        auto value=arguments[0u].Load();if(value.kind==Kind::Nothing)value=authored->variables[0u].zero;
        value=TypedLiveValue(authored->variables[0u],value);
        auto* target=value.text.empty()?nullptr:Object(value.text);
        if(index==1486u) {NativeWindowReleaseGc(target,execution);return {};}
        if(!target)throw std::runtime_error("GC.CopyGC cannot dereference a None source");
        NativeGcOperations operations{*this,execution};operations.CopyContext(receiver,target);return {};
    }
