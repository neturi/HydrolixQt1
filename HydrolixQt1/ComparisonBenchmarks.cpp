#include "ComparisonBenchmarks.hpp"
#include "CommonValuesFinder.hpp"
#include "RandomizedWorkload.hpp"
#include "FindCommonVals3.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr int trials = 10;
constexpr int queriesPerTrial = 128;
constexpr int queryPatterns = 32;

struct Dataset {
    std::string name;
    std::vector<int> large;
    std::vector<std::vector<int>> queries;
    std::vector<std::vector<int>> expected;
};
struct Sample {
    double buildMilliseconds;
    double queryMilliseconds;
};

double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Dataset makeDataset(int count) {
    auto inputs = randomizedWorkload::generate(count, queryPatterns);
    Dataset data;
    data.name = std::to_string(count) + " random large / 100 random small; duplicates + hits/misses";
    data.large = std::move(inputs.large);
    data.queries = std::move(inputs.queries);
    auto sortedLarge = data.large;
    std::sort(sortedLarge.begin(), sortedLarge.end());
    for (const auto& small : data.queries) {
        auto sortedSmall = small;
        std::sort(sortedSmall.begin(), sortedSmall.end());
        std::vector<int> expected;
        std::set_intersection(sortedLarge.begin(), sortedLarge.end(),
                              sortedSmall.begin(), sortedSmall.end(), std::back_inserter(expected));
        data.expected.push_back(std::move(expected));
    }
    const auto distinctEnd = std::unique(sortedLarge.begin(), sortedLarge.end());
    std::cout << "Generated " << count << " values | distinct="
              << std::distance(sortedLarge.begin(), distinctEnd)
              << " | seed=" << randomizedWorkload::seed << '\n';
    std::size_t minMatches = randomizedWorkload::querySize, maxMatches = 0;
    for (const auto& expected : data.expected) {
        minMatches = std::min(minMatches, expected.size());
        maxMatches = std::max(maxMatches, expected.size());
    }
    std::cout << "  " << queryPatterns << " queries of 100 elements; output counts range "
              << minMatches << ".." << maxMatches << " after duplicate limits\n";
    return data;
}

template <typename Finder, typename Query>
Sample measure(Dataset& data, Query query, std::uint64_t& checksum) {
    const auto originalLarge = data.large;
    const auto originalQueries = data.queries;
    auto start = Clock::now();
    Finder finder(data.large);
    const double build = milliseconds(start);

    // Validate and warm every query pattern outside the timed batch.
    for (std::size_t i = 0; i < data.queries.size(); ++i) {
        const auto output = query(finder, data.queries[i]);
        std::vector<int> normalized(output.begin(), output.end());
        std::sort(normalized.begin(), normalized.end());
        if (normalized != data.expected[i]) throw std::runtime_error("Benchmark mismatch: " + data.name);
    }
    start = Clock::now();
    for (int i = 0; i < queriesPerTrial; ++i) {
        const auto result = query(finder, data.queries[i % data.queries.size()]);
        checksum += result.size();
        for (int value : result) checksum += static_cast<std::uint32_t>(value);
    }
    const double batch = milliseconds(start);
    // Check all patterns again after repeated calls, outside the timed batch.
    for (std::size_t i = 0; i < data.queries.size(); ++i) {
        const auto output = query(finder, data.queries[i]);
        std::vector<int> normalized(output.begin(), output.end());
        std::sort(normalized.begin(), normalized.end());
        if (normalized != data.expected[i]) throw std::runtime_error("Repeated-query mismatch: " + data.name);
    }
    if (data.large != originalLarge || data.queries != originalQueries) {
        throw std::runtime_error("Benchmark input changed: " + data.name);
    }
    return {build, batch};
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

void compare(Dataset data) {
    std::cout << "\nBENCHMARK: " << data.name << std::endl;
    constexpr std::array<const char*, 2> names = {"findCommonVals3", "CommonValuesFinder"};
    // Fixed execution and display order; timing can retain order/cache effects.
    constexpr std::array<std::size_t, 2> executionOrder = {0, 1};
    std::array<std::vector<double>, 2> builds, batches;
    std::array<std::uint64_t, 2> checksums{};
    for (int trial = 0; trial < trials; ++trial) {
        std::array<Sample, 2> samples{};
        samples[0] = measure<UnorderedMapFilter>(data, [](auto& finder, auto& small) {
            return finder.findCommonVals3(small);
        }, checksums[0]);
        samples[1] = measure<CommonValuesFinder>(data, [](auto& finder, auto& small) {
            return finder.common(small);
        }, checksums[1]);
        std::cout << "  iteration " << trial + 1 << '/' << trials << '\n';
        for (const auto i : executionOrder) {
            builds[i].push_back(samples[i].buildMilliseconds);
            batches[i].push_back(samples[i].queryMilliseconds);
            std::cout << "    " << names[i] << " | build=" << samples[i].buildMilliseconds
                      << " ms | batch=" << samples[i].queryMilliseconds << " ms"
                      << std::setprecision(6) << " | query=" << samples[i].queryMilliseconds / queriesPerTrial
                      << " ms/call" << std::setprecision(3) << std::endl;
        }
    }
    if (!std::all_of(checksums.begin(), checksums.end(), [&](auto value) { return value == checksums[0]; })) {
        throw std::runtime_error("Benchmark checksum mismatch");
    }
    std::cout << std::setprecision(6);
    for (const auto i : executionOrder) {
        std::cout << "  MEDIAN " << names[i] << ": build=" << median(builds[i])
                  << " ms | query=" << median(batches[i]) / queriesPerTrial << " ms/call\n";
    }
    std::cout << "  Matching checksum (both algorithms): " << checksums[0]
              << std::setprecision(3) << std::endl;
}
}

void runComparisonBenchmarks() {
    std::cout << "\n========== ALGORITHM COMPARISON ==========\n";
#if !defined(__OPTIMIZE__) || defined(DEBUG)
    std::cout << "DEBUG BUILD: use Release without the debugger for performance conclusions.\n";
#endif
    std::cout << "Fixed order: findCommonVals3 -> CommonValuesFinder.\n"
                 "Fixed-order timings may retain cache/order effects.\n";
    std::cout << trials << " trials, " << queriesPerTrial << " queries/trial, "
              << queryPatterns << " pre-generated patterns.\n"
                 "Query batches include output allocation/destruction and checksums; exclude verification.\n"
                 "Both algorithms return sorted vectors. Random seed: 20261004; queries contain 100 values.\n";
    for (const int largeSize : randomizedWorkload::largeSizes) compare(makeDataset(largeSize));
}

// Other alternative apporaches consideration and tradeoffs
//
// 1. Brute force approach would have resulted in O(n * m)  time complexity
// 2. Wanted to use vector sorting approach but that would have been O (n log n + m log m) where n is the size of large vector(1M or greater) and m is the size of small vector
// 3. Condsidered using unordered_set but as there are duplicates allowed switched to unordered_multiset
// 4. unordered_map with count of value and occurance gives a O(1) lookup then sorting on the small vector O (m log m ) since its 100 numbers much less than N.
//       Expected O(N + M + K log K) time, O(M) extra space, for smaller size M and output size K <= M; pathological hash collisions can worsen lookup time.
//       unordered_map is simple, supports every int32 key, and needs no custom probing.reserve avoids growth-related rehashes, but distinct keys still allocate nodes.