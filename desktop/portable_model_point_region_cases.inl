// Independent source-data/hardware oracle for the real first Training map.
// No production PointRegion/plane-distance helper is called by the oracle.
void ModelPointRegionCoverage(const PortablePackageTables& map,const std::vector<Actor>& actors,
    const std::function<void()>& rollback) {
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    struct Precision {
        unsigned short previous{};
        Precision(){static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
            const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
        ~Precision(){asm volatile("fldcw %0"::"m"(previous));}
    } precision;
    const auto native=ReadPortableRuntimeNativeModel();
    Require(native.available&&native.mapGeneration&&!native.geometryCached&&!actors.empty()&&actors.front().path=="00_Training.LevelInfo0"&&
        native.modelExportIndex==FindPortableRootModel68Export(map),
        "First Training PointRegion lacks original native slot0/loader Model authority");
    const auto model=LoadPortableModel68(map,native.modelExportIndex);
    Require(!model.nodes.empty()&&!model.zones.empty(),"Actual first Training PointRegion oracle lacks BSP/zones");
    const auto distance=[](const Array& point,const PortableModelNode& n) {
        long double result;
        asm volatile("flds %[pz]; fmuls %[nz]; flds %[py]; fmuls %[ny]; faddp; "
            "flds %[px]; fmuls %[nx]; faddp; fsubs %[nw]; fstpt %[out]"
            :[out]"=m"(result):[pz]"m"(point[2u]),[nz]"m"(n.planeZ),[py]"m"(point[1u]),[ny]"m"(n.planeY),
            [px]"m"(point[0u]),[nx]"m"(n.planeX),[nw]"m"(n.planeW):"st");return result;
    };
    std::size_t visited{},fallbacks{},assigned{},passes{};std::set<std::uint8_t> selectedZones;
    const auto compare=[&] {
        for(const auto& actor:actors) {
            std::string zoneActor=actors.front().path;std::int32_t leaf=-1;std::uint8_t zone{};
            if(actor.path!=actors.front().path) {
                std::size_t node{},steps{};
                while(true) {
                    Require(node<model.nodes.size()&&++steps<=model.nodes.size(),"Source PointRegion oracle has an invalid/cyclic active path");
                    const auto& n=model.nodes[node];const bool front=distance(actor.location,n)>=0.0L;
                    const auto child=front?n.front:n.back;
                    if(child!=-1){Require(child>=0,"Source PointRegion oracle saw a negative nonterminal child");node=static_cast<std::size_t>(child);continue;}
                    leaf=front?n.leaf1:n.leaf0;const auto selected=model.zones.empty()?0:(front?n.zone1:n.zone0);
                    Require(selected>=0&&selected<64,"Source PointRegion terminal zone is invalid");zone=static_cast<std::uint8_t>(selected);
                    const auto reference=zone<model.zones.size()?model.zones[zone].actorReference:0;
                    if(reference)zoneActor=(reference>0?"00_Training.":"")+GetPortableObjectPath(map,reference);
                    break;
                }
                visited+=steps;
            }
            const auto actual=Declared(actor.path,"Engine.Actor.Region",Kind::Struct);
            Require(actual.fields.size()==3u&&actual.fields.at("zone").kind==Kind::Object&&
                actual.fields.at("ileaf").kind==Kind::Int&&actual.fields.at("zonenumber").kind==Kind::Byte&&
                actual.fields.at("zone").text==zoneActor&&actual.fields.at("ileaf").integer==leaf&&
                actual.fields.at("zonenumber").integer==zone,
                "Actual first Training qualified Region disagrees with independent source/hardware tuple: "+actor.path);
            selectedZones.insert(zone);if(zoneActor==actors.front().path)++fallbacks;else ++assigned;
        }
        ++passes;rollback();
    };
    compare();
    const auto warmed=ReadPortableRuntimeNativeModel();
    Require(warmed.geometryCached&&warmed.cachedNodes==model.nodes.size()&&warmed.modelPath==native.modelPath&&
        warmed.mapGeneration==native.mapGeneration,"First Training Region did not retain only the loader-owned immutable geometry cache");
    GC::Collect();compare();
    Require(passes==2u&&visited>actors.size()&&assigned&&selectedZones.size()>1u,
        "First Training PointRegion coverage missed warm/GC, multilevel paths or actual zone actors");
    std::cout<<"FIRST TRAINING PointRegion: "<<actors.size()<<" actual actors x "<<passes<<" cold/warm-GC passes, "<<visited
        <<" independent hardware-x87 node visits, "<<fallbacks<<" fallback / "<<assigned<<" assigned zone tuples, "
        <<selectedZones.size()<<" selected zone numbers; no Region overlays/world/save changes\n";
#else
    (void)map;(void)actors;(void)rollback;
    std::cout<<"SKIP independent owned PointRegion hardware oracle on non-x86 compiler\n";
#endif
}
