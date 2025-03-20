#include "cache.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------------- Internal Global Variables ---------------- */

/* Pointers to the cache levels */
static CacheLevel *g_l1_data = NULL;
static CacheLevel *g_l1_instr = NULL;
static CacheLevel *g_l2 = NULL;
static CacheLevel *g_l3 = NULL;
static CacheLevel *g_l4 = NULL;

/* Global internal time counter */
static unsigned long g_current_time = 0;

/* Global simulation counters */
static int g_mem_accesses = 0;
static int g_instr_accesses = 0;
static int g_data_accesses = 0;
static unsigned long g_total_latency_instr = 0;
static unsigned long g_total_latency_data = 0;

/* Flag to indicate whether simulation counting is active */
static int g_counting = 0;

/* ---------------- Cache Miss/Hit Statistics Counters ---------------- */
/* For L1, separate for instruction and data caches */
static int l1_data_accesses_stats = 0, l1_data_hits_stats = 0;
static int l1_instr_accesses_stats = 0, l1_instr_hits_stats = 0;
/* For lower levels */
static int l2_accesses_stats = 0, l2_hits_stats = 0;
static int l3_accesses_stats = 0, l3_hits_stats = 0;
static int l4_accesses_stats = 0, l4_hits_stats = 0;

/* ---------------- Replacement Policy Implementations ---------------- */

