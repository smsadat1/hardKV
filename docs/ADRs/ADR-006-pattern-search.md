# ADR-006: Pattern-Based Key Enumeration Without Pattern-Based Deletion

**Status:** Accepted

## Context
hardKV requires key enumeration and pattern-based searching.

Pattern-based deletion presents a significant risk when persistent records are involved.

## Decision
Implement pattern-based key enumeration through `LIST`, while keeping deletion restricted to exact key matches.

## Rationale
- Allows users to discover keys matching a pattern.
- Avoids accidental deletion of multiple persistent records through wildcard matching.
- Uses hash-table traversal instead of introducing a secondary indexing structure.
- Bucket-specific mutexes allow unrelated cache operations to proceed concurrently during enumeration.

## Consequences
- Pattern enumeration requires scanning the hash table.
- Enumeration has O(N) complexity in the number of cache entries.
- Operations targeting scanned buckets may experience lock contention.
- Pattern-based deletion is intentionally unsupported.