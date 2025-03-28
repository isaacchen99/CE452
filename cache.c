#include "cache.h"
#include <errno.h>
#include <stdlib.h>
#include <time.h>

CacheConfig g_config;

void read_config(const char *filename) {
    // default values
    g_config.use_l1 = 1;
    g_config.use_l2 = 1;
    g_config.use_l3 = 1;
    g_config.use_l4 = 0;

    g_config.l1_size = 32 * 1024;
    g_config.l1_assoc = 8;
    g_config.l1_line = 64;
    g_config.l1_latency = 1;
    snprintf(g_config.l1_policy_str, sizeof(g_config.l1_policy_str), "LRU");

    g_config.l2_size = 256 * 1024;
    g_config.l2_assoc = 8;
    g_config.l2_line = 64;
    g_config.l2_latency = 10;
    snprintf(g_config.l2_policy_str, sizeof(g_config.l2_policy_str), "LRU");

    g_config.l3_size = 2048 * 1024;
    g_config.l3_assoc = 8;
    g_config.l3_line = 64;
    g_config.l3_latency = 20;
    snprintf(g_config.l3_policy_str, sizeof(g_config.l3_policy_str), "LRU");

    g_config.l4_size = 0;
    g_config.l4_assoc = 16;
    g_config.l4_line = 64;
    g_config.l4_latency = 40;
    snprintf(g_config.l4_policy_str, sizeof(g_config.l4_policy_str), "LRU");

    g_config.mem_latency = 100;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Warning: config file %s not found, using default configuration.\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *trim = line;
        while (*trim == ' ' || *trim == '\t') trim++;
        if (*trim == '#' || *trim == '\n' || *trim == '\0')
            continue;
        char *eq = strchr(trim, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char *key = trim;
        char *value = eq + 1;
        char *newline = strchr(value, '\n');
        if (newline)
            *newline = '\0';

        if (strcmp(key, "USE_L1") == 0)
            g_config.use_l1 = strtoul(value, NULL, 10);
        else if (strcmp(key, "USE_L2") == 0)
            g_config.use_l2 = strtoul(value, NULL, 10);
        else if (strcmp(key, "USE_L3") == 0)
            g_config.use_l3 = strtoul(value, NULL, 10);
        else if (strcmp(key, "USE_L4") == 0)
            g_config.use_l4 = strtoul(value, NULL, 10);
        else if (strcmp(key, "L1_SIZE") == 0)
            g_config.l1_size = strtoul(value, NULL, 10);
        else if (strcmp(key, "L1_ASSOC") == 0)
            g_config.l1_assoc = strtoul(value, NULL, 10);
        else if (strcmp(key, "L1_LINE") == 0)
            g_config.l1_line = strtoul(value, NULL, 10);
        else if (strcmp(key, "L1_LATENCY") == 0)
            g_config.l1_latency = strtoul(value, NULL, 10);
        else if (strcmp(key, "L1_POLICY") == 0)
            strncpy(g_config.l1_policy_str, value, sizeof(g_config.l1_policy_str)-1);
        else if (strcmp(key, "L2_SIZE") == 0)
            g_config.l2_size = strtoul(value, NULL, 10);
        else if (strcmp(key, "L2_ASSOC") == 0)
            g_config.l2_assoc = strtoul(value, NULL, 10);
        else if (strcmp(key, "L2_LINE") == 0)
            g_config.l2_line = strtoul(value, NULL, 10);
        else if (strcmp(key, "L2_LATENCY") == 0)
            g_config.l2_latency = strtoul(value, NULL, 10);
        else if (strcmp(key, "L2_POLICY") == 0)
            strncpy(g_config.l2_policy_str, value, sizeof(g_config.l2_policy_str)-1);
        else if (strcmp(key, "L3_SIZE") == 0)
            g_config.l3_size = strtoul(value, NULL, 10);
        else if (strcmp(key, "L3_ASSOC") == 0)
            g_config.l3_assoc = strtoul(value, NULL, 10);
        else if (strcmp(key, "L3_LINE") == 0)
            g_config.l3_line = strtoul(value, NULL, 10);
        else if (strcmp(key, "L3_LATENCY") == 0)
            g_config.l3_latency = strtoul(value, NULL, 10);
        else if (strcmp(key, "L3_POLICY") == 0)
            strncpy(g_config.l3_policy_str, value, sizeof(g_config.l3_policy_str)-1);
        else if (strcmp(key, "L4_SIZE") == 0)
            g_config.l4_size = strtoul(value, NULL, 10);
        else if (strcmp(key, "L4_ASSOC") == 0)
            g_config.l4_assoc = strtoul(value, NULL, 10);
        else if (strcmp(key, "L4_LINE") == 0)
            g_config.l4_line = strtoul(value, NULL, 10);
        else if (strcmp(key, "L4_LATENCY") == 0)
            g_config.l4_latency = strtoul(value, NULL, 10);
        else if (strcmp(key, "L4_POLICY") == 0)
            strncpy(g_config.l4_policy_str, value, sizeof(g_config.l4_policy_str)-1);
        else if (strcmp(key, "MEM_LATENCY") == 0)
            g_config.mem_latency = strtoul(value, NULL, 10);
    }
    fclose(fp);
}


