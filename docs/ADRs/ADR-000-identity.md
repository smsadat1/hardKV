# ADR-000: First-Class Cache and Persistent Storage

**Status:** Accepted

## Context

hardKV is designed as a key-value system with both in-memory caching and persistent storage.

Traditional cache-backed architectures often treat persistence as a supplementary mechanism, while persistent databases treat caching primarily as a performance optimization.

hardKV requires both components to have independent architectural responsibilities.

## Decision

Treat in-memory caching and persistent storage as first-class components of hardKV.

Cache residency and persistent record existence are independent states.

Eviction removes entries from cache memory without implicitly deleting their persistent records.

## Rationale

- Allows cache eviction without compromising persistent data.
- Establishes clear separation between memory management and persistent data ownership.
- Enables independent cache and disk synchronization.
- Supports recovery of persistent records into cache memory when required.

## Consequences

- Cache eviction and persistent deletion must remain separate operations.
- Cache misses may require persistent storage access.
- Recovery and cache population require explicit coordination.
- Persistent storage lifecycle cannot be governed solely by cache eviction policies.