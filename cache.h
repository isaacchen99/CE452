#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

/* ---------------- Configuration Macros ---------------- */

#define USE_L1      1
#define USE_L2      1
#define USE_L3      1
#define USE_L4      1

#define L1_SIZE     (32 * 1024)   // 32 KB for each L1 cache
#define L1_ASSOC    8
#define L1_LINE     64
#define L1_LATENCY  1

#define L2_SIZE     (256 * 1024)  // 256 KB
#define L2_ASSOC    8
#define L2_LINE     64
#define L2_LATENCY  10

#define L3_SIZE     (512 * 1024)  // 512 KB
#define L3_ASSOC    8
#define L3_LINE     64
#define L3_LATENCY  20

#define L4_SIZE     (2 * 1024 * 1024) // 2 MB
#define L4_ASSOC    16
#define L4_LINE     64
#define L4_LATENCY  40

#define MEM_LATENCY 100

/* ---------------- Type Definitions ---------------- */

/* Replacement Policy Enum */
typedef enum {
    POLICY_LRU,
    POLICY_BIP
} ReplacementPolicy;

/* Cache Line */
typedef struct {
    unsigned int tag;
    int valid;
    unsigned long last_access_time;  // used for LRU/BIP
} CacheLine;

/* Cache Set: contains an array of cache lines */
typedef struct {
    int num_lines;         // equals associativity
    CacheLine *lines;      // dynamically allocated array of cache lines
} CacheSet;

/* Cache Level structure */
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

/* ---------------- Replacement Policy Function Prototypes ---------------- */
void update_policy_lru(CacheSet *set, int line_index);
int find_victim_lru(CacheSet *set);

void update_policy_bip(CacheSet *set, int line_index);
int find_victim_bip(CacheSet *set);

/* ---------------- Cache Level Initialization ---------------- */
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

/* ---------------- New Prefetch Functions ---------------- */
int simulate_prefetch_t0(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_t1(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_t2(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_nta(unsigned int vaddr, unsigned int paddr, int access_type);
int simulate_prefetch_w(unsigned int vaddr, unsigned int paddr, int access_type);

/* ---------------- Internal Global Variables ---------------- */
static CacheLevel *g_l1_data = NULL;
static CacheLevel *g_l1_instr = NULL;
static CacheLevel *g_l2 = NULL;
static CacheLevel *g_l3 = NULL;
static CacheLevel *g_l4 = NULL;
static unsigned long g_current_time = 0;
static int g_mem_accesses = 0;
static int g_instr_accesses = 0;
static int g_data_accesses = 0;
static unsigned long g_total_latency_instr = 0;
static unsigned long g_total_latency_data = 0;
static int g_counting = 0;

/* Cache miss/hit statistics */
static int l1_data_accesses_stats = 0, l1_data_hits_stats = 0;
static int l1_instr_accesses_stats = 0, l1_instr_hits_stats = 0;
static int l2_accesses_stats = 0, l2_hits_stats = 0;
static int l3_accesses_stats = 0, l3_hits_stats = 0;
static int l4_accesses_stats = 0, l4_hits_stats = 0;

#endif /* CACHE_H */