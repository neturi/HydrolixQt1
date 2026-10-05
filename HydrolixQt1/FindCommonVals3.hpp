#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class UnorderedMapFilter {
public:
    explicit UnorderedMapFilter(const std::vector<std::int32_t>& large);
    [[nodiscard]] std::vector<std::int32_t>
    findCommonVals3(const std::vector<std::int32_t>& small) const;

    // Shares the same large index; preserves the small input's matching order.
    [[nodiscard]] std::vector<std::int32_t>
    findCommonVals4(const std::vector<std::int32_t>& small) const;

    // One-shot call: builds only a small-side index; sorted output, inputs unchanged.
    // Static means callers do not need to construct a large-side index first.
    [[nodiscard]] static std::vector<std::int32_t>
    findCommonVals3Once(const std::vector<std::int32_t>& largeValues,
                       const std::vector<std::int32_t>& smallValues);

private:
    std::unordered_map<std::int32_t, std::size_t> counts_;
};
