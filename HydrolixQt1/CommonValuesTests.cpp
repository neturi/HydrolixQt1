#include "CommonValuesTests.hpp"
#include "CommonValuesFinder.hpp"
#include "FindCommonVals3.hpp"
#include "RandomizedWorkload.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/*
 
 Large input    findCommonVals3: sort    findCommonVals4: small map    V4 slowdown
 1,000 unique    0.000782 ms    0.001568 ms    2.0×
 1,000,001 unique    0.000860 ms    0.002052 ms    2.4×
 10,000 identical    0.000157 ms    0.000356 ms    2.3×


 For these 100-element queries, sorting was faster. The small map adds hashing and allocations; repeated values also cause repeated large-map lookups. The better expected complexity didn’t translate into better measured speed here.
 
 */

/*
 Updated and ran all five in your requested order. Release build succeeded; all 300,720 query checks passed.
 Median query time in milliseconds, across 10 benchmark trials:
 Algorithm              1,000 unique    1,000,001 unique    10,000 identical
 
 findCommonVals3        0.000796    0.000975    0.000173
 CommonValuesFinder     0.000680    0.000757    0.000201
 */

namespace {
constexpr std::uint32_t seed = 20261004;
constexpr int iterationCount = 3;
using Clock = std::chrono::steady_clock;

double elapsedMilliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

std::string describe(const std::vector<int>& values) {
    std::ostringstream text;
    text << '[';
    for (auto value : values) text << value << ' ';
    return text.str() + ']';
}

// A sorted intersection independently checks the hash-based implementation.
std::vector<int> reference(std::vector<int> large, std::vector<int> small) {
    std::sort(large.begin(), large.end());
    std::sort(small.begin(), small.end());
    std::vector<int> expected;
    std::set_intersection(large.begin(), large.end(), small.begin(), small.end(),
                          std::back_inserter(expected));
    return expected;
}

struct ResearchAlgorithm {
    using Finder = CommonValuesFinder;
    static constexpr const char* name = "CommonValuesFinder (research count table)";
    static auto query(Finder& finder, std::vector<int>& small) {
        return finder.common(small);
    }
};
struct UnorderedMapAlgorithm {
    using Finder = UnorderedMapFilter;
    static constexpr const char* name = "findCommonVals3 (std::unordered_map counts)";
    static auto query(Finder& finder, std::vector<int>& small) {
        return finder.findCommonVals3(small);
    }
};
// Borrow the large input without building an index: each one-shot call must
// include its own small-index construction and destruction in the query timing.
struct OneShotInput {
    const std::vector<int>& large;
    explicit OneShotInput(const std::vector<int>& values) : large(values) {}
};
struct FlatOneShotAlgorithm {
    using Finder = OneShotInput;
    static constexpr const char* name = "CommonValuesFinder::commonOnce";
    static auto query(Finder& finder, std::vector<int>& small) {
        return CommonValuesFinder::commonOnce(finder.large, small);
    }
};
struct MapOneShotAlgorithm {
    using Finder = OneShotInput;
    static constexpr const char* name = "UnorderedMapFilter::findCommonVals3Once";
    static auto query(Finder& finder, std::vector<int>& small) {
        return UnorderedMapFilter::findCommonVals3Once(finder.large, small);
    }
};

template <typename Algorithm>
struct TestSuite {
    std::size_t datasets = 0;
    std::size_t queries = 0;
    double buildMilliseconds = 0;
    double queryMilliseconds = 0;

    template <typename Work>
    void group(const std::string& name, Work work) {
        const auto datasetsBefore = datasets;
        const auto queriesBefore = queries;
        const auto buildBefore = buildMilliseconds;
        const auto queryBefore = queryMilliseconds;
        std::cout << "  RUN  " << name << std::endl;
        const auto start = Clock::now();
        work();
        const auto total = elapsedMilliseconds(start);
        std::cout << "  PASS " << name << '\n'
                  << "       datasets=" << datasets - datasetsBefore
                  << " | queries=" << queries - queriesBefore
                  << " | build=" << buildMilliseconds - buildBefore << " ms"
                  << " | query=" << queryMilliseconds - queryBefore << " ms"
                  << " | total=" << total << " ms" << std::endl;
    }

    void dataset(std::vector<int> large, std::vector<int> small, const std::string& name) {
        group(name, [&] { check(large, small, name); });
    }

    void check(std::vector<int> large, std::vector<int> small,
               const std::string& name) {
        const auto originalLarge = large;
        const auto originalSmall = small;
        const auto buildStart = Clock::now();
        typename Algorithm::Finder finder(large);
        buildMilliseconds += elapsedMilliseconds(buildStart);
        if (large != originalLarge) throw std::runtime_error(name + ": constructor changed input");

        auto verify = [&](std::vector<int>& query, const std::string& phase) {
            const auto before = query;
            const auto expected = reference(originalLarge, before);
            const auto queryStart = Clock::now();
            const auto output = Algorithm::query(finder, query);
            queryMilliseconds += elapsedMilliseconds(queryStart);
            if (!std::is_sorted(output.begin(), output.end())) {
                throw std::runtime_error(name + " / " + phase + ": output is not sorted");
            }
            std::vector<int> actual(output.begin(), output.end());
            std::sort(actual.begin(), actual.end());
            if (actual != expected) {
                throw std::runtime_error(name + " / " + phase +
                    "\nExpected: " + describe(expected) + "\nActual: " + describe(actual));
            }
            if (query != before || large != originalLarge || small != originalSmall) {
                throw std::runtime_error(name + " / " + phase + ": input changed");
            }
            ++queries;
        };

        verify(small, "first query");
        verify(small, "repeated query");
        std::vector<int> empty;
        verify(empty, "different (empty) query");
        verify(small, "original query after empty query");
        ++datasets;
    }
};
}