/* LRU: update simply records the current internal time */
void update_policy_lru(CacheSet *set, int line_index) {
    set->lines[line_index].last_access_time = g_current_time;
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

/* Simple BIP update: update with half the internal time */
void update_policy_bip(CacheSet *set, int line_index) {
    set->lines[line_index].last_access_time = g_current_time / 2;
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

/* ---------------- Simulator Internal Registration ---------------- */

/*
 * init_cache_simulator() registers the cache levels with the simulator.
 */
static void init_cache_simulator(CacheLevel *l1_data, CacheLevel *l1_instr, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4) {
    g_l1_data   = l1_data;
    g_l1_instr  = l1_instr;
    g_l2        = l2;
    g_l3        = l3;
    g_l4        = l4;
    g_current_time = 0;
}

/* ---------------- Simulator API Functions ---------------- */

/*
 * init()
 *   Instantiates the cache levels using hard-coded configuration and registers them.
 *   This does not start simulation counting.
 */
void init(void) {
    int use_l1 = 1;      // Instantiate L1 caches (data and instruction)
    int use_l2 = 1;
    int use_l3 = 1;
    int use_l4 = 1;
    
    /* Hard-coded cache configuration parameters */
    int l1_size = 32 * 1024;       // 32 KB for each L1 cache
    int l1_assoc = 8;
    int l1_line = 64;
    int l1_latency = 1;
    
    int l2_size = 256 * 1024;      // 256 KB
    int l2_assoc = 8;
    int l2_line = 64;
    int l2_latency = 10;
    
    int l3_size = 512 * 1024;      // 512 KB
    int l3_assoc = 8;
    int l3_line = 64;
    int l3_latency = 20;
    
    int l4_size = 2 * 1024 * 1024; // 2 MB
    int l4_assoc = 16;
    int l4_line = 64;
    int l4_latency = 40;
    
    /* Instantiate caches if enabled */
    if (use_l1) {
        g_l1_data = init_cache_level(l1_size, l1_assoc, l1_line, l1_latency, POLICY_LRU);
        g_l1_instr = init_cache_level(l1_size, l1_assoc, l1_line, l1_latency, POLICY_LRU);
    }
    if (use_l2)
        g_l2 = init_cache_level(l2_size, l2_assoc, l2_line, l2_latency, POLICY_LRU);
    if (use_l3)
        g_l3 = init_cache_level(l3_size, l3_assoc, l3_line, l3_latency, POLICY_LRU);
    if (use_l4)
        g_l4 = init_cache_level(l4_size, l4_assoc, l4_line, l4_latency, POLICY_LRU);
    
    /* Register the cache levels */
    init_cache_simulator(g_l1_data, g_l1_instr, g_l2, g_l3, g_l4);
    
    /* Do not start counting yet */
    g_counting = 0;
}

/*
 * start()
 *   Resets simulation counters and starts counting memory accesses.
 */
void start(void) {
    g_mem_accesses = 0;
    g_instr_accesses = 0;
    g_data_accesses = 0;
    g_total_latency_instr = 0;
    g_total_latency_data = 0;
    g_current_time = 0;
    g_counting = 1;

    /* Reset cache miss/hit statistics */
    l1_data_accesses_stats = l1_data_hits_stats = 0;
    l1_instr_accesses_stats = l1_instr_hits_stats = 0;
    l2_accesses_stats = l2_hits_stats = 0;
    l3_accesses_stats = l3_hits_stats = 0;
    l4_accesses_stats = l4_hits_stats = 0;
}

/*
 * end()
 *   Stops counting and writes simulation statistics to "results.log".
 */
void end(void) {
    FILE *fp = fopen("results.log", "a");
    if (!fp) {
        perror("fopen");
        exit(1);
    }
    
    fprintf(fp, "--- Simulation Statistics ---\n");
    fprintf(fp, "Total memory accesses: %d\n", g_mem_accesses);
    if (g_instr_accesses > 0)
        fprintf(fp, "Instruction accesses: average latency = %.2f cycles\n", (double)g_total_latency_instr / g_instr_accesses);
    else
        fprintf(fp, "Instruction accesses: none\n");
    if (g_data_accesses > 0)
        fprintf(fp, "Data accesses: average latency = %.2f cycles\n", (double)g_total_latency_data / g_data_accesses);
    else
        fprintf(fp, "Data accesses: none\n");

    /* Print cache miss rate per level */
    fprintf(fp, "\n--- Cache Miss Rates ---\n");
    if (l1_instr_accesses_stats > 0)
        fprintf(fp, "L1 Instruction: %.2f%% misses\n", 100.0 * (l1_instr_accesses_stats - l1_instr_hits_stats) / l1_instr_accesses_stats);
    if (l1_data_accesses_stats > 0)
        fprintf(fp, "L1 Data: %.2f%% misses\n", 100.0 * (l1_data_accesses_stats - l1_data_hits_stats) / l1_data_accesses_stats);
    if (l2_accesses_stats > 0)
        fprintf(fp, "L2: %.2f%% misses\n", 100.0 * (l2_accesses_stats - l2_hits_stats) / l2_accesses_stats);
    if (l3_accesses_stats > 0)
        fprintf(fp, "L3: %.2f%% misses\n", 100.0 * (l3_accesses_stats - l3_hits_stats) / l3_accesses_stats);
    if (l4_accesses_stats > 0)
        fprintf(fp, "L4: %.2f%% misses\n", 100.0 * (l4_accesses_stats - l4_hits_stats) / l4_accesses_stats);
    
    /* Print replacement policy for each cache level */
    fprintf(fp, "\n--- Replacement Policy ---\n");
    if (g_l1_instr)
        fprintf(fp, "L1 Instruction: %s\n", g_l1_instr->policy == POLICY_LRU ? "LRU" : "BIP");
    if (g_l1_data)
        fprintf(fp, "L1 Data: %s\n", g_l1_data->policy == POLICY_LRU ? "LRU" : "BIP");
    if (g_l2)
        fprintf(fp, "L2: %s\n", g_l2->policy == POLICY_LRU ? "LRU" : "BIP");
    if (g_l3)
        fprintf(fp, "L3: %s\n", g_l3->policy == POLICY_LRU ? "LRU" : "BIP");
    if (g_l4)
        fprintf(fp, "L4: %s\n", g_l4->policy == POLICY_LRU ? "LRU" : "BIP");
    
    fclose(fp);
    
    g_counting = 0;
}

/*
 * deinit()
 *   Frees all allocated cache memory and performs final cleanup.
 */
void deinit(void) {
    if (g_l1_data) { free_cache_level(g_l1_data); g_l1_data = NULL; }
    if (g_l1_instr) { free_cache_level(g_l1_instr); g_l1_instr = NULL; }
    if (g_l2) { free_cache_level(g_l2); g_l2 = NULL; }
    if (g_l3) { free_cache_level(g_l3); g_l3 = NULL; }
    if (g_l4) { free_cache_level(g_l4); g_l4 = NULL; }
}

/* ---------------- Extended Cache Access Simulation ---------------- */

/*
 * simulate_memory_access()
 *   Simulates a memory access.
 *   Parameters:
 *     - vaddr: virtual address (used for tagging)
 *     - paddr: physical address (used for lookup)
 *     - access_type: use 1 for instruction access (instruction cache), 0 for data access.
 *   Returns the total latency (in cycles).
 */
int simulate_memory_access(unsigned int vaddr, unsigned int paddr, int access_type) {
    g_current_time++;
    int latency = 0;
    
    /* Select the appropriate L1 cache */
    CacheLevel *l1 = (access_type == 1 ? g_l1_instr : g_l1_data);
    
    /* L1 Check */
    if (l1 != NULL) {
        int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
        unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
        CacheSet *l1_set = &l1->sets[l1_set_index];
        
        /* Update L1 access counters */
        if (access_type == 1)
            l1_instr_accesses_stats++;
        else
            l1_data_accesses_stats++;
        
        for (int i = 0; i < l1_set->num_lines; i++) {
            if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
                l1->update_policy(l1_set, i);
                latency += l1->access_latency;
                
                /* Record a hit */
                if (access_type == 1)
                    l1_instr_hits_stats++;
                else
                    l1_data_hits_stats++;
                
                goto DONE;
            }
        }
        latency += l1->access_latency;  // L1 miss penalty.
    }
    
    /* L2 Check */
    if (g_l2 != NULL) {
        l2_accesses_stats++;
        int l2_set_index = (paddr / g_l2->line_size) % g_l2->num_sets;
        unsigned int l2_tag = paddr / (g_l2->line_size * g_l2->num_sets);
        CacheSet *l2_set = &g_l2->sets[l2_set_index];
        int hit_in_l2 = 0;
        for (int i = 0; i < l2_set->num_lines; i++) {
            if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
                g_l2->update_policy(l2_set, i);
                latency += g_l2->access_latency;
                hit_in_l2 = 1;
                l2_hits_stats++;
                break;
            }
        }
        if (!hit_in_l2)
            latency += g_l2->access_latency;
        if (hit_in_l2) {
            if (l1 != NULL) {
                int victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = g_current_time;
            }
            goto DONE;
        }
    }
    
    /* L3 Check */
    if (g_l3 != NULL) {
        l3_accesses_stats++;
        int l3_set_index = (paddr / g_l3->line_size) % g_l3->num_sets;
        unsigned int l3_tag = paddr / (g_l3->line_size * g_l3->num_sets);
        CacheSet *l3_set = &g_l3->sets[l3_set_index];
        int hit_in_l3 = 0;
        for (int i = 0; i < l3_set->num_lines; i++) {
            if (l3_set->lines[i].valid && l3_set->lines[i].tag == l3_tag) {
                g_l3->update_policy(l3_set, i);
                latency += g_l3->access_latency;
                hit_in_l3 = 1;
                l3_hits_stats++;
                break;
            }
        }
        if (!hit_in_l3)
            latency += g_l3->access_latency;
        if (hit_in_l3) {
            if (g_l2 != NULL) {
                int victim_l2 = g_l2->find_victim(&g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets]);
                CacheSet *l2_set = &g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets];
                l2_set->lines[victim_l2].tag = paddr / (g_l2->line_size * g_l2->num_sets);
                l2_set->lines[victim_l2].valid = 1;
                l2_set->lines[victim_l2].last_access_time = g_current_time;
            }
            if (l1 != NULL) {
                int victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = g_current_time;
            }
            goto DONE;
        }
    }
    
    /* L4 Check */
    if (g_l4 != NULL) {
        l4_accesses_stats++;
        int l4_set_index = (paddr / g_l4->line_size) % g_l4->num_sets;
        unsigned int l4_tag = paddr / (g_l4->line_size * g_l4->num_sets);
        CacheSet *l4_set = &g_l4->sets[l4_set_index];
        int hit_in_l4 = 0;
        for (int i = 0; i < l4_set->num_lines; i++) {
            if (l4_set->lines[i].valid && l4_set->lines[i].tag == l4_tag) {
                g_l4->update_policy(l4_set, i);
                latency += g_l4->access_latency;
                hit_in_l4 = 1;
                l4_hits_stats++;
                break;
            }
        }
        if (!hit_in_l4)
            latency += g_l4->access_latency;
        if (hit_in_l4) {
            if (g_l3 != NULL) {
                int victim_l3 = g_l3->find_victim(&g_l3->sets[(paddr / g_l3->line_size) % g_l3->num_sets]);
                CacheSet *l3_set = &g_l3->sets[(paddr / g_l3->line_size) % g_l3->num_sets];
                l3_set->lines[victim_l3].tag = paddr / (g_l3->line_size * g_l3->num_sets);
                l3_set->lines[victim_l3].valid = 1;
                l3_set->lines[victim_l3].last_access_time = g_current_time;
            }
            if (g_l2 != NULL) {
                int victim_l2 = g_l2->find_victim(&g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets]);
                CacheSet *l2_set = &g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets];
                l2_set->lines[victim_l2].tag = paddr / (g_l2->line_size * g_l2->num_sets);
                l2_set->lines[victim_l2].valid = 1;
                l2_set->lines[victim_l2].last_access_time = g_current_time;
            }
            if (l1 != NULL) {
                int victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = g_current_time;
            }
            goto DONE;
        }
    }
    
    /* Main Memory Access */
    latency += MEM_LATENCY;
    if (g_l4 != NULL) {
        int l4_set_index = (paddr / g_l4->line_size) % g_l4->num_sets;
        unsigned int l4_tag = paddr / (g_l4->line_size * g_l4->num_sets);
        CacheSet *l4_set = &g_l4->sets[l4_set_index];
        int victim_l4 = g_l4->find_victim(l4_set);
        l4_set->lines[victim_l4].tag = l4_tag;
        l4_set->lines[victim_l4].valid = 1;
        l4_set->lines[victim_l4].last_access_time = g_current_time;
    }
    if (g_l3 != NULL) {
        int l3_set_index = (paddr / g_l3->line_size) % g_l3->num_sets;
        unsigned int l3_tag = paddr / (g_l3->line_size * g_l3->num_sets);
        CacheSet *l3_set = &g_l3->sets[l3_set_index];
        int victim_l3 = g_l3->find_victim(l3_set);
        l3_set->lines[victim_l3].tag = l3_tag;
        l3_set->lines[victim_l3].valid = 1;
        l3_set->lines[victim_l3].last_access_time = g_current_time;
    }
    if (g_l2 != NULL) {
        int l2_set_index = (paddr / g_l2->line_size) % g_l2->num_sets;
        unsigned int l2_tag = paddr / (g_l2->line_size * g_l2->num_sets);
        CacheSet *l2_set = &g_l2->sets[l2_set_index];
        int victim_l2 = g_l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = g_current_time;
    }
    if (l1 != NULL) {
        int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
        unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
        CacheSet *l1_set = &l1->sets[l1_set_index];
        int victim_l1 = l1->find_victim(l1_set);
        l1_set->lines[victim_l1].tag = l1_tag;
        l1_set->lines[victim_l1].valid = 1;
        l1_set->lines[victim_l1].last_access_time = g_current_time;
    }
    
