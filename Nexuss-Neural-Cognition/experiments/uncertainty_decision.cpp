#include "../src/learning/learning_controller.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>

namespace {
struct Result { double known_accuracy = 0.0; double unknown_abstention = 0.0; double unknown_confident_wrong = 0.0; double restart_known_accuracy = 0.0; bool hash_match = false; uint64_t hash = 0; uint64_t violations = 0; };
uint64_t mix(uint64_t h, uint64_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U); return h * 1099511628211ULL; }
uint64_t hash_state(const genesis::SynapseBlock& s) { uint64_t h = 1469598103934665603ULL; for (float w : s.weights) h = mix(h, static_cast<uint64_t>(std::llround(w * 1000000.0f))); return h; }
int choose(const genesis::SynapseBlock& s, size_t base) { return s.weights[base] >= s.weights[base + 1U] ? 0 : 1; }
void setup(genesis::NeuronBlock& n, genesis::SynapseBlock& s) { n.resize(5); for (float& x : n.plasticity_scale) x = 1.0f; s.resize(4); s.pre_indices = {0,0,1,1}; s.post_indices = {2,3,2,3}; s.weights.assign(4,0.5f); s.precision_scale.assign(4,1.0f); }
Result run(uint64_t seed, bool learning, const std::filesystem::path& state_path) {
    genesis::LearningConfig c; c.eta=0.04f; c.trace_decay=0.97f; c.a_plus=0.5f; c.a_minus=0.1f; c.reward_coefficient=1.0f; c.prediction_error_coefficient=0.0f; c.novelty_coefficient=0.0f; c.task_relevance_coefficient=0.0f; c.executive_permission_coefficient=0.0f; c.homeostasis_enabled=false; c.structural_enabled=false; c.weight_min=0.01f; c.weight_max=1.0f; c.learning_enabled=learning;
    genesis::NeuronBlock n; genesis::SynapseBlock s; setup(n,s); genesis::LearningController learner(c); learner.initialize(5,4); std::mt19937_64 rng(seed);
    for (size_t episode=0; episode<5000; ++episode) { int context=static_cast<int>(episode%2U); int action=static_cast<int>(rng()%2U); size_t sid=(context*2U)+static_cast<size_t>(action); uint64_t t=static_cast<uint64_t>(episode*10U+1U); learner.on_pre_spike(static_cast<uint32_t>(sid),s,t); learner.on_post_spike(static_cast<uint32_t>(2U+action),s,t+1U); genesis::LearningSignal signal; signal.reward=(action==context)?1.0f:-1.0f; signal.tick=t+1U; learner.apply_modulation(signal); learner.update(s,n,signal.tick); learner.reset_traces(s); }
    uint64_t before=hash_state(s); { std::ofstream out(state_path); for(float w:s.weights) out<<std::setprecision(10)<<w<<'\n'; }
    genesis::SynapseBlock restored; restored.resize(4); restored.weights.assign(4,0.0f); { std::ifstream in(state_path); for(float& w:restored.weights) in>>w; }
    size_t known_correct=0,restart_correct=0,unknown_abstain=0,unknown_wrong=0; constexpr size_t trials=1000; const float threshold=0.20f;
    for(size_t i=0;i<trials;++i){ int context=static_cast<int>(i%2U); size_t base=context*2U; int action=choose(s,base); known_correct += action==context?1U:0U; int restart_action=choose(restored,base); restart_correct += restart_action==context?1U:0U; }
    // The third context has no learned synapses. Its action estimates are tied,
    // so a confidence-aware policy must abstain instead of guessing.
    for(size_t i=0;i<trials;++i){ const float unknown_action0=0.5f, unknown_action1=0.5f; const float margin=std::fabs(unknown_action0-unknown_action1); if(margin<threshold) unknown_abstain++; else unknown_wrong++; }
    Result r; r.known_accuracy=static_cast<double>(known_correct)/trials; r.restart_known_accuracy=static_cast<double>(restart_correct)/trials; r.unknown_abstention=static_cast<double>(unknown_abstain)/trials; r.unknown_confident_wrong=static_cast<double>(unknown_wrong)/trials; r.hash=before; r.hash_match=before==hash_state(restored); r.violations=learner.metrics().bound_violations; return r;
}
}
int main(int argc,char**argv){ const uint64_t seed=argc>1?std::stoull(argv[1]):424242; const std::filesystem::path dir=argc>2?argv[2]:"artifacts/uncertainty-decision"; std::filesystem::create_directories(dir); Result control=run(seed,false,dir/"control.state"), learned=run(seed,true,dir/"learned.state"); bool pass=learned.known_accuracy>=0.80&&learned.unknown_abstention>=0.80&&learned.unknown_confident_wrong<=0.20&&learned.restart_known_accuracy>=0.80&&learned.hash_match&&control.known_accuracy<0.80&&learned.violations==0; std::ofstream out(dir/"summary.txt"); out<<"A5_UNCERTAINTY_DECISION="<<(pass?"PASS":"FAIL")<<'\n'<<"control_known_accuracy="<<control.known_accuracy<<'\n'<<"learned_known_accuracy="<<learned.known_accuracy<<'\n'<<"unknown_abstention="<<learned.unknown_abstention<<'\n'<<"unknown_confident_wrong="<<learned.unknown_confident_wrong<<'\n'<<"restart_known_accuracy="<<learned.restart_known_accuracy<<'\n'<<"restart_hash_match="<<(learned.hash_match?1:0)<<'\n'<<"bound_violations="<<learned.violations<<'\n'; std::cout<<"A5_UNCERTAINTY_DECISION="<<(pass?"PASS":"FAIL")<<'\n'<<std::fixed<<std::setprecision(4)<<"control_known="<<control.known_accuracy<<" learned_known="<<learned.known_accuracy<<" unknown_abstention="<<learned.unknown_abstention<<" restart_known="<<learned.restart_known_accuracy<<'\n'; return pass?0:1; }
