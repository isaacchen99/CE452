#include "cache.h"
#include <errno.h>

/* ---------------- Replacement Policy Implementations ---------------- */

/* LRU: update simply records the current time */
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

/* Simple BIP update: update with half the current time to simulate lower recency.
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

/*
 * This function now checks each cache level only if the pointer is non-NULL.
 * If a level is omitted, it simply does not add its latency or update that level.
 */
int simulate_memory_access(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4,
                           unsigned int paddr, int is_write, unsigned long current_time, int *hit_level) {
    int latency = 0;

    /* ---------------- L1 Check (if exists) ---------------- */
    if (l1 != NULL) {
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
        latency += l1->access_latency;  // L1 access attempted (miss)
    }
    
    /* ---------------- L2 Check (if exists) ---------------- */
    if (l2 != NULL) {
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
            /* Bring block into L1 if L1 exists */
            if (l1 != NULL) {
                int victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = current_time;
            }
            return latency;
        }
    }
    
    /* ---------------- L3 Check (if exists) ---------------- */
    if (l3 != NULL) {
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
            /* Bring block into L2 and L1 if they exist */
            if (l2 != NULL) {
                int victim_l2 = l2->find_victim(&l2->sets[(paddr / l2->line_size) % l2->num_sets]);
                CacheSet *l2_set = &l2->sets[(paddr / l2->line_size) % l2->num_sets];
                l2_set->lines[victim_l2].tag = paddr / (l2->line_size * l2->num_sets);
                l2_set->lines[victim_l2].valid = 1;
                l2_set->lines[victim_l2].last_access_time = current_time;
            }
            if (l1 != NULL) {
                int victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = current_time;
            }
            return latency;
        }
    }
    
    /* ---------------- L4 Check (if exists) ---------------- */
    if (l4 != NULL) {
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
            /* Bring block into L3, L2, and L1 if they exist */
            if (l3 != NULL) {
                int victim_l3 = l3->find_victim(&l3->sets[(paddr / l3->line_size) % l3->num_sets]);
                CacheSet *l3_set = &l3->sets[(paddr / l3->line_size) % l3->num_sets];
                l3_set->lines[victim_l3].tag = paddr / (l3->line_size * l3->num_sets);
                l3_set->lines[victim_l3].valid = 1;
                l3_set->lines[victim_l3].last_access_time = current_time;
            }
            if (l2 != NULL) {
                int victim_l2 = l2->find_victim(&l2->sets[(paddr / l2->line_size) % l2->num_sets]);
                CacheSet *l2_set = &l2->sets[(paddr / l2->line_size) % l2->num_sets];
                l2_set->lines[victim_l2].tag = paddr / (l2->line_size * l2->num_sets);
                l2_set->lines[victim_l2].valid = 1;
                l2_set->lines[victim_l2].last_access_time = current_time;
            }
            if (l1 != NULL) {
                int victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = current_time;
            }
            return latency;
        }
    }
    
    /* ---------------- Main Memory Access ---------------- */
    latency += MEM_LATENCY;
    *hit_level = 0;
    
    /* Insert block into all cache levels that exist,
       starting from the lowest level (highest in the hierarchy). */
    if (l4 != NULL) {
        int l4_set_index = (paddr / l4->line_size) % l4->num_sets;
        unsigned int l4_tag = paddr / (l4->line_size * l4->num_sets);
        CacheSet *l4_set = &l4->sets[l4_set_index];
        int victim_l4 = l4->find_victim(l4_set);
        l4_set->lines[victim_l4].tag = l4_tag;
        l4_set->lines[victim_l4].valid = 1;
        l4_set->lines[victim_l4].last_access_time = current_time;
    }
    if (l3 != NULL) {
        int l3_set_index = (paddr / l3->line_size) % l3->num_sets;
        unsigned int l3_tag = paddr / (l3->line_size * l3->num_sets);
        CacheSet *l3_set = &l3->sets[l3_set_index];
        int victim_l3 = l3->find_victim(l3_set);
        l3_set->lines[victim_l3].tag = l3_tag;
        l3_set->lines[victim_l3].valid = 1;
        l3_set->lines[victim_l3].last_access_time = current_time;
    }
    if (l2 != NULL) {
        int l2_set_index = (paddr / l2->line_size) % l2->num_sets;
        unsigned int l2_tag = paddr / (l2->line_size * l2->num_sets);
        CacheSet *l2_set = &l2->sets[l2_set_index];
        int victim_l2 = l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = current_time;
    }
    if (l1 != NULL) {
        int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
        unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
        CacheSet *l1_set = &l1->sets[l1_set_index];
        int victim_l1 = l1->find_victim(l1_set);
        l1_set->lines[victim_l1].tag = l1_tag;
        l1_set->lines[victim_l1].valid = 1;
        l1_set->lines[victim_l1].last_access_time = current_time;
    }
    
    return latency;
}