CacheLevel *g_l1_data = NULL;
CacheLevel *g_l1_instr = NULL;
CacheLevel *g_l2 = NULL;
CacheLevel *g_l3 = NULL;
CacheLevel *g_l4 = NULL;

unsigned long g_current_time = 0;

unsigned long g_mem_accesses = 0;
unsigned long g_instr_accesses = 0;
unsigned long g_data_accesses = 0;
unsigned long g_total_latency_instr = 0;
unsigned long g_total_latency_data = 0;

unsigned long g_counting = 0;

static unsigned long l1_data_accesses_stats = 0, l1_data_hits_stats = 0;
static unsigned long l1_instr_accesses_stats = 0, l1_instr_hits_stats = 0;
static unsigned long l2_accesses_stats = 0, l2_hits_stats = 0;
static unsigned long l3_accesses_stats = 0, l3_hits_stats = 0;
static unsigned long l4_accesses_stats = 0, l4_hits_stats = 0;


void update_policy_lru(CacheSet *set, unsigned long line_index) {
    set->lines[line_index].last_access_time = g_current_time;
}

unsigned long find_victim_lru(CacheSet *set) {
    unsigned long victim = 0;
    unsigned long min_time = set->lines[0].last_access_time;
    for (unsigned long i = 1; i < set->num_lines; i++) {
        if (set->lines[i].last_access_time < min_time) {
            min_time = set->lines[i].last_access_time;
            victim = i;
        }
    }
    return victim;
}

void update_policy_bip(CacheSet *set, unsigned long line_index) {
    if (rand() % 32 == 0) { // 1/32 insert at most recently used
        set->lines[line_index].last_access_time = g_current_time;
    } else {
        // insert at least recently used
        set->lines[line_index].last_access_time = 0;
    }
}

unsigned long find_victim_bip(CacheSet *set) {
    return find_victim_lru(set);
}

void update_policy_random(CacheSet *set, unsigned long line_index) {
    (void) set; // unused parameter warning if unused
    (void) line_index;
}

unsigned long find_victim_random(CacheSet *set) {
    return rand() % set->num_lines;
}

static ReplacementPolicy parse_policy(const char *policy_str) {
    if (strcmp(policy_str, "LRU") == 0)
        return POLICY_LRU;
    else if (strcmp(policy_str, "BIP") == 0)
        return POLICY_BIP;
    else if (strcmp(policy_str, "RANDOM") == 0)
        return POLICY_RANDOM;
    else
        return POLICY_LRU;  /* Default */
}

