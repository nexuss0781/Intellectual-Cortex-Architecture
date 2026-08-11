#include "memory_system.h"

#include <cstring>
#include <sstream>

namespace genesis {
namespace {

struct SectionHeader {
    uint32_t section_id;
    uint32_t version;
    uint32_t schema_id;
    uint32_t required;
    uint64_t byte_length;
    uint32_t checksum;
};

constexpr uint32_t kFormatVersion = 1;
constexpr uint32_t kSchemaVersion = 1;
constexpr uint32_t kEndianMarker = 0x01020304;

uint64_t fnv64(const uint8_t* data, size_t size, uint64_t seed = 1469598103934665603ULL) {
    uint64_t hash = seed;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

template <typename T>
void append_scalar(std::vector<uint8_t>& output, const T& value) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    output.insert(output.end(), bytes, bytes + sizeof(T));
}

template <typename T>
T read_scalar(const std::vector<uint8_t>& input, size_t& offset) {
    if (offset + sizeof(T) > input.size()) throw MemoryError("state section is truncated");
    T value{};
    std::memcpy(&value, input.data() + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw MemoryError("cannot open brain state: " + path.string());
    const auto end = input.tellg();
    if (end < 0) throw MemoryError("cannot determine brain state size");
    std::vector<uint8_t> bytes(static_cast<size_t>(end));
    input.seekg(0);
    if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input && !bytes.empty()) throw MemoryError("brain state read was truncated");
    return bytes;
}

void write_file(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw MemoryError("cannot create temporary brain state: " + temporary);
        if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) throw MemoryError("brain state write failed: " + temporary);
    }
    std::filesystem::rename(temporary, path);
}

std::vector<uint8_t> raw_bytes(const void* data, size_t bytes) {
    std::vector<uint8_t> output(bytes);
    if (bytes > 0) std::memcpy(output.data(), data, bytes);
    return output;
}

template <typename T>
std::vector<uint8_t> vector_bytes(const std::vector<T>& values) {
    return raw_bytes(values.data(), values.size() * sizeof(T));
}

template <typename T>
std::vector<T> decode_vector(const std::vector<uint8_t>& bytes) {
    if (bytes.size() % sizeof(T) != 0) throw MemoryError("state vector section has invalid byte length");
    std::vector<T> result(bytes.size() / sizeof(T));
    if (!bytes.empty()) std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

} // namespace

void MemorySystem::save(const std::filesystem::path& path) const {
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> sections;

    std::vector<uint8_t> meta;
    append_scalar(meta, seed_);
    append_scalar(meta, tick_);
    append_scalar(meta, next_episode_id_);
    sections.emplace_back(1, std::move(meta));

    std::vector<uint8_t> arena;
    append_scalar(arena, static_cast<uint64_t>(arena_.neuron_capacity()));
    append_scalar(arena, static_cast<uint64_t>(arena_.synapse_capacity()));
    append_scalar(arena, arena_.next_neuron_id_);
    append_scalar(arena, arena_.next_synapse_id_);
    append_scalar(arena, static_cast<uint64_t>(arena_.neuron_ids_.size()));
    for (uint64_t id : arena_.neuron_ids_) append_scalar(arena, id);
    append_scalar(arena, static_cast<uint64_t>(arena_.synapse_ids_.size()));
    for (uint64_t id : arena_.synapse_ids_) append_scalar(arena, id);
    sections.emplace_back(2, std::move(arena));

    sections.emplace_back(3, vector_bytes(events_));
    sections.emplace_back(4, vector_bytes(episodes_));
    sections.emplace_back(5, vector_bytes(replay_indices_));
    sections.emplace_back(6, vector_bytes(candidates_));
    sections.emplace_back(7, raw_bytes(&config_, sizeof(config_)));

    BrainStateHeader header;
    header.format_version = kFormatVersion;
    header.state_schema_version = kSchemaVersion;
    header.seed = seed_;
    header.tick = tick_;
    header.section_count = static_cast<uint32_t>(sections.size());

    uint64_t manifest_hash = 1469598103934665603ULL;
    for (const auto& section : sections) {
        manifest_hash = fnv64(reinterpret_cast<const uint8_t*>(&section.first), sizeof(section.first), manifest_hash);
        manifest_hash = fnv64(section.second.data(), section.second.size(), manifest_hash);
    }
    header.manifest_hash = manifest_hash;

    std::vector<uint8_t> file;
    append_scalar(file, header);
    for (const auto& section : sections) {
        SectionHeader section_header{};
        section_header.section_id = section.first;
        section_header.version = 1;
        section_header.schema_id = kSchemaVersion;
        section_header.required = 1;
        section_header.byte_length = section.second.size();
        section_header.checksum = checksum_bytes(section.second.data(), section.second.size());
        append_scalar(file, section_header);
        file.insert(file.end(), section.second.begin(), section.second.end());
    }
    write_file(path, file);
}

