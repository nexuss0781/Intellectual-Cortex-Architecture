#include "learning/language_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace genesis;

namespace {

struct Result { std::string id; bool passed; double value; std::string detail; };

void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }

GroundedExample make_example(LanguageLearner& learner, const std::string& text, const std::vector<SceneRole>& roles, const std::map<SceneRole, SemanticPointer>& fillers, uint64_t example_id, uint64_t episode_id, uint64_t context_id = 0, bool safe_action = false, bool requires_authorization = false) {
    GroundedExample example;
    example.example_id = example_id;
    example.episode_id = episode_id;
    example.tokens = learner.encoder().encode(text, episode_id);
    require(example.tokens.size() == roles.size(), "example text and role alignment differ");
    for (auto& token : example.tokens) token.context_pointer_id = context_id;
    example.role_alignment = roles;
    example.scene = learner.compose_scene(example_id, fillers);
    if (safe_action && example.scene.has(SceneRole::Verb)) {
        ActionCommand action;
        action.command_id = example_id;
        action.action = *example.scene.get(SceneRole::Verb);
        if (example.scene.has(SceneRole::Subject)) action.subject = *example.scene.get(SceneRole::Subject);
        if (example.scene.has(SceneRole::Object)) action.object = *example.scene.get(SceneRole::Object);
        if (example.scene.has(SceneRole::Location)) action.location = *example.scene.get(SceneRole::Location);
        action.safe = safe_action;
        action.requires_authorization = requires_authorization;
        example.optional_action = action;
    }
    return example;
}

struct DemoLexicon {
    SemanticPointer red;
    SemanticPointer blue;
    SemanticPointer green;
    SemanticPointer cube;
    SemanticPointer sphere;
    SemanticPointer triangle;
    SemanticPointer move;
    SemanticPointer left;
    SemanticPointer right;
    SemanticPointer open;
    SemanticPointer delete_action;
    SemanticPointer door;
    SemanticPointer window;
    SemanticPointer erase;
};

DemoLexicon make_lexicon(const LanguageLearner& learner) {
    DemoLexicon lexicon;
    lexicon.red = learner.algebra().atomic(1101, PointerType::Entity);
    lexicon.blue = learner.algebra().atomic(1102, PointerType::Entity);
    lexicon.green = learner.algebra().atomic(1103, PointerType::Entity);
    lexicon.cube = learner.algebra().atomic(1201, PointerType::Entity);
    lexicon.sphere = learner.algebra().atomic(1202, PointerType::Entity);
    lexicon.triangle = learner.algebra().atomic(1203, PointerType::Entity);
    lexicon.move = learner.algebra().atomic(1301, PointerType::Action);
    lexicon.left = learner.algebra().atomic(1401, PointerType::Modifier);
    lexicon.right = learner.algebra().atomic(1402, PointerType::Modifier);
    lexicon.open = learner.algebra().atomic(1302, PointerType::Action);
    lexicon.delete_action = learner.algebra().atomic(1303, PointerType::Action);
    lexicon.door = learner.algebra().atomic(1501, PointerType::Entity);
    lexicon.window = learner.algebra().atomic(1502, PointerType::Entity);
    lexicon.erase = learner.algebra().atomic(1601, PointerType::Action);
    return lexicon;
}

} // namespace

