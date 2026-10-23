// Detached observations of original pre-pair Frob or ParseRightClick. The
// latter supplies an actual member target but is NOT a controller input bridge
// or acceptance of an uncaptured/returned-but-not-collected callback.
void ObserveTrainingLoot(const PortableWorldStartupProbeResult& world,bool requested,bool viaPlayer,std::size_t writeLimit) {
    const auto& observations=world.lootObservations;
    Require(observations.size()<=2u,"First Training collection exceeded its two actual crate-lockpick bound");
    if(!requested) {
        Require(!viaPlayer,"Player collection requires the shared loot diagnostic");
        Require(observations.empty(),"Normal/damage-only startup executed unrequested diagnostic loot Frob");return;
    }
    Require(world.execution.writes<=writeLimit,"Startup loot exceeded the selected shared write allowance");
    struct Birth {std::size_t damage,index;const PortableStartupDamageBirth* value;};
    std::vector<Birth> births;
    for(std::size_t damage=0u;damage<world.damageObservations.size();++damage) {
        const auto& row=world.damageObservations[damage];if(!row.captured)continue;
        for(std::size_t index=0u;index<row.births.size();++index) {
            const auto& item=row.births[index];
            if(item.classPath=="DeusEx.Lockpick"&&item.pickup&&!item.removed&&!item.deleteMe)
                births.push_back({damage,index,&item});
        }
    }
    Require(births.size()<=2u&&observations.size()<=births.size(),
        "Training collection did not use only completed surviving damage-birth identities");
    const auto item=[&](const PortableStartupLootItem& value,const char* label) {
        Require(CanonicalIdentity(value.actorPath)&&value.actorPath.starts_with("00_Training.")&&
            CanonicalIdentity(value.classPath)&&value.classPath.find('.')!=std::string::npos&&
            CanonicalIdentity(value.statePath,true)&&CanonicalIdentity(value.ownerPath,true)&&
            CanonicalIdentity(value.basePath,true)&&CanonicalIdentity(value.inventoryPath,true)&&
            CanonicalIdentity(value.instigatorPath,true),std::string("Loot snapshot has an invalid canonical identity: ")+label);
        Require(value.physics>=0&&value.physics<=255,"Loot snapshot Physics is not the actual stored Byte");
        Require(value.remoteRole>=0&&value.remoteRole<=255,"Loot snapshot RemoteRole is not the actual stored Byte");
    };
    const auto chain=[&](const std::vector<PortableStartupLootItem>& values,const std::string& player,const char* label) {
        Require(values.size()<=128u,"Training loot inventory exceeds the bounded actual linked-list observation");
        std::vector<std::string> visited;
        for(std::size_t i=0u;i<values.size();++i) {
            const auto& value=values[i];item(value,label);
            Require(std::find(visited.begin(),visited.end(),value.actorPath)==visited.end(),
                "Training loot inventory contains a repeated node/cycle");visited.push_back(value.actorPath);
            Require(value.inventoryPath==(i+1u<values.size()?values[i+1u].actorPath:std::string{}),
                "Training loot inventory observation replaced the actual ordered Inventory links");
            Require(value.ownerPath==player&&!value.removed&&!value.deleteMe,
                "Training loot inventory chain contains an unowned or deleted node");
        }
    };
    const auto printItem=[](const PortableStartupLootItem& value,const char* stage,std::size_t command,std::optional<std::size_t> link={}) {
        std::cout<<"TRAINING LOOT ITEM command="<<command<<" stage="<<stage;
        if(link)std::cout<<" link="<<*link;
        std::cout<<" actor="<<std::quoted(value.actorPath)<<" class="<<std::quoted(value.classPath)
            <<" state="<<std::quoted(value.statePath)<<" owner="<<std::quoted(value.ownerPath)
            <<" base="<<std::quoted(value.basePath)<<" inventory="<<std::quoted(value.inventoryPath)
            <<" instigator="<<std::quoted(value.instigatorPath)<<" physics="<<value.physics<<" remoteRole="<<value.remoteRole
            <<" copiesAvailable="<<value.numCopies.has_value()<<" copies="<<value.numCopies.value_or(-1)
            <<" maxCopiesAvailable="<<value.maxCopies.has_value()<<" maxCopies="<<value.maxCopies.value_or(-1)<<" removed="<<value.removed
            <<" invPos=("<<value.invPosX<<','<<value.invPosY<<") invSlots=("<<value.invSlotsX<<','<<value.invSlotsY<<')'
            <<" deleteMe="<<value.deleteMe<<" held="<<value.heldItem<<" carried="<<value.carriedItem
            <<" onlyOwnerSee="<<value.onlyOwnerSee<<" sleepTouch="<<value.sleepTouch<<" hidden="<<value.hidden
            <<" collideActors="<<value.collideActors<<" collideWorld="<<value.collideWorld
            <<" blockActors="<<value.blockActors<<" blockPlayers="<<value.blockPlayers<<'\n';
    };
    const auto grid=[](const PortableStartupLootGrid& value,const char* stage,std::size_t command) {
        Require(value.rows>0&&value.columns>0&&value.cells.size()<=128u&&
            std::int64_t{value.rows}*value.columns<=static_cast<std::int64_t>(value.cells.size()),
            "Player loot grid lacks bounded actual dimensions/fixed Byte storage");
        Require(std::all_of(value.cells.begin(),value.cells.end(),[](auto cell){return cell<=1u;}),
            "Player loot grid contains an unexpected original occupancy value");
        std::cout<<"TRAINING LOOT GRID command="<<command<<" stage="<<stage<<" rows="<<value.rows
            <<" columns="<<value.columns<<" occupied="<<std::count(value.cells.begin(),value.cells.end(),std::uint8_t{1u})
            <<" cells=(";
        for(std::size_t n=0u;n<value.cells.size();++n){if(n)std::cout<<',';std::cout<<unsigned(value.cells[n]);}
        std::cout<<")\n";
    };
    const auto clock=[](const Bytes& bytes,const std::string& player,const char* stage,std::size_t command) {
        Require(bytes.size()<=64u*1024u,"Player loot clock exceeds its bounded canonical observation");
        std::cout<<"TRAINING LOOT PLAYER CLOCK command="<<command<<" stage="<<stage<<" bytes="<<bytes.size();
        if(!bytes.empty()) {
            const auto state=QuestVr::DecodeScriptSavedState(bytes);
            Require(QuestVr::EncodeScriptSavedState(state)==bytes&&state.mapName=="00_Training"&&state.objects.size()==1u&&
                state.objects.front().path==player&&state.objects.front().clock.has_value(),
                "Player loot clock is not the actual player's canonical full animation-clock packet");
            const auto& value=*state.objects.front().clock;
            std::cout<<std::setprecision(9)<<" sequence="<<std::quoted(value.pose.main.sequence)
                <<" frame="<<value.pose.main.normalizedFrame<<" rate="<<value.main.rate
                <<" tweenRate="<<value.main.tweenRate<<" loop="<<value.main.loop<<" finished="<<value.main.finished
                <<" simulationTime="<<value.simulationTime;
        }
        std::cout<<'\n';
    };
    const auto emptyGrid=[](const PortableStartupLootGrid& value) {
        return value.rows==0&&value.columns==0&&value.cells.empty();
    };
    std::size_t entered{},returned{},captured{},transferred{},merged{},other{};
    std::size_t previousWrites=world.damageObservations.empty()?0u:world.damageObservations.back().writesAfter;
    for(std::size_t i=0u;i<observations.size();++i) {
        const auto& row=observations[i];const auto& birth=births[i];
        Require(row.commandIndex==i&&row.damageCommandIndex==birth.damage&&row.birthIndex==birth.index&&
            row.actorPath==birth.value->actorPath,"Training loot observations are not the original surviving birth prefix");
        Require(CanonicalIdentity(row.actorPath)&&CanonicalIdentity(row.playerPath)&&CanonicalIdentity(row.gameInfoPath)&&
            CanonicalIdentity(row.functionPath,!row.entered)&&CanonicalIdentity(row.playerStatePath,true),
            "Training loot has an invalid/unbounded original function/player/GameInfo/state identity");
        Require(row.worldBegun&&!row.worldStartup&&!row.playerPaired&&row.isPawn&&row.isPlayer&&row.playerHealth>0,
            "Training loot did not use the actual eligible begun-world unpaired original player");
        Require(row.gameInfoPath==world.gameInfoPath&&row.playerPath=="00_Training.JCDentonMale0"&&
            std::any_of(world.playerLogins.begin(),world.playerLogins.end(),[&](const auto& login) {
                return login.loginReturned&&login.playerPath==row.playerPath&&login.gamePath==row.gameInfoPath;
            }),"Training loot did not use the genuine original Login-returned JC/GameInfo");
        Require((!row.returned||row.entered)&&(!row.captured||row.returned),
            "Training loot returned/captured without its actual preceding original-action entry/return");
        Require(row.playerRightClick==viaPlayer,"Training loot observation changed its requested original action");
        if(!viaPlayer)Require(row.targetBefore.empty()&&row.targetAfter.empty()&&emptyGrid(row.gridBefore)&&
            emptyGrid(row.gridAfter)&&row.playerClockBefore.empty()&&row.playerClockAfter.empty(),
            "Direct Frob diagnostic fabricated selected-player/grid/clock observations");
        Require(row.writesBefore>=previousWrites&&row.writesAfter>=row.writesBefore&&
            row.writesAfter<=world.execution.writes,"Training loot lost its genuine shared outer write prefix");
        previousWrites=row.writesAfter;
        if(row.entered) {
            Require(row.functionPath==(viaPlayer?"DeusEx.DeusExPlayer.ParseRightClick":"Engine.Pickup.Pickup.Frob"),
                "Training loot did not select the requested actual original state-aware action");
            item(row.before,"before");chain(row.inventoryBefore,row.playerPath,"inventory-before");
            Require(row.before.actorPath==row.actorPath&&row.before.classPath=="DeusEx.Lockpick"&&
                row.before.statePath=="DeusEx.DeusExPickup.Pickup"&&!row.before.removed&&!row.before.deleteMe&&
                row.before.physics==2&&row.before.ownerPath.empty()&&row.before.basePath.empty()&&
                row.before.numCopies.has_value()&&row.before.maxCopies.has_value()&&
                *row.before.numCopies>0&&*row.before.maxCopies>0&&*row.before.numCopies<=*row.before.maxCopies,
                "Loot before snapshot does not describe the genuine untouched surviving Lockpick");
            printItem(row.before,"before",i);
            for(std::size_t n=0u;n<row.inventoryBefore.size();++n)printItem(row.inventoryBefore[n],"inventory-before",i,n);
            if(viaPlayer) {
                Require(CanonicalIdentity(row.targetBefore,true),"Player loot target-before is not an actual canonical member identity");
                grid(row.gridBefore,"before",i);clock(row.playerClockBefore,row.playerPath,"before",i);
            }
            ++entered;
        }
        std::string outcome="incomplete-prefix";
        if(row.captured) {
            item(row.after,"after");chain(row.inventoryAfter,row.playerPath,"inventory-after");
            if(viaPlayer) {
                Require(CanonicalIdentity(row.targetAfter,true),"Player loot target-after is not an actual canonical member identity");
                grid(row.gridAfter,"after",i);clock(row.playerClockAfter,row.playerPath,"after",i);
                Require(row.gridBefore.rows==row.gridAfter.rows&&row.gridBefore.columns==row.gridAfter.columns&&
                    row.gridBefore.cells.size()==row.gridAfter.cells.size(),
                    "Original player loot action replaced inventory-grid dimensions/storage");
            }
            Require(row.after.actorPath==row.actorPath&&row.after.classPath==row.before.classPath,
                "Loot action observation replaced the actual standalone born item identity/class");
            const auto owned=std::find_if(row.inventoryAfter.begin(),row.inventoryAfter.end(),[&](const auto& value) {
                return value.actorPath==row.actorPath;
            });
            if(row.after.ownerPath==row.playerPath&&!row.after.removed&&!row.after.deleteMe) {
                Require(owned!=row.inventoryAfter.end()&&*owned==row.after&&!row.inventoryAfter.empty()&&
                    row.inventoryAfter.front().actorPath==row.actorPath&&row.inventoryAfter.size()==row.inventoryBefore.size()+1u&&
                    std::equal(row.inventoryBefore.begin(),row.inventoryBefore.end(),row.inventoryAfter.begin()+1u)&&
                    std::none_of(row.inventoryBefore.begin(),row.inventoryBefore.end(),[&](const auto& value) {
                        return value.classPath==row.before.classPath;
                    }),
                    "Transferred first lockpick did not become the actual new original Inventory head");
                Require(row.after.physics==0&&row.after.remoteRole==2&&row.after.heldItem&&row.after.carriedItem&&
                    row.after.onlyOwnerSee&&!row.after.sleepTouch&&row.after.hidden&&!row.after.collideActors&&
                    !row.after.collideWorld&&!row.after.blockActors&&!row.after.blockPlayers&&
                    row.after.instigatorPath==row.playerPath&&row.after.statePath=="Engine.Inventory.Idle2"&&
                    row.after.numCopies==row.before.numCopies&&row.after.maxCopies==row.before.maxCopies&&
                    row.after.basePath==(viaPlayer?row.playerPath:std::string{})&&
                    row.after.invSlotsX==row.before.invSlotsX&&row.after.invSlotsY==row.before.invSlotsY,
                    "Transferred lockpick lacks original BecomeItem/GiveTo/Idle2 state or changed its copy count");
                if(viaPlayer) {
                    const auto& before=row.gridBefore;const auto& after=row.gridAfter;const auto& value=row.after;
                    Require(row.targetAfter.empty()&&value.invPosX>=0&&value.invPosY>=0&&value.invSlotsX>0&&value.invSlotsY>0&&
                        std::int64_t{value.invPosX}+value.invSlotsX<=after.columns&&
                        std::int64_t{value.invPosY}+value.invSlotsY<=after.rows,
                        "Accepted original player pickup lacks cleared member target and actual in-range inventory placement");
                    std::optional<std::pair<std::int32_t,std::int32_t>> firstFit;
                    for(std::int32_t y=0;y<=before.rows-value.invSlotsY&&!firstFit;++y)
                        for(std::int32_t x=0;x<=before.columns-value.invSlotsX;++x) {
                            bool available=true;
                            for(std::int32_t sy=0;sy<value.invSlotsY;++sy)for(std::int32_t sx=0;sx<value.invSlotsX;++sx)
                                if(before.cells[static_cast<std::size_t>((y+sy)*before.columns+x+sx)]==1u)available=false;
                            if(available){firstFit=std::pair{x,y};break;}
                        }
                    Require(firstFit&&firstFit->first==value.invPosX&&firstFit->second==value.invPosY,
                        "Original FindInventorySlot differs from independent row-major first-empty footprint oracle");
                    auto expected=before.cells;std::size_t occupied{};
                    for(std::int32_t y=0;y<value.invSlotsY;++y)for(std::int32_t x=0;x<value.invSlotsX;++x) {
                        const auto slot=static_cast<std::size_t>((value.invPosY+y)*after.columns+value.invPosX+x);
                        Require(expected[slot]==0u,"Original accepted pickup overwrote an occupied inventory slot");
                        expected[slot]=1u;++occupied;
                    }
                    Require(occupied==1u&&expected==after.cells,
                        "First Training lockpick did not fill its actual one-cell footprint while preserving every other grid Byte");
                    outcome="original-player-transfer-grid-placed";
                }else {
                    Require(row.after.invPosX==row.before.invPosX&&row.after.invPosY==row.before.invPosY,
                        "Direct Frob manufactured player-only inventory placement");
                    outcome="original-transfer";
                }
                ++transferred;
            }else if(row.after.removed&&row.after.deleteMe) {
                Require(owned==row.inventoryAfter.end()&&row.inventoryAfter.size()==row.inventoryBefore.size(),
                    "Merged lockpick retained a duplicate world/inventory node");
                std::size_t changed{};
                for(std::size_t n=0u;n<row.inventoryBefore.size();++n) {
                    const auto& prior=row.inventoryBefore[n];const auto& next=row.inventoryAfter[n];
                    if(prior.classPath!=row.before.classPath)Require(prior==next,
                        "Same-class pickup merge changed an unrelated original inventory node");
                    else {
                        auto expected=prior;
                        Require(prior.numCopies.has_value()&&prior.maxCopies.has_value()&&row.before.numCopies.has_value(),
                            "Same-class merge lacks its actual original optional copy/capacity fields");
                        const auto copies=std::int64_t{*prior.numCopies}+std::int64_t{*row.before.numCopies};
                        Require(copies>=0&&copies<=std::numeric_limits<std::int32_t>::max()&&
                            *prior.maxCopies>0&&copies<=*prior.maxCopies,"Merged copy count exceeds the original item's actual capacity");
                        expected.numCopies=static_cast<std::int32_t>(copies);
                        Require(next==expected,"Original same-class merge did not retain node identity/fields and add actual NumCopies");++changed;
                    }
                }
                Require(changed==1u,"Consumed crate lockpick lacks one actual preexisting same-class merged node");
                if(viaPlayer)Require(row.gridBefore==row.gridAfter&&row.playerClockBefore==row.playerClockAfter,
                    "Original same-class merge changed the existing grid/placement or replayed player pickup animation");
                // A consumed merge target may remain in FrobTarget until the
                // original deletion/reference cleanup; do not fabricate clear.
                ++merged;outcome=viaPlayer?"original-player-merge-grid-preserved":"original-merge";
            }else {
                // Discovery must preserve a genuine returned-but-not-collected
                // branch rather than calling the diagnostic a completed pickup.
                ++other;outcome="returned-other-no-collection-proof";
            }
            printItem(row.after,"after",i);
            for(std::size_t n=0u;n<row.inventoryAfter.size();++n)printItem(row.inventoryAfter[n],"inventory-after",i,n);
            ++captured;
        }else Require(i+1u==observations.size()&&!world.execution.passed()&&!world.execution.committed,
            "Training collection continued after an incomplete callback/observation or fabricated acceptance");
        if(row.returned)++returned;
        std::cout<<"TRAINING LOOT command="<<i<<" damageCommand="<<row.damageCommandIndex<<" birth="<<row.birthIndex
            <<" actor="<<std::quoted(row.actorPath)<<" player="<<std::quoted(row.playerPath)
            <<" game="<<std::quoted(row.gameInfoPath)<<" function="<<std::quoted(row.functionPath)
            <<" playerState="<<std::quoted(row.playerStatePath)<<" entered="<<row.entered<<" returned="<<row.returned
            <<" captured="<<row.captured<<" begun="<<row.worldBegun<<" startup="<<row.worldStartup
            <<" paired="<<row.playerPaired<<" isPawn="<<row.isPawn<<" isPlayer="<<row.isPlayer<<" playerHealth="<<row.playerHealth
            <<" playerRightClick="<<row.playerRightClick<<" targetBefore="<<std::quoted(row.targetBefore)
            <<" targetAfter="<<std::quoted(row.captured?row.targetAfter:std::string{"<uncaptured>"})
            <<" inventoryBefore="<<row.inventoryBefore.size()<<" inventoryAfter="<<row.inventoryAfter.size()
            <<" writesBefore="<<row.writesBefore<<" writesAfter="<<row.writesAfter<<" outcome="<<outcome<<'\n';
    }
    if(observations.size()<births.size())Require(!world.execution.passed()&&!world.execution.committed,
        "Startup skipped requested surviving loot Frob attempts and reported completion");
    std::cout<<"TRAINING LOOT OBSERVATIONS commands="<<observations.size()<<" entered="<<entered<<" returned="<<returned
        <<" captured="<<captured<<" transferred="<<transferred<<" merged="<<merged<<" otherReturned="<<other
        <<"; actual original same-root pre-pair "<<(viaPlayer?"ParseRightClick with supplied real member target":"Frob")
        <<" prefix only. "<<(viaPlayer?"Grid/placement outcomes require captured accepted transfer or merge above. ":
            "NOT selected-player input or inventory-grid placement. ")
        <<"NOT controller reach/occlusion, paired pickup audio, falling simulation, saved/visible loot or published gameplay.\n";
}