CacheLevel* init_cache_level(unsigned long cache_size, unsigned long associativity, unsigned long line_size, unsigned long access_latency, ReplacementPolicy policy) {
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
    for (unsigned long i = 0; i < cache->num_sets; i++) {
        cache->sets[i].num_lines = associativity;
        cache->sets[i].lines = malloc(sizeof(CacheLine) * associativity);
        if (!cache->sets[i].lines) { perror("malloc"); exit(1); }
        for (unsigned long j = 0; j < associativity; j++) {
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
        case POLICY_RANDOM:
            cache->update_policy = update_policy_random;
            cache->find_victim = find_victim_random;
            break;
        default:
            cache->update_policy = update_policy_lru;
            cache->find_victim = find_victim_lru;
    }
    
    return cache;
}

void free_cache_level(CacheLevel *cache) {
    if (cache) {
        for (unsigned long i = 0; i < cache->num_sets; i++) {
            free(cache->sets[i].lines);
        }
        free(cache->sets);
        free(cache);
    }
}

static void init_cache_simulator(CacheLevel *l1_data, CacheLevel *l1_instr, CacheLevel *l2, CacheLevel *l3, CacheLevel *l4) {
    g_l1_data   = l1_data;
    g_l1_instr  = l1_instr;
    g_l2        = l2;
    g_l3        = l3;
    g_l4        = l4;
    g_current_time = 0;
}

void init(void) {
    read_config(CONFIG);

    if (g_config.use_l1) {
        g_l1_data = init_cache_level(g_config.l1_size, g_config.l1_assoc, g_config.l1_line,
                                       g_config.l1_latency, parse_policy(g_config.l1_policy_str));
        g_l1_instr = init_cache_level(g_config.l1_size, g_config.l1_assoc, g_config.l1_line,
                                       g_config.l1_latency, parse_policy(g_config.l1_policy_str));
    }
    if (g_config.use_l2)
        g_l2 = init_cache_level(g_config.l2_size, g_config.l2_assoc, g_config.l2_line,
                                g_config.l2_latency, parse_policy(g_config.l2_policy_str));
    if (g_config.use_l3)
        g_l3 = init_cache_level(g_config.l3_size, g_config.l3_assoc, g_config.l3_line,
                                g_config.l3_latency, parse_policy(g_config.l3_policy_str));
    if (g_config.use_l4)
        g_l4 = init_cache_level(g_config.l4_size, g_config.l4_assoc, g_config.l4_line,
                                g_config.l4_latency, parse_policy(g_config.l4_policy_str));
    
    init_cache_simulator(g_l1_data, g_l1_instr, g_l2, g_l3, g_l4);
    g_counting = 0;
}

void start(void) {
    g_mem_accesses = 0;
    g_instr_accesses = 0;
    g_data_accesses = 0;
    g_total_latency_instr = 0;
    g_total_latency_data = 0;
    g_current_time = 0;
    g_counting = 1;

    l1_data_accesses_stats = l1_data_hits_stats = 0;
    l1_instr_accesses_stats = l1_instr_hits_stats = 0;
    l2_accesses_stats = l2_hits_stats = 0;
    l3_accesses_stats = l3_hits_stats = 0;
    l4_accesses_stats = l4_hits_stats = 0;
}

void end(void) {
    FILE *fp = fopen("results.log", "a");
    if (!fp) {
        perror("fopen");
        exit(1);
    }
    
    fprintf(fp, "--- Simulation Statistics ---\n");
    fprintf(fp, "Total memory accesses: %lu\n", g_mem_accesses);
    if (g_instr_accesses > 0)
        fprintf(fp, "Instruction accesses: average latency = %.2f cycles\n", (double)g_total_latency_instr / g_instr_accesses);
    else
        fprintf(fp, "Instruction accesses: none\n");
    if (g_data_accesses > 0)
        fprintf(fp, "Data accesses: average latency = %.2f cycles\n", (double)g_total_latency_data / g_data_accesses);
    else
        fprintf(fp, "Data accesses: none\n");

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
    
    fprintf(fp, "\n--- Replacement Policy ---\n");
    if (g_l1_instr)
        fprintf(fp, "L1 Instruction: %s\n", (g_l1_instr->policy == POLICY_LRU) ? "LRU" : ((g_l1_instr->policy == POLICY_BIP) ? "BIP" : "RANDOM"));
    if (g_l1_data)
        fprintf(fp, "L1 Data: %s\n", (g_l1_data->policy == POLICY_LRU) ? "LRU" : ((g_l1_data->policy == POLICY_BIP) ? "BIP" : "RANDOM"));
    if (g_l2)
        fprintf(fp, "L2: %s\n", (g_l2->policy == POLICY_LRU) ? "LRU" : ((g_l2->policy == POLICY_BIP) ? "BIP" : "RANDOM"));
    if (g_l3)
        fprintf(fp, "L3: %s\n", (g_l3->policy == POLICY_LRU) ? "LRU" : ((g_l3->policy == POLICY_BIP) ? "BIP" : "RANDOM"));
    if (g_l4)
        fprintf(fp, "L4: %s\n", (g_l4->policy == POLICY_LRU) ? "LRU" : ((g_l4->policy == POLICY_BIP) ? "BIP" : "RANDOM"));
    
    fclose(fp);
    g_counting = 0;
}

void deinit(void) {
    if (g_l1_data) { free_cache_level(g_l1_data); g_l1_data = NULL; }
    if (g_l1_instr) { free_cache_level(g_l1_instr); g_l1_instr = NULL; }
    if (g_l2) { free_cache_level(g_l2); g_l2 = NULL; }
    if (g_l3) { free_cache_level(g_l3); g_l3 = NULL; }
    if (g_l4) { free_cache_level(g_l4); g_l4 = NULL; }
}

unsigned long simulate_memory_access(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) 
    return 0;  // simulator inactive
    g_current_time++;
    unsigned long latency = 0;
    
    CacheLevel *l1 = (access_type == 1 ? g_l1_instr : g_l1_data);
    
    // l1 check
    if (l1 != NULL) {
        unsigned long l1_set_index = (vaddr / l1->line_size) % l1->num_sets;
        unsigned int l1_tag = vaddr / (l1->line_size * l1->num_sets);
        CacheSet *l1_set = &l1->sets[l1_set_index];
        
        if (access_type == 1)
            l1_instr_accesses_stats++;
        else
            l1_data_accesses_stats++;
        
        for (unsigned long i = 0; i < l1_set->num_lines; i++) {
            if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag) {
                l1->update_policy(l1_set, i);
                latency += l1->access_latency;
                if (access_type == 1)
                    l1_instr_hits_stats++;
                else
                    l1_data_hits_stats++;
                goto DONE; // couldnt think of a better way to avoid goto statement
            }
        }
        latency += l1->access_latency;
    }
    
    // l2 check
    if (g_l2 != NULL) {
        l2_accesses_stats++;
        unsigned long l2_set_index = (paddr / g_l2->line_size) % g_l2->num_sets;
        unsigned int l2_tag = paddr / (g_l2->line_size * g_l2->num_sets);
        CacheSet *l2_set = &g_l2->sets[l2_set_index];
        unsigned long hit_in_l2 = 0;
        for (unsigned long i = 0; i < l2_set->num_lines; i++) {
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
        if (hit_in_l2) { // elevate data
            if (l1 != NULL) {
                unsigned long victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = g_current_time;
            }
            goto DONE;
        }
    }
    
    // l3 check
    if (g_l3 != NULL) {
        l3_accesses_stats++;
        unsigned long l3_set_index = (paddr / g_l3->line_size) % g_l3->num_sets;
        unsigned int l3_tag = paddr / (g_l3->line_size * g_l3->num_sets);
        CacheSet *l3_set = &g_l3->sets[l3_set_index];
        unsigned long hit_in_l3 = 0;
        for (unsigned long i = 0; i < l3_set->num_lines; i++) {
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
        if (hit_in_l3) { // elevate data
            if (g_l2 != NULL) {
                unsigned long victim_l2 = g_l2->find_victim(&g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets]);
                CacheSet *l2_set = &g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets];
                l2_set->lines[victim_l2].tag = paddr / (g_l2->line_size * g_l2->num_sets);
                l2_set->lines[victim_l2].valid = 1;
                l2_set->lines[victim_l2].last_access_time = g_current_time;
            }
            if (l1 != NULL) {
                unsigned long victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = g_current_time;
            }
            goto DONE;
        }
    }
    
    // l4 check
    if (g_l4 != NULL) {
        l4_accesses_stats++;
        unsigned long l4_set_index = (paddr / g_l4->line_size) % g_l4->num_sets;
        unsigned int l4_tag = paddr / (g_l4->line_size * g_l4->num_sets);
        CacheSet *l4_set = &g_l4->sets[l4_set_index];
        unsigned long hit_in_l4 = 0;
        for (unsigned long i = 0; i < l4_set->num_lines; i++) {
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
                unsigned long victim_l3 = g_l3->find_victim(&g_l3->sets[(paddr / g_l3->line_size) % g_l3->num_sets]);
                CacheSet *l3_set = &g_l3->sets[(paddr / g_l3->line_size) % g_l3->num_sets];
                l3_set->lines[victim_l3].tag = paddr / (g_l3->line_size * g_l3->num_sets);
                l3_set->lines[victim_l3].valid = 1;
                l3_set->lines[victim_l3].last_access_time = g_current_time;
            }
            if (g_l2 != NULL) {
                unsigned long victim_l2 = g_l2->find_victim(&g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets]);
                CacheSet *l2_set = &g_l2->sets[(paddr / g_l2->line_size) % g_l2->num_sets];
                l2_set->lines[victim_l2].tag = paddr / (g_l2->line_size * g_l2->num_sets);
                l2_set->lines[victim_l2].valid = 1;
                l2_set->lines[victim_l2].last_access_time = g_current_time;
            }
            if (l1 != NULL) {
                unsigned long victim_l1 = l1->find_victim(&l1->sets[(paddr / l1->line_size) % l1->num_sets]);
                CacheSet *l1_set = &l1->sets[(paddr / l1->line_size) % l1->num_sets];
                l1_set->lines[victim_l1].tag = paddr / (l1->line_size * l1->num_sets);
                l1_set->lines[victim_l1].valid = 1;
                l1_set->lines[victim_l1].last_access_time = g_current_time;
            }
            goto DONE;
        }
    }
    
    // if here, no cache hit, go to main memory
    latency += g_config.mem_latency;
    if (g_l4 != NULL) {
        unsigned long l4_set_index = (paddr / g_l4->line_size) % g_l4->num_sets;
        unsigned int l4_tag = paddr / (g_l4->line_size * g_l4->num_sets);
        CacheSet *l4_set = &g_l4->sets[l4_set_index];
        unsigned long victim_l4 = g_l4->find_victim(l4_set);
        l4_set->lines[victim_l4].tag = l4_tag;
        l4_set->lines[victim_l4].valid = 1;
        l4_set->lines[victim_l4].last_access_time = g_current_time;
    }
    if (g_l3 != NULL) {
        unsigned long l3_set_index = (paddr / g_l3->line_size) % g_l3->num_sets;
        unsigned int l3_tag = paddr / (g_l3->line_size * g_l3->num_sets);
        CacheSet *l3_set = &g_l3->sets[l3_set_index];
        unsigned long victim_l3 = g_l3->find_victim(l3_set);
        l3_set->lines[victim_l3].tag = l3_tag;
        l3_set->lines[victim_l3].valid = 1;
        l3_set->lines[victim_l3].last_access_time = g_current_time;
    }
    if (g_l2 != NULL) {
        unsigned long l2_set_index = (paddr / g_l2->line_size) % g_l2->num_sets;
        unsigned int l2_tag = paddr / (g_l2->line_size * g_l2->num_sets);
        CacheSet *l2_set = &g_l2->sets[l2_set_index];
        unsigned long victim_l2 = g_l2->find_victim(l2_set);
        l2_set->lines[victim_l2].tag = l2_tag;
        l2_set->lines[victim_l2].valid = 1;
        l2_set->lines[victim_l2].last_access_time = g_current_time;
    }
    if (l1 != NULL) {
        unsigned long l1_set_index = (paddr / l1->line_size) % l1->num_sets;
        unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
        CacheSet *l1_set = &l1->sets[l1_set_index];
        unsigned long victim_l1 = l1->find_victim(l1_set);
        l1_set->lines[victim_l1].tag = l1_tag;
        l1_set->lines[victim_l1].valid = 1;
        l1_set->lines[victim_l1].last_access_time = g_current_time;
    }
    