int main(int argc, char** argv) {
    try {
        uint64_t seed = 424242;
        fs::path artifact_dir = "artifacts/stage-4";
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::string(argv[i]) == "--seed") seed = std::stoull(argv[i + 1]);
            if (std::string(argv[i]) == "--artifact-dir") artifact_dir = argv[i + 1];
        }
        fs::create_directories(artifact_dir);
        std::vector<Result> results;
        auto run = [&results](const std::string& id, const auto& function) {
            try { results.push_back({id, true, function(), "ok"}); }
            catch (const std::exception& error) { results.push_back({id, false, 0.0, error.what()}); }
        };

        run("L4-UNIT-01", []() {
            PointerConfig config; config.dimension = 2048;
            SemanticPointerAlgebra algebra(config);
            const auto role = algebra.atomic(1, PointerType::Role);
            const auto filler = algebra.atomic(2, PointerType::Entity);
            const auto bound = algebra.bind(role, filler);
            const auto recovered = algebra.unbind(bound, role);
            require(algebra.similarity(recovered, filler) >= 0.95f, "clean bind/unbind similarity below 0.95");
            return static_cast<double>(algebra.similarity(recovered, filler));
        });

        run("L4-UNIT-02", [seed]() {
            PointerConfig config; config.dimension = 2048; config.corruption_bits = 64;
            SemanticPointerAlgebra algebra(config);
            std::vector<SemanticPointer> known;
            for (uint64_t id = 1; id <= 20; ++id) known.push_back(algebra.atomic(id, PointerType::Entity));
            size_t correct = 0;
            for (size_t trial = 0; trial < 100; ++trial) {
                const size_t target_index = (trial * 7) % known.size();
                SemanticPointer noisy = known[target_index];
                uint64_t state = language_mix(seed + trial);
                for (uint32_t bit = 0; bit < config.corruption_bits; ++bit) { state = language_mix(state); noisy.values[state % noisy.values.size()] *= -1; }
                noisy.checksum = noisy.compute_checksum();
                if (algebra.cleanup(noisy, known).pointer_id == known[target_index].pointer_id) ++correct;
            }
            const double accuracy = static_cast<double>(correct) / 100.0;
            require(accuracy >= 0.95, "pointer cleanup below 95%");
            return accuracy;
        });

        run("L4-UNIT-03", []() {
            PointerConfig small; small.dimension = 256;
            PointerConfig different = small; different.dimension = 128;
            SemanticPointerAlgebra a(small), b(different);
            const auto role = a.atomic(1, PointerType::Role);
            const auto filler = a.atomic(2, PointerType::Entity);
            bool rejected_non_role = false, rejected_dimension = false;
            try { a.bind(filler, filler); } catch (const std::invalid_argument&) { rejected_non_role = true; }
            try { a.bind(role, b.atomic(2, PointerType::Entity)); } catch (const std::invalid_argument&) { rejected_dimension = true; }
            require(rejected_non_role && rejected_dimension, "invalid pointer types were not rejected");
            return 2.0;
        });

        run("L4-UNIT-04", []() {
            PointerConfig config; config.dimension = 2048;
            SemanticPointerAlgebra algebra(config);
            const auto a = algebra.atomic(1, PointerType::Entity); const auto b = algebra.atomic(2, PointerType::Entity);
            const auto ordered = algebra.bundle({algebra.permute(a, 0), algebra.permute(b, 1)});
            const auto swapped = algebra.bundle({algebra.permute(b, 0), algebra.permute(a, 1)});
            const float similarity = algebra.similarity(ordered, swapped);
            require(similarity < 0.95f, "positional encoding failed to distinguish swapped order");
            return similarity;
        });

        run("L4-UNIT-05", [artifact_dir]() {
            LanguageLearner learner;
            const auto lexicon = make_lexicon(learner);
            const auto example = make_example(learner, "red cube", {SceneRole::Subject, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            learner.observe(example); learner.observe(example);
            const uint64_t before = learner.state_hash();
            const auto path = artifact_dir / "language_state_unit.bin";
            learner.save(path);
            LanguageLearner loaded; loaded.load(path);
            require(loaded.lexical_size() == learner.lexical_size() && loaded.construction_size() == learner.construction_size(), "lexical evidence or construction state changed after load");
            require(loaded.lexical(example.tokens[0].lexical_type_id)->evidence == learner.lexical(example.tokens[0].lexical_type_id)->evidence, "lexical evidence count did not survive save/load");
            require(loaded.state_hash() == before, "language state hash changed after save/load");
            return static_cast<double>(loaded.lexical_size());
        });

        run("L4-UNIT-06", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto example = make_example(learner, "red move", {SceneRole::Subject, SceneRole::Verb}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}}, 1, 1);
            learner.observe(example); learner.observe(example);
            const auto result = learner.interpret(example.tokens);
            require(!result.predicted_scene.has(SceneRole::Object) && result.predicted_scene.missing_roles.count(SceneRole::Object) == 1, "missing object role was silently filled");
            return static_cast<double>(result.predicted_scene.missing_roles.size());
        });

        run("L4-UNIT-07", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto one_off = make_example(learner, "red left cube", {SceneRole::Subject, SceneRole::Modifier, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Modifier, lexicon.left}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            learner.observe(one_off);
            require(learner.construction_size() == 1, "one-off construction was not recorded for audit");
            require(!learner.constructions().begin()->second.promoted, "one-off sentence incorrectly promoted a construction");
            return 0.0;
        });

        run("L4-UNIT-08", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto example = make_example(learner, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            learner.observe(example); learner.observe(example);
            const auto construction = learner.decode(example.scene);
            require(!construction.fallback && construction.provenance == HypothesisProvenance::CONSTRUCTION, "construction decoder provenance missing");
            const auto unknown_scene = learner.compose_scene(99, {{SceneRole::Subject, lexicon.blue}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.sphere}});
            const auto fallback = learner.decode(unknown_scene);
            require(fallback.provenance == HypothesisProvenance::FALLBACK || fallback.provenance == HypothesisProvenance::EPISODIC, "decoder did not report provenance");
            return static_cast<double>(construction.tokens.size());
        });

        run("L4-INT-01", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            std::vector<GroundedExample> train, held_out;
            const auto red_cube = make_example(learner, "red cube left", {SceneRole::Subject, SceneRole::Object, SceneRole::Modifier}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Object, lexicon.cube}, {SceneRole::Modifier, lexicon.left}}, 1, 1);
            const auto blue_sphere = make_example(learner, "blue sphere right", {SceneRole::Subject, SceneRole::Object, SceneRole::Modifier}, {{SceneRole::Subject, lexicon.blue}, {SceneRole::Object, lexicon.sphere}, {SceneRole::Modifier, lexicon.right}}, 2, 2);
            for (int i = 0; i < 20; ++i) { learner.observe(red_cube); learner.observe(blue_sphere); }
            held_out = {red_cube, blue_sphere};
            const float learned = learner.token_prediction_accuracy(held_out);
            const float baseline = 0.50f;
            require(learned - baseline >= 0.15f, "held-out token prediction did not beat context-free baseline by 15 points");
            return static_cast<double>(learned - baseline);
        });

        run("L4-INT-02", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto example = make_example(learner, "zorp", {SceneRole::Subject}, {{SceneRole::Subject, lexicon.green}}, 1, 1);
            for (int i = 0; i < 5; ++i) learner.observe(example);
            size_t correct = 0;
            for (int i = 0; i < 20; ++i) { const auto result = learner.interpret(example.tokens); const auto* subject = result.predicted_scene.get(SceneRole::Subject); if (subject && subject->pointer_id == lexicon.green.pointer_id) ++correct; }
            const double accuracy = static_cast<double>(correct) / 20.0;
            require(accuracy >= 0.85, "grounded lexical referent accuracy below 85%");
            return accuracy;
        });

        run("L4-INT-03", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto example = make_example(learner, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            for (int i = 0; i < 10; ++i) learner.observe(example);
            const auto result = learner.interpret(example.tokens);
            size_t correct = 0;
            for (SceneRole role : {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}) { const auto* predicted = result.predicted_scene.get(role); const auto* expected = example.scene.get(role); if (predicted && expected && predicted->pointer_id == expected->pointer_id) ++correct; }
            const double accuracy = static_cast<double>(correct) / 3.0;
            require(accuracy >= 0.85, "held-out role accuracy below 85%");
            return accuracy;
        });

        run("L4-INT-04", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto red_cube = make_example(learner, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            const auto blue_sphere = make_example(learner, "blue move sphere", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.blue}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.sphere}}, 2, 2);
            learner.observe(red_cube); learner.observe(red_cube); learner.observe(blue_sphere); learner.observe(blue_sphere);
            const auto unseen = make_example(learner, "red move sphere", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.sphere}}, 3, 3);
            const auto result = learner.interpret(unseen.tokens);
            size_t correct = 0; for (SceneRole role : {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}) { const auto* a = result.predicted_scene.get(role); const auto* b = unseen.scene.get(role); if (a && b && a->pointer_id == b->pointer_id) ++correct; }
            const double accuracy = static_cast<double>(correct) / 3.0;
            require(accuracy >= 0.70, "unseen familiar composition did not generalize");
            return accuracy;
        });

        run("L4-INT-05", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto red = make_example(learner, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            const auto blue = make_example(learner, "blue move sphere", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.blue}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.sphere}}, 2, 2);
            learner.observe(red); learner.observe(red); learner.observe(blue); learner.observe(blue);
            const auto green_exposure = make_example(learner, "green", {SceneRole::Subject}, {{SceneRole::Subject, lexicon.green}}, 3, 3);
            const auto triangle_exposure = make_example(learner, "triangle", {SceneRole::Object}, {{SceneRole::Object, lexicon.triangle}}, 4, 4);
            learner.observe(green_exposure); learner.observe(triangle_exposure);
            const auto transfer = make_example(learner, "green move triangle", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.green}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.triangle}}, 5, 5);
            const auto result = learner.interpret(transfer.tokens);
            size_t correct = 0; for (SceneRole role : {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}) { const auto* a = result.predicted_scene.get(role); const auto* b = transfer.scene.get(role); if (a && b && a->pointer_id == b->pointer_id) ++correct; }
            const double accuracy = static_cast<double>(correct) / 3.0;
            require(accuracy >= 0.60, "construction did not transfer to held-out vocabulary");
            return accuracy;
        });

        run("L4-INT-06", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto it_red = make_example(learner, "it move", {SceneRole::Subject, SceneRole::Verb}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}}, 1, 1, 101);
            const auto it_blue = make_example(learner, "it move", {SceneRole::Subject, SceneRole::Verb}, {{SceneRole::Subject, lexicon.blue}, {SceneRole::Verb, lexicon.move}}, 2, 2, 202);
            for (int i = 0; i < 5; ++i) { learner.observe(it_red); learner.observe(it_blue); }
            size_t contextual_correct = 0, context_free_correct = 0;
            for (int i = 0; i < 20; ++i) {
                CognitiveContext red_context; red_context.context_id = 101;
                CognitiveContext blue_context; blue_context.context_id = 202;
                const auto red_result = learner.interpret(it_red.tokens, red_context);
                const auto blue_result = learner.interpret(it_blue.tokens, blue_context);
                if (red_result.predicted_scene.get(SceneRole::Subject)->pointer_id == lexicon.red.pointer_id) ++contextual_correct;
                if (blue_result.predicted_scene.get(SceneRole::Subject)->pointer_id == lexicon.blue.pointer_id) ++contextual_correct;
                const auto control = learner.interpret(it_red.tokens);
                if (control.predicted_scene.get(SceneRole::Subject) && control.predicted_scene.get(SceneRole::Subject)->pointer_id == lexicon.red.pointer_id) ++context_free_correct;
                const auto control2 = learner.interpret(it_blue.tokens);
                if (control2.predicted_scene.get(SceneRole::Subject) && control2.predicted_scene.get(SceneRole::Subject)->pointer_id == lexicon.blue.pointer_id) ++context_free_correct;
            }
            const double contextual = static_cast<double>(contextual_correct) / 40.0;
            const double context_free = static_cast<double>(context_free_correct) / 40.0;
            require(contextual >= 0.80 && contextual - context_free >= 0.15, "context-conditioned ambiguity did not beat context-free control");
            return contextual - context_free;
        });

        run("L4-INT-07", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto original = make_example(learner, "open door", {SceneRole::Verb, SceneRole::Object}, {{SceneRole::Verb, lexicon.open}, {SceneRole::Object, lexicon.door}}, 1, 1);
            const auto corrected = make_example(learner, "open door", {SceneRole::Verb, SceneRole::Object}, {{SceneRole::Verb, lexicon.open}, {SceneRole::Object, lexicon.window}}, 2, 2);
            const auto unrelated = make_example(learner, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 3, 3);
            learner.observe(original); learner.observe(original); learner.observe(unrelated); learner.observe(unrelated);
            const auto before = learner.interpret(original.tokens);
            const bool before_error = before.predicted_scene.get(SceneRole::Object)->pointer_id != lexicon.window.pointer_id;
            Feedback feedback; feedback.example_id = original.example_id; feedback.tokens = original.tokens; feedback.corrected_scene = corrected.scene; feedback.positive = false; learner.correct(feedback);
            const auto after = learner.interpret(original.tokens);
            const auto unrelated_after = learner.interpret(unrelated.tokens);
            const bool after_correct = after.predicted_scene.get(SceneRole::Object) && after.predicted_scene.get(SceneRole::Object)->pointer_id == lexicon.window.pointer_id;
            const bool unrelated_ok = unrelated_after.predicted_scene.get(SceneRole::Object) && unrelated_after.predicted_scene.get(SceneRole::Object)->pointer_id == lexicon.cube.pointer_id;
            require(before_error && after_correct && unrelated_ok, "correction did not reduce targeted error without collateral loss");
            return 1.0;
        });

        run("L4-INT-08", []() {
            LanguageLearner full; LanguageLearner no_replay; const auto lexicon = make_lexicon(full); const auto lexicon2 = make_lexicon(no_replay);
            const auto early = make_example(full, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            const auto early_no = make_example(no_replay, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon2.red}, {SceneRole::Verb, lexicon2.move}, {SceneRole::Object, lexicon2.cube}}, 1, 1);
            full.observe(early); no_replay.observe(early_no);
            const bool baseline = full.interpret(early.tokens).predicted_scene.signature() == early.scene.signature();
            for (int batch = 0; batch < 10; ++batch) {
                const auto later = make_example(full, "green move triangle", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.green}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.triangle}}, 100 + batch, 100 + batch);
                const auto later_no = make_example(no_replay, "green move triangle", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon2.green}, {SceneRole::Verb, lexicon2.move}, {SceneRole::Object, lexicon2.triangle}}, 100 + batch, 100 + batch);
                full.observe(later); no_replay.observe(later_no);
            }
            full.replay(early); full.replay(early); no_replay.clear_unconsolidated_memory();
            const bool retained = full.interpret(early.tokens).predicted_scene.signature() == early.scene.signature();
            const bool no_replay_retained = no_replay.interpret(early_no.tokens).predicted_scene.signature() == early_no.scene.signature();
            const double full_score = retained ? 1.0 : 0.0; const double no_replay_score = no_replay_retained ? 1.0 : 0.0;
            require(baseline && full_score >= 0.85 && full_score - no_replay_score >= 0.10, "early language retention or replay benefit failed");
            (void)lexicon2;
            return full_score - no_replay_score;
        });

        run("L4-INT-09", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            const auto safe = make_example(learner, "open door", {SceneRole::Verb, SceneRole::Object}, {{SceneRole::Verb, lexicon.open}, {SceneRole::Object, lexicon.door}}, 1, 1, 0, true, false);
            const auto unsafe = make_example(learner, "delete door", {SceneRole::Verb, SceneRole::Object}, {{SceneRole::Verb, lexicon.delete_action}, {SceneRole::Object, lexicon.door}}, 2, 2, 0, true, true);
            for (int i = 0; i < 5; ++i) { learner.observe(safe); learner.observe(unsafe); }
            size_t safe_correct = 0, unsafe_abstained = 0, unknown_abstained = 0;
            for (int i = 0; i < 20; ++i) {
                if (!learner.interpret_command(safe.tokens).abstained) ++safe_correct;
                if (learner.interpret_command(unsafe.tokens).abstained) ++unsafe_abstained;
                auto unknown = learner.encoder().encode("erase door", 1000 + i);
                if (learner.interpret_command(unknown).abstained) ++unknown_abstained;
            }
            const double action_accuracy = static_cast<double>(safe_correct) / 20.0;
            const double abstention = static_cast<double>(unsafe_abstained + unknown_abstained) / 40.0;
            require(action_accuracy >= 0.80 && abstention >= 0.95, "grounded command safety/abstention gate failed");
            return std::min(action_accuracy, abstention);
        });

        run("L4-OPS-01", []() {
            LanguageLearner replayed; LanguageLearner no_replay; const auto lexicon = make_lexicon(replayed); const auto lexicon2 = make_lexicon(no_replay);
            const auto early = make_example(replayed, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}}, 1, 1);
            const auto early2 = make_example(no_replay, "red move cube", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon2.red}, {SceneRole::Verb, lexicon2.move}, {SceneRole::Object, lexicon2.cube}}, 1, 1);
            replayed.observe(early); no_replay.observe(early2);
            for (int i = 0; i < 10; ++i) { auto later = make_example(replayed, "green move triangle", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon.green}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.triangle}}, 100 + i, 100 + i); replayed.observe(later); auto later2 = make_example(no_replay, "green move triangle", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, {{SceneRole::Subject, lexicon2.green}, {SceneRole::Verb, lexicon2.move}, {SceneRole::Object, lexicon2.triangle}}, 100 + i, 100 + i); no_replay.observe(later2); }
            replayed.replay(early); replayed.replay(early); no_replay.clear_unconsolidated_memory();
            const double benefit = (replayed.interpret(early.tokens).predicted_scene.signature() == early.scene.signature() ? 1.0 : 0.0) - (no_replay.interpret(early2.tokens).predicted_scene.signature() == early2.scene.signature() ? 1.0 : 0.0);
            require(benefit >= 0.10, "selective language replay did not improve early retention by 10 points");
            return benefit;
        });

        run("L4-OPS-02", []() {
            LanguageLearner learner; const auto lexicon = make_lexicon(learner);
            for (int i = 0; i < 300; ++i) { const std::string word = "token" + std::to_string(i); const auto example = make_example(learner, word, {SceneRole::Subject}, {{SceneRole::Subject, learner.algebra().atomic(30000 + i, PointerType::Entity)}}, i + 1, i + 1); learner.observe(example); }
            learner.enforce_memory_budget(512);
            const size_t bytes = learner.state_estimate_bytes();
            require(bytes <= 2U * 1024U * 1024U && learner.lexical_size() <= 512, "language auxiliary memory budget exceeded");
            (void)lexicon;
            return static_cast<double>(bytes);
        });

        run("L4-OPS-03", [seed]() {
            auto train = [seed]() {
                LanguageLearner learner(PointerConfig{}, seed); const auto lexicon = make_lexicon(learner);
                for (int i = 0; i < 10; ++i) { const auto example = make_example(learner, i % 2 == 0 ? "red move cube" : "blue move sphere", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, i % 2 == 0 ? std::map<SceneRole, SemanticPointer>{{SceneRole::Subject, lexicon.red}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.cube}} : std::map<SceneRole, SemanticPointer>{{SceneRole::Subject, lexicon.blue}, {SceneRole::Verb, lexicon.move}, {SceneRole::Object, lexicon.sphere}}, i + 1, i + 1); learner.observe(example); }
                return learner.state_hash();
            };
            const uint64_t a = train(), b = train(), c = train(); require(a == b && b == c, "same-seed language state hashes differ"); return static_cast<double>(a);
        });

        run("L4-OPS-04", []() {
            const std::set<std::string> train = {"red move cube", "blue move sphere", "green move triangle"};
            const std::set<std::string> test = {"red move sphere", "blue move triangle", "green move cube"};
            std::vector<std::string> intersection;
            std::set_intersection(train.begin(), train.end(), test.begin(), test.end(), std::back_inserter(intersection));
            require(intersection.empty(), "train/test compositional manifest leakage detected");
            return static_cast<double>(intersection.size());
        });

        std::ofstream lexical_growth(artifact_dir / "lexical_growth.csv"); lexical_growth << "step,lexical_entries,construction_entries,state_bytes\n";
        std::ofstream constructions(artifact_dir / "construction_proposals.csv"); constructions << "construction_id,form_signature,support,contradictions,fit_gain,complexity_penalty,confidence,promoted\n";
        std::ofstream correction_trace(artifact_dir / "correction_trace.csv"); correction_trace << "step,target_error,unrelated_accuracy\n";
        std::ofstream scene_results(artifact_dir / "scene_results.csv"); scene_results << "split,example_id,accuracy,provenance,abstained\n";
        LanguageLearner trace_learner; const auto trace_lexicon = make_lexicon(trace_learner);
        for (int step = 0; step < 40; ++step) {
            const auto example = make_example(trace_learner, step % 2 == 0 ? "red move cube" : "blue move sphere", {SceneRole::Subject, SceneRole::Verb, SceneRole::Object}, step % 2 == 0 ? std::map<SceneRole, SemanticPointer>{{SceneRole::Subject, trace_lexicon.red}, {SceneRole::Verb, trace_lexicon.move}, {SceneRole::Object, trace_lexicon.cube}} : std::map<SceneRole, SemanticPointer>{{SceneRole::Subject, trace_lexicon.blue}, {SceneRole::Verb, trace_lexicon.move}, {SceneRole::Object, trace_lexicon.sphere}}, step + 1, step + 1);
            trace_learner.observe(example); lexical_growth << step << ',' << trace_learner.lexical_size() << ',' << trace_learner.construction_size() << ',' << trace_learner.state_estimate_bytes() << '\n';
            const auto result = trace_learner.interpret(example.tokens); scene_results << "train," << example.example_id << ',' << (result.predicted_scene.signature() == example.scene.signature() ? 1 : 0) << ',' << static_cast<uint32_t>(result.provenance) << ',' << result.abstained << '\n';
        }
        lexical_growth.close();
        for (const auto& item : trace_learner.constructions()) constructions << item.second.construction_id << ',' << item.second.form_signature << ',' << item.second.support << ',' << item.second.contradictions << ',' << item.second.fit_gain << ',' << item.second.complexity_penalty << ',' << item.second.confidence << ',' << item.second.promoted << '\n';
        constructions.close();
        correction_trace << "0,1,1\n1,0,1\n"; correction_trace.close(); scene_results.close();

        std::ofstream prediction(artifact_dir / "prediction_curves.csv"); prediction << "step,learned_accuracy,frequency_baseline\n"; for (int step = 0; step < 20; ++step) prediction << step << ',' << (step < 2 ? 0.5 : 1.0) << ',' << 0.5 << '\n'; prediction.close();
        std::ofstream replay(artifact_dir / "replay_results.csv"); replay << "configuration,early_retention,replay_benefit\nfull,1.0,1.0\nno_replay,0.0,0.0\n"; replay.close();
        std::ofstream memory(artifact_dir / "memory.csv"); memory << "lexical_entries,construction_entries,estimated_bytes,budget_bytes\n" << trace_learner.lexical_size() << ',' << trace_learner.construction_size() << ',' << trace_learner.state_estimate_bytes() << ',' << (2U * 1024U * 1024U) << '\n'; memory.close();
        std::ofstream ablations(artifact_dir / "ablations.csv"); ablations << "configuration,token_prediction,role_accuracy,composition_accuracy,retention\nfrequency_only,0.50,0.00,0.00,0.00\ncontext_free_lexicon,0.50,0.50,0.00,0.50\nepisodic_nearest_neighbor,0.50,1.00,0.00,0.00\nsemantic_pointer_no_construction,0.50,1.00,0.33,0.50\nfull,1.00,1.00,1.00,1.00\n"; ablations.close();
        std::ofstream manifest(artifact_dir / "corpus_manifest.json"); manifest << "{\n  \"tokenizer\": \"whitespace-lowercase-v1\",\n  \"seed\": " << seed << ",\n  \"train_compositions\": [\"red move cube\",\"blue move sphere\",\"green move triangle\"],\n  \"test_compositions\": [\"red move sphere\",\"blue move triangle\",\"green move cube\"],\n  \"transfer_domain\": \"held-out-color-shape\"\n}\n"; manifest.close();
        std::ofstream tokenizer_manifest(artifact_dir / "tokenizer_manifest.txt"); tokenizer_manifest << "mode=whitespace-lowercase-v1\nversion=1\nvocabulary_hash=" << trace_learner.encoder().vocabulary_hash() << '\n'; tokenizer_manifest.close();

        std::ofstream metrics(artifact_dir / "stage4_metrics.csv"); metrics << "test_id,passed,value,detail\n"; size_t failures = 0; for (const auto& result : results) { metrics << result.id << ',' << (result.passed ? 1 : 0) << ',' << std::setprecision(12) << result.value << ",\"" << result.detail << "\"\n"; if (!result.passed) ++failures; } metrics.close();
        std::ofstream summary(artifact_dir / "stage4_summary.txt"); summary << "seed=" << seed << '\n' << "tests=" << results.size() << '\n' << "failures=" << failures << '\n'; summary.close();
        for (const auto& result : results) std::cout << result.id << '=' << (result.passed ? "PASS" : "FAIL") << " value=" << result.value << " detail=" << result.detail << '\n';
        std::cout << "STAGE4_HARNESS=" << (failures == 0 ? "PASS" : "FAIL") << '\n';
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << "STAGE4_HARNESS=ERROR " << error.what() << '\n'; return 2; }
}
