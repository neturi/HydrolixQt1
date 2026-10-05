#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

// Shared by correctness tests and benchmarks so both use the same workload.
namespace randomizedWorkload {
inline constexpr std::uint32_t seed = 20261004;
inline constexpr int largeSizes[] = {1000, 100000, 1000000};
inline constexpr std::size_t querySize = 100;

struct Inputs {
    std::vector<int> large;
    std::vector<std::vector<int>> queries;
};

inline Inputs generate(int largeSize, int queryCount) {
    if (largeSize < 4 || largeSize > 1000000 || queryCount < 1) {
        throw std::invalid_argument("Random workload requires 4..1,000,000 large values and at least one query");
    }
    // Reset for every size so algorithms and repeated runs see identical inputs
    // on the same toolchain. Distribution mappings can vary across libraries.
    std::mt19937 random(seed);
    const int valueLimit = largeSize / 4;
    std::uniform_int_distribution<int> largeValue(-valueLimit, valueLimit);
    Inputs inputs;
    inputs.large.resize(largeSize);
    for (auto& value : inputs.large) value = largeValue(random);
    // There are fewer possible values than elements, guaranteeing duplicates
    // without making all values identical or sorting the constructor input.

    std::uniform_int_distribution<std::size_t> largeIndex(0, inputs.large.size() - 1);
    std::uniform_int_distribution<int> missingValue(valueLimit + 1, valueLimit + largeSize);
    std::uniform_int_distribution<std::size_t> candidateIndex(0, 31);
    for (int pattern = 0; pattern < queryCount; ++pattern) {
        // A random 32-entry candidate pool: 16 values sampled from the large
        // input and 16 outside its range. Drawing 100 times guarantees query
        // duplicates and gives a roughly 50/50 mix of present/absent values.
        // Actual output may be smaller: min(large count, query count) still applies.
        std::vector<int> candidates;
        candidates.reserve(32);
        for (int i = 0; i < 16; ++i) candidates.push_back(inputs.large[largeIndex(random)]);
        for (int i = 0; i < 16; ++i) candidates.push_back(missingValue(random));
        std::vector<int> query(querySize);
        for (auto& value : query) value = candidates[candidateIndex(random)];
        // Ensure each pattern exercises both a hit and a miss.
        query[0] = candidates[0];
        query[1] = candidates[16];
        inputs.queries.push_back(std::move(query));
    }
    return inputs;
}
}