DONE:
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

unsigned long simulate_prefetch(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) 
      return 0;  // simulator inactive
    g_current_time++;
    unsigned long latency = 0;
    CacheLevel *l1 = (access_type == 1 ? g_l1_instr : g_l1_data);
    if (l1 == NULL)
        return 0;
    
    unsigned long l1_set_index = (paddr / l1->line_size) % l1->num_sets;
    unsigned int l1_tag = paddr / (l1->line_size * l1->num_sets);
    CacheSet *l1_set = &l1->sets[l1_set_index];
    
    for (unsigned long i = 0; i < l1_set->num_lines; i++) {
        if (l1_set->lines[i].valid && l1_set->lines[i].tag == l1_tag)
            return 0; // already there
    }
    
    if (g_l2 != NULL) {
        unsigned long l2_set_index = (paddr / g_l2->line_size) % g_l2->num_sets;
        CacheSet *l2_set = &g_l2->sets[l2_set_index];
        unsigned long hit_in_l2 = 0;
        for (unsigned long i = 0; i < l2_set->num_lines; i++) {
            if (l2_set->lines[i].valid && l2_set->lines[i].tag == (paddr / (g_l2->line_size * g_l2->num_sets))) {
                g_l2->update_policy(l2_set, i);
                latency += g_l2->access_latency;
                hit_in_l2 = 1;
                break;
            }
        }
        if (!hit_in_l2)
            latency += g_l2->access_latency;
    }
    
    unsigned long victim_l1 = l1->find_victim(l1_set);
    l1_set->lines[victim_l1].tag = l1_tag;
    l1_set->lines[victim_l1].valid = 1;
    l1_set->lines[victim_l1].last_access_time = g_current_time;
    latency += l1->access_latency;
    return latency;
}


