find the common values in two vectors
======================================

There are two scenarios: For both we need to find the common values. There are two vectors: A large, unsorted vector with over 1M integers, and a small, unsorted vector with no more than 100 integers.

1) In the first scenario, let’s assume that the large vector will rarely change. Build a performance- optimized class which takes the large vector in its constructor, and a member function which takes a passed-in smaller vector and returns the common values.
2 ) In the second scenario, let’s assume that both vectors are expected to be short-lived (i.e., looked up once and then immediately destroyed). Write an alternative function you would use to optimize performance in this case.
Please include comments regarding alternative implementations and tradeoffs (i.e., why you chose a particular approach).

There are two approaches that have been narrowed down after considering several approaches. Here is the brief outline of the considerations

Brute force approach would have resulted in O (n * m) time complexity - Not considered

1. Wanted to use vector sorting approach but that would have been O (n log n + m log m) where n is the size of large vector(1M or greater) and m is the size of small vector
2. Condsidered using unordered_set but as there are duplicates allowed switched to unordered_multiset
3. FindCommonVals3 - unordered_map with count of value and occurrence gives a O(1) lookup then sorting on the small vector O (m log m) since its 100 numbers much less than N.
     Expected O(N + M + K log K) time, O(M) extra space, for smaller size M and output size K <= M; pathological hash collisions can worsen lookup time.
     unordered_map is simple, supports every int32 key, and needs no custom probing.reserve avoids growth-related rehashes, but distinct keys still allocate nodes.
4. Best approach is A fixed-capacity hash table stores one {value, occurrenceCount} entry per distinct large-input value. All entries live in one contiguous vector;
      construction does not grow/rehash the table or allocate a node for each key. Queries leave the table unchanged, so one constructed finder serves many queries.
      Carefully construct the slot count with capacity, mask and discardedHashBits_(by shifting) after multiplier to calculate the slotIndex.
      Then use LINEAR PROBING (for both and insertion and lookup until the empty slot or zero occurrence is hit). Thus, the time complexity here with	Constructor is O(N) average time
      to build unordered_map<value,count>. And the common() method: O(M + R) average time, where R is the number of values returned (total duplicates produced).