template <typename Algorithm>
std::size_t runAlgorithmIteration(int iteration) {
    const auto iterationStart = Clock::now();
    TestSuite<Algorithm> suite;
    std::cout << "\n========== ITERATION " << iteration << " / " << iterationCount
              << " ==========\n";
    std::cout << "Testing " << Algorithm::name << "; random seed " << seed << std::endl;
    suite.dataset({}, {}, "both empty");
    suite.dataset({}, {1, 1}, "empty large");
    suite.dataset({1, 1}, {}, "empty small");
    suite.dataset({1, 2}, {3, 4}, "no overlap");
    suite.dataset({1, 2, 2, 2, 4}, {2, 2, 3, 4, 4}, "original example");
    suite.dataset({5, 5, 5}, {5}, "large has more duplicates");
    suite.dataset({5}, {5, 5, 5}, "small has more duplicates");
    suite.dataset({7, 7, 7}, {7, 7, 7}, "all identical");
    suite.dataset({INT_MIN, INT_MAX, 0, -1, -1},
                {INT_MAX, -1, INT_MIN, 0, -1, -1}, "negatives and integer boundaries");
    suite.dataset({4, 3, 2, 1}, {1, 2, 3, 4}, "different order");

    std::mt19937 random(seed);
    suite.group("5000 randomized cases: alternating duplicate-heavy / wide values", [&] {
        std::uniform_int_distribution<int> n(0, 500), m(0, 100);
        std::uniform_int_distribution<int> dense(-8, 8), wide(-1000000, 1000000);
        for (int trial = 0; trial < 5000; ++trial) {
            std::vector<int> large(n(random)), small(m(random));
            for (auto& value : large) value = trial % 2 ? dense(random) : wide(random);
            for (auto& value : small) value = trial % 2 ? dense(random) : wide(random);
            if (trial % 3 == 0 && !large.empty()) {
                for (std::size_t j = 0; j < small.size() / 2; ++j) {
                    small[j] = large[random() % large.size()];
                }
            }
            suite.check(large, small, "seed " + std::to_string(seed) +
                        ", random trial " + std::to_string(trial));
        }
    });

    for (const int largeSize : randomizedWorkload::largeSizes) {
        auto inputs = randomizedWorkload::generate(largeSize, 1);
        suite.dataset(std::move(inputs.large), std::move(inputs.queries.front()),
                      std::to_string(largeSize) + " random large / 100 random small; duplicates + hits/misses");
    }

    suite.dataset(std::vector<int>(10000, 5), std::vector<int>(100, 5),
                "10,000 identical large values and 100 identical small values");

    std::cout << "PASS: " << suite.queries << " query checks across "
              << suite.datasets << " datasets; inputs unchanged\n";
    std::cout << "ITERATION " << iteration
              << " | build=" << suite.buildMilliseconds << " ms"
              << " | query=" << suite.queryMilliseconds << " ms"
              << " | elapsed=" << elapsedMilliseconds(iterationStart) << " ms" << std::endl;
    return suite.queries;
}

void runCommonValuesTests() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Correctness timings: build = constructors; query = all calls combined; "
                 "total includes reference checks and cleanup.\n";
    const auto start = Clock::now();
    std::size_t total = 0;
    for (int iteration = 1; iteration <= iterationCount; ++iteration) {
        total += runAlgorithmIteration<UnorderedMapAlgorithm>(iteration);
        total += runAlgorithmIteration<ResearchAlgorithm>(iteration);
    }
    std::cout << "\nALL PASS: " << iterationCount << " iterations per algorithm, " << total
              << " query checks | elapsed=" << elapsedMilliseconds(start) << " ms" << std::endl;

    std::cout << "\nONE-SHOT CORRECTNESS: query = full one-shot calls including temporary indexes; "
                 "build = borrowing test adapter only.\n";
    std::size_t oneShotTotal = 0;
    for (int iteration = 1; iteration <= iterationCount; ++iteration) {
        oneShotTotal += runAlgorithmIteration<MapOneShotAlgorithm>(iteration);
        oneShotTotal += runAlgorithmIteration<FlatOneShotAlgorithm>(iteration);
    }
    // 0, 34, and 89 select slot 0 in a 16-slot table. Consuming 0 must
    // not hide another colliding key, including when the zero key appears again.
    TestSuite<FlatOneShotAlgorithm> collisionSuite;
    collisionSuite.dataset({0, 0, 34, 89, 999, 1000}, {0, 34, 89},
                           "exhausted slot must preserve collision-chain lookup");
    oneShotTotal += collisionSuite.queries;
    std::cout << "ONE-SHOT ALL PASS: " << oneShotTotal << " query checks\n";

}


