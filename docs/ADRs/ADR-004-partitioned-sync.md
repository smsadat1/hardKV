# ADR-004: Partitioned Synchronization

**Status:** Accepted

## Context
A global mutex would serialize unrelated cache operations and disk operations.

## Decision
Use bucket-specific mutexes for cache operations and partition-specific mutexes for disk operations.

## Rationale
- Allows unrelated cache buckets to be accessed concurrently.
- Allows disk operations targeting different partitions to proceed independently.
- Reduces contention compared to global synchronization.

## Consequences
- Introduces more synchronization primitives to initialize and destroy.
- Requires careful lock ordering when operations involve multiple synchronization domains.
- Operations targeting the same bucket or disk partition remain serialized.

