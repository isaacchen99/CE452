#include "cache.h"

/* ---------------- Replacement Policy Implementations ---------------- */

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

/* ---------------- Extended Cache Access Simulation ---------------- */

int simulate_memory_access(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4,
                           unsigned int paddr, int is_write, unsigned long current_time, int *hit_level) {
    int latency = 0;
    
    /* ---------------- L1 Check ---------------- */
    int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    for (int i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
            l1->update_policy(l1_set, i, current_time);
            latency += l1->access_latency;
            *hit_level = 1;
            return latency;
        }
    }
    latency += l1->access_latency;  // L1 miss

    /* ---------------- L2 Check ---------------- */
    int l2_set_index = (paddr / l2->line_size) % l2->num_sets;
    unsigned int l2_tag = paddr / (l2->line_size * l2->num_sets);
    CacheSet *l2_set = &l2->sets[l2_set_index];
    int hit_in_l2 = 0;
    for (int i = 0; i < l2_set->num_lines; i++) {
        if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
            l2->update_policy(l2_set, i, current_time);
            latency += l2->access_latency;
            hit_in_l2 = 1;
            break;
        }
    }
    if (!hit_in_l2) {
        latency += l2->access_latency;
    }
    if (hit_in_l2) {
        *hit_level = 2;
        /* Bring block into L1 */
        int victim_l1 = l1->find_victim(l1_set);
        l1_set->lines[victim_l1].tag = l1_tag;
        l1_set->lines[victim_l1].valid = 1;
        l1_set->lines[victim_l1].last_access_time = current_time;
        return latency;
    }
    
    /* ---------------- L3 Check ---------------- */
    int l3_set_index = (paddr / l3->line_size) % l3->num_sets;
    unsigned int l3_tag = paddr / (l3->line_size * l3->num_sets);
    CacheSet *l3_set = &l3->sets[l3_set_index];
    int hit_in_l3 = 0;
    for (int i = 0; i < l3_set->num_lines; i++) {
        if (l3_set->lines[i].valid && l3_set->lines[i].tag == l3_tag) {
            l3->update_policy(l3_set, i, current_time);
            latency += l3->access_latency;
            hit_in_l3 = 1;
            break;
        }
    }
    if (!hit_in_l3) {
        latency += l3->access_latency;
    }
    if (hit_in_l3) {
        *hit_level = 3;
        /* Bring block into L2 and L1 */
        int victim_l2 = l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = current_time;
        int victim_l1 = l1->find_victim(l1_set);
        l1_set->lines[victim_l1].tag = l1_tag;
        l1_set->lines[victim_l1].valid = 1;
        l1_set->lines[victim_l1].last_access_time = current_time;
        return latency;
    }
    
    /* ---------------- L4 Check ---------------- */
    int l4_set_index = (paddr / l4->line_size) % l4->num_sets;
    unsigned int l4_tag = paddr / (l4->line_size * l4->num_sets);
    CacheSet *l4_set = &l4->sets[l4_set_index];
    int hit_in_l4 = 0;
    for (int i = 0; i < l4_set->num_lines; i++) {
        if (l4_set->lines[i].valid && l4_set->lines[i].tag == l4_tag) {
            l4->update_policy(l4_set, i, current_time);
            latency += l4->access_latency;
            hit_in_l4 = 1;
            break;
        }
    }
    if (!hit_in_l4) {
        latency += l4->access_latency;
    }
    if (hit_in_l4) {
        *hit_level = 4;
        /* Bring block into L3, L2, and L1 */
        int victim_l3 = l3->find_victim(l3_set);
        l3_set->lines[victim_l3].tag = l3_tag;
        l3_set->lines[victim_l3].valid = 1;
        l3_set->lines[victim_l3].last_access_time = current_time;
        int victim_l2 = l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = current_time;
        int victim_l1 = l1->find_victim(l1_set);
        l1_set->lines[victim_l1].tag = l1_tag;
        l1_set->lines[victim_l1].valid = 1;
        l1_set->lines[victim_l1].last_access_time = current_time;
        return latency;
    }
    
    /* ---------------- Main Memory Access ---------------- */
    latency += MEM_LATENCY;
    *hit_level = 0;
    /* Insert block into all levels: L4, then L3, then L2, then L1 */
    int victim_l4 = l4->find_victim(l4_set);
    l4_set->lines[victim_l4].tag = l4_tag;
    l4_set->lines[victim_l4].valid = 1;
    l4_set->lines[victim_l4].last_access_time = current_time;
    
    int victim_l3 = l3->find_victim(l3_set);
    l3_set->lines[victim_l3].tag = l3_tag;
    l3_set->lines[victim_l3].valid = 1;
    l3_set->lines[victim_l3].last_access_time = current_time;
    
    int victim_l2 = l2->find_victim(l2_set);
    l2_set->lines[victim_l2].tag = l2_tag;
    l2_set->lines[victim_l2].valid = 1;
    l2_set->lines[victim_l2].last_access_time = current_time;
    
    int victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = current_time;
    
    return latency;
}

