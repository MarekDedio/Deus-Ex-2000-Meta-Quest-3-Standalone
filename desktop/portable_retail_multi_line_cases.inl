// Included after the source census/hash oracle helpers. These are first-Training
// adapter checks, not movement, a second world registry, or gameplay acceptance.
void MultiLineCoverage(const PortablePackageTables& map,const std::vector<Actor>& actors,
    const std::function<void()>& rollback) {
    using Trace=QuestVr::OriginalMultiLineResult;
    const auto array=[](const PortableModelVec3& value)->Array {return {value.x,value.y,value.z};};
    const auto native=ReadPortableRuntimeNativeModel();
    Require(native.available&&!actors.empty()&&native.modelExportIndex<map.exports.size(),
        "MultiLine fixture lacks the actual loader-bound first Training Model");
    const auto model=LoadPortableModel68(map,native.modelExportIndex);
    Require(native.modelPath=="00_Training."+GetPortableObjectPath(map,static_cast<std::int32_t>(native.modelExportIndex+1u))&&
        !model.nodes.empty(),"MultiLine expected world is not the loader-owned nonempty Training Model");
    const auto snapshots=GetPortableRuntimeMapActors(true);
    std::set<std::string> movers;
    for(const auto& actor:snapshots)if(actor.mover)movers.insert(actor.objectPath);

    // Local detached tokens carry only equality/classification between the
    // independent providers and the separately verified collector. They never
    // stand in for live UObject addresses or leave this expected-value path.
    struct Tokens {
        std::vector<std::string> paths;
        std::uintptr_t Id(const std::string& path) {
            if(path.empty())return 0u;
            const auto found=std::find(paths.begin(),paths.end(),path);
            if(found!=paths.end())return static_cast<std::uintptr_t>(found-paths.begin()+1);
            paths.push_back(path);return paths.size();
        }
        std::string Path(std::uintptr_t identity) const {
            if(!identity)return {};
            Require(identity<=paths.size(),"Expected MultiLine token is outside its detached path table");
            return paths[static_cast<std::size_t>(identity-1u)];
        }
    } tokens;
    for(const auto& actor:actors)static_cast<void>(tokens.Id(actor.path));
    const auto modelToken=tokens.Id(native.modelPath),worldToken=tokens.Id(actors.front().path);
    std::size_t ownerCaptures{},worldPredictions{},cases{},worldBlocks{},worldClears{},shortened{};
    struct World {QuestVr::OriginalWorldLineReply reply;bool itemKnown{true};};
    const auto world=[&](const Command& command)->World {
        ++worldPredictions;World result;
        // Same documented portable undefined-field policy as the adapter:
        // extent hits keep Item=-1, but that word is explicitly NOT authored.
        result.reply.hit={};
        if(command.extent==Array{}) {
            result.reply.clear=QuestVr::CheckOriginalModelPoint(result.reply.hit,model,modelToken,0u,
                Vector(command.end),Vector(command.start),command.flags,[&]()->QuestVr::OriginalModelCoords {
                    ++ownerCaptures;throw std::runtime_error("Expected null-owner world requested Owner.ToWorld");
                });
        }else {
            result.reply.clear=QuestVr::CheckOriginalModelExtent(result.reply.hit,model,modelToken,0u,nullptr,
                Vector(command.end),Vector(command.start),Vector(command.extent));
            result.itemKnown=result.reply.clear;
        }
        return result;
    };
    const auto crate=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {
        return actor.path=="00_Training.CrateBreakableMedGeneral1"&&actor.collide&&!actor.deleted;
    });
    Require(crate!=actors.end(),"MultiLine fixture lost the actual collidable first Training crate");
    std::array<std::optional<Command>,2u> clear,blocked;
    constexpr std::array<std::pair<std::size_t,float>,10u> rays{{{2u,-512.0f},{2u,8.0f},{0u,8.0f},{1u,8.0f},
        {2u,-4096.0f},{0u,512.0f},{0u,-512.0f},{1u,512.0f},{1u,-512.0f},{2u,512.0f}}};
    for(std::size_t branch=0u;branch<2u;++branch) {
        for(const float above:{128.0f,256.0f}) {
            for(const auto& [axis,distance]:rays) {
                Command command{Operation::MultiLineCheck};command.checkActors=false;
                command.start=command.end=crate->location;command.start[2u]+=above;command.end=command.start;
                command.end[axis]+=distance;if(branch)command.extent={4.0f,4.0f,4.0f};
                const auto predicted=world(command);
                if(predicted.reply.clear) {if(!clear[branch])clear[branch]=command;}
                else if(!blocked[branch])blocked[branch]=command;
                if(clear[branch]&&blocked[branch])break;
            }
            if(clear[branch]&&blocked[branch])break;
        }
        Require(clear[branch].has_value()&&blocked[branch].has_value(),
            "Bounded actual Training Model queries did not establish both clear and blocked point/extent controls");
        for(const auto* selection:{&*clear[branch],&*blocked[branch]})
            std::cout<<"MULTILINE WORLD SOURCE branch="<<(branch?"extent":"point")
                <<" start=("<<selection->start[0u]<<','<<selection->start[1u]<<','<<selection->start[2u]
                <<") end=("<<selection->end[0u]<<','<<selection->end[1u]<<','<<selection->end[2u]<<")\n";
    }
    const auto sourceOracle=std::make_unique<Oracle>(actors);
    const auto expected=[&](const Command& command,bool enabled,Oracle& oracle) {
        QuestVr::OriginalMultiLineQueries providers;bool itemKnown=true;
        providers.world=[&](std::uintptr_t identity,const PortableModelVec3& end,const PortableModelVec3& start,
            const PortableModelVec3& extent,std::uint32_t flags) {
            Require(identity==worldToken&&flags==command.flags,"Expected collector redirected the actual world or byte flags");
            auto supplied=command;supplied.end=array(end);supplied.start=array(start);supplied.extent=array(extent);
            const auto predicted=world(supplied);itemKnown=predicted.itemKnown;return predicted.reply;
        };
        if(enabled)providers.actors=[&](const PortableModelVec3& end,const PortableModelVec3& start,
            const PortableModelVec3& extent,std::uint8_t flags) {
            auto supplied=command;supplied.operation=Operation::LineCheck;
            supplied.end=array(end);supplied.start=array(start);supplied.extent=array(extent);supplied.flags=flags;
            const auto packets=oracle.Query(supplied,actors);QuestVr::OriginalLineHitArena arena;
            arena.first=packets.second;arena.records.reserve(packets.first.size());
            for(const auto& packet:packets.first)arena.records.push_back({packet.next,tokens.Id(packet.actorPath),
                tokens.Id(packet.primitivePath),Vector(packet.location),Vector(packet.normal),packet.time,packet.item});
            return arena;
        };
        providers.isMover=[&](std::uintptr_t identity) {return movers.count(tokens.Path(identity))!=0u;};
        auto result=QuestVr::CollectOriginalMultiLineHits(Vector(command.end),Vector(command.start),Vector(command.extent),
            command.checkActors?1:0,command.checkWorld?worldToken:0u,command.flags,providers);
        return std::pair<Trace,bool>{std::move(result),itemKnown};
    };
    const auto run=[&](const std::vector<Command>& commands,const std::string& label,bool positiveAnchor=false) {
        Require(!commands.empty()&&commands.size()<=3u,"MultiLine test exceeded bounded per-root detached snapshots");
        auto oracle=std::make_unique<Oracle>(*sourceOracle);bool enabled{},initialized{};
        std::optional<std::pair<Trace,bool>> prediction;std::vector<std::vector<std::uint32_t>> tags;
        std::vector<std::uint32_t> queryTags;
        std::vector<bool> enabledAt,initializedAt;
        for(const auto& command:commands) {
            if(command.operation==Operation::Enable) {enabled=true;initialized=true;}
            else if(command.operation==Operation::Disable)enabled=false;
            else {
                Require(command.operation==Operation::MultiLineCheck&&!prediction,"MultiLine root must contain exactly one query");
                prediction=expected(command,enabled,*oracle);
            }
            tags.push_back(oracle->tags);queryTags.push_back(oracle->tag);
            enabledAt.push_back(enabled);initializedAt.push_back(initialized);
        }
        Require(prediction.has_value(),"MultiLine root has no expected query");
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.states.size()==commands.size()&&
            result.queries.size()==1u&&result.queryHeads.size()==1u&&result.multiLine.size()==1u,
            "Actual first Training MultiLine adapter refused "+label+": "+result.execution.error);
        const auto& trace=prediction->first;const auto& metadata=result.multiLine.front();
        Require(metadata.queryIndex==0u&&metadata.collectorSteps==trace.stats.steps&&metadata.actorVisits==trace.stats.actorVisits&&
            metadata.moverTests==trace.stats.moverTests&&metadata.skipped==trace.stats.skipped&&metadata.corrected==trace.stats.corrected&&
            metadata.hasWorldHit==trace.hasWorldHit&&metadata.nearWorldHit==trace.nearWorldHit&&metadata.worldItemKnown==prediction->second,
            "Actual first Training MultiLine collector metadata differs from pure kernels: "+label);
        SameVector(metadata.actorQueryEnd,array(trace.actorQueryEnd),"MultiLine shortened actor-query endpoint differs");
        SameFloat(metadata.scale,trace.scale,"MultiLine world shortening scale differs");
        std::vector<PortableRetailCollisionHit> packets;packets.reserve(trace.hits.records.size());
        for(const auto& hit:trace.hits.records)packets.push_back({tokens.Path(hit.actor),tokens.Path(hit.primitive),hit.next,
            array(hit.location),array(hit.normal),hit.time,hit.item});
        SameHits(result.queries.front(),result.queryHeads.front(),{packets,trace.hits.first});
        if(positiveAnchor)Require(std::any_of(packets.begin(),packets.end(),[&](const auto& hit) {return hit.actorPath==crate->path;}),
            "MultiLine actual crate anchor was excluded or lacks a positive independent source hit");
        const auto& query=*std::find_if(commands.begin(),commands.end(),[](const auto& command) {return command.operation==Operation::MultiLineCheck;});
        if(query.checkActors&&enabled&&trace.hasWorldHit&&trace.scale<1.0f&&array(trace.actorQueryEnd)!=query.end)++shortened;
        if(query.checkWorld) {
            if(trace.hasWorldHit) {
                ++worldBlocks;
                const auto hit=std::find_if(packets.begin(),packets.end(),[&](const auto& packet) {return packet.actorPath==actors.front().path;});
                Require(hit!=packets.end()&&hit->primitivePath==native.modelPath,
                    "World hit lost actual slot-zero LevelInfo/loader Model identity");
                if(query.extent==Array{})Require(prediction->second&&hit->item>=0&&static_cast<std::size_t>(hit->item)<model.nodes.size(),
                    "Point world hit lacks its defined original node Item");
                else Require(!prediction->second&&hit->item==-1,"Extent world Item was falsely exposed as an authored node");
            }else ++worldClears;
        }
        for(std::size_t index=0u;index<result.states.size();++index) {
            const auto& state=result.states[index];
            Require(state.initialized==initializedAt[index]&&state.enabled==enabledAt[index]&&
                state.seed==(initializedAt[index]?sourceOracle->seed:InitialSeed)&&
                state.initializationDraws==(initializedAt[index]?49152u:0u),
                "MultiLine created/redrew absent hash tables or changed shared RNG: "+label);
            if(initializedAt[index]) {
                SameActors(state,actors,tags[index]);
                Require(state.tag==queryTags[index],"MultiLine lost query tag/no-query short circuit");
                Require(enabledAt[index]?state.hashIdentity!=0u:state.hashIdentity==0u,
                    "MultiLine changed the actual Level.Hash enabled lifetime");
            }else Require(!state.hashIdentity&&state.actors.empty()&&!state.tag,
                "Model-only/inactive MultiLine initialized actor storage or collision tags");
        }
        ++cases;rollback();
    };
    for(std::size_t branch=0u;branch<2u;++branch) {
        run({*blocked[branch]},branch?"dormant extent world block":"dormant point world block");
        run({*clear[branch]},branch?"dormant extent world clear":"dormant point world clear");
    }
    auto withAbsentHash=*blocked[0u];withAbsentHash.checkActors=true;
    run({withAbsentHash},"world plus absent actor hash");
    Command actor{Operation::MultiLineCheck};actor.checkWorld=false;actor.actorPath=crate->path;
    actor.start=actor.end=crate->location;actor.start[0u]-=128.0f;actor.end[0u]+=128.0f;
    run({actor},"actor request without hash");
    run({Command{Operation::Enable},actor},"actor-only crate anchor",true);
    auto actorExtent=actor;actorExtent.extent={4.0f,4.0f,4.0f};
    run({Command{Operation::Enable},actorExtent},"actor-only extent crate anchor",true);
    for(std::size_t branch=0u;branch<2u;++branch) {
        run({Command{Operation::Enable},*blocked[branch]},branch?"enabled world-only extent":"enabled world-only point");
        auto combined=*blocked[branch];combined.checkActors=true;combined.actorPath=crate->path;
        const auto predicted=world(combined);
        Require(!predicted.reply.clear,"Selected world/hash block ceased to be a genuine source Model hit");
        run({Command{Operation::Enable},combined},branch?"world/hash shortened extent":"world/hash shortened point");
        combined=*clear[branch];combined.checkActors=true;
        run({Command{Operation::Enable},combined},branch?"world/hash clear extent":"world/hash clear point");
    }
    auto flags=withAbsentHash;flags.flags=0x81u;
    run({Command{Operation::Enable},flags},"world/hash byte node flags");
    run({Command{Operation::Enable},Command{Operation::Disable},withAbsentHash},"world after disabled actor hash");
    Command inactive{Operation::MultiLineCheck};inactive.checkActors=inactive.checkWorld=false;
    inactive.start=inactive.end=inactive.extent={std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::quiet_NaN()};
    run({inactive},"both providers inactive NaNs");
    run({Command{Operation::Enable},inactive},"enabled hash but both providers inactive NaNs");
    const auto reject=[&](Command bad,const std::string& label) {
        const auto result=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},actor,bad});
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Late unsupported MultiLine input accepted its successful hash/query prefix: "+label);
        ++refusals;rollback();
    };
    auto invalid=actor;invalid.actorPath="00_Training.MissingMultiLineAnchor";reject(invalid,"missing anchor");
    invalid.actorPath="Engine.Actor";reject(invalid,"foreign non-actor-class anchor");
    invalid=actor;invalid.extent[1u]=std::numeric_limits<float>::quiet_NaN();reject(invalid,"active hash extent NaN");
    invalid=withAbsentHash;invalid.end[2u]=std::numeric_limits<float>::quiet_NaN();reject(invalid,"active world endpoint NaN");
    Require(ownerCaptures==0u&&worldBlocks>0u&&worldClears>0u&&shortened>0u,
        "MultiLine controls missed genuine world/hash shortening/admission or requested owner coordinates");
    std::cout<<"FIRST TRAINING MultiLine: "<<cases<<" small rollback-only roots, "<<worldPredictions
        <<" loader-Model predictions; actual actor hash order, source-anchor admission, null-owner world point/extent packets, "
        "defined/unknown Item metadata, flags/shortening and late complete rollback. NOT Move or playable startup.\n";
}