static void flush_cache_line(CacheLevel *cache, unsigned long paddr) {
    if (cache == NULL)
        return;
    unsigned long set_index = (paddr / cache->line_size) % cache->num_sets;
    unsigned int tag = paddr / (cache->line_size * cache->num_sets);
    CacheSet *set = &cache->sets[set_index];
    for (unsigned long i = 0; i < set->num_lines; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            set->lines[i].valid = 0;
            break;
        }
    }
}

void flush_instruction(unsigned long paddr) {
    if (!g_counting) return;
    flush_cache_line(g_l1_instr, paddr);
    flush_cache_line(g_l2, paddr);
    flush_cache_line(g_l3, paddr);
    flush_cache_line(g_l4, paddr);
}

void flush_data(unsigned long paddr) {
    if (!g_counting) return;
    flush_cache_line(g_l1_data, paddr);
    flush_cache_line(g_l2, paddr);
    flush_cache_line(g_l3, paddr);
    flush_cache_line(g_l4, paddr);
}

void invalidate(unsigned long paddr) {
    if (!g_counting) return;
    if (g_l1_instr) { flush_cache_line(g_l1_instr, paddr); }
    if (g_l1_data) { flush_cache_line(g_l1_data, paddr); }
    if (g_l2)      { flush_cache_line(g_l2, paddr); }
    if (g_l3)      { flush_cache_line(g_l3, paddr); }
    if (g_l4)      { flush_cache_line(g_l4, paddr); }
}

