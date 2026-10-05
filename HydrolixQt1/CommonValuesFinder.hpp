#pragma once

//
//  CommonValuesFinder.hpp
//

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// A fixed-capacity hash table stores one {value, occurrenceCount} entry per
// distinct large-input value. All entries live in one contiguous vector;
// construction does not grow/rehash the table or allocate a node for each key.
// Queries leave the table unchanged, so one constructed finder serves many queries.
//
// Example: large [7, 7, 7, 12], small [12, 7, 9, 7, 7, 7] returns [7, 7, 7, 12].
// Each value appears min(large count, small count) times, in sorted order.
// Courtesy: Calaude Code and tests developed using Github co-pilot
class CommonValuesFinder {
public:
    explicit CommonValuesFinder(std::span<const std::int32_t> largeValues)
        : discardedHashBits_{}, slotIndexMask_{}, table_{} {
        // Round up to a power of two, with at least twice as many slots as input
        // elements. Distinct values <= input elements, so at most half the slots
        // are occupied. Extra empty slots reduce collision-probe lengths and let
        // missing-key searches terminate; they do not eliminate collisions/clusters.
        // The tradeoff is extra memory, especially for duplicate-heavy input.
        const std::size_t slotCount =
            std::bit_ceil(std::max<std::size_t>(16, largeValues.size() * 2));

        // For a power-of-two capacity, '& (capacity - 1)' wraps an index just as
        // '% capacity' does. For example, capacity 16 gives mask 15 (binary 1111).
        slotIndexMask_ = slotCount - 1;

        // For capacity 2^k, countr_zero(capacity) is k. Discard the lower 32-k
        // hash bits to keep k high bits, producing an index in [0, capacity).
        discardedHashBits_ = 32 - std::countr_zero(slotCount);

        // One allocation initializes every count to zero (empty). Each slot is
        // 8 bytes: a 32-bit value and a 32-bit count. For 1,000,001 input elements,
        // this allocates 2,097,152 slots, or 16 MiB of slot storage.
        table_.assign(slotCount, Slot{});

        for (std::int32_t value : largeValues) {
            std::size_t slotIndex = startingSlotFor(value);
            for (;;) {
                Slot& slot = table_[slotIndex];
                if (slot.occurrenceCount == 0) {
                    slot.value = value;
                    slot.occurrenceCount = 1;
                    break;
                }
                if (slot.value == value) {
                    // Duplicates increase a stored count rather than occupy new slots.
                    ++slot.occurrenceCount;
                    break;
                }

                // LINEAR PROBING (insertion): a different value occupies this slot.
                // Try the immediately following slot, wrapping at the end. The loop
                // repeats until it finds this value or an empty slot.
                slotIndex = (slotIndex + 1) & slotIndexMask_;
            }
        }
    }

    [[nodiscard]] std::vector<std::int32_t>
    common(std::span<const std::int32_t> smallValues) const {
        // Sort a copy so equal values become contiguous groups. This preserves
        // the caller's input and needs O(M log M) work for M small-input elements.
        // It avoids a second hash map for tracking small-input duplicate counts.
        std::vector<std::int32_t> sortedQuery(smallValues.begin(), smallValues.end());
        std::ranges::sort(sortedQuery);

        // The result cannot have more elements than the small input. Together with
        // sortedQuery, this is per-query vector storage, separate from the fixed table.
        std::vector<std::int32_t> result;
        result.reserve(sortedQuery.size());

        for (std::size_t groupBegin = 0; groupBegin < sortedQuery.size();) {
            const std::int32_t value = sortedQuery[groupBegin];
            std::size_t groupEnd = groupBegin + 1;
            while (groupEnd < sortedQuery.size() && sortedQuery[groupEnd] == value) {
                ++groupEnd;
            }
            const std::size_t smallOccurrenceCount = groupEnd - groupBegin;

            // One large-table lookup per distinct query value. For example, four
            // query copies and three stored copies yield three output copies.
            // Stored counts are read, never consumed, so later queries see the same data.
            if (const std::uint32_t largeOccurrenceCount = countInLargeInput(value)) {
                result.insert(result.end(),
                              std::min<std::size_t>(smallOccurrenceCount, largeOccurrenceCount),
                              value);
            }
            groupBegin = groupEnd;
        }
        return result;
    }

