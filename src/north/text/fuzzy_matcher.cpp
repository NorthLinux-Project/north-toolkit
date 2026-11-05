#include "north/text/fuzzy_matcher.hpp"

#include <cctype>

namespace north::text {

namespace {

[[nodiscard]] constexpr char to_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] bool is_word_boundary(std::string_view target, size_t idx) noexcept {
    if (idx == 0) {
        return true;
    }
    char prev = target[idx - 1];
    char curr = target[idx];
    if (prev == ' ' || prev == '_' || prev == '-' || prev == '/' || prev == '\\' || prev == '.' || prev == ':') {
        return true;
    }
    if (std::islower(static_cast<unsigned char>(prev)) && std::isupper(static_cast<unsigned char>(curr))) {
        return true;
    }
    return false;
}

void search_fuzzy(std::string_view pattern, size_t p_idx,
                  std::string_view target, size_t t_idx,
                  int32_t current_score, int32_t consecutive,
                  std::vector<uint32_t>& current_indices,
                  FuzzyMatchResult& best_result,
                  int& iteration_budget) {
    if (--iteration_budget <= 0) {
        return;
    }

    if (p_idx == pattern.size()) {
        if (!best_result.matched || current_score > best_result.score) {
            best_result.matched = true;
            best_result.score = current_score;
            best_result.matched_indices = current_indices;
        }
        return;
    }

    char p_char = to_lower(pattern[p_idx]);

    for (size_t t = t_idx; t < target.size(); ++t) {
        if (to_lower(target[t]) == p_char) {
            int32_t char_score = 10;
            int32_t next_consecutive = 0;

            if (t == 0) {
                char_score += 25;
            } else if (is_word_boundary(target, t)) {
                char_score += 20;
            }

            if (!current_indices.empty()) {
                size_t prev_idx = current_indices.back();
                if (t == prev_idx + 1) {
                    next_consecutive = consecutive + 1;
                    char_score += 15 * next_consecutive;
                } else {
                    int32_t gap = static_cast<int32_t>(t - prev_idx - 1);
                    char_score -= (gap > 5) ? 5 : gap;
                }
            }

            current_indices.push_back(static_cast<uint32_t>(t));
            search_fuzzy(pattern, p_idx + 1, target, t + 1,
                         current_score + char_score, next_consecutive,
                         current_indices, best_result, iteration_budget);
            current_indices.pop_back();

            if (p_char == ' ' || is_word_boundary(target, t)) {
                continue;
            }
        }
    }
}

} // namespace

FuzzyMatchResult fuzzy_match(std::string_view pattern, std::string_view target) {
    if (pattern.empty()) {
        return FuzzyMatchResult{.matched = true, .score = 0, .matched_indices = {}};
    }
    if (target.empty() || pattern.size() > target.size()) {
        return FuzzyMatchResult{.matched = false, .score = 0, .matched_indices = {}};
    }

    FuzzyMatchResult result;
    std::vector<uint32_t> current_indices;
    current_indices.reserve(pattern.size());
    int iteration_budget = 400;

    search_fuzzy(pattern, 0, target, 0, 0, 0, current_indices, result, iteration_budget);

    if (!result.matched) {
        size_t t = 0;
        int32_t greedy_score = 0;
        int32_t consecutive = 0;
        current_indices.clear();

        for (size_t p = 0; p < pattern.size(); ++p) {
            char p_char = to_lower(pattern[p]);
            bool found = false;
            while (t < target.size()) {
                if (to_lower(target[t]) == p_char) {
                    found = true;
                    int32_t char_score = 10;
                    if (t == 0) {
                        char_score += 25;
                    } else if (is_word_boundary(target, t)) {
                        char_score += 20;
                    }
                    if (!current_indices.empty()) {
                        size_t prev_idx = current_indices.back();
                        if (t == prev_idx + 1) {
                            consecutive++;
                            char_score += 15 * consecutive;
                        } else {
                            consecutive = 0;
                            int32_t gap = static_cast<int32_t>(t - prev_idx - 1);
                            char_score -= (gap > 5) ? 5 : gap;
                        }
                    }
                    greedy_score += char_score;
                    current_indices.push_back(static_cast<uint32_t>(t));
                    t++;
                    break;
                }
                t++;
            }
            if (!found) {
                return FuzzyMatchResult{.matched = false, .score = 0, .matched_indices = {}};
            }
        }

        result.matched = true;
        result.score = greedy_score;
        result.matched_indices = std::move(current_indices);
    }

    return result;
}

} // namespace north::text