DONE:
    /* Update simulation counters only if counting is active */
    if (g_counting) {
        if (access_type == 1) {
            g_total_latency_instr += latency;
            g_instr_accesses++;
        } else {
            g_total_latency_data += latency;
            g_data_accesses++;
        }
        g_mem_accesses++;
    }
    
    return latency;
}

/*
 * simulate_prefetch()
 *   Works similarly to simulate_memory_access() but only inserts a block into the L1 cache.
 *   Use the same access_type flag (1 for instruction, 0 for data).
 *
 * Returns the latency incurred by the prefetch.
 */
int simulate_prefetch(unsigned int vaddr, unsigned int paddr, int access_type) {
    g_current_time++;
    int latency = 0;
    CacheLevel *l1 = (access_type == 1 ? g_l1_instr : g_l1_data);
    if (l1 == NULL)
        return 0;
    
    int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    
    for (int i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
            return 0;  // Already present.
        }
    }
    
    if (g_l2 != NULL) {
        int l2_set_index = (paddr / g_l2->line_size) % g_l2->num_sets;
        unsigned int l2_tag = paddr / (g_l2->line_size * g_l2->num_sets);
        CacheSet *l2_set = &g_l2->sets[l2_set_index];
        int hit_in_l2 = 0;
        for (int i = 0; i < l2_set->num_lines; i++) {
            if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
                g_l2->update_policy(l2_set, i);
                latency += g_l2->access_latency;
                hit_in_l2 = 1;
                break;
            }
        }
        if (!hit_in_l2)
            latency += g_l2->access_latency;
    }
    
    int victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = g_current_time;
    latency += l1->access_latency;
    return latency;
}

/* ---------------- Cache Maintenance ---------------- */

static void flush_cache_line(CacheLevel *cache, unsigned int paddr) {
    if (cache == NULL)
        return;
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

void flush_instruction(unsigned int paddr) {
    flush_cache_line(g_l1_instr, paddr);
    flush_cache_line(g_l2, paddr);
    flush_cache_line(g_l3, paddr);
    flush_cache_line(g_l4, paddr);
}

void flush_data(unsigned int paddr) {
    flush_cache_line(g_l1_data, paddr);
    flush_cache_line(g_l2, paddr);
    flush_cache_line(g_l3, paddr);
    flush_cache_line(g_l4, paddr);
}