    // One-shot intersection: no large-side index needs to outlive this call.
    // Static lets callers use this without first constructing a large index.
    [[nodiscard]] static std::vector<std::int32_t>
    commonOnce(std::span<const std::int32_t> largeValues,
               std::span<const std::int32_t> smallValues) {
        if (largeValues.empty() || smallValues.empty()) return {};
        const auto countedValues = largeValues.size() < smallValues.size() ? largeValues : smallValues;
        const auto scannedValues = largeValues.size() < smallValues.size() ? smallValues : largeValues;

        // Reuse the flat-table constructor on the SMALLER input. A 100-element
        // input needs 256 slots, instead of 2,097,152 for a million-element index.
        const CommonValuesFinder smallIndex(countedValues);
        std::vector<std::uint32_t> remainingCounts;
        remainingCounts.reserve(smallIndex.table_.size());
        for (const auto& slot : smallIndex.table_) remainingCounts.push_back(slot.occurrenceCount);

        // Keep original occupancy intact: zeroing a table count after consuming
        // its last occurrence would falsely mark the slot empty, hiding keys
        // further along the same probe chain. Separate remaining counts avoid
        // tombstones and preserve the existing 8-byte slot layout.
        std::vector<std::int32_t> result;
        result.reserve(countedValues.size());
        std::size_t remainingOccurrences = countedValues.size();
        for (const auto value : scannedValues) {
            auto slotIndex = smallIndex.startingSlotFor(value);
            while (smallIndex.table_[slotIndex].occurrenceCount != 0) {
                if (smallIndex.table_[slotIndex].value == value) {
                    if (remainingCounts[slotIndex] != 0) {
                        result.push_back(value);
                        --remainingCounts[slotIndex];
                        --remainingOccurrences;
                    }
                    break;
                }
                slotIndex = (slotIndex + 1) & smallIndex.slotIndexMask_;
            }
            // Missing or underrepresented values require scanning the whole input.
            if (remainingOccurrences == 0) break;
        }
        // Preserve the sorted-output contract by sorting at most M results,
        // not the N-element large input. Neither caller-owned vector is modified.
        std::ranges::sort(result);
        return result;
    }

    // One-shot tradeoffs: expected O(N + M + K log K) time and O(M) extra space,
    // where M is the smaller size and K <= M the output size; collisions can
    // worsen probing. This keeps allocations contiguous, but requires more
    // bookkeeping than an unordered_map. Sorting both disposable inputs in place
    // avoids a count map but costs O(N log N) on an unsorted large input. Already
    // sorted inputs favor a two-pointer merge. Repeated queries favor common()
    // with a reusable large-side index, avoiding another full scan each time.

private:
    struct Slot {
        std::int32_t value = 0;
        // Only the count marks emptiness: 0, negative values and INT_MIN are valid keys.
        std::uint32_t occurrenceCount = 0;
    };

    // Multiplicative hashing maps arbitrary signed integers into this smaller table.
    // Unsigned conversion handles negatives; multiplication wraps modulo 2^32.
    // Keeping the high bits spreads values across starting slots. Different values
    // can still select the same slot, so both insertion and lookup must probe.
    [[nodiscard]] std::size_t startingSlotFor(std::int32_t value) const noexcept {
        // Also used as xxHash's XXH_PRIME32_1. This odd multiplier is near the
        // golden-ratio-derived floor(2^32 / phi) = 0x9E3779B9, but is not identical.
        // Multiplication is followed by high-bit selection, not a low-bit mask.
        // Reference: https://xxhash.com/doc/v0.8.2/xxhash_8h_source.html#l02560
        constexpr std::uint32_t hashMultiplier = 0x9E3779B1u;
        const std::uint32_t mixedValue = static_cast<std::uint32_t>(value) * hashMultiplier;
        return mixedValue >> discardedHashBits_;
    }

    [[nodiscard]] std::uint32_t countInLargeInput(std::int32_t value) const noexcept {
        std::size_t slotIndex = startingSlotFor(value);
        for (;;) {
            const Slot& slot = table_[slotIndex];

            // No entries are deleted. Insertion would have used this empty slot
            // before any later slot in the probe sequence, so the value is absent.
            if (slot.occurrenceCount == 0) return 0;
            if (slot.value == value) return slot.occurrenceCount;

            // LINEAR PROBING (lookup): follow exactly the same consecutive-slot
            // sequence as insertion. Checking only the starting slot could miss a
            // value displaced by a collision.
            slotIndex = (slotIndex + 1) & slotIndexMask_;
        }
    }

    unsigned discardedHashBits_;
    std::size_t slotIndexMask_;
    std::vector<Slot> table_;
};