/* ---------------- Cache Maintenance: Flush and Prefetch ---------------- */

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

void flush_instruction(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, unsigned int paddr) {
    flush_cache_line(l1, paddr);
    flush_cache_line(l2, paddr);
    flush_cache_line(l3, paddr);
    flush_cache_line(l4, paddr);
}

int simulate_prefetch(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4,
                      unsigned int paddr, unsigned long current_time, int *hit_level) {
    int latency = 0;
    int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    
    /* Check if already in L1 */
    for (int i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
            *hit_level = 1;
            return 0;  // already present; no latency incurred.
        }
    }
    
    /* Not in L1; check L2 */
    int l2_set_index = (paddr / l2->line_size) % l2->num_sets;
    unsigned int l2_tag = paddr / (l2->line_size * l2->num_sets);
    CacheSet *l2_set = &l2->sets[l2_set_index];
    int hit_in_l2 = 0;
    for (int i = 0; i < l2_set->num_lines; i++) {
        if (l2_set->lines[i].valid && l2_set->lines[i].tag == l2_tag) {
            l2->update_policy(l2_set, i, current_time);
            latency += l2->access_latency;
            hit_in_l2 = 1;
            *hit_level = 2;
            break;
        }
    }
    if (!hit_in_l2) {
        latency += l2->access_latency;
        // For simplicity, we continue the prefetch path just as in simulate_memory_access.
        // (You could extend this to check L3 and L4 as well if desired.)
    }
    
    /* Insert block into L1 */
    int victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = current_time;
    latency += l1->access_latency;
    
    return latency;
}

/* ---------------- Main Simulator ---------------- */

int main() {
    /* Configure cache levels. */
    int l1_size = 32 * 1024;       // 32 KB
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
    
    CacheLevel *l1 = init_cache_level(l1_size, l1_assoc, l1_line, l1_latency, POLICY_LRU);
    CacheLevel *l2 = init_cache_level(l2_size, l2_assoc, l2_line, l2_latency, POLICY_LRU);
    CacheLevel *l3 = init_cache_level(l3_size, l3_assoc, l3_line, l3_latency, POLICY_LRU);
    CacheLevel *l4 = init_cache_level(l4_size, l4_assoc, l4_line, l4_latency, POLICY_LRU);
    
    unsigned long current_time = 0;
    
    /* Statistics for normal memory accesses (R/W) */
    int mem_accesses = 0;
    int l1_hits = 0, l1_misses = 0;
    int l2_hits = 0, l2_misses = 0;
    int l3_hits = 0, l3_misses = 0;
    int l4_hits = 0, l4_misses = 0;
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
            int latency = simulate_memory_access(l1, l2, l3, l4, paddr, (op == 'W'), current_time, &hit_level);
            total_latency += latency;
            if (hit_level == 1) {
                l1_hits++;
                printf("%c access at 0x%x: L1 hit, latency = %d cycles\n", op, paddr, latency);
            } else if (hit_level == 2) {
                l1_misses++;
                l2_hits++;
                printf("%c access at 0x%x: L1 miss, L2 hit, latency = %d cycles\n", op, paddr, latency);
            } else if (hit_level == 3) {
                l1_misses++;
                l2_misses++;
                l3_hits++;
                printf("%c access at 0x%x: L1 & L2 miss, L3 hit, latency = %d cycles\n", op, paddr, latency);
            } else if (hit_level == 4) {
                l1_misses++;
                l2_misses++;
                l3_misses++;
                l4_hits++;
                printf("%c access at 0x%x: L1, L2 & L3 miss, L4 hit, latency = %d cycles\n", op, paddr, latency);
            } else {
                l1_misses++;
                l2_misses++;
                l3_misses++;
                l4_misses++;
                printf("%c access at 0x%x: Miss in all caches, latency = %d cycles\n", op, paddr, latency);
            }
        } else if (op == 'F') {
            flush_instruction(l1, l2, l3, l4, paddr);
            printf("Flush command for address 0x%x executed.\n", paddr);
        } else if (op == 'P') {
            int hit_level = 0;
            int latency = simulate_prefetch(l1, l2, l3, l4, paddr, current_time, &hit_level);
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
    printf("L3 hits: %d, L3 misses: %d\n", l3_hits, l3_misses);
    printf("L4 hits: %d, L4 misses: %d\n", l4_hits, l4_misses);
    if (mem_accesses > 0)
        printf("Average access latency: %.2f cycles\n", (double)total_latency / mem_accesses);
    
    free_cache_level(l1);
    free_cache_level(l2);
    free_cache_level(l3);
    free_cache_level(l4);
    
    return 0;
}