#include "FindCommonVals3.hpp"

#include <algorithm>
#include <execution>

UnorderedMapFilter::UnorderedMapFilter(const std::vector<std::int32_t>& large) {
    // There can be at most large.size() distinct keys; reserve buckets upfront.
    counts_.reserve(large.size());
    for (auto value : large) ++counts_[value];
}

std::vector<std::int32_t>
UnorderedMapFilter::findCommonVals3(const std::vector<std::int32_t>& small) const {
    auto sorted = small;
    std::sort(std::execution::par, sorted.begin(), sorted.end());
    std::vector<std::int32_t> result;
    result.reserve(small.size());
    for (std::size_t i = 0; i < sorted.size();) {
        std::size_t end = i + 1;
        while (end < sorted.size() && sorted[end] == sorted[i]) ++end;
        const auto found = counts_.find(sorted[i]);
        if (found != counts_.end()) {
            result.insert(result.end(), std::min(end - i, found->second), sorted[i]);
        }
        i = end;
    }
    return result;
}

std::vector<std::int32_t>
UnorderedMapFilter::findCommonVals3Once(const std::vector<std::int32_t>& largeValues,
                                      const std::vector<std::int32_t>& smallValues) {
    if (largeValues.empty() || smallValues.empty()) return {};
    const auto& countedValues = largeValues.size() < smallValues.size() ? largeValues : smallValues;
    const auto& scannedValues = largeValues.size() < smallValues.size() ? smallValues : largeValues;

    // Index at most the smaller input, normally <= 100 values. For a single
    // query, there is no later lookup to amortize building a million-value map.
    // Reuse the constructor's counting/reserve logic on that smaller input.
    UnorderedMapFilter smallIndex(countedValues);
    std::vector<std::int32_t> result;
    result.reserve(countedValues.size());
    std::size_t remainingOccurrences = countedValues.size();
    for (const auto value : scannedValues) {
        // find(), rather than operator[], avoids inserting irrelevant large keys.
        const auto found = smallIndex.counts_.find(value);
        if (found == smallIndex.counts_.end() || found->second == 0) continue;
        result.push_back(value);
        --found->second; // Emit min(large count, small count) copies, then skip extras.
        if (--remainingOccurrences == 0) break;
    }
    // Sorted output matches findCommonVals3; only K <= M results are sorted.
    std::sort(result.begin(), result.end());
    return result;
}

// Expected O(N + M + K log K) time, O(M) extra space, for smaller size M and
// output size K <= M; pathological hash collisions can worsen lookup time.
// unordered_map is simple, supports every int32 key, and needs no custom probing.
// reserve avoids growth-related rehashes, but distinct keys still allocate nodes.
// A flat table (CommonValuesFinder::commonOnce) trades implementation complexity
// for contiguous storage. Sorting the small input and binary-searching each large
// value avoids hashing but adds O(N log M) search work and count bookkeeping.
// Although these inputs are disposable, sorting both in place would cost
// O(N log N) for the large unsorted input. Borrowing const references needs no
// whole-input copies and preserves reuse options for callers.
// For many queries, prefer a reusable large index with findCommonVals3 instead.
