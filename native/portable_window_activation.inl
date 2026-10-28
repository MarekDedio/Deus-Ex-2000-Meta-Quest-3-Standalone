// PortableActorVmHost members: actual native window activation, not rendering.
    bool NativeChildRequestedReconfiguration(RuntimeObject* parent,RuntimeObject* child,
        QuestVr::Vm::Execution& execution) {
        RequireNativeWindow(parent);RequireNativeWindow(child);
        const auto native=NativeWindowAncestor(parent->cls);
        //100421d0 is the shared native TabGroup/Modal/Radio/Tile override.
        if(native=="extension.tabgroupwindow" || native=="extension.modalwindow" ||
            native=="extension.radioboxwindow" || native=="extension.tilewindow") return false;
        if(const auto callback=OptionalNativeWindowEvent(parent,"ChildRequestedReconfiguration")) {
            if(callback->flags&0x400u)
                throw std::runtime_error("Native ChildRequestedReconfiguration script override is native");
            const auto value=execution.CallFunction(*callback,parent->reflection.objectPath,
                {{Value::Text(Kind::Object,child->reflection.objectPath),{}}});
            if(value.kind!=Kind::Bool)
                throw std::runtime_error("Native ChildRequestedReconfiguration result is not Bool");
            return value.boolean;
        }
        return false; //10003580/1003a6d0 initialize the optional result to zero.
    }
    template<class Visitor>
    void VisitNativeWindowTree(RuntimeObject* window,bool locallyVisibleChildren,Visitor&& visitor) {
        constexpr std::size_t maximumWindows=4096u;
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,sizeof(std::vector<RuntimeObject*>)+maximumWindows*sizeof(RuntimeObject*),
            [this](std::size_t extra){ValidateStateBudget(extra);});
        std::vector<RuntimeObject*> visited;visited.reserve(maximumWindows);
        if(visited.capacity()!=maximumWindows)
            throw std::runtime_error("Native activation tree scratch exceeds its admitted capacity");
        const auto visit=[&](const auto& self,RuntimeObject* current,std::size_t depth)->void {
            LifecycleStep();RequireNativeWindow(current);
            if(depth>=128u || visited.size()>=maximumWindows || std::find(visited.begin(),visited.end(),current)!=visited.end())
                throw std::runtime_error("Native activation tree cycles or exceeds its bound");
            visited.push_back(current);
            if(!visitor(current)) return;
            // Callbacks precede firstChild; nextSibling is read AFTER recursion.
            for(auto* child=WindowLink(current,"firstChild");child;child=WindowLink(child,"nextSibling")) {
                LifecycleStep();RequireNativeWindow(child);
                if(WindowLink(child,"winParent")!=current)
                    throw std::runtime_error("Native activation child has invalid parent ownership");
                if(!locallyVisibleChildren || NativeWindowBool(child,"bIsVisible")) self(self,child,depth+1u);
                else {
                    if(visited.size()>=maximumWindows || std::find(visited.begin(),visited.end(),child)!=visited.end())
                        throw std::runtime_error("Native activation hidden sibling chain cycles or exceeds its bound");
                    visited.push_back(child);
                }
            }
        };
        visit(visit,window,0u);
    }
    void CheckNativeWindowVisibilityPointer(RuntimeObject* window,const char* name) {
        auto* root=NativeWindowRoot(window);
        if(!root) return;
        const auto pointer=Read(root,DescribeProperty("Extension.RootWindow."+std::string(name)),0u);
        if(pointer.kind!=Kind::Object)
            throw std::runtime_error("Native visibility focus/grab pointer is not Object storage");
        if(!pointer.text.empty())
            throw std::runtime_error("Native visibility nonnull focus/grab negotiation is not implemented: "+std::string(name));
    }
    void RecomputeNativeWindowInitTree(RuntimeObject* window,bool visible,QuestVr::Vm::Execution& execution) {
        VisitNativeWindowTree(window,true,[&](RuntimeObject* current) {
            if(!NativeWindowBool(current,"bIsInitialized")) if(auto* root=NativeWindowRoot(current)) {
                const auto field=DescribeProperty("Extension.RootWindow.initCount");
                const auto count=Read(root,field,0u);
                if(count.kind!=Kind::Int) throw std::runtime_error("Native root initCount is not Int storage");
                // Original increment/decrement is DWORD arithmetic, not a clamp
                // and not an initialization event or bIsInitialized store.
                const auto bits=static_cast<std::uint32_t>(count.integer)+(visible?1u:0xffffffffu);
                NativeWindowWrite(root,field.key.c_str(),Value::Integer(std::bit_cast<std::int32_t>(bits)),execution);
            }
            return true;
        });
    }
    void NativeWindowVisibilitySound(RuntimeObject* window,bool visible) {
        //10050050 obtains actual Root and Player even for SoundNone. Audio is
        // not required when either owner or the selected optional sound is None.
        auto* root=NativeWindowRoot(window);
        RuntimeObject* pawn{};
        if(root) {
            const auto owner=Read(root,DescribeProperty("Extension.RootWindow.parentPawn"),0u);
            if(owner.kind!=Kind::Object) throw std::runtime_error("Native root parentPawn is not Object storage");
            if(!owner.text.empty()) {pawn=Object(owner.text);RequireMapActor(pawn);}
        }
        if(!root || !pawn) return;
        const auto sound=Read(window,DescribeProperty(visible?"Extension.Window.visibleSound":"Extension.Window.invisibleSound"),0u);
        if(sound.kind!=Kind::Object) throw std::runtime_error("Native visibility sound is not Object storage");
        if(sound.text.empty()) return;
        throw std::runtime_error("Native Window visibility sound backend is not implemented");
    }
    void InvokeNativeWindowVisibilityTree(RuntimeObject* window,bool visible,QuestVr::Vm::Execution& execution) {
        VisitNativeWindowTree(window,true,[&](RuntimeObject* current) {
            const auto native=NativeWindowAncestor(current->cls);
            if(const auto callback=OptionalNativeWindowEvent(current,"VisibilityChanged")) {
                if(callback->flags&0x400u)
                    throw std::runtime_error("Native VisibilityChanged script override is native");
                static_cast<void>(execution.CallFunction(*callback,current->reflection.objectPath,{{Value::Bool(visible),{}}}));
            }
            // Modal10037330 dispatches script FIRST. Only parented modals then
            // adjust root keyboard/mouse handling references; a root skips it.
            if((native=="extension.modalwindow" || native=="extension.rootwindow") &&
                WindowLink(current,"winParent") && NativeWindowRoot(current))
                throw std::runtime_error("Native parented Modal visibility keyboard/mouse handling is not implemented");
            if(NativeWindowBool(current,"bBeingDestroyed"))
                throw std::runtime_error("Native visibility callback deferred destruction is not implemented");
            NativeWindowVisibilitySound(current,visible);
            return true;
        });
    }
    void ReconfigureNativeWindowTree(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        VisitNativeWindowTree(window,false,[&](RuntimeObject* current) {
            if(!WindowEffectiveVisibility(current)) return false;
            if(NativeWindowBool(current,"bNeedsReconfigure")) AskHiddenWindowForReconfigure(current,execution);
            return true;
        });
    }
    struct NativeWindowVisibilityOperations {
        PortableActorVmHost& host;RuntimeObject* window;QuestVr::Vm::Execution& execution;
        bool ReadLocalVisibility() {return host.NativeWindowBool(window,"bIsVisible");}
        bool ReadEffectiveVisibility() {return host.WindowEffectiveVisibility(window);}
        void WriteLocalVisibility(bool value) {host.NativeWindowWrite(window,"Extension.Window.bIsVisible",Value::Bool(value),execution);}
        void CheckFocus() {host.CheckNativeWindowVisibilityPointer(window,"FocusWindow");}
        void CheckGrab() {host.CheckNativeWindowVisibilityPointer(window,"grabbedWindow");}
        void MarkNeedsReconfigure() {host.NativeWindowWrite(window,"Extension.Window.bNeedsReconfigure",Value::Bool(true),execution);}
        void RecomputeInitTree(bool visible) {host.RecomputeNativeWindowInitTree(window,visible,execution);}
        void InvokeVisibilityTree(bool visible) {host.InvokeNativeWindowVisibilityTree(window,visible,execution);}
        void ReconfigureTree() {host.ReconfigureNativeWindowTree(window,execution);}
        void DirtyModalAccelerators() {
            if(auto* modal=host.NativeWindowModal(window))
                host.NativeWindowWrite(modal,"Extension.ModalWindow.bDirtyAccelerators",Value::Bool(true),execution);
        }
    };
