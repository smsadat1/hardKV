# 1. Core operations — must pass first

| Test                | Action                              | Expected result                               |
|---------------------|-------------------------------------|-----------------------------------------------|
| Set a key           | `SET apple 123`                     | `OK`                                          |
| Read a key          | `GET apple`                         | `123`                                         |
| Read missing key    | `GET missing`                       | `(nil)`                                       |
| Update a key        | `SET apple 456`                     | `OK`; subsequent `GET` returns `456`          |
| Delete a key        | `DELETE apple`                      | `OK`                                          |
| Read deleted key    | `GET apple`                         | `(nil)`                                       |
| Delete missing key  | `DELETE missing`                    | Consistent missing-key response               |
| Check existence     | `EXISTS apple`                      | Correct result before and after deletion      |
| Empty value         | `SET empty ""`                      | `FAILED`                                      |
| Special characters  | Keys/values containing punctuation  | Correct round-trip                            |
| Long key            | Key with 1,000 characters           | Works or returns a documented error           |
| Short key           | Key with 1 character                | Correct round trip                            |
| Long value          | Value with 1 MB of data             | Works within configured limits                |

---

# 2. TTL and expiration

| Test                  | Action                                            | Expected result                                 |
|-----------------------|---------------------------------------------------|-------------------------------------------------|
| Set TTL               | `EXPIRE apple 10`                                 | Expiration set                                  |
| Check TTL             | `TTL apple`                                       | Positive remaining time                         |
| Wait for expiration   | Wait 11 seconds, then `GET apple`                 | Key treated as missing                          |
| TTL on missing key    | `TTL missing`                                     | Documented missing-key response                 |
| TTL without expiry    | `SET apple 123`, then `TTL apple`                 | Documented no-expiry response                   |
| Reset TTL             | Set TTL to 30, then 60                            | Expiration updated                              |
| Replace value         | Update a key that has a TTL                       | TTL behavior matches your documented semantics  |
| Delete expiring key   | Delete before expiration                          | Key is removed; no later double-free            |
| Expire then recreate  | Let a key expire, then insert the same key again  | New entry works normally                        |
| Zero TTL              | `EXPIRE apple 0`                                  | Immediate expiration or a documented error      |
| Negative TTL          | `EXPIRE apple -5`                                 | Rejected or handled consistently                |


---

# 3. Memory accounting and eviction

Test that accounting stays correct.

| Test            | Action                                          | Expected result                           |
|-----------------|-------------------------------------------------|-------------------------------------------|
| Insert          | Insert a 10-byte value                          | Usage increases by 10                     |
| Update          | Replace it with a 20-byte value                 | Usage increases by a net 10               |
| Delete          | Delete the key                                  | Usage decreases by 20                     |
| Expire          | Let the key expire and trigger cleanup          | Usage decreases exactly once              |
| Evict           | Force an eviction                               | Usage decreases by the victim's value size|
| Empty value     | Insert an empty value, if supported             | Usage remains consistent                  |
| Repeated update | Update the same key 1,000 times                 | No accounting drift                       |
| Oversized value | Insert a value larger than the allowed maximum  | Rejected without corrupting the cache     |


### Test the actual memory limit

Run TinyCache with a deliberately tiny memory limit.

For example, if the maximum is 100 bytes:

1. Insert values totaling 80 bytes.
2. Insert another value requiring 30 bytes.
3. Verify the configured policy evicts enough entries or rejects the write.
4. Verify `mem_used <= mem_max`.
5. Verify all surviving keys still return the correct values.

If your policy rejects values larger than `mem_max / 2`, test that boundary too:

- Value size exactly at the limit.
- Value size one byte above it.
- Value too large when the cache is empty.
- Value too large when the cache contains other entries.

**Important:** decide whether an oversized write is rejected before any eviction. I'd recommend rejecting it first so one impossible write can't empty the cache.

---

# 4. LRU eviction

For LRU, the key property is:

> Evict the entry that has gone the longest without being accessed.

Use a tiny memory limit so eviction happens quickly.

### Test A: Basic LRU

Assume each value uses 10 bytes and the cache can hold three entries.

```text
SET a 1111111111
SET b 2222222222
SET c 3333333333
```

Now access `a`:

```text
GET a
```

Then insert `d`:

```text
SET d 4444444444
```

Expected state:

| Key | Expected |
|---|---|
| `a` | Present |
| `b` | Evicted |
| `c` | Present |
| `d` | Present |

Why? Accessing `a` makes it recently used, so `b` becomes the least recently used entry.

### Test B: Updating an existing key

1. Insert `a`, `b`, and `c`.
2. Access `a`.
3. Update `b`.
4. Insert another key to force eviction.

Decide whether updating an existing key counts as an access. I'd recommend that it does, moving `b` to the MRU position.

### Test C: LRU linked-list edge cases

Test eviction when:
- The cache has one entry.
- The victim is the tail.
- The accessed entry is already the head.
- The list has exactly two entries.
- An entry is deleted from the middle.
- An entry expires while it's in the list.
- An entry is updated repeatedly.

These cases are important because pointer bugs often occur at the head and tail.

---

# 5. LFU eviction

For LFU, the rule changes:

> Evict the entry with the lowest access frequency.

Your current `uint32_t frequency` field is enough to implement a simple version.

### Test A: Different frequencies

Insert three keys:

```text
SET a alpha
SET b bravo
SET c charlie
```

Access them different numbers of times:

```text
GET a
GET a
GET a
GET b
GET b
GET c
```

Ignoring insertion frequency for this example, their access counts are:

| Key | Accesses |
|---|---:|
| `a` | 3 |
| `b` | 2 |
| `c` | 1 |

Force an eviction.

**Expected:** `c` is evicted.

Make sure your implementation consistently defines which operations count as accesses. I'd count successful `GET` operations and decide explicitly whether `EXISTS` and updating an existing key increment frequency.

### Test B: Frequency ties

Insert several keys and access each the same number of times.

Force an eviction.

If you're using LRU as the tie-breaker, the least recently used among the tied entries should be evicted.

### Test C: Frequency survives updates

Decide what happens when you update an existing key.

For a straightforward cache, I'd preserve the existing entry's frequency and treat the update as an access. Alternatively, you can reset frequency, but document the behavior and test it.

### Test D: Expiration under LFU

1. Insert two entries with different frequencies.
2. Give one a TTL.
3. Let it expire.
4. Trigger expiration cleanup.
5. Force eviction.

Verify that the expired entry is removed correctly and isn't selected as a live eviction victim.

### Test E: Counter overflow

Your `uint32_t` counter can overflow. Either saturate it at `UINT32_MAX` or define another aging strategy.