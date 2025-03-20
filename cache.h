#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

/* ---------------- Replacement Policy Enum ---------------- */
typedef enum {
    POLICY_LRU,
    POLICY_BIP,
    POLICY_RANDOM
} ReplacementPolicy;

/* ---------------- Cache Line ---------------- */
typedef struct {
    unsigned int tag;
    int valid;
    unsigned long last_access_time;  // used for LRU/BIP (not used for RANDOM)
} CacheLine;

/* ---------------- Cache Set ---------------- */
typedef struct {
    int num_lines;         // equals associativity
    CacheLine *lines;      // dynamically allocated array of cache lines
} CacheSet;

/* ---------------- Cache Level ---------------- */
typedef struct CacheLevel {
    int cache_size;        // in bytes
    int associativity;     // number of lines per set
    int line_size;         // in bytes
    int num_sets;          // computed as cache_size / (line_size * associativity)
    int access_latency;    // in cycles (access time)
    ReplacementPolicy policy;
    CacheSet *sets;        // array of sets

    /* Function pointers for replacement policy operations */
    void (*update_policy)(CacheSet *set, int line_index);
    int (*find_victim)(CacheSet *set);
} CacheLevel;

/* ---------------- Configuration Structure ---------------- */
typedef struct {
    int use_l1;
    int use_l2;
    int use_l3;
    int use_l4;

    int l1_size;
    int l1_assoc;
    int l1_line;
    int l1_latency;
    char l1_policy_str[16];

    int l2_size;
    int l2_assoc;
    int l2_line;
    int l2_latency;
    char l2_policy_str[16];

    int l3_size;
    int l3_assoc;
    int l3_line;
    int l3_latency;
    char l3_policy_str[16];

    int l4_size;
    int l4_assoc;
    int l4_line;
    int l4_latency;
    char l4_policy_str[16];

    int mem_latency;
} CacheConfig;

/* Global configuration variable. */
extern CacheConfig g_config;

/* ---------------- Function Prototypes ---------------- */

/* Configuration file reader */
void read_config(const char *filename);

/* Replacement Policy Function Prototypes */
void update_policy_lru(CacheSet *set, int line_index);
int find_victim_lru(CacheSet *set);

void update_policy_bip(CacheSet *set, int line_index);
int find_victim_bip(CacheSet *set);

void update_policy_random(CacheSet *set, int line_index);
int find_victim_random(CacheSet *set);

/* Cache Level Initialization and Deallocation */
CacheLevel* init_cache_level(int cache_size, int associativity, int line_size, int access_latency, ReplacementPolicy policy);
void free_cache_level(CacheLevel *cache);

/* ---------------- Simulator API ---------------- */
void init(void);
void start(void);
void end(void);
void deinit(void);
int simulate_memory_access(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch(unsigned int vaddr, unsigned int paddr, int access_type);
void flush_instruction(unsigned int paddr);
void flush_data(unsigned int paddr);
void invalidate(unsigned int paddr);
void invalidate_all(void);

/* New Prefetch Functions */
int simulate_prefetch_t0(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_t1(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_t2(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_nta(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_w(unsigned int vaddr, unsigned int paddr, int access_type);

/* ---------------- Global Internal Variables ---------------- */
extern CacheLevel *g_l1_data;
extern CacheLevel *g_l1_instr;
extern CacheLevel *g_l2;
extern CacheLevel *g_l3;
extern CacheLevel *g_l4;
extern unsigned long g_current_time;
extern int g_mem_accesses;
extern int g_instr_accesses;
extern int g_data_accesses;
extern unsigned long g_total_latency_instr;
extern unsigned long g_total_latency_data;
/* g_counting indicates whether simulation counting is active */
extern int g_counting;

#endif /* CACHE_H */