/*
 * simulate_prefetch() attempts to bring a block into the given L1 cache.
 * If L1 is omitted (NULL), no prefetch is performed.
 */
int simulate_prefetch(CacheLevel *l1, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4,
                      unsigned int paddr, unsigned long current_time, int *hit_level) {
    if (l1 == NULL)
        return 0;  // Nothing to prefetch into.
    
    int latency = 0;
    int l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    
    /* Check if already present in L1 */
    for (int i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
            *hit_level = 1;
            return 0;  // Already present; no latency incurred.
        }
    }
    
    /* Optionally check L2 if it exists */
    if (l2 != NULL) {
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
        if (!hit_in_l2)
            latency += l2->access_latency;
    }
    
    /* Insert block into L1 */
    int victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = current_time;
    latency += l1->access_latency;
    
    return latency;
}

/* ---------------- Cache Maintenance: Flush and Prefetch ---------------- */

void flush_cache_line(CacheLevel *cache, unsigned int paddr) {
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

void flush_instruction(CacheLevel *l1_instr, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, unsigned int paddr) {
    flush_cache_line(l1_instr, paddr);
    flush_cache_line(l2, paddr);
    flush_cache_line(l3, paddr);
    flush_cache_line(l4, paddr);
}

void flush_data(CacheLevel *l1_data, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4, unsigned int paddr) {
    flush_cache_line(l1_data, paddr);
    flush_cache_line(l2, paddr);
    flush_cache_line(l3, paddr);
    flush_cache_line(l4, paddr);
}

/* ---------------- Main Simulator ---------------- */

int main() {
    /* ---------------- Cache Configuration Flags ---------------- */
    int use_l1 = 1;      // If 1, instantiate L1 caches (both data and instruction)
    int use_l2 = 1;
    int use_l3 = 1;
    int use_l4 = 1;
    
    /* ---------------- Cache Configuration Parameters ---------------- */
    int l1_size = 32 * 1024;       // 32 KB for each L1 (data and instruction)
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
    
    /* ---------------- Cache Instantiation ---------------- */
    CacheLevel *l1_data = (use_l1 ? init_cache_level(l1_size, l1_assoc, l1_line, l1_latency, POLICY_LRU) : NULL);
    CacheLevel *l1_instr = (use_l1 ? init_cache_level(l1_size, l1_assoc, l1_line, l1_latency, POLICY_LRU) : NULL);
    CacheLevel *l2 = (use_l2 ? init_cache_level(l2_size, l2_assoc, l2_line, l2_latency, POLICY_LRU) : NULL);
    CacheLevel *l3 = (use_l3 ? init_cache_level(l3_size, l3_assoc, l3_line, l3_latency, POLICY_LRU) : NULL);
    CacheLevel *l4 = (use_l4 ? init_cache_level(l4_size, l4_assoc, l4_line, l4_latency, POLICY_LRU) : NULL);
    
    unsigned long current_time = 0;
    
    /* ---------------- Simulation Statistics ---------------- */
    int mem_accesses = 0;
    int data_hits = 0, data_misses = 0;
    int instr_hits = 0, instr_misses = 0;
    unsigned long total_latency_data = 0;
    unsigned long total_latency_instr = 0;
    
    /*
     * Supported commands:
     *   I <vaddr> <paddr>         : Instruction fetch access
     *   R <vaddr> <paddr>         : Data read access
     *   W <vaddr> <paddr>         : Data write access
     *   F I <vaddr> <paddr>       : Flush instruction cache line
     *   F D <vaddr> <paddr>       : Flush data cache line
     *   P I <vaddr> <paddr>       : Prefetch into instruction cache
     *   P D <vaddr> <paddr>       : Prefetch into data cache
     */
    char op;
    char subtype; // For flush/prefetch commands, indicates I (instruction) or D (data)
    unsigned int vaddr, paddr;
    
    printf("Enter commands:\n");
    printf("  I <vaddr> <paddr>         : Instruction fetch access\n");
    printf("  R <vaddr> <paddr>         : Data read access\n");
    printf("  W <vaddr> <paddr>         : Data write access\n");
    printf("  F I <vaddr> <paddr>       : Flush instruction cache line\n");
    printf("  F D <vaddr> <paddr>       : Flush data cache line\n");
    printf("  P I <vaddr> <paddr>       : Prefetch into instruction cache\n");
    printf("  P D <vaddr> <paddr>       : Prefetch into data cache\n");
    printf("Ctrl+D (or Ctrl+Z on Windows) to end input.\n\n");
    
    while (1) {
        int n = scanf(" %c", &op);
        if (n != 1)
            break;
        
        if (op == 'I' || op == 'R' || op == 'W') {
            if (scanf(" %x %x", &vaddr, &paddr) != 2)
                break;
            current_time++;
            mem_accesses++;
            if (op == 'I') {
                int hit_level = 0;
                int latency = simulate_memory_access(l1_instr, l2, l3, l4, paddr, 0, current_time, &hit_level);
                total_latency_instr += latency;
                if (hit_level == 1)
                    instr_hits++;
                else
                    instr_misses++;
                printf("Instruction access at 0x%x: %s, latency = %d cycles\n", paddr,
                       (hit_level ? "hit" : "miss"), latency);
            } else { // R or W => data access
                int hit_level = 0;
                int latency = simulate_memory_access(l1_data, l2, l3, l4, paddr, (op == 'W'), current_time, &hit_level);
                total_latency_data += latency;
                if (hit_level == 1)
                    data_hits++;
                else
                    data_misses++;
                printf("%c access at 0x%x: %s, latency = %d cycles\n", op, paddr,
                       (hit_level ? "hit" : "miss"), latency);
            }
        } else if (op == 'F' || op == 'P') {
            if (scanf(" %c %x %x", &subtype, &vaddr, &paddr) != 3)
                break;
            current_time++;
            if (op == 'F') {
                if (subtype == 'I') {
                    flush_instruction(l1_instr, l2, l3, l4, paddr);
                    printf("Flushed instruction cache line for address 0x%x\n", paddr);
                } else if (subtype == 'D') {
                    flush_data(l1_data, l2, l3, l4, paddr);
                    printf("Flushed data cache line for address 0x%x\n", paddr);
                } else {
                    printf("Unknown flush subtype: %c\n", subtype);
                }
            } else { // op == 'P'
                int hit_level = 0;
                if (subtype == 'I') {
                    int latency = simulate_prefetch(l1_instr, l2, l3, l4, paddr, current_time, &hit_level);
                    if (hit_level == 1)
                        printf("Prefetch (instruction) for address 0x%x: already in L1 (hit), latency = %d cycles\n", paddr, latency);
                    else if (hit_level == 2)
                        printf("Prefetch (instruction) for address 0x%x: present in L2 (hit), latency = %d cycles\n", paddr, latency);
                    else
                        printf("Prefetch (instruction) for address 0x%x: fetched from memory, latency = %d cycles\n", paddr, latency);
                } else if (subtype == 'D') {
                    int latency = simulate_prefetch(l1_data, l2, l3, l4, paddr, current_time, &hit_level);
                    if (hit_level == 1)
                        printf("Prefetch (data) for address 0x%x: already in L1 (hit), latency = %d cycles\n", paddr, latency);
                    else if (hit_level == 2)
                        printf("Prefetch (data) for address 0x%x: present in L2 (hit), latency = %d cycles\n", paddr, latency);
                    else
                        printf("Prefetch (data) for address 0x%x: fetched from memory, latency = %d cycles\n", paddr, latency);
                } else {
                    printf("Unknown prefetch subtype: %c\n", subtype);
                }
            }
        } else {
            printf("Unknown command: %c\n", op);
        }
    }
    
    /* ---------------- Simulation Statistics ---------------- */
    printf("\n--- Simulation Statistics ---\n");
    printf("Total memory accesses: %d\n", mem_accesses);
    printf("Instruction accesses: hits = %d, misses = %d, average latency = %.2f cycles\n",
           instr_hits, instr_misses, (instr_hits+instr_misses) ? (double)total_latency_instr/(instr_hits+instr_misses) : 0.0);
    printf("Data accesses: hits = %d, misses = %d, average latency = %.2f cycles\n",
           data_hits, data_misses, (data_hits+data_misses) ? (double)total_latency_data/(data_hits+data_misses) : 0.0);
    
    if (l1_data != NULL)
        free_cache_level(l1_data);
    if (l1_instr != NULL)
        free_cache_level(l1_instr);
    if (l2 != NULL)
        free_cache_level(l2);
    if (l3 != NULL)
        free_cache_level(l3);
    if (l4 != NULL)
        free_cache_level(l4);
    
    return 0;
}