# ADR-002: Dedicated Eviction Worker

**Status:** Accepted

## Context
hardKV supports configurable eviction policies, including LRU and LFU.

Eviction should occur independently of ordinary cache operations when memory usage exceeds the configured limit.

## Decision
Use a dedicated eviction worker thread responsible for monitoring memory usage and evicting entries according to the configured policy.

## Rationale
- Separates eviction responsibilities from client request processing.
- Avoids performing potentially expensive eviction work directly in request handlers.
- Provides a centralized location for eviction policy management.

## Consequences
- Introduces additional thread lifecycle and synchronization complexity.
- Requires coordination between cache operations and the eviction worker.
- Memory accounting must remain consistent across concurrent operations.

