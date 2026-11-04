# ADR-001: Hash Table-Based Cache Storage

**Status:** Accepted

## Context
hardKV requires efficient key-value storage with concurrent access and support for cache eviction.

## Decision
Use a hash table with separate chaining for cache storage.

## Rationale
- Provides efficient average-case key lookup, insertion, and deletion.
- Supports partitioning synchronization by bucket.
- Allows independent operations on different buckets to proceed concurrently.
- Provides a straightforward foundation for eviction policies.

## Consequences
- Hash collisions require linked-list traversal.
- Performance depends on hash distribution and load factor.
- Bucket-level synchronization introduces contention when multiple operations target the same bucket.

