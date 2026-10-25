// Included inside PortableActorVmHost. Native Tile preferred sizing only;
// ConfigurationChanged's later child callbacks remain a separate service.
    void AdmitNativeTileReferences(std::size_t count) {
        // Shared references and captured read/write closures survive lexical
        // callbacks in the VM undo journal. Charge them until root termination,
        // including typed property captures used by child held-OUT aliases.
        constexpr auto perReference=2u*sizeof(Reference)+2u*sizeof(Property)+32u*sizeof(void*)+256u;
        const auto bytes=QuestVr::DecalStateDetail::Multiply(count,perReference);
        ValidateStateBudget(bytes);nativeTileReferenceBytes_+=bytes;
    }
    void RequireTileWindowAlive(RuntimeObject* window) {
        RequireNativeWindow(window);
        if(Object(window->reflection.objectPath)!=window)
            throw std::runtime_error("Native Tile query lost its registered Window identity");
        const auto destroyed=Read(window,DescribeProperty("Extension.Window.bBeingDestroyed"),0u);
        if(destroyed.kind!=Kind::Bool || destroyed.boolean)
            throw std::runtime_error("Native Tile query encountered a destroyed Window");
    }
    std::vector<QuestVr::NativeTileRow>& NativeTileRows(RuntimeObject* window) {
        RequireTileWindowAlive(window);
        if(!IsDerivedFromPath(window->cls,"Extension.TileWindow") || !window->nativeWindowState ||
            !window->nativeWindowState->tileRowsInitialized)
            throw std::runtime_error("Native Tile query lacks constructed row storage");
        auto& rows=window->nativeWindowState->tileRows;
        if(rows.size()>4096u)throw std::runtime_error("Native Tile rows exceed their bound");
        return rows;
    }
    void NativeTileRowSize(RuntimeObject* window,std::size_t count,QuestVr::Vm::Execution& execution) {
        auto& rows=NativeTileRows(window);
        if(count>4096u || count>rows.capacity())throw std::runtime_error("Native Tile row size has no admitted capacity");
        LifecycleStep();Touch(window);
        // Count is a native field, not a script DynamicArray alias. Retaining
        // capacity makes every VM undo resize allocation-free. The complete
        // native before-image subsequently restores contents/capacity by swap.
        AdmitNativeTileReferences(1u);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
        reference->read=[window] {return Value::Integer(static_cast<std::int32_t>(window->nativeWindowState->tileRows.size()));};
        reference->write=[window](const Value& value) {
            auto& target=window->nativeWindowState->tileRows;
            if(value.kind!=Kind::Int || value.integer<0 || static_cast<std::size_t>(value.integer)>target.capacity())
                throw std::runtime_error("Native Tile row-count storage is malformed");
            target.resize(static_cast<std::size_t>(value.integer));
        };
        execution.WriteReference(reference,Value::Integer(static_cast<std::int32_t>(count)));
    }
    void NativeTileRowWord(RuntimeObject* window,std::size_t index,unsigned word,std::int32_t value,
        QuestVr::Vm::Execution& execution) {
        auto& rows=NativeTileRows(window);
        if(index>=rows.size() || word>2u)throw std::runtime_error("Native Tile row-word index is invalid");
        LifecycleStep();Touch(window);
        AdmitNativeTileReferences(1u);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
        reference->read=[window,index,word] {
            const auto& row=window->nativeWindowState->tileRows.at(index);
            return Value::Integer(word==0u?row.crossSize:word==1u?row.mainExtent:row.childCount);
        };
        reference->write=[window,index,word](const Value& incoming) {
            if(incoming.kind!=Kind::Int)throw std::runtime_error("Native Tile row word is not INT");
            auto& row=window->nativeWindowState->tileRows.at(index);
            (word==0u?row.crossSize:word==1u?row.mainExtent:row.childCount)=incoming.integer;
        };
        execution.WriteReference(reference,Value::Integer(value));
    }
    struct NativeTileOperations {
        using Child=RuntimeObject*;
        PortableActorVmHost& host;RuntimeObject* window;QuestVr::Vm::Execution& execution;
        void Step(){host.LifecycleStep();host.RequireTileWindowAlive(window);}
        Value Field(RuntimeObject* object,const std::string& key,Kind kind) {
            host.RequireTileWindowAlive(object);
            const auto property=host.DescribeProperty(key);
            if(property.zero.kind!=kind || property.arrayDimension!=1u || !host.OwnsProperty(object->cls,property))
                throw std::runtime_error("Native Tile field lacks its original scalar declaration");
            auto value=host.Read(object,property,0u);
            if(value.kind!=kind || (kind==Kind::Float && !std::isfinite(value.floating)))
                throw std::runtime_error("Native Tile field has malformed active storage");
            return value;
        }
        std::uint8_t ReadTileByte(const char* name) {
            const auto value=Field(window,"Extension.TileWindow."+std::string(name),Kind::Byte);
            if(value.integer<0 || value.integer>255)throw std::runtime_error("Native Tile Byte is outside its storage domain");
            return static_cast<std::uint8_t>(value.integer);
        }
        bool ReadTileBool(const char* name) {return Field(window,"Extension.TileWindow."+std::string(name),Kind::Bool).boolean;}
        QuestVr::NativeTileFlags ReadTileFlags() {
            // No callbacks/work callbacks between these typed reads: this is
            // the semantic equivalent of one original packed flags load.
            return {ReadTileBool("bWrap"),ReadTileBool("bFillParent"),ReadTileBool("bEqualWidth"),ReadTileBool("bEqualHeight")};
        }
        float ReadTileFloat(const char* name){return Field(window,"Extension.TileWindow."+std::string(name),Kind::Float).floating;}
        float ReadHeldFloat(Child child,const char* name){return Field(child,"Extension.Window."+std::string(name),Kind::Float).floating;}
        void WriteHeldFloat(Child child,const char* name,float value) {
            host.RequireTileWindowAlive(child);
            if(!std::isfinite(value))throw std::runtime_error("Native Tile held write is nonfinite");
            host.AdmitNativeTileReferences(1u);
            host.NativeWindowWrite(child,("Extension.Window."+std::string(name)).c_str(),Value::Float(value),execution);
        }
        bool ReadChildVisible(Child child){return Field(child,"Extension.Window.bIsVisible",Kind::Bool).boolean;}
        Child Link(Child child,const char* name) {
            host.LifecycleStep();host.RequireTileWindowAlive(child);
            auto* next=host.WindowLink(child,name);if(next)host.RequireTileWindowAlive(next);return next;
        }
        Child LastChild(){return Link(window,"lastChild");}
        Child PreviousSibling(Child child){return Link(child,"prevSibling");}
        Child Visible(Child child) {
            while(child) {
                Step();if(ReadChildVisible(child))return child;child=Link(child,"nextSibling");
            }
            return nullptr;
        }
        Child FirstVisibleChild(){return Visible(Link(window,"firstChild"));}
        Child NextVisibleSibling(Child child){return Visible(Link(child,"nextSibling"));}
        float QueryAxis(Child child,std::uint16_t index,float constraint) {
            host.RequireTileWindowAlive(child);
            const auto value=execution.CallFunction(*host.WindowNativeDeclaration(index,nullptr),child->reflection.objectPath,
                {{Value::Float(constraint),{}}});
            if(value.kind!=Kind::Float || !std::isfinite(value.floating))
                throw std::runtime_error("Native Tile child axis query returned no finite Float");
            return value.floating;
        }
        float QueryPreferredWidth(Child child,float constraint){return QueryAxis(child,1463u,constraint);}
        float QueryPreferredHeight(Child child,float constraint){return QueryAxis(child,1464u,constraint);}
        void QueryPreferredSize(Child child) {
            host.RequireTileWindowAlive(child);
            // Actual held-property OUT aliases, written width then height by
            // the native1465 wrapper in THIS Execution. No local copy-out.
            host.AdmitNativeTileReferences(2u);
            auto width=host.ReferenceFor(child,host.DescribeProperty("Extension.Window.holdWidth"),0u,false);
            auto height=host.ReferenceFor(child,host.DescribeProperty("Extension.Window.holdHeight"),0u,false);
            static_cast<void>(execution.CallFunction(*host.WindowNativeDeclaration(1465u,nullptr),child->reflection.objectPath,
                {{Value::Float(0.0f),width},{Value::Float(0.0f),height}}));
        }
        void ClearRows(){host.NativeTileRowSize(window,0u,execution);}
        void AppendRow(QuestVr::NativeTileRow row) {
            auto& rows=host.NativeTileRows(window);const auto count=rows.size();
            if(count>=4096u)throw std::runtime_error("Native Tile row append exceeds its bound");
            host.LifecycleStep();host.Touch(window);
            if(count==rows.capacity()) {
                const auto size=count+1u;
                const auto capacity=std::min<std::size_t>(4096u,size+32u+(3u*size)/8u);
                // Reserve can temporarily hold BOTH allocations, not merely
                // their size difference. Charge that peak before allocating.
                host.ValidateStateBudget(capacity*sizeof(QuestVr::NativeTileRow));rows.reserve(capacity);
                host.ValidateStateBudget();
            }
            host.NativeTileRowSize(window,count+1u,execution);
            host.NativeTileRowWord(window,count,0u,row.crossSize,execution);
            host.NativeTileRowWord(window,count,1u,row.mainExtent,execution);
            host.NativeTileRowWord(window,count,2u,row.childCount,execution);
        }
        QuestVr::NativeTileRow ReadRow(std::size_t index) {
            auto& rows=host.NativeTileRows(window);
            if(index>=rows.size())throw std::runtime_error("Native Tile row read is out of bounds");
            return rows[index];
        }
        void WriteRowChildCount(std::size_t index,std::int32_t count){host.NativeTileRowWord(window,index,2u,count,execution);}
        std::size_t RowCount(){return host.NativeTileRows(window).size();}
    };
    void NativeWindowTileSize(RuntimeObject* window,bool widthSpecified,float& width,
        bool heightSpecified,float& height,QuestVr::Vm::Execution& execution) {
        static_cast<void>(NativeTileRows(window));ValidateNativeTileFields(window);
        for(const auto* field:{"holdX","holdY","holdWidth","holdHeight"})
            ValidateNativeWindowScalar(window,("Extension.Window."+std::string(field)).c_str(),Kind::Float,"FloatProperty");
        if(nativeTileSizeDepth_>=128u)throw std::runtime_error("Native Tile recursive query depth exceeded");
        struct Depth {std::size_t& value;~Depth(){--value;}} depth{nativeTileSizeDepth_};
        ++nativeTileSizeDepth_;
        NativeTileOperations operations{*this,window,execution};
        QuestVr::ComputeNativeTileChildSizes(operations,widthSpecified,width,heightSpecified,height);
    }
