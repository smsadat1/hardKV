# ADR-003: Append-Only, Hash-Partitioned Binary Records

**Status:** Accepted

## Context
hardKV requires persistent storage capable of recovering key-value data after process termination.

## Decision
Use append-only binary records organized into hash-based partitions.

## Rationale
- Avoids repeatedly rewriting existing records during ordinary updates.
- Supports sequential writes.
- Hash partitioning allows disk operations to be synchronized independently.
- Provides a foundation for replay-based recovery.

## Consequences
- Deleted and superseded records may remain in the append-only storage.
- Recovery requires identifying the latest valid state of each key.
- Storage reclamation and compaction may become necessary as the record history grows.
- Crash consistency and incomplete record handling must be addressed.

