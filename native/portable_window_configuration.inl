// Included inside PortableActorVmHost before its ConfigurationChanged dispatch.
// Actual native +90 overrides from owned Extension.dll, not generic Window
// script callbacks. All geometry, GC and row writes share the caller's root.
    void ConfigureNativeWindowChildrenToParent(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        // Radio10038840 and TabGroup10045640(bSizeChildrenToParent): start at
        // lastChild and walk previous siblings, skipping locally hidden nodes.
        // Parent Width/Height are reread immediately before EACH child call.
        constexpr std::size_t maximumChildren=4096u;
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,sizeof(std::vector<RuntimeObject*>)+maximumChildren*sizeof(RuntimeObject*),
            [this](std::size_t extra){ValidateStateBudget(extra);});
        std::vector<RuntimeObject*> visited;visited.reserve(maximumChildren);
        if(visited.capacity()!=maximumChildren)throw std::runtime_error("Native parent configuration child storage exceeded admitted capacity");
        auto previousVisible=[&](RuntimeObject* child) {
            while(child) {
                LifecycleStep();RequireNativeWindow(child);
                if(WindowLink(child,"winParent")!=window || visited.size()==maximumChildren ||
                    std::find(visited.begin(),visited.end(),child)!=visited.end())
                    throw std::runtime_error("Native parent-sized configuration has cyclic or unowned children");
                visited.push_back(child);
                if(NativeWindowBool(child,"bIsVisible"))return child;
                child=WindowLink(child,"prevSibling");
            }
            return static_cast<RuntimeObject*>(nullptr);
        };
        for(auto* child=previousVisible(WindowLink(window,"lastChild"));child;
            child=previousVisible(WindowLink(child,"prevSibling"))) {
            //10038877/10045680 push Height first, then Width, then Y and X.
            const auto height=NativeWindowFloat(window,"Height"),width=NativeWindowFloat(window,"Width");
            ConfigureNativeWindowChild(child,0.0f,0.0f,width,height,execution);
        }
    }
    void ConfigureNativeTileWindow(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        static_cast<void>(NativeTileRows(window));ValidateNativeTileFields(window);
        for(const auto* name:{"holdX","holdY","holdWidth","holdHeight"})
            ValidateNativeWindowScalar(window,("Extension.Window."+std::string(name)).c_str(),Kind::Float,"FloatProperty");
        if(nativeTileSizeDepth_>=128u)throw std::runtime_error("Native Tile configuration query depth exceeded");
        //100489FE passes pointers to the actual Width/Height with BOTH flags
        // true. ComputeChildSizes therefore never writes those two outputs.
        // They remain live inputs across child query callbacks, however; keep
        // the helper's constraint references synchronized after each callback.
        float width=NativeWindowFloat(window,"Width"),height=NativeWindowFloat(window,"Height");
        struct LiveTileOperations : NativeTileOperations {
            float& width;float& height;
            void RefreshConstraints() {
                width=host.NativeWindowFloat(window,"Width");height=host.NativeWindowFloat(window,"Height");
            }
            float QueryPreferredWidth(Child child,float constraint) {
                const auto value=NativeTileOperations::QueryPreferredWidth(child,constraint);RefreshConstraints();return value;
            }
            float QueryPreferredHeight(Child child,float constraint) {
                const auto value=NativeTileOperations::QueryPreferredHeight(child,constraint);RefreshConstraints();return value;
            }
            void QueryPreferredSize(Child child) {NativeTileOperations::QueryPreferredSize(child);RefreshConstraints();}
        };
        {
            struct Depth {std::size_t& value;~Depth(){--value;}} depth{nativeTileSizeDepth_};++nativeTileSizeDepth_;
            LiveTileOperations operations{{*this,window,execution},width,height};
            QuestVr::ComputeNativeTileChildSizes(operations,true,width,true,height);
        }
        //10048A07/10048A39 walk firstChild and nextSibling, locally visible
        // only. Held values are read at each child's call, after all queries.
        constexpr std::size_t maximumChildren=4096u;
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,sizeof(std::vector<RuntimeObject*>)+maximumChildren*sizeof(RuntimeObject*),
            [this](std::size_t extra){ValidateStateBudget(extra);});
        std::vector<RuntimeObject*> visited;visited.reserve(maximumChildren);
        if(visited.capacity()!=maximumChildren)throw std::runtime_error("Native Tile configuration child storage exceeded admitted capacity");
        for(auto* child=WindowLink(window,"firstChild");child;child=WindowLink(child,"nextSibling")) {
            LifecycleStep();RequireNativeWindow(child);
            if(WindowLink(child,"winParent")!=window || visited.size()==maximumChildren ||
                std::find(visited.begin(),visited.end(),child)!=visited.end())
                throw std::runtime_error("Native Tile configuration has cyclic or unowned children");
            visited.push_back(child);if(!NativeWindowBool(child,"bIsVisible"))continue;
            const auto childHeight=NativeWindowFloat(child,"holdHeight"),childWidth=NativeWindowFloat(child,"holdWidth"),
                childY=NativeWindowFloat(child,"holdY"),childX=NativeWindowFloat(child,"holdX");
            ConfigureNativeWindowChild(child,childX,childY,childWidth,childHeight,execution);
        }
    }
    void ConfigureNativeTextLogWindow(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        ValidateNativeTextLogFields(window);
        //1004793A acquires/copies the genuine context before inspecting Text.
        // A None gGC skips measurement and still dispatches the script below.
        auto* gc=NativeWindowGetGc(window,execution);
        if(gc) {
            const auto property=DescribeProperty("Extension.TextWindow.Text");
            if(ClassProperty(window->cls,"Text").key!=property.key)
                throw std::runtime_error("Native TextLog configuration cannot alias a shadowed Text field");
            const auto* input=FindScriptOverlay(window,"Text",0u);if(!input)input=&property.zero;
            if(input->kind!=Kind::String)throw std::runtime_error("Native TextLog configuration Text is not FString");
            const auto bytes=QuestVr::DecalStateDetail::Add(sizeof(NativeTextGcOperations)+sizeof(std::u16string),
                QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(input->text.size(),1u),16u));
            QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,bytes,[this](std::size_t extra){ValidateStateBudget(extra);});
            for(std::size_t index=0u;index<input->text.size();++index)LifecycleStep();
            const auto text=QuestVr::UnrealString::Decode(input->text);
            // Native passes a Float width but compares the available height
            // while still in PC64 registers; preserve that actual spill split.
            const auto hMargin=QuestVr::NativeScalar::Load(NativeWindowTextFloat(window,"hMargin"));
            const auto width=QuestVr::NativeScalar::StoreFloat(QuestVr::NativeScalar::Subtract(
                QuestVr::NativeScalar::Load(NativeWindowFloat(window,"Width")),QuestVr::NativeScalar::Add(hMargin,hMargin)));
            NativeTextGcOperations operations{*this,gc};float x{},y{};
            QuestVr::NativeTextMeasurement::GetTextExtent(operations,text,width,x,y);
            const auto vMargin=QuestVr::NativeScalar::Load(NativeWindowTextFloat(window,"vMargin"));
            const auto available=QuestVr::NativeScalar::Subtract(QuestVr::NativeScalar::Load(NativeWindowFloat(window,"Height")),
                QuestVr::NativeScalar::Add(vMargin,vMargin));
            // FCOMP/TEST C0 follows less OR unordered, including raw NaN y.
            const auto tooTall=QuestVr::NativeScalar::Compare(176u,available,QuestVr::NativeScalar::Load(y));
            NativeWindowWrite(window,"Extension.TextLogWindow.bTooTall",Value::Bool(tooTall),execution);
            NativeWindowReleaseGc(gc,execution);
        }
        InvokeNativeWindowConfigurationScript(window,execution);
    }
    float NativeWindowTextFloat(RuntimeObject* window,const char* name) {
        const auto key="Extension.TextWindow."+std::string(name);
        ValidateNativeWindowScalar(window,key.c_str(),Kind::Float,"FloatProperty");
        const auto value=Read(window,DescribeProperty(key),0u);
        if(value.kind!=Kind::Float)throw std::runtime_error("Native TextWindow margin is not Float storage");
        return value.floating;
    }
    bool ConfigureNativeWindowOverride(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(window);const auto native=NativeWindowAncestor(window->cls);
        if(native=="extension.tabgroupwindow"||native=="extension.modalwindow"||native=="extension.rootwindow") {
            // Root1003A660 delegates to TabGroup10045640 without extra work.
            const auto key="Extension.TabGroupWindow.bSizeChildrenToParent";
            ValidateNativeWindowScalar(window,key,Kind::Bool,"BoolProperty");
            const auto sizeChildren=Read(window,DescribeProperty(key),0u);
            if(sizeChildren.kind!=Kind::Bool)throw std::runtime_error("TabGroup child-sizing flag is not Bool");
            if(sizeChildren.boolean)ConfigureNativeWindowChildrenToParent(window,execution);
            else InvokeNativeWindowConfigurationScript(window,execution);
            return true;
        }
        if(native=="extension.radioboxwindow") {ConfigureNativeWindowChildrenToParent(window,execution);return true;}
        if(native=="extension.tilewindow") {ConfigureNativeTileWindow(window,execution);return true;}
        if(native=="extension.textlogwindow") {ConfigureNativeTextLogWindow(window,execution);return true;}
        return false; // Ordinary Window/Text/Button/Toggle use their script.
    }
