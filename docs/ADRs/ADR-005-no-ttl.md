# ADR-005: No TTL; Explicit Eviction Instead

**Status:** Accepted

## Context
hardKV combines cache eviction with persistent storage.

Traditional TTL-based expiration introduces additional complexity when expired entries must be reconciled with persistent records.

Two approaches were considered:

**Lazy expiration**
- Check expiration during GET.
- Remove expired cache entries.
- Retrieve the persistent record.
- Reload the cache.

This introduces additional lookup and synchronization work on expired entries.

**Background expiration worker**
- Periodically inspect entries.
- Remove expired entries.
- Introduce another worker or additional responsibilities for an existing worker.

This increases synchronization and lifecycle complexity.

## Decision
Do not implement TTL-based expiration.

Use explicit eviction through the `EVICT` operation and configured eviction policies.

## Rationale
- Simplifies cache lifecycle management.
- Avoids expiration checks during ordinary GET operations.
- Avoids introducing another background worker solely for expiration.
- Separates cache eviction from persistent data deletion.

## Consequences
- Entries do not expire automatically based on time.
- Cache reclamation depends on eviction policies or explicit operations.
- Eviction must not implicitly delete persistent records.

