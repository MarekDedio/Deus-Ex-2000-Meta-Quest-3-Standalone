// Included inside PortableActorVmHost. Genuine GC/font/UTF16 providers for the
// inherited TextWindow preferred-size virtual, not presentation font estimates.
    using NativeFontCacheEntry=std::pair<RuntimeObject*,std::unique_ptr<QuestVr::NativeFontMetrics>>;
    std::vector<NativeFontCacheEntry> nativeFontMetrics_;
    std::size_t nativeFontMetricsBytes_{};
    void RequireNativeFontAsset(RuntimeObject* font) {
        if(!font || font->defaultOwner || font->runtimeBirth || font->nativeWindowObject ||
            Object(font->reflection.objectPath)!=font)
            throw std::runtime_error("Native text metrics require the actual immutable Font object");
        // Engine.Font is an original registered asset class absent from the
        // serialized UClass exports. Its resource may correctly have cls=None.
        // Authenticate its real export/import identity through the existing
        // native asset cast resolver, not a fabricated UClass or meta suffix.
        const auto identity=CastObjectIdentity(font->reflection.objectPath);
        if(identity.classObject || LowerAscii(identity.classPath)!="engine.font")
            throw std::runtime_error("Native text metrics resource is not actual Engine.Font");
    }
    const QuestVr::NativeFontMetrics& NativeFontMetricsFor(RuntimeObject* font) {
        RequireNativeFontAsset(font);
        for(const auto& cached:nativeFontMetrics_) {
            LifecycleStep();if(cached.first==font)return *cached.second;
        }
        constexpr std::size_t maximumFonts=128u;
        if(nativeFontMetrics_.size()==maximumFonts)throw std::runtime_error("Native font metrics cache exceeds its bounded font count");
        if(nativeFontMetrics_.capacity()==0u) {
            const auto bytes=maximumFonts*sizeof(NativeFontCacheEntry);ValidateStateBudget(bytes);
            nativeFontMetrics_.reserve(maximumFonts);
            if(nativeFontMetrics_.capacity()!=maximumFonts)
                throw std::runtime_error("Native font cache library cannot retain its admitted capacity");
            nativeFontMetricsBytes_=bytes;
        }
        const auto& table=Table(font->sourcePath);
        if(font->exportIndex>=table.exports.size() ||
            LowerAscii(Qualified(table,static_cast<std::int32_t>(font->exportIndex+1u)))!=LowerAscii(font->reflection.objectPath))
            throw std::runtime_error("Native font object lost its actual source/export identity");
        QuestVr::NativeFontMetricsLimits limits;
        limits.chargeWork=[this](std::size_t units) {for(std::size_t i=0u;i<units;++i)LifecycleStep();};
        limits.admitPeak=[this](std::size_t bytes) {ValidateStateBudget(bytes);};
        auto metrics=QuestVr::LoadNativeFontMetrics(table,font->exportIndex,limits);
        const auto bytes=QuestVr::NativeFontMetricsRetainedBytes(metrics);
        ValidateStateBudget(QuestVr::DecalStateDetail::Add(bytes,sizeof(metrics)));
        auto owned=std::make_unique<QuestVr::NativeFontMetrics>(std::move(metrics));
        nativeFontMetrics_.emplace_back(font,std::move(owned));
        nativeFontMetricsBytes_=QuestVr::DecalStateDetail::Add(nativeFontMetricsBytes_,bytes);
        ValidateStateBudget();return *nativeFontMetrics_.back().second;
    }
    struct NativeTextGcOperations {
        using Font=RuntimeObject*;
        PortableActorVmHost& host;RuntimeObject* gc;
        void Step() {host.LifecycleStep();}
        Font FontField(const char* name) {
            const auto value=host.NativeGcRead(gc,name);
            if(value.kind!=Kind::Object)throw std::runtime_error("Native GC Font field is not Object");
            if(value.text.empty())return nullptr;
            auto* font=host.Object(value.text);
            host.RequireNativeFontAsset(font);
            return font;
        }
        Font NormalFont() {return FontField("normalFont");}
        Font BoldFont() {return FontField("boldFont");}
        bool ParseMetachars() {
            const auto value=host.NativeGcRead(gc,"bParseMetachars");
            if(value.kind!=Kind::Bool)throw std::runtime_error("Native GC metachar flag is not Bool");return value.boolean;
        }
        std::array<std::uint32_t,4u> TextPlaneWords() {
            const auto value=host.NativeGcRead(gc,"textPlane");
            if(value.kind!=Kind::Struct || value.fields.size()!=4u)throw std::runtime_error("Native GC text Plane is not canonical");
            std::array<std::uint32_t,4u> words{};std::size_t slot{};
            for(const auto* name:{"x","y","z","w"}) {
                const auto found=value.fields.find(name);
                if(found==value.fields.end() || found->second.kind!=Kind::Float)
                    throw std::runtime_error("Native GC text Plane lost an actual Float word");
                words[slot++]=std::bit_cast<std::uint32_t>(found->second.floating);
            }
            return words;
        }
        float TextVSpacing() {
            const auto value=host.NativeGcRead(gc,"textVSpacing");
            if(value.kind!=Kind::Float)throw std::runtime_error("Native GC text spacing is not Float");return value.floating;
        }
        QuestVr::NativeTextMeasurement::Glyph Glyph(Font font,char16_t character) {
            if(!font)return {};
            const auto& metrics=host.NativeFontMetricsFor(font);
            const auto glyph=QuestVr::LookupNativeFontGlyph(metrics,static_cast<std::uint32_t>(character));
            return {glyph.width,glyph.height};
        }
    };
    struct NativeTextPreferredOperations {
        PortableActorVmHost& host;RuntimeObject* window;QuestVr::Vm::Execution& execution;
        std::u16string text;
        std::optional<QuestVr::NativeScratchScope> textReservation;
        void Step() {host.LifecycleStep();}
        RuntimeObject* GetGC() {return host.NativeWindowGetGc(window,execution);}
        void ReleaseGC(RuntimeObject* gc) {host.NativeWindowReleaseGc(gc,execution);}
        Value Read(const char* name,Kind kind) {
            const auto value=host.Read(window,host.DescribeProperty("Extension.TextWindow."+std::string(name)),0u);
            if(value.kind!=kind)throw std::runtime_error("Native TextWindow preferred-size field changed type");return value;
        }
        bool HasText() {
            const auto property=host.DescribeProperty("Extension.TextWindow.Text");
            if(host.ClassProperty(window->cls,"Text").key!=property.key)
                throw std::runtime_error("Native text sizing cannot alias a shadowed private Text field");
            const auto* value=FindScriptOverlay(window,"Text",0u);
            if(!value)value=&property.zero; // Actual copied implicit-empty CDO.
            if(value->kind!=Kind::String)throw std::runtime_error("Native TextWindow Text is not FString");
            // Borrow the actual field until decoding completes; no UTF8 copy
            // precedes admission. Charge overlapping old/new UTF16 growth.
            const auto bytes=QuestVr::DecalStateDetail::Add(sizeof(*this),
                QuestVr::DecalStateDetail::Multiply(value->text.size()+1u,8u));
            textReservation.emplace(host.nativeQueryScratchBytes_,bytes,
                [this](std::size_t extra){host.ValidateStateBudget(extra);});
            text=QuestVr::UnrealString::Decode(value->text);
            return !QuestVr::UnrealString::CString(text).empty();
        }
        std::int32_t MinimumLines() {return Read("minLines",Kind::Int).integer;}
        std::int32_t MaximumLines() {return Read("MaxLines",Kind::Int).integer;}
        bool WordWrap() {return Read("bWordWrap",Kind::Bool).boolean;}
        float HorizontalMargin() {return Read("hMargin",Kind::Float).floating;}
        float VerticalMargin() {return Read("vMargin",Kind::Float).floating;}
        float MinimumWidth() {return Read("MinWidth",Kind::Float).floating;}
        void Measure(RuntimeObject* gc,float destinationWidth,float& width,float& height) {
            NativeTextGcOperations operations{host,gc};
            QuestVr::NativeTextMeasurement::GetTextExtent(operations,text,destinationWidth,width,height);
        }
        float FontHeight(RuntimeObject* gc,bool includeSpacing) {
            NativeTextGcOperations operations{host,gc};return QuestVr::NativeTextMeasurement::GetFontHeight(operations,includeSpacing);
        }
        RuntimeObject* Background() {
            const auto value=host.Read(window,host.DescribeProperty("Extension.Window.Background"),0u);
            if(value.kind!=Kind::Object)throw std::runtime_error("Native text Background is not Object");
            if(value.text.empty())return nullptr;
            auto* texture=host.Object(value.text);
            if(!IsDerivedFromPath(texture->cls,"Engine.Texture"))throw std::runtime_error("Native text Background is not actual Texture");
            return texture;
        }
        std::int32_t Dimension(RuntimeObject* texture,bool width) {
            if(!texture)throw std::runtime_error("Native text Background disappeared during preferred sizing");
            if(const auto* changed=FindScriptOverlay(texture,width?"USize":"VSize",0u)) {
                if(changed->kind!=Kind::Int)throw std::runtime_error("Native text Background size override is not signed Int");
                return changed->integer;
            }
            if(!texture->bitmapDimensions)throw std::runtime_error("Native text Background dimensions are unavailable");
            return (*texture->bitmapDimensions)[width?0u:1u];
        }
        std::int32_t BackgroundWidth(RuntimeObject* texture) {return Dimension(texture,true);}
        std::int32_t BackgroundHeight(RuntimeObject* texture) {return Dimension(texture,false);}
        void BasePreferredSize(bool widthSpecified,float& width,bool heightSpecified,float& height) {
            host.NativeBaseWindowPreferredSize(window,widthSpecified,width,heightSpecified,height,execution);
        }
    };
    void NativeTextWindowPreferredSize(RuntimeObject* window,bool widthSpecified,float& width,
        bool heightSpecified,float& height,QuestVr::Vm::Execution& execution) {
        RequireNativeTextWindowSetters(window);
        for(const auto* name:{"hMargin","vMargin","MinWidth"})
            ValidateNativeWindowScalar(window,("Extension.TextWindow."+std::string(name)).c_str(),Kind::Float,"FloatProperty","",2u);
        for(const auto* name:{"minLines","MaxLines"})
            ValidateNativeWindowScalar(window,("Extension.TextWindow."+std::string(name)).c_str(),Kind::Int,"IntProperty","",2u);
        ValidateNativeWindowScalar(window,"Extension.TextWindow.bWordWrap",Kind::Bool,"BoolProperty","",2u);
        ValidateNativeWindowScalar(window,"Extension.TextWindow.Text",Kind::String,"StrProperty","",0x1000u);
        // Text and its scratch reservation are lazy AFTER the genuine GetGC-
        // null early return, not a prerequisite copy/allocation before it.
        NativeTextPreferredOperations operations{*this,window,execution,{},std::nullopt};
        QuestVr::NativeTextPreferredSize::Query(operations,widthSpecified,width,heightSpecified,height);
    }
