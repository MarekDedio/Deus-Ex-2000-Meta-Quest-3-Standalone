namespace {
const std::shared_ptr<QuestVr::QuestInputSource>& CurrentQuestInputSource(std::uint64_t identity) {
    static_cast<void>(NativeSessionViewport(identity));
    const auto found=std::find_if(persistentQuestInputSources.begin(),persistentQuestInputSources.end(),
        [&](const auto& binding){return binding.viewport==identity;});
    if(found==persistentQuestInputSources.end()||!found->source||
        !persistentNativeClient.MatchesLifetime(found->source->Lifetime())||
        !found->source->Matches(persistentNativeClient.ClientIdentity(),identity))
        throw std::runtime_error("Native Quest viewport has no current bound input observation source");
    return found->source;
}
}
void BindPortableRuntimeQuestInputSource(std::uint64_t identity,std::shared_ptr<QuestVr::QuestInputSource> source) {
    static_cast<void>(NativeSessionViewport(identity));
    if(!source||!persistentNativeClient.MatchesLifetime(source->Lifetime())||
        !source->Matches(persistentNativeClient.ClientIdentity(),identity))
        throw std::runtime_error("Quest input source does not own this exact client/viewport lifetime");
    for(const auto& binding:persistentQuestInputSources)if(binding.viewport==identity) {
        if(binding.source!=source)throw std::runtime_error("Quest input source replacement requires client teardown");
        return;
    }
    if(persistentQuestInputSources.size()>=QuestVr::NativeClientRegistry::MaximumViewports)
        throw std::runtime_error("Quest input source inventory exceeds viewport capacity");
    PortableActorVmHost host;
    const auto capacity=persistentQuestInputSources.size()+1u;
    host.ValidateAdditionalPackageBudget(capacity*sizeof(RuntimeQuestInputBinding)+QuestVr::QuestInputSource::RetainedBytes());
    auto staged=persistentQuestInputSources;staged.reserve(capacity);staged.push_back({identity,source});
    host.ValidateAdditionalPackageBudget(staged.capacity()*sizeof(RuntimeQuestInputBinding)+QuestVr::QuestInputSource::RetainedBytes());
    // Hardware can revoke between validation and publish. No mutation before
    // this last check; subsequent source reads also revalidate the token.
    if(!persistentNativeClient.MatchesLifetime(source->Lifetime()))
        throw std::runtime_error("Quest input source revoked before binding publication");
    persistentQuestInputSources.swap(staged);
}
QuestVr::QuestInputSource::Snapshot ReadPortableRuntimeQuestInputSource(std::uint64_t identity) {
    return CurrentQuestInputSource(identity)->Read();
}
QuestVr::QuestInputSource::Batch StagePortableRuntimeQuestInputSource(std::uint64_t identity,
    const QuestVr::QuestInputCursor& cursor,bool reset) {
    PortableActorVmHost host;host.ValidateAdditionalPackageBudget(sizeof(QuestVr::QuestInputSource::Batch));
    return CurrentQuestInputSource(identity)->Stage(cursor,reset);
}
