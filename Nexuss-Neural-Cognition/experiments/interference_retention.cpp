#include "../src/learning/memory_system.h"
#include "../src/learning/learning_controller.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
namespace {
struct R{double before=0,after=0,task2=0;uint32_t replay=0;bool restart=false;uint64_t violations=0;};
void setup(genesis::NeuronBlock&n,genesis::SynapseBlock&s){n.resize(3);for(float&x:n.plasticity_scale)x=1;s.resize(2);s.pre_indices={0,0};s.post_indices={1,2};s.weights={.5f,.5f};s.precision_scale={1,1};}
int pick(const genesis::SynapseBlock&s){return s.weights[0]>=s.weights[1]?0:1;}
void update(genesis::LearningController&l,genesis::SynapseBlock&s,genesis::NeuronBlock&n,int action,float reward,uint64_t t){l.on_pre_spike((uint32_t)action,s,t);l.on_post_spike((uint32_t)(1+action),s,t+1);genesis::LearningSignal q;q.reward=reward;q.tick=t+1;l.apply_modulation(q);l.update(s,n,t+1);l.reset_traces(s);}
double eval(const genesis::SynapseBlock&s,int target){size_t good=0;for(size_t i=0;i<1000;++i)good+=(pick(s)==target)?1:0;return(double)good/1000;}
R run(uint64_t seed,bool replay,const std::filesystem::path&path){genesis::LearningConfig c;c.eta=.08f;c.trace_decay=.9f;c.a_plus=.6f;c.a_minus=.1f;c.reward_coefficient=1;c.prediction_error_coefficient=0;c.novelty_coefficient=0;c.task_relevance_coefficient=0;c.executive_permission_coefficient=0;c.homeostasis_enabled=false;c.structural_enabled=false;c.weight_min=.01f;c.weight_max=1;genesis::NeuronBlock n;genesis::SynapseBlock s;setup(n,s);genesis::LearningController l(c);l.initialize(3,2);genesis::MemoryConfig mc;mc.replay_quota_per_context=100;genesis::MemorySystem mem(seed,mc);mem.initialize_arena(3,2);uint64_t tick=1;uint32_t replay_count=0;
 for(size_t i=0;i<2000;++i){int a=(int)(i%2);update(l,s,n,a,a==0?1:-1,tick);genesis::EventHeader h{i+1,tick,1,100,(uint64_t)a,seed};auto id=mem.begin_episode(h);mem.set_active_metrics(.2f,0,.0f,a==0?1:-1);mem.close_episode(id,a==0?1:-1);tick+=5;}
 R r;r.before=eval(s,0);
 if(replay){mem.set_replay_callback([&](const genesis::MemoryEvent&e,genesis::ReplayMode){int a=(int)(e.header.payload_id%2);update(l,s,n,a,a==0?1:-1,tick++);++replay_count;});}
 for(size_t i=0;i<4000;++i){int a=(int)(i%2);update(l,s,n,a,a==0?-1:1,tick);tick+=5;if(replay&&i%20==19){auto sel=mem.select_replay({0,0,tick,std::numeric_limits<uint64_t>::max(),-std::numeric_limits<float>::infinity()},20);mem.replay(sel,genesis::ReplayMode::ExactEvent,1000);}}
 r.task2=eval(s,1);r.after=eval(s,0);mem.save(path);genesis::MemorySystem restored(seed,mc);restored.load(path);r.restart=restored.state_hash()==mem.state_hash();r.replay=replay_count;r.violations=l.metrics().bound_violations;return r;}
}
int main(int ac,char**av){uint64_t seed=ac>1?std::stoull(av[1]):424242;std::filesystem::path d=ac>2?av[2]:"artifacts/interference-retention";std::filesystem::create_directories(d);R no=run(seed,false,d/"no_replay.bin"),yes=run(seed,true,d/"replay.bin");bool pass=yes.before>=.8&&yes.after>=.8&&yes.replay>0&&yes.restart&&yes.violations==0&&no.after<yes.after;std::ofstream o(d/"summary.txt");o<<"B2_INTERFERENCE_RETENTION="<<(pass?"PASS":"FAIL")<<'\n'<<"no_replay_before="<<no.before<<'\n'<<"no_replay_after="<<no.after<<'\n'<<"replay_before="<<yes.before<<'\n'<<"replay_after="<<yes.after<<'\n'<<"replay_task2="<<yes.task2<<'\n'<<"replay_events="<<yes.replay<<'\n'<<"restart_match="<<(yes.restart?1:0)<<'\n'<<"bound_violations="<<yes.violations<<'\n';std::cout<<"B2_INTERFERENCE_RETENTION="<<(pass?"PASS":"FAIL")<<'\n'<<std::fixed<<std::setprecision(4)<<"no_replay_after="<<no.after<<" replay_after="<<yes.after<<" task2="<<yes.task2<<" replay_events="<<yes.replay<<'\n';return pass?0:1;}