void invalidate_all(void) {
    if (!g_counting) return;
    unsigned long i, j;
    if (g_l1_instr) {
        for (i = 0; i < g_l1_instr->num_sets; i++)
            for (j = 0; j < g_l1_instr->sets[i].num_lines; j++)
                g_l1_instr->sets[i].lines[j].valid = 0;
    }
    if (g_l1_data) {
        for (i = 0; i < g_l1_data->num_sets; i++)
            for (j = 0; j < g_l1_data->sets[i].num_lines; j++)
                g_l1_data->sets[i].lines[j].valid = 0;
    }
    if (g_l2) {
        for (i = 0; i < g_l2->num_sets; i++)
            for (j = 0; j < g_l2->sets[i].num_lines; j++)
                g_l2->sets[i].lines[j].valid = 0;
    }
    if (g_l3) {
        for (i = 0; i < g_l3->num_sets; i++)
            for (j = 0; j < g_l3->sets[i].num_lines; j++)
                g_l3->sets[i].lines[j].valid = 0;
    }
    if (g_l4) {
        for (i = 0; i < g_l4->num_sets; i++)
            for (j = 0; j < g_l4->sets[i].num_lines; j++)
                g_l4->sets[i].lines[j].valid = 0;
    }
}


static void prefetch_into_cache(CacheLevel *cache, unsigned long paddr) {
    if (cache == NULL) return;
    unsigned long set_index = (paddr / cache->line_size) % cache->num_sets;
    unsigned int tag = paddr / (cache->line_size * cache->num_sets);
    CacheSet *set = &cache->sets[set_index];
    for (unsigned long i = 0; i < set->num_lines; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag)
            return; // already there
    }
    unsigned long victim = cache->find_victim(set);
    set->lines[victim].tag = tag;
    set->lines[victim].valid = 1;
    set->lines[victim].last_access_time = g_current_time;
}

