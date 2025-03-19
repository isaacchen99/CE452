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

/* LRU: update simply records current time */
void update_policy_lru(CacheSet *set, int line_index, unsigned long current_time) {
    set->lines[line_index].last_access_time = current_time;
}

/* LRU: victim is the line with the smallest last_access_time */
int find_victim_lru(CacheSet *set) {
    int victim = 0;
    unsigned long min_time = set->lines[0].last_access_time;
    for (int i = 1; i < set->num_lines; i++) {
        if (set->lines[i].last_access_time < min_time) {
            min_time = set->lines[i].last_access_time;
            victim = i;
        }
    }
    return victim;
}

/* Simple BIP update: here we update with half the current time to simulate lower recency.
   (A real BIP would probabilistically update recency on hit.) */
void update_policy_bip(CacheSet *set, int line_index, unsigned long current_time) {
    set->lines[line_index].last_access_time = current_time / 2;
}

/* Use the same victim selection as LRU */
int find_victim_bip(CacheSet *set) {
    return find_victim_lru(set);
}

/* ---------------- Cache Level Initialization ---------------- */

CacheLevel* init_cache_level(int cache_size, int associativity, int line_size, int access_latency, ReplacementPolicy policy) {
    CacheLevel *cache = malloc(sizeof(CacheLevel));
    if (!cache) { perror("malloc"); exit(1); }
    cache->cache_size = cache_size;
    cache->associativity = associativity;
    cache->line_size = line_size;
    cache->access_latency = access_latency;
    cache->num_sets = cache_size / (line_size * associativity);
    cache->policy = policy;
    
    cache->sets = malloc(sizeof(CacheSet) * cache->num_sets);
    if (!cache->sets) { perror("malloc"); exit(1); }
    for (int i = 0; i < cache->num_sets; i++) {
        cache->sets[i].num_lines = associativity;
        cache->sets[i].lines = malloc(sizeof(CacheLine) * associativity);
        if (!cache->sets[i].lines) { perror("malloc"); exit(1); }
        for (int j = 0; j < associativity; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].last_access_time = 0;
        }
    }
    
    switch(policy) {
        case POLICY_LRU:
            cache->update_policy = update_policy_lru;
            cache->find_victim = find_victim_lru;
            break;
        case POLICY_BIP:
            cache->update_policy = update_policy_bip;
            cache->find_victim = find_victim_bip;
            break;
        default:
            cache->update_policy = update_policy_lru;
            cache->find_victim = find_victim_lru;
    }
    
    return cache;
}

void free_cache_level(CacheLevel *cache) {
    if (cache) {
        for (int i = 0; i < cache->num_sets; i++) {
            free(cache->sets[i].lines);
        }
        free(cache->sets);
        free(cache);
    }
}

/* ---------------- Cache Access Simulation ---------------- */

/*
 * simulate_memory_access:
 *   Checks L1 first; on a miss, checks L2.
 *   If L1 hits, returns L1 latency.
 *   If L2 hits, returns L1_latency + L2_latency (and inserts the block into L1).
 *   If miss in both, returns L1_latency + L2_latency + MEM_LATENCY and brings the block into both L2 and L1.
 *   'hit_level' is set to 1 for L1 hit, 2 for L2 hit, and 0 for miss in both.
 */
int simulate_memory_access(CacheLevel *l1, CacheLevel *l2, unsigned int paddr, int is_write, 
                             unsigned long current_time, int *hit_level) {
    int latency = 0;
    
    /* Compute L1 set index and tag */
    int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    
    // Check L1 for a hit.
    for (int i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
            l1->update_policy(l1_set, i, current_time);
            latency += l1->access_latency;
            *hit_level = 1;
            return latency;
        }
    }
    // L1 miss
    latency += l1->access_latency;
    
    /* Check L2 */
    int l2_set_index = (paddr / l2->line_size) % l2->num_sets;
    unsigned int l2_tag = paddr / (l2->line_size * l2->num_sets);
    CacheSet *l2_set = &l2->sets[l2_set_index];
    int found_in_l2 = 0;
    for (int i = 0; i < l2_set->num_lines; i++) {
        if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
            l2->update_policy(l2_set, i, current_time);
            latency += l2->access_latency;
            found_in_l2 = 1;
            *hit_level = 2;
            break;
        }
    }
    if (!found_in_l2) {
        latency += l2->access_latency;
        latency += MEM_LATENCY;  // main memory access penalty
        *hit_level = 0;
        // Insert block into L2 (simulate replacement)
        int victim_l2 = l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = current_time;
    }
    
    // In either case, bring the block into L1 (simulate inclusive cache).
    int victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = current_time;
    
    return latency;
}

/* ---------------- Cache Maintenance: Flush and Prefetch ---------------- */

/*
 * flush_cache_line: Given a cache level and a physical address,
 * searches for the cache line and invalidates it.
 */
void flush_cache_line(CacheLevel *cache, unsigned int paddr) {
    int set_index = (paddr / cache->line_size) % cache->num_sets;
    unsigned int tag = paddr / (cache->line_size * cache->num_sets);
    CacheSet *set = &cache->sets[set_index];
    for (int i = 0; i < set->num_lines; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            set->lines[i].valid = 0;
            break;
        }
    }
}

/*
 * flush_instruction: Flush the cache line from both L1 and L2.
 */
void flush_instruction(CacheLevel *l1, CacheLevel *l2, unsigned int paddr) {
    flush_cache_line(l1, paddr);
    flush_cache_line(l2, paddr);
}

