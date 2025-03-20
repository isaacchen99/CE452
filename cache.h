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

/* ---------------- Simulator API ---------------- */
/*
 * init()
 *   Instantiates the cache levels using hard-coded parameters and registers them.
 *   (This does not start counting simulation data.)
 */
void init(void);

/*
 * start()
 *   Resets internal simulation counters (number of accesses and latency totals)
 *   and begins counting data.
 */
void start(void);

/*
 * end()
 *   Stops the simulation (i.e. stops counting) and writes a report to "results.log".
 */
void end(void);

/*
 * deinit()
 *   Frees all allocated cache memory and performs final cleanup.
 */
void deinit(void);

/*
 * simulate_memory_access()
 *   Simulates a memory access.
 *
 * Parameters:
 *   - vaddr: virtual address (used for tagging, etc.)
 *   - paddr: physical address (used for cache lookup)
 *   - access_type: use 1 for an instruction access (instruction cache), 0 for a data access.
 *
 * Returns the total latency (in cycles) incurred by the access.
 * (Global counters are updated only if the simulation is active.)
 */
int simulate_memory_access(unsigned int vaddr, unsigned int paddr, int access_type);

/*
 * simulate_prefetch()
 *   Works similarly to simulate_memory_access() but only inserts a block into the L1 cache.
 *   Use the same access_type flag (1 for instruction, 0 for data).
 *
 * Returns the latency incurred by the prefetch.
 */
int simulate_prefetch(unsigned int vaddr, unsigned int paddr, int access_type);

/*
 * flush_instruction()
 *   Flushes (invalidates) the cache line corresponding to the given physical address
 *   from the instruction cache (if present) and from lower levels.
 */
void flush_instruction(unsigned int paddr);

/*
 * flush_data()
 *   Flushes (invalidates) the cache line corresponding to the given physical address
 *   from the data cache (if present) and from lower levels.
 */
void flush_data(unsigned int paddr);

#endif /* CACHE_SIMULATOR_H */