void MemorySystem::load(const std::filesystem::path& path) {
    const std::vector<uint8_t> file = read_file(path);
    size_t offset = 0;
    const BrainStateHeader header = read_scalar<BrainStateHeader>(file, offset);
    if (header.format_version != kFormatVersion) throw MemoryError("unsupported brain state format version");
    if (header.state_schema_version != kSchemaVersion) throw MemoryError("incompatible brain state schema version");
    if (header.endian_marker != kEndianMarker) throw MemoryError("brain state endian marker is incompatible");
    if (header.section_count == 0 || header.section_count > 32) throw MemoryError("invalid brain state section count");

    std::map<uint32_t, std::vector<uint8_t>> sections;
    uint64_t manifest_hash = 1469598103934665603ULL;
    for (uint32_t i = 0; i < header.section_count; ++i) {
        const SectionHeader section_header = read_scalar<SectionHeader>(file, offset);
        if (section_header.version != 1 || section_header.schema_id != kSchemaVersion) {
            if (section_header.required) throw MemoryError("unknown required brain state section version");
            if (offset + section_header.byte_length > file.size()) throw MemoryError("optional brain state section is truncated");
            offset += static_cast<size_t>(section_header.byte_length);
            continue;
        }
        if (section_header.byte_length > file.size() - offset) throw MemoryError("brain state section is truncated");
        std::vector<uint8_t> payload(file.begin() + static_cast<std::ptrdiff_t>(offset),
                                     file.begin() + static_cast<std::ptrdiff_t>(offset + section_header.byte_length));
        offset += static_cast<size_t>(section_header.byte_length);
        if (checksum_bytes(payload.data(), payload.size()) != section_header.checksum) {
            throw MemoryError("brain state section checksum mismatch");
        }
        if (!sections.emplace(section_header.section_id, payload).second) throw MemoryError("duplicate brain state section");
        manifest_hash = fnv64(reinterpret_cast<const uint8_t*>(&section_header.section_id), sizeof(section_header.section_id), manifest_hash);
        manifest_hash = fnv64(payload.data(), payload.size(), manifest_hash);
    }
    if (offset != file.size()) throw MemoryError("unexpected trailing brain state bytes");
    if (manifest_hash != header.manifest_hash) throw MemoryError("brain state manifest checksum mismatch");
    for (uint32_t required = 1; required <= 7; ++required) {
        if (sections.find(required) == sections.end()) throw MemoryError("missing required brain state section " + std::to_string(required));
    }

    MemorySystem loaded(header.seed, config_);
    loaded.seed_ = header.seed;
    loaded.tick_ = header.tick;
    {
        size_t cursor = 0;
        loaded.seed_ = read_scalar<uint64_t>(sections.at(1), cursor);
        loaded.tick_ = read_scalar<uint64_t>(sections.at(1), cursor);
        loaded.next_episode_id_ = read_scalar<uint64_t>(sections.at(1), cursor);
        if (cursor != sections.at(1).size()) throw MemoryError("metadata section has trailing bytes");
    }
    {
        const auto& bytes = sections.at(2);
        size_t cursor = 0;
        const size_t neuron_capacity = static_cast<size_t>(read_scalar<uint64_t>(bytes, cursor));
        const size_t synapse_capacity = static_cast<size_t>(read_scalar<uint64_t>(bytes, cursor));
        loaded.arena_.neuron_capacity_ = neuron_capacity;
        loaded.arena_.synapse_capacity_ = synapse_capacity;
        loaded.arena_.next_neuron_id_ = read_scalar<uint64_t>(bytes, cursor);
        loaded.arena_.next_synapse_id_ = read_scalar<uint64_t>(bytes, cursor);
        const size_t neuron_count = static_cast<size_t>(read_scalar<uint64_t>(bytes, cursor));
        loaded.arena_.neuron_ids_.resize(neuron_count);
        for (uint64_t& id : loaded.arena_.neuron_ids_) id = read_scalar<uint64_t>(bytes, cursor);
        const size_t synapse_count = static_cast<size_t>(read_scalar<uint64_t>(bytes, cursor));
        loaded.arena_.synapse_ids_.resize(synapse_count);
        for (uint64_t& id : loaded.arena_.synapse_ids_) id = read_scalar<uint64_t>(bytes, cursor);
        if (neuron_count > neuron_capacity || synapse_count > synapse_capacity || cursor != bytes.size()) throw MemoryError("arena section is inconsistent");
    }
    loaded.events_ = decode_vector<MemoryEvent>(sections.at(3));
    loaded.episodes_ = decode_vector<EpisodeRecord>(sections.at(4));
    loaded.replay_indices_ = decode_vector<ReplayIndex>(sections.at(5));
    loaded.candidates_ = decode_vector<ConsolidationCandidate>(sections.at(6));
    if (sections.at(7).size() != sizeof(MemoryConfig)) throw MemoryError("memory config section has invalid size");
    std::memcpy(&loaded.config_, sections.at(7).data(), sizeof(MemoryConfig));

    for (const EpisodeRecord& episode : loaded.episodes_) {
        const auto episode_events = loaded.events_for(episode);
        if (checksum_events(episode_events) != episode.checksum) throw MemoryError("episode checksum mismatch");
    }
    if (loaded.replay_indices_.size() != loaded.episodes_.size()) throw MemoryError("replay index and episode counts differ");

    // Commit only after every section, checksum, range, and episode invariant
    // has passed. Any exception above leaves the current object untouched.
    loaded.replay_callback_ = replay_callback_;
    *this = std::move(loaded);
}

} // namespace genesis
