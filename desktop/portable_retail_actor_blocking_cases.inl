// Actual first-Training adapter evidence only. Expectations are constructed
// from declared fields and detached class ancestry, never production predicates.
void ActorBlockingCases(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    struct Source {
        std::string path,classPath,level,base;
        bool brush{},playerPawn{},pawn{},projectile{},localPlayer{},world{},blockActors{},blockPlayers{};
        Array location{},colLocation{};
        std::array<std::int32_t,3u> rotation{};
    };
    const auto snapshots=GetPortableRuntimeMapActors(true);
    Require(snapshots.size()==actors.size(),"Blocking fixture lost actual first-Training actor census");
    const auto pointer=[](const std::string& path,const char* key) {
        auto value=Declared(path,key,Kind::Object).text;
        if(Fold(value)=="none")value.clear();
        return value;
    };
    // Cache detached ancestry only. No calls to the native cast/predicate or
    // actor720 GetPlayerPawn fallback are used to predict any result.
    std::vector<std::pair<std::string,std::vector<std::string>>> ancestry;
    const auto derives=[&](const std::string& classPath,const char* wanted) {
        const auto folded=Fold(classPath);
        auto found=std::find_if(ancestry.begin(),ancestry.end(),[&](const auto& entry){return entry.first==folded;});
        if(found==ancestry.end()) {
            std::vector<std::string> chain;auto current=classPath;
            while(!current.empty()) {
                Require(chain.size()<128u,"Blocking fixture actual class ancestry exceeds its bound");
                const auto name=Fold(current);
                Require(std::find(chain.begin(),chain.end(),name)==chain.end(),"Blocking fixture actual class ancestry cycles");
                chain.push_back(name);
                const auto declaration=ReadPortableRuntimeDispatchClass(current);
                Require(Fold(declaration.path)==name,"Blocking fixture dispatch class identity changed");
                current=declaration.parentPath;
            }
            ancestry.push_back({folded,std::move(chain)});found=ancestry.end()-1;
        }
        return std::find(found->second.begin(),found->second.end(),Fold(wanted))!=found->second.end();
    };
    std::vector<Source> source;source.reserve(actors.size());
    std::vector<std::size_t> crates,movers,npcs,players,levels,projectiles;
    std::size_t localPlayers{},baseLinks{};
    for(const auto& actor:actors) {
        const auto snapshot=std::find_if(snapshots.begin(),snapshots.end(),[&](const auto& item){return item.objectPath==actor.path;});
        Require(snapshot!=snapshots.end(),"Blocking fixture source actor has no actual class snapshot");
        Source row;row.path=actor.path;row.classPath=snapshot->classPath;
        row.level=pointer(row.path,"Engine.Actor.Level");row.base=pointer(row.path,"Engine.Actor.Base");
        row.playerPawn=derives(row.classPath,"Engine.PlayerPawn");row.pawn=derives(row.classPath,"Engine.Pawn");
        row.projectile=derives(row.classPath,"Engine.Projectile");
        row.brush=!pointer(row.path,"Engine.Actor.Brush").empty()&&derives(row.classPath,"Engine.Brush");
        row.localPlayer=row.playerPawn&&!pointer(row.path,"Engine.PlayerPawn.Player").empty();
        row.world=Declared(row.path,"Engine.Actor.bCollideWorld",Kind::Bool).boolean;
        row.blockActors=Declared(row.path,"Engine.Actor.bBlockActors",Kind::Bool).boolean;
        row.blockPlayers=Declared(row.path,"Engine.Actor.bBlockPlayers",Kind::Bool).boolean;
        row.location=Declared(row.path,"Engine.Actor.Location",Kind::Vector).vector;
        row.colLocation=Declared(row.path,"Engine.Actor.ColLocation",Kind::Vector).vector;
        row.rotation=Declared(row.path,"Engine.Actor.Rotation",Kind::Rotator).rotation;
        SameVector(row.location,actor.location,"Blocking fixture altered initial Location");
        SameVector(row.colLocation,actor.colLocation,"Blocking fixture altered initial ColLocation");
        const auto index=source.size();
        // Names select a real fixture; branch predictions below use only fields
        // and genuine ancestry, never the selected name or renderer flags.
        if(Fold(row.path).find(".cratebreakable")!=std::string::npos)crates.push_back(index);
        if(derives(row.classPath,"Engine.Mover"))movers.push_back(index);
        if(row.pawn&&!row.playerPawn)npcs.push_back(index);
        if(row.playerPawn)players.push_back(index);
        if(derives(row.classPath,"Engine.LevelInfo"))levels.push_back(index);
        if(row.projectile)projectiles.push_back(index);
        if(row.localPlayer)++localPlayers;
        if(!row.base.empty())++baseLinks;
        source.push_back(std::move(row));
    }
    Require(!crates.empty()&&!movers.empty()&&!npcs.empty()&&!players.empty()&&levels.size()==1u,
        "First Training lacks its real crate/mover/NPC/Camera-PlayerPawn/LevelInfo blocking controls");
    const auto locate=[&](const std::string& path)->std::size_t {
        const auto found=std::find_if(source.begin(),source.end(),[&](const auto& row){return Fold(row.path)==Fold(path);});
        Require(found!=source.end(),"Blocking fixture pointer is outside the actual first-Training actor census: "+path);
        return static_cast<std::size_t>(found-source.begin());
    };
    const auto command=[](Operation operation,const std::string& path,const std::string& other={}) {
        Command result;result.operation=operation;result.actorPath=path;result.otherActorPath=other;return result;
    };
    struct Expected {Command command;bool returned{};std::string localPlayer;};
    std::vector<Expected> expected;expected.reserve(actors.size()+512u);
    for(const auto& row:source)
        expected.push_back({command(Operation::LocalPlayerPawn,row.path),row.localPlayer,row.localPlayer?row.path:std::string{}});
    std::vector<std::size_t> representatives;
    const auto select=[&](const std::vector<std::size_t>& category) {
        for(std::size_t i=0u;i<std::min<std::size_t>(category.size(),2u);++i)
            if(std::find(representatives.begin(),representatives.end(),category[i])==representatives.end())
                representatives.push_back(category[i]);
    };
    select(crates);select(movers);select(npcs);select(players);select(levels);select(projectiles);
    std::array<std::size_t,4u> branches{};std::size_t blocked{},clear{},basedTrue{},basedFalse{},basedCommands{};
    const auto blockedBy=[&](const Source& a,const Source& b) {
        if(Fold(b.path)==Fold(a.level)){++branches[0u];return a.world;}
        if(b.brush){++branches[1u];return a.world&&(a.localPlayer?b.blockPlayers:b.blockActors);}
        if(a.brush){++branches[2u];return b.world&&(b.localPlayer?a.blockPlayers:a.blockActors);}
        ++branches[3u];
        return ((a.localPlayer||a.projectile)?b.blockPlayers:b.blockActors)&&
            ((b.localPlayer||b.projectile)?a.blockPlayers:a.blockActors);
    };
    for(const auto a:representatives)for(const auto b:representatives) {
        const bool wanted=blockedBy(source[a],source[b]);
        expected.push_back({command(Operation::IsBlockedBy,source[a].path,source[b].path),wanted,{}});
        if(wanted)++blocked;else ++clear;
    }
    Require(std::all_of(branches.begin(),branches.end(),[](std::size_t count){return count!=0u;})&&blocked&&clear,
        "Actual first-Training representative matrix does not exercise all four original branches and both outcomes");
    const auto basedOn=[&](std::size_t start,const std::string& target) {
        std::vector<std::size_t> visited;auto current=source[start].path;
        while(!current.empty()) {
            const auto index=locate(current);
            Require(visited.size()<source.size()&&std::find(visited.begin(),visited.end(),index)==visited.end(),
                "Actual first-Training Base chain cycles or exceeds actor census");
            visited.push_back(index);
            if(!target.empty()&&Fold(current)==Fold(target))return true;
            current=source[index].base;
        }
        return false;
    };
    const auto addBased=[&](std::size_t start,const std::string& target) {
        const bool wanted=basedOn(start,target);++basedCommands;
        Require(expected.size()<16384u,"First-Training predicate fixture exceeded its bounded command census");
        expected.push_back({command(Operation::IsBasedOn,source[start].path,target),wanted,{}});
        if(wanted)++basedTrue;else ++basedFalse;
    };
    for(const auto index:representatives) {
        addBased(index,source[index].path);addBased(index,{});
        for(const auto other:representatives)if(other!=index)addBased(index,source[other].path);
    }
    // Exercise every real nonempty chain in both directions. None is never a
    // successful terminator; do not invent Base edges when the map has none.
    for(std::size_t i=0u;i<source.size();++i)if(!source[i].base.empty()) {
        auto current=source[i].base;std::vector<std::size_t> visited;
        while(!current.empty()) {
            const auto index=locate(current);
            Require(visited.size()<source.size()&&std::find(visited.begin(),visited.end(),index)==visited.end(),
                "Actual first-Training Base traversal cycles");
            visited.push_back(index);addBased(i,current);addBased(index,source[i].path);current=source[index].base;
        }
        addBased(i,{});
    }
    Require(basedTrue&&basedFalse,"First-Training Base tests lack self/None positive and negative controls");
    std::size_t batches{};
    for(std::size_t begin=0u;begin<expected.size();begin+=16u) {
        const auto end=std::min(expected.size(),begin+16u);std::vector<Command> commands;
        for(auto i=begin;i<end;++i)commands.push_back(expected[i].command);
        const auto result=ProbePortableRuntimeRetailCollision(commands);++batches;
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&
            result.states.size()==commands.size()&&result.predicates.size()==commands.size()&&
            result.queries.empty()&&result.queryHeads.empty()&&result.mutations.empty()&&result.multiLine.empty(),
            "Read-only actor predicates failed or emitted nonpredicate outputs");
        std::size_t work{};
        for(std::size_t i=0u;i<commands.size();++i) {
            const auto& actual=result.predicates[i];const auto& wanted=expected[begin+i];const auto& state=result.states[i];
            Require(actual.commandIndex==i&&actual.returned==wanted.returned&&actual.localPlayerPath==wanted.localPlayer,
                "Actual native predicate differs from declared first-Training fields/ancestry: "+commands[i].actorPath+
                    " -> "+commands[i].otherActorPath);
            Require(!state.initialized&&!state.enabled&&!state.initializedHere&&!state.hashIdentity&&!state.hashes&&
                !state.allocatedLinks&&!state.activeLinks&&!state.retainedBytes&&state.actors.empty()&&state.tag==0u&&
                state.seed==InitialSeed&&state.initializationDraws==0u&&state.lifecycleOperations>work,
                "Read-only predicate initialized hash/RNG state or reset its same-root work ledger");
            work=state.lifecycleOperations;
        }
        rollback();
    }
    const auto reject=[&](const std::vector<Command>& commands,const std::string& why,const QuestVr::Vm::Limits& limits={}) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Original actor predicate admitted invalid input/budget: "+why);++refusals;rollback();
    };
    const auto witness=command(Operation::IsBlockedBy,source[crates.front()].path,source[movers.front()].path);
    reject({command(Operation::IsBlockedBy,witness.actorPath)},"IsBlockedBy None");
    reject({command(Operation::LocalPlayerPawn,{})},"empty receiver");
    reject({command(Operation::LocalPlayerPawn,"Engine.Actor")},"class receiver");
    reject({command(Operation::IsBasedOn,"00_Training.__MissingOriginalActor")},"unknown receiver");
    reject({command(Operation::IsBlockedBy,witness.actorPath,"Engine.Actor")},"non-Actor candidate");
    auto embedded=witness;embedded.actorPath.push_back('\0');embedded.actorPath+="tail";
    reject({embedded},"embedded receiver NUL");
    auto oversized=witness;oversized.otherActorPath.assign(8193u,'x');reject({oversized},"unbounded target");
    reject({Command{Operation::Enable},witness,Command{static_cast<Operation>(999)}},"late unknown after actual Enable and predicate");
    QuestVr::Vm::Limits tiny;tiny.retainedBytes=1u;
    reject({witness},"tiny shared retained-byte allowance",tiny);
    for(const auto& row:source) {
        SameVector(Declared(row.path,"Engine.Actor.Location",Kind::Vector).vector,row.location,"Predicate changed source Location");
        SameVector(Declared(row.path,"Engine.Actor.ColLocation",Kind::Vector).vector,row.colLocation,"Predicate changed source ColLocation");
        Require(Declared(row.path,"Engine.Actor.Rotation",Kind::Rotator).rotation==row.rotation&&
            pointer(row.path,"Engine.Actor.Base")==row.base&&pointer(row.path,"Engine.Actor.Level")==row.level,
            "Predicate changed source Rotation/Base/Level");
    }
    rollback();
    std::cout<<"FIRST TRAINING blocking predicates: "<<source.size()<<" declared actor local-Player controls ("
        <<players.size()<<" PlayerPawn ancestry, "<<localPlayers<<" actually nonnull Player); "
        <<representatives.size()<<" real crate/mover/NPC/Camera/LevelInfo representatives, "
        <<blocked<<" blocked / "<<clear<<" clear directional pairs; branches Level/candidateBrush/sourceBrush/ordinary="
        <<branches[0u]<<'/'<<branches[1u]<<'/'<<branches[2u]<<'/'<<branches[3u]<<"; "
        <<basedCommands<<" self/None/actual Base-chain controls, "<<baseLinks<<" authored nonempty Base links; "
        <<batches<<" uninitialized-hash rollback-only batches. No possession, Move or physics claim.\n";
}