/*
 * simulate_prefetch:
 *   Checks if the block is already in L1 (if so, returns a prefetch hit with 0 added latency).
 *   Otherwise, checks L2 and then (if needed) simulates a main-memory access and inserts
 *   the block into both L2 and L1.
 *   Returns the latency cost for the prefetch and sets *hit_level accordingly.
 */
int simulate_prefetch(CacheLevel *l1, CacheLevel *l2, unsigned int paddr, 
                      unsigned long current_time, int *hit_level) {
    int latency = 0;
    int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    
    // Check if already in L1.
    for (int i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
            *hit_level = 1;
            return 0;  // already present; no latency incurred.
        }
    }
    
    // Not in L1; check L2.
    int l2_set_index = (paddr / l2->line_size) % l2->num_sets;
    unsigned int l2_tag = paddr / (l2->line_size * l2->num_sets);
    CacheSet *l2_set = &l2->sets[l2_set_index];
    int found_in_l2 = 0;
    for (int i = 0; i < l2_set->num_lines; i++) {
        if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
            l2->update_policy(l2_set, i, current_time);
            latency += l2->access_latency;
            found_in_l2 = 1;
            *hit_level = 2;
            break;
        }
    }
    if (!found_in_l2) {
        latency += l2->access_latency;
        latency += MEM_LATENCY;
        *hit_level = 0;
        // Insert block into L2.
        int victim_l2 = l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = current_time;
    }
    // Insert block into L1.
    int victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = current_time;
    latency += l1->access_latency;
    
    return latency;
}

/* ---------------- Main Simulator ---------------- */

int main() {
    /* Configure two cache levels.
       (x86 systems typically have split L1 data/instruction caches;
        here we simulate a unified L1 and an L2. You can extend this as needed.) */
    int l1_size = 32 * 1024;       // 32 KB
    int l1_assoc = 8;
    int l1_line = 64;
    int l1_latency = 1;
    
    int l2_size = 256 * 1024;      // 256 KB
    int l2_assoc = 8;
    int l2_line = 64;
    int l2_latency = 10;
    
    CacheLevel *l1 = init_cache_level(l1_size, l1_assoc, l1_line, l1_latency, POLICY_LRU);
    CacheLevel *l2 = init_cache_level(l2_size, l2_assoc, l2_line, l2_latency, POLICY_LRU);
    
    unsigned long current_time = 0;
    
    /* Statistics for normal memory accesses (R/W) */
    int mem_accesses = 0;
    int l1_hits = 0, l1_misses = 0;
    int l2_hits = 0, l2_misses = 0;
    unsigned long total_latency = 0;
    
    char op;
    unsigned int vaddr, paddr;
    
    printf("Enter commands:\n");
    printf("  R <vaddr> <paddr> : Read access\n");
    printf("  W <vaddr> <paddr> : Write access\n");
    printf("  F <vaddr> <paddr> : Flush cache line (from all levels)\n");
    printf("  P <vaddr> <paddr> : Prefetch cache line\n");
    printf("Ctrl+D (or Ctrl+Z on Windows) to end input.\n\n");
    
    while (scanf(" %c %x %x", &op, &vaddr, &paddr) == 3) {
        current_time++;
        if (op == 'R' || op == 'W') {
            mem_accesses++;
            int hit_level = 0;
            int latency = simulate_memory_access(l1, l2, paddr, (op == 'W'), current_time, &hit_level);
            total_latency += latency;
            if (hit_level == 1) {
                l1_hits++;
                printf("%c access at 0x%x: L1 hit, latency = %d cycles\n", op, paddr, latency);
            } else if (hit_level == 2) {
                l1_misses++;  /* L1 missed but L2 hit */
                l2_hits++;
                printf("%c access at 0x%x: L1 miss, L2 hit, latency = %d cycles\n", op, paddr, latency);
            } else {
                l1_misses++;
                l2_misses++;
                printf("%c access at 0x%x: Miss in L1 and L2, latency = %d cycles\n", op, paddr, latency);
            }
        } else if (op == 'F') {
            flush_instruction(l1, l2, paddr);
            printf("Flush command for address 0x%x executed.\n", paddr);
        } else if (op == 'P') {
            int hit_level = 0;
            int latency = simulate_prefetch(l1, l2, paddr, current_time, &hit_level);
            if (hit_level == 1) {
                printf("Prefetch for address 0x%x: already in L1 (prefetch hit), latency = %d cycles\n", paddr, latency);
            } else if (hit_level == 2) {
                printf("Prefetch for address 0x%x: present in L2 (prefetch hit), latency = %d cycles\n", paddr, latency);
            } else {
                printf("Prefetch for address 0x%x: not present, fetched from memory, latency = %d cycles\n", paddr, latency);
            }
        } else {
            printf("Unknown command: %c\n", op);
        }
    }
    
    /* Print overall simulation statistics for memory accesses */
    printf("\n--- Simulation Statistics ---\n");
    printf("Memory accesses (R/W): %d\n", mem_accesses);
    printf("L1 hits: %d, L1 misses: %d\n", l1_hits, l1_misses);
    printf("L2 hits: %d, L2 misses: %d\n", l2_hits, l2_misses);
    if (mem_accesses > 0)
        printf("Average access latency: %.2f cycles\n", (double)total_latency / mem_accesses);
    
    free_cache_level(l1);
    free_cache_level(l2);
    
    return 0;
}
