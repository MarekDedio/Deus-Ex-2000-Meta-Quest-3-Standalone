#include "retained_bank_publication.h"

#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
std::size_t checks{};
void Require(bool value,const char* description) {
    ++checks;
    if (!value) throw std::runtime_error(description);
}
struct Atlas {
    unsigned id{};
    std::vector<unsigned>* releases{};
    ~Atlas() { if (releases) releases->push_back(id); }
};
struct Bank {
    unsigned id{};
    std::deque<float> uniforms;
    std::shared_ptr<Atlas> atlas;
    float moverX{};
    std::vector<unsigned>* releases{};
    ~Bank() { if (releases) releases->push_back(id); }
};
using Publication=QuestVr::RetainedBankPublication<Bank>;
std::unique_ptr<Bank> Make(unsigned id,std::shared_ptr<Atlas> atlas,std::vector<unsigned>& releases) {
    auto bank=std::make_unique<Bank>();
    bank->id=id;
    bank->atlas=std::move(atlas);
    bank->releases=&releases;
    bank->uniforms.push_back(static_cast<float>(id));
    return bank;
}

void RetentionAtomicityAndAddresses() {
    std::vector<unsigned> releasedBanks,releasedAtlases;
    auto atlas=std::make_shared<Atlas>();
    atlas->id=1; atlas->releases=&releasedAtlases;
    Publication publication;
    const QuestVr::RetainedBankKey first{1,3,1};
    const auto initial=publication.Begin(Make(1,atlas,releasedBanks),first);
    auto* initialAddress=publication.Staged();
    const auto* uniformAddress=&initialAddress->uniforms.front();
    Require(!publication.Active(),"Initial partial bank became visible");
    Require(!publication.Commit(initial,first),"Unready initial bank published");
    Require(publication.MarkReady(initial) && publication.Commit(initial,first),"Complete initial bank refused");
    Require(publication.Active()==initialAddress && &publication.Active()->uniforms.front()==uniformAddress,
            "Publication moved a renderer/uniform address");
    const QuestVr::RetainedBankKey second{1,4,1};
    const auto update=publication.Begin(Make(2,atlas,releasedBanks),second);
    for (unsigned chunk=0;chunk<200;++chunk) publication.Staged()->uniforms.push_back(static_cast<float>(chunk));
    Require(publication.Active()->id==1 && releasedBanks.empty(),"Incremental stage retired the visible bank");
    Require(!publication.Commit(update,second) && publication.Active()->id==1,"Partial replacement became visible");
    auto* replacement=publication.Staged();
    const auto* replacementUniform=&replacement->uniforms.front();
    replacement->moverX=56.0f; // Authoritative open/idle snapshot, not old closed pose.
    Require(publication.MarkReady(update) && publication.Commit(update,second),"Complete replacement did not publish");
    Require(publication.Active()==replacement && &publication.Active()->uniforms.front()==replacementUniform,
            "Replacement renderer address was relocated");
    Require(publication.Active()->moverX==56.0f,"Replacement lost its authoritative live mover pose");
    Require(releasedBanks==std::vector<unsigned>{1} && releasedAtlases.empty(),"Atomic publish released the wrong bank or shared atlas");
    Require(!publication.Current(update) && !publication.MarkReady(update),"Committed token still controls a stage");
    publication.Clear(); atlas.reset();
    Require(releasedBanks==std::vector<unsigned>({1,2}) && releasedAtlases==std::vector<unsigned>{1},
            "Explicit teardown leaked a bank or duplicated atlas release");
}

