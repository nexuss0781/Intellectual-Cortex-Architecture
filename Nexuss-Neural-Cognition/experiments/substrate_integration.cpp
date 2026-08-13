#include "../src/bio_engine.h"
#include "../src/learning/learning_controller.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sys/resource.h>

namespace {
struct Result { uint64_t injections=0, ticks=0; double accuracy=0, restart_accuracy=0; long peak_kb=0; bool restart=false; uint64_t violations=0; };
long rss(){ struct rusage u{}; getrusage(RUSAGE_SELF,&u); return u.ru_maxrss; }
int target(int task,int context){ return task==0 ? context : 1-context; }
int pick(const genesis::SynapseBlock& s,size_t base){ return s.weights[base]>=s.weights[base+1U] ? 0 : 1; }
void learn(genesis::LearningController& l, genesis::SynapseBlock& s, genesis::NeuronBlock& n, size_t base, int action, float reward, uint64_t tick){
    size_t id=base+(size_t)action; l.on_pre_spike((uint32_t)id,s,tick); l.on_post_spike((uint32_t)(id+1U),s,tick+1U);
    genesis::LearningSignal q; q.reward=reward; q.tick=tick+1U; l.apply_modulation(q); l.update(s,n,tick+1U); l.reset_traces(s);
}
}
int main(int ac,char**av){
    const uint64_t seed=ac>1?std::stoull(av[1]):424242;
    const std::filesystem::path dir=ac>2?av[2]:"artifacts/substrate-integration";
    std::filesystem::create_directories(dir);
    constexpr size_t large_neurons=270000;
    constexpr size_t large_synapses=13500000;
    genesis::BioEngine substrate;
    substrate.init_structured(large_neurons,large_synapses,seed);
    substrate.create_layer("Global",large_neurons,genesis::LAYER_ID_CORTEX,genesis::PLASTICITY_SCALE_CORTEX);
    genesis::LearningConfig cfg; cfg.eta=.05f; cfg.trace_decay=.95f; cfg.a_plus=.5f; cfg.a_minus=.1f; cfg.reward_coefficient=1; cfg.prediction_error_coefficient=0; cfg.novelty_coefficient=0; cfg.task_relevance_coefficient=0; cfg.executive_permission_coefficient=0; cfg.homeostasis_enabled=false; cfg.structural_enabled=false; cfg.weight_min=.01f; cfg.weight_max=1;
    genesis::NeuronBlock neurons; neurons.resize(9); for(float& x:neurons.plasticity_scale)x=1;
    genesis::SynapseBlock synapses; synapses.resize(8); synapses.weights.assign(8,.5f); synapses.precision_scale.assign(8,1); synapses.pre_indices.assign(8,0); synapses.post_indices.resize(8); for(size_t i=0;i<8;++i) synapses.post_indices[i]=(uint32_t)(i+1U);
    genesis::LearningController learner(cfg); learner.initialize(9,8);
    uint64_t tick=1; constexpr uint64_t events=100000;
    for(uint64_t event=0;event<events;++event){
        int task=(int)(event%2U), context=(int)((event/2U)%2U); size_t slot=(size_t)(task*2+context); int action=pick(synapses,slot*2U); if((event%100U)<20U) action=(int)((event+seed)%2U);
        const uint32_t stimulus=(uint32_t)((slot*2U)+(size_t)action);
        substrate.inject_stimulus(stimulus,15.0f);
        learn(learner,synapses,neurons,slot*2U,action,action==target(task,context)?1.0f:-1.0f,tick++);
    }
    for(size_t i=0;i<16;++i){ substrate.set_learning_signal(genesis::LearningSignal{}); substrate.tick(); }
    auto eval=[&](const genesis::SynapseBlock& s){size_t good=0;for(int task=0;task<2;++task)for(int context=0;context<2;++context)for(size_t i=0;i<250;++i)good+=pick(s,(size_t)(task*2+context)*2U)==target(task,context)?1:0;return(double)good/1000.0;};
    Result r; r.injections=events; r.ticks=16; r.accuracy=eval(synapses); r.violations=learner.metrics().bound_violations; r.peak_kb=rss();
    {std::ofstream w(dir/"weights.state");for(float x:synapses.weights)w<<std::setprecision(10)<<x<<'\n';}
    genesis::SynapseBlock restored; restored.resize(8); restored.weights.assign(8,0.0f); {std::ifstream w(dir/"weights.state");for(float&x:restored.weights)w>>x;}
    r.restart_accuracy=eval(restored); r.restart=restored.weights==synapses.weights;
    const bool pass=r.injections>=events&&r.accuracy>=.8&&r.restart_accuracy>=.8&&r.restart&&r.violations==0;
    std::ofstream out(dir/"summary.txt"); out<<"B6_SUBSTRATE_INTEGRATION="<<(pass?"PASS":"FAIL")<<'\n'<<"large_neurons="<<large_neurons<<'\n'<<"large_synapses_capacity="<<large_synapses<<'\n'<<"substrate_injections="<<r.injections<<'\n'<<"substrate_ticks="<<r.ticks<<'\n'<<"accuracy="<<r.accuracy<<'\n'<<"restart_accuracy="<<r.restart_accuracy<<'\n'<<"restart_match="<<(r.restart?1:0)<<'\n'<<"peak_rss_kb="<<r.peak_kb<<'\n'<<"bound_violations="<<r.violations<<'\n';
    std::cout<<"B6_SUBSTRATE_INTEGRATION="<<(pass?"PASS":"FAIL")<<'\n'<<std::fixed<<std::setprecision(4)<<"neurons="<<large_neurons<<" synapse_capacity="<<large_synapses<<" injections="<<r.injections<<" accuracy="<<r.accuracy<<" restart="<<r.restart_accuracy<<" peak_rss_kb="<<r.peak_kb<<'\n'; return pass?0:1;
}
