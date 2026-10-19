// Actual Core.u metadata through the real runtime and original first-map
// Actor. Pure arithmetic does not warm the Level Model or publish gameplay.
void CoreRotatorAddCoverage(const std::function<void()>& unchanged){
    using Value=QuestVr::Vm::Value;using Rotation=std::array<std::int32_t,3u>;
    const std::string actor="00_Training.LevelInfo0",function="Core.Object.Add_RotatorRotator";
    const auto oracle=[](const Rotation& a,const Rotation& b){
        Rotation out;
        for(std::size_t channel=0u;channel<out.size();++channel){
            const auto x=std::bit_cast<std::uint32_t>(a[channel]),y=std::bit_cast<std::uint32_t>(b[channel]);
            std::uint32_t word{},carry{};
            for(unsigned byte=0u;byte<4u;++byte){const auto sum=((x>>(8u*byte))&255u)+((y>>(8u*byte))&255u)+carry;
                word|=(sum&255u)<<(8u*byte);carry=sum>>8u;}
            out[channel]=std::bit_cast<std::int32_t>(word);
        }
        return out;
    };
    const auto run=[&](std::vector<QuestVr::Vm::Evaluation> inputs,const Rotation& expected){
        const auto result=ExecutePortableActorFunction(actor,function,inputs);
        Require(result.passed()&&result.committed&&result.value.kind==Kind::Rotator&&result.value.rotation==expected&&
            result.writes==0u,"Actual Core rotator addition schema/value/pure-write contract failed: "+result.error);
        unchanged();
    };
    const Rotation a{std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::min(),70000};
    const Rotation b{1,std::numeric_limits<std::int32_t>::min(),-1};
    run({{Value::Rotator(a),{}},{Value::Rotator(b),{}}},oracle(a,b));
    // This per-element canonical capability must accept the original fixed
    // array, while each actual native parameter remains dimension1.
    const auto key=ReadPortableActorDeclaredScriptProperty("00_Training.DeusExMover16","Engine.Mover.KeyRot",7u);
    const auto base=Declared("00_Training.DeusExMover16","Engine.Mover.BaseRot",Kind::Rotator);
    Require(key.kind==Kind::Rotator,"Actual first Training KeyRot[7] lost canonical per-element Rotator layout");
    run({{base,{}},{key,{}}},oracle(base.rotation,key.rotation));
    for(unsigned slot=0u;slot<2u;++slot){
        std::vector<QuestVr::Vm::Evaluation> inputs{{Value::Rotator(a),{}},{Value::Rotator(b),{}}};
        inputs[slot]={Value::Vector({9,8,7}),{},QuestVr::Vm::ResultDisposition::ZeroFill,12u};
        run(inputs,slot?a:b);
        for(const auto width:{0u,1u,4u,8u,11u,13u}){
            inputs[slot].zeroFillBytes=static_cast<std::uint8_t>(width);
            const auto failed=ExecutePortableActorFunction(actor,function,inputs);
            Require(!failed.passed()&&!failed.committed&&!failed.error.empty()&&!failed.writes,
                "Actual Core native316 accepted an incomplete/oversize Rotator destination");++refusals;unchanged();
        }
        inputs[slot]={Value::Rotator(a),{},QuestVr::Vm::ResultDisposition::NoWrite};
        const auto absent=ExecutePortableActorFunction(actor,function,inputs);
        Require(!absent.passed()&&!absent.committed&&!absent.error.empty()&&!absent.writes,
            "Actual Core native316 seeded NoWrite from reflected Rotator defaults");++refusals;unchanged();
    }
    std::cout<<"FIRST TRAINING original Core316: genuine Core.u scalar/result schema, signed32 wrap, original KeyRot[8] element, "
        "exact12-byte clear versus14 partial/NoWrite refusals; pure math leaves save/revision/hash/GC/RNG unchanged. "
        "Non-Actor receiver support has separate generated-host coverage, not this Actor API witness.\n";
}
