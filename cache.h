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
    void (*update_policy)(CacheSet *set, int line_index, unsigned long current_time);
    int (*find_victim)(CacheSet *set);
} CacheLevel;

/* ---------------- Replacement Policy Function Prototypes ---------------- */
void update_policy_lru(CacheSet *set, int line_index, unsigned long current_time);
int find_victim_lru(CacheSet *set);

void update_policy_bip(CacheSet *set, int line_index, unsigned long current_time);
int find_victim_bip(CacheSet *set);

/* ---------------- Cache Level Initialization ---------------- */
/*
 * Initializes a cache level with the given parameters. Returns a pointer to a
 * CacheLevel structure or exits on allocation error.
 */
CacheLevel* init_cache_level(int cache_size, int associativity, int line_size, int access_latency, ReplacementPolicy policy);
void free_cache_level(CacheLevel *cache);

/* ---------------- Cache Access Simulation ---------------- */
/*
 * simulate_memory_access() simulates a memory access through the hierarchy.
 * Each cache level pointer (l1, l2, l3, l4) is optional (pass NULL if omitted).
 * Levels are checked in order: L1, then L2, then L3, then L4, then main memory.
 *
 * On a hit in any level, the function updates that level and (if applicable)
 * brings the block into higher levels (if present). The accumulated latency
 * is returned and *hit_level is set to:
 *   1: hit in L1, 2: hit in L2, 3: hit in L3, 4: hit in L4, 0: miss (accessed main memory).
 */
int simulate_memory_access(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, 
                           unsigned int paddr, int is_write, unsigned long current_time, int *hit_level);

/*
 * simulate_prefetch() simulates a prefetch operation into the given L1 cache.
 * The L1 pointer is where the block is to be inserted. The other levels are optional.
 *
 * Returns the latency incurred during the prefetch and sets *hit_level similarly
 * to simulate_memory_access().
 */
int simulate_prefetch(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, 
                      unsigned int paddr, unsigned long current_time, int *hit_level);

/* ---------------- Cache Maintenance ---------------- */
/*
 * flush_cache_line() flushes (invalidates) the cache line corresponding to the given
 * physical address in a single cache level.
 */
void flush_cache_line(CacheLevel *cache, unsigned int paddr);

/*
 * flush_instruction() flushes the cache line for an instruction fetch from the
 * instruction L1 cache and from the unified lower-level caches (if present).
 */
void flush_instruction(CacheLevel *l1_instr, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, unsigned int paddr);

/*
 * flush_data() flushes the cache line for a data access from the data L1 cache and
 * from the unified lower-level caches (if present).
 */
void flush_data(CacheLevel *l1_data, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, unsigned int paddr);

#endif /* CACHE_SIMULATOR_H */