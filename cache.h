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
CacheLevel* init_cache_level(int cache_size, int associativity, int line_size, int access_latency, ReplacementPolicy policy);
void free_cache_level(CacheLevel *cache);

/* ---------------- Cache Access Simulation ---------------- */
/* Extended to support L1, L2, L3, and L4 cache levels */
int simulate_memory_access(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, 
                           unsigned int paddr, int is_write, unsigned long current_time, int *hit_level);

/* ---------------- Cache Maintenance ---------------- */
void flush_cache_line(CacheLevel *cache, unsigned int paddr);
void flush_instruction(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, unsigned int paddr);
int simulate_prefetch(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, 
                      unsigned int paddr, unsigned long current_time, int *hit_level);

#endif /* CACHE_SIMULATOR_H */