unsigned long simulate_prefetch_t0(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) return 0;
    g_current_time++;
    unsigned long latency = 0;
    CacheLevel *l1 = (access_type == 1 ? g_l1_instr : g_l1_data);
    if (l1) {
        unsigned long set_index = (paddr / l1->line_size) % l1->num_sets;
        unsigned int tag = paddr / (l1->line_size * l1->num_sets);
        CacheSet *set = &l1->sets[set_index];
        unsigned long found = 0;
        for (unsigned long i = 0; i < set->num_lines; i++) {
            if (set->lines[i].valid && set->lines[i].tag == tag) {
                found = 1;
                break;
            }
        }
        if (!found) {
            unsigned long victim = l1->find_victim(set);
            set->lines[victim].tag = tag;
            set->lines[victim].valid = 1;
            set->lines[victim].last_access_time = g_current_time;
            latency += l1->access_latency;
        } else {
            latency += l1->access_latency;
        }
    }
    if (g_l2) {
        prefetch_into_cache(g_l2, paddr);
        latency += g_l2->access_latency;
    }
    if (g_l3) {
        prefetch_into_cache(g_l3, paddr);
        latency += g_l3->access_latency;
    }
    return latency;
}

unsigned long simulate_prefetch_t1(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) return 0;
    g_current_time++;
    unsigned long latency = 0;
    if (g_l2) {
        prefetch_into_cache(g_l2, paddr);
        latency += g_l2->access_latency;
    }
    if (g_l3) {
        prefetch_into_cache(g_l3, paddr);
        latency += g_l3->access_latency;
    }
    return latency;
}

unsigned long simulate_prefetch_t2(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) return 0;
    g_current_time++;
    unsigned long latency = 0;
    if (g_l3) {
        prefetch_into_cache(g_l3, paddr);
        latency += g_l3->access_latency;
    } else {
        latency += g_config.mem_latency;
    }
    return latency;
}

unsigned long simulate_prefetch_nta(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) return g_config.mem_latency;
    g_current_time++;
    return g_config.mem_latency;
}

unsigned long simulate_prefetch_w(unsigned long vaddr, unsigned long paddr, unsigned long access_type) {
    if (!g_counting) return 0;
    g_current_time++;
    unsigned long latency = 0;
    if (access_type != 0)
        return 0;
    if (g_l1_data) {
        unsigned long set_index = (paddr / g_l1_data->line_size) % g_l1_data->num_sets;
        unsigned int tag = paddr / (g_l1_data->line_size * g_l1_data->num_sets);
        CacheSet *set = &g_l1_data->sets[set_index];
        unsigned long found = 0;
        for (unsigned long i = 0; i < set->num_lines; i++) {
            if (set->lines[i].valid && set->lines[i].tag == tag) {
                found = 1;
                break;
            }
        }
        if (!found) {
            unsigned long victim = g_l1_data->find_victim(set);
            set->lines[victim].tag = tag;
            set->lines[victim].valid = 1;
            set->lines[victim].last_access_time = g_current_time;
            latency += g_l1_data->access_latency;
        } else {
            latency += g_l1_data->access_latency;
        }
    }
    return latency;
}