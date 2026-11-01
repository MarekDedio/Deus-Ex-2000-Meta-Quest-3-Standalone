#include "quest_process_loop.h"
#include <atomic>
#include <iostream>
#include <thread>

namespace {
using Control=QuestVr::ProcessLoopControl;
std::size_t checks{},refusals{};
void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refuse(F&& operation) {
    bool refused{};try{operation();}catch(const std::exception&){refused=true;}
    Check(refused,"Invalid process transition/observation succeeded");++refusals;
}
}
int main(){try {
    Control control,other;
    Check(control.Identity()&&other.Identity()>control.Identity(),"Process identity is absent/reused");
    Check(control.Alive()&&!control.Running()&&control.Observe()==Control::State::Ready,"Ready was not genuine live pre-loop false");
    Refuse([&]{control.LeaveExecution();});
    control.EnterExecution();Check(control.Alive()&&control.Running(),"Real entry did not publish true");
    Refuse([&]{control.EnterExecution();});
    control.LeaveExecution();Check(control.Alive()&&!control.Running()&&control.Observe()==Control::State::Stopped,"Stop confused false with revocation");
    Refuse([&]{control.EnterExecution();});Refuse([&]{control.LeaveExecution();});
    control.Revoke();control.Revoke();Check(!control.Alive(),"Revoke was not irreversible/idempotent");
    Refuse([&]{static_cast<void>(control.Observe());});Refuse([&]{static_cast<void>(control.Running());});
    Refuse([&]{control.EnterExecution();});Refuse([&]{control.LeaveExecution();});
    other.Revoke();Refuse([&]{static_cast<void>(other.Running());});
    Check(Control::RetainedBytes()>=sizeof(Control),"Process shared control storage is unaccounted");
    for(unsigned repeat=0u;repeat<64u;++repeat) {
        Control concurrent;std::atomic<bool> observed{},done{},valid{true};std::size_t reads{};
        std::thread reader([&] {
            unsigned previous{};
            while(!done.load(std::memory_order_acquire)) {
                try {
                    const auto state=concurrent.Observe();const auto ordinal=static_cast<unsigned>(state);
                    if(ordinal<previous||state==Control::State::Revoked)valid.store(false,std::memory_order_release);
                    previous=ordinal;++reads;observed.store(true,std::memory_order_release);
                }catch(const std::exception&) {
                    if(concurrent.Alive())valid.store(false,std::memory_order_release);
                    break;
                }
            }
        });
        while(!observed.load(std::memory_order_acquire))std::this_thread::yield();
        concurrent.EnterExecution();concurrent.LeaveExecution();concurrent.Revoke();
        done.store(true,std::memory_order_release);reader.join();
        Check(valid.load(std::memory_order_acquire)&&reads&&!concurrent.Alive(),"Concurrent observation revived/backtracked or lost source lifetime");
    }
    std::cout<<"PASS process loop control checks="<<checks<<" refusals="<<refusals
        <<"; real typed transition/concurrent component only, NOT Android hook timing or original startup\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL process loop control: "<<error.what()<<'\n';return 1;}}
