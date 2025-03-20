#ifndef CACHE_SIMULATOR_H
#define CACHE_SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define MEM_LATENCY 100  // main memory penalty in cycles

/* ---------------- Replacement Policy Definitions ---------------- */

typedef enum {
    POLICY_LRU,
    POLICY_BIP
} ReplacementPolicy;

typedef struct {
    unsigned int tag;
    int valid;
    unsigned long last_access_time;  // used for LRU/BIP
} CacheLine;

typedef struct {
    int num_lines;         // equals associativity
    CacheLine *lines;      // dynamically allocated array of cache lines
} CacheSet;

typedef struct CacheLevel {
    int cache_size;        // in bytes
    int associativity;     // number of lines per set
    int line_size;         // in bytes
    int num_sets;          // computed as cache_size / (line_size * associativity)
    int access_latency;    // in cycles (access time)
    ReplacementPolicy policy;
    CacheSet *sets;        // array of sets

    // Function pointers for replacement policy operations:
    void (*update_policy)(CacheSet *set, int line_index);
    int (*find_victim)(CacheSet *set);
} CacheLevel;

/* ---------------- Replacement Policy Function Prototypes ---------------- */
void update_policy_lru(CacheSet *set, int line_index);
int find_victim_lru(CacheSet *set);

void update_policy_bip(CacheSet *set, int line_index);
int find_victim_bip(CacheSet *set);

/* ---------------- Cache Level Initialization ---------------- */
CacheLevel* init_cache_level(int cache_size, int associativity, int line_size, int access_latency, ReplacementPolicy policy);
void free_cache_level(CacheLevel *cache);

/* ---------------- Cache Simulator API ---------------- */
/*
 * init() instantiates the cache levels (using hard-coded values),
 * registers them with the simulator, and resets internal counters.
 */
void init();

/*
 * end() writes a report to a file called "results.log" and frees all cache memory.
 */
void end();

/*
 * simulate_memory_access() simulates a memory access.
 *
 * Parameters:
 *  - vaddr: virtual address (used for tagging, etc.)
 *  - paddr: physical address (used for cache lookup)
 *  - access_type: use -1 for an instruction access (instruction cache) and any other value for a data access (data cache)
 *
 * Returns the total latency (in cycles) incurred by the access.
 * (Global counters are updated internally.)
 */
int simulate_memory_access(unsigned int vaddr, unsigned int paddr, int access_type);

/*
 * simulate_prefetch() works similarly to simulate_memory_access() but only inserts a block into the L1 cache.
 * Use the same access_type flag (-1 for instruction, otherwise data).
 *
 * Returns the latency incurred by the prefetch.
 */
int simulate_prefetch(unsigned int vaddr, unsigned int paddr, int access_type);

/*
 * flush_instruction() flushes (invalidates) the cache line corresponding to the given physical address
 * from the instruction cache (if present) and all lower levels.
 */
void flush_instruction(unsigned int paddr);

/*
 * flush_data() flushes (invalidates) the cache line corresponding to the given physical address
 * from the data cache (if present) and all lower levels.
 */
void flush_data(unsigned int paddr);

#endif /* CACHE_SIMULATOR_H */