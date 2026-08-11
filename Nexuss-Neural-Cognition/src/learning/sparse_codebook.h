#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace genesis {

class SparseCodebook {
public:
    SparseCodebook(size_t classes, size_t width, size_t active_bits)
        : classes_(classes), width_(width), active_bits_(active_bits),
          support_(classes, std::vector<uint32_t>(width, 0)) {
        if (classes == 0 || width == 0 || active_bits == 0 || active_bits > width) {
            throw std::invalid_argument("SparseCodebook dimensions are invalid");
        }
    }

    void learn(size_t class_id, const std::vector<uint8_t>& input) {
        validate_input(class_id, input);
        for (size_t bit = 0; bit < width_; ++bit) {
            if (input[bit]) ++support_[class_id][bit];
        }
        ++observations_[class_id];
    }

    std::vector<uint8_t> code(size_t class_id) const {
        if (class_id >= classes_) throw std::out_of_range("SparseCodebook class ID out of range");
        std::vector<size_t> indices(width_);
        std::iota(indices.begin(), indices.end(), 0);
        std::stable_sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            if (support_[class_id][a] != support_[class_id][b]) return support_[class_id][a] > support_[class_id][b];
            return a < b;
        });
        std::vector<uint8_t> result(width_, 0);
        for (size_t i = 0; i < active_bits_; ++i) result[indices[i]] = 1;
        return result;
    }

    size_t decode(const std::vector<uint8_t>& input) const {
        if (input.size() != width_) throw std::invalid_argument("SparseCodebook input width mismatch");
        size_t best_class = 0;
        size_t best_distance = static_cast<size_t>(-1);
        for (size_t class_id = 0; class_id < classes_; ++class_id) {
            const auto prototype = code(class_id);
            size_t distance = 0;
            for (size_t bit = 0; bit < width_; ++bit) distance += prototype[bit] != static_cast<uint8_t>(input[bit] != 0);
            if (distance < best_distance) {
                best_distance = distance;
                best_class = class_id;
            }
        }
        return best_class;
    }

    size_t width() const { return width_; }
    size_t active_bits() const { return active_bits_; }
    uint32_t observations(size_t class_id) const { return observations_.at(class_id); }

private:
    void validate_input(size_t class_id, const std::vector<uint8_t>& input) const {
        if (class_id >= classes_) throw std::out_of_range("SparseCodebook class ID out of range");
        if (input.size() != width_) throw std::invalid_argument("SparseCodebook input width mismatch");
    }

    size_t classes_;
    size_t width_;
    size_t active_bits_;
    std::vector<std::vector<uint32_t>> support_;
    std::vector<uint32_t> observations_ = std::vector<uint32_t>(classes_, 0);
};

} // namespace genesis
