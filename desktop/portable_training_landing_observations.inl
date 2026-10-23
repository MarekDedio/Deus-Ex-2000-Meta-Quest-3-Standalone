// Explicit real sweep/contact/native landing observation; NOT acceptance of a
// gravity timestep, automatic settling or controller-driven crate gameplay.
void ObserveTrainingLandings(const PortableWorldStartupProbeResult& world,bool requested) {
    const auto& rows=world.landingObservations;
    if(!requested){Require(rows.empty(),"Normal startup manufactured landing commands");return;}
    Require(rows.size()<=2u,"Training sweep exceeded two requested crates");
    std::size_t captured{},landed{};
    for(std::size_t i=0u;i<rows.size();++i) {
        const auto& row=rows[i];
        Require(row.commandIndex==i&&row.actorPath==std::string("00_Training.CrateBreakableMedGeneral")+std::to_string(i+1u)&&
            row.gameInfoPath==world.gameInfoPath&&row.entered&&(!row.returned||row.entered)&&(!row.captured||row.returned)&&
            row.writesBefore<=row.writesAfter&&row.writesAfter<=world.execution.writes,
            "Training landing lost original crate/root/write-prefix identity");
        if(row.captured) {
            Require(row.physicsBefore==2u&&row.counterBefore==0&&row.baseBefore.empty()&&!row.zoneBefore.empty()&&
                !row.removed&&!row.deleteMe&&row.locationBefore[0u]==row.locationAfter[0u]&&
                row.locationBefore[1u]==row.locationAfter[1u]&&row.locationAfter[2u]<=row.locationBefore[2u],
                "Explicit Training downward sweep changed original profile or moved laterally/upward");
            if(row.landingEntered) {
                Require(row.hitTime>=0.0f&&row.hitTime<1.0f&&row.normal[2u]>0.7&&
                    row.selectedLanded=="DeusEx.DeusExDecoration.Landed"&&!row.floorPath.empty(),
                    "Training landing lacks actual floor collision and selected original event");
                if(!row.recovery&&!row.zoneBounce) {
                    Require(row.landedCalls==1u&&row.setPhysicsCalls==1u&&row.counterAfter==0&&row.physicsAfter==0u&&
                        row.baseAfter==row.floorPath&&row.velocityAfter==std::array<float,3u>{},
                        "Ordinary Training landing did not establish original support and stop via SetPhysics");
                    ++landed;
                }
            }
            ++captured;
        }else Require(i+1u==rows.size()&&!world.execution.passed()&&!world.execution.committed,
            "Training sweep continued after incomplete native movement/landing");
        std::cout<<std::setprecision(9)<<"TRAINING LANDING command="<<i<<" actor="<<std::quoted(row.actorPath)
            <<" entered="<<row.entered<<" moved="<<row.moved<<" landingEntered="<<row.landingEntered
            <<" returned="<<row.returned<<" captured="<<row.captured<<" floor="<<std::quoted(row.floorPath)
            <<" selected="<<std::quoted(row.selectedLanded)<<" hitTime="<<row.hitTime<<" normalZ="<<row.normal[2u]
            <<" zBefore="<<row.locationBefore[2u]<<" zAfter="<<row.locationAfter[2u]
            <<" physicsBefore="<<unsigned(row.physicsBefore)<<" physicsAfter="<<unsigned(row.physicsAfter)
            <<" baseAfter="<<std::quoted(row.baseAfter)<<" zoneBefore="<<std::quoted(row.zoneBefore)
            <<" zoneAfter="<<std::quoted(row.zoneAfter)<<" counterAfter="<<row.counterAfter<<" traces="<<row.traceCalls
            <<" landedCalls="<<row.landedCalls<<" setterCalls="<<row.setPhysicsCalls<<" recovery="<<row.recovery
            <<" zoneBounce="<<row.zoneBounce<<" writesBefore="<<row.writesBefore<<" writesAfter="<<row.writesAfter<<'\n';
    }
    if(rows.size()<2u)Require(!world.execution.passed()&&!world.execution.committed,
        "Training startup skipped requested sweeps and fabricated success");
    std::cout<<"TRAINING LANDING OBSERVATIONS commands="<<rows.size()<<" captured="<<captured<<" ordinaryLanded="<<landed
        <<"; explicit swept-delta contact only, NOT physFalling, automatic gravity, support-loss or published headset gameplay.\n";
}