void RevisionCancellationAndFailure() {
    std::vector<unsigned> releasedBanks,releasedAtlases;
    auto atlas=std::make_shared<Atlas>(); atlas->id=1; atlas->releases=&releasedAtlases;
    Publication publication;
    const QuestVr::RetainedBankKey original{10,20,1};
    const auto initial=publication.Begin(Make(1,atlas,releasedBanks),original);
    Require(publication.MarkReady(initial) && publication.Commit(initial,original),"Cancellation fixture failed");
    const QuestVr::RetainedBankKey replacement{10,21,1};
    const auto staged=publication.Begin(Make(2,atlas,releasedBanks),replacement);
    Require(publication.MarkReady(staged),"Replacement was not current");
    for (const QuestVr::RetainedBankKey changed : {QuestVr::RetainedBankKey{11,21,1},{10,22,1},{10,21,2}}) {
        Require(!publication.Commit(staged,changed),"Wrong scene/revision/atlas was published");
        Require(publication.Active()->id==1 && publication.Staged()->id==2,"Refused commit damaged retained visible generation");
    }
    publication.Cancel();
    Require(publication.Active()->id==1 && !publication.Staged() && releasedBanks==std::vector<unsigned>{2},
            "Cancelled/failed build erased retained actors");
    Require(!publication.Current(staged) && !publication.MarkReady(staged) && !publication.Commit(staged,replacement),
            "Cancelled worker token published a bank");
    const auto newer=publication.Begin(Make(3,atlas,releasedBanks),replacement);
    const auto newest=publication.Begin(Make(4,atlas,releasedBanks),replacement);
    Require(!publication.Current(newer) && publication.Current(newest),"Superseded asynchronous completion remained current");
    Require(!publication.MarkReady(newer) && !publication.Commit(newer,replacement),"Superseded build published");
    Require(publication.Active()->id==1 && releasedBanks==std::vector<unsigned>({2,3}),"Supersession retired visible actors");
    bool refused{};
    try { publication.Begin({},replacement); } catch (const std::runtime_error&) { refused=true; }
    Require(refused && publication.Current(newest),"Invalid replacement mutated current staged publication");
    Require(publication.MarkReady(newest) && publication.Commit(newest,replacement),"Latest complete replacement refused");
    publication.Clear(); atlas.reset();
    Require(releasedBanks==std::vector<unsigned>({2,3,1,4}) && releasedAtlases==std::vector<unsigned>{1},
            "Cancel/failure/revision cleanup leaked or double-released resources");
}

void AtlasReplacementLifetime() {
    std::vector<unsigned> releasedBanks,releasedAtlases;
    auto oldAtlas=std::make_shared<Atlas>(); oldAtlas->id=1; oldAtlas->releases=&releasedAtlases;
    std::weak_ptr<Atlas> oldHandle=oldAtlas;
    Publication publication;
    const QuestVr::RetainedBankKey original{1,1,1},replacement{1,2,2};
    const auto first=publication.Begin(Make(1,oldAtlas,releasedBanks),original);
    Require(publication.MarkReady(first) && publication.Commit(first,original),"Atlas fixture failed");
    auto newAtlas=std::make_shared<Atlas>(); newAtlas->id=2; newAtlas->releases=&releasedAtlases;
    const auto update=publication.Begin(Make(2,newAtlas,releasedBanks),replacement);
    oldAtlas.reset();
    Require(!oldHandle.expired() && publication.Active()->atlas->id==1 && publication.Staged()->atlas->id==2,
            "Uploading a new atlas retired the visible generation's layers");
    publication.Cancel();
    Require(!oldHandle.expired() && publication.Active()->atlas->id==1 && releasedAtlases.empty(),
            "Failed new-atlas geometry corrupted the old bank");
    Require(!publication.MarkReady(update),"Cancelled atlas token remained current");
    const auto retry=publication.Begin(Make(3,newAtlas,releasedBanks),replacement);
    Require(publication.MarkReady(retry) && publication.Commit(retry,replacement),"Atlas retry did not publish");
    Require(oldHandle.expired() && releasedAtlases==std::vector<unsigned>{1},"Old atlas outlived last renderer or freed too early");
    newAtlas.reset();
    Require(publication.Active()->atlas->id==2 && releasedAtlases==std::vector<unsigned>{1},"Current build alias owned the visible atlas exclusively");
    publication.Clear();
    Require(releasedAtlases==std::vector<unsigned>({1,2}),"Atlas teardown leaked or double-freed a texture generation");
}
} // namespace

int main() {
    try {
        RetentionAtomicityAndAddresses();
        RevisionCancellationAndFailure();
        AtlasReplacementLifetime();
        std::cout << "PASS: " << checks << " retained actor-bank lifecycle checks; incomplete/cancelled/stale builds invisible, "
                  << "stable renderer addresses and atlas references retained. Synthetic ownership evidence, not GPU/video acceptance.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
