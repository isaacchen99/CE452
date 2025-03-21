#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#define CONFIG "configDEFAULT.txt"

typedef enum {
    POLICY_LRU,
    POLICY_BIP,
    POLICY_RANDOM
} ReplacementPolicy;

typedef struct {
    unsigned int tag;
    unsigned long valid;
    unsigned long last_access_time;  // used for LRU/BIP 
} CacheLine;

typedef struct {
    unsigned long num_lines; // == associativity
    CacheLine *lines;               
} CacheSet;

typedef struct CacheLevel {
    unsigned long cache_size; // bytes
    unsigned long associativity;
    unsigned long line_size;         
    unsigned long num_sets;          
    unsigned long access_latency; // cycles
    ReplacementPolicy policy;
    CacheSet *sets;                  

    void (*update_policy)(CacheSet *set, unsigned long line_index);
    unsigned long (*find_victim)(CacheSet *set);
} CacheLevel;

typedef struct {
    unsigned long use_l1;
    unsigned long use_l2;
    unsigned long use_l3;
    unsigned long use_l4;

    unsigned long l1_size;
    unsigned long l1_assoc;
    unsigned long l1_line;
    unsigned long l1_latency;
    char l1_policy_str[16];

    unsigned long l2_size;
    unsigned long l2_assoc;
    unsigned long l2_line;
    unsigned long l2_latency;
    char l2_policy_str[16];

    unsigned long l3_size;
    unsigned long l3_assoc;
    unsigned long l3_line;
    unsigned long l3_latency;
    char l3_policy_str[16];

    unsigned long l4_size;
    unsigned long l4_assoc;
    unsigned long l4_line;
    unsigned long l4_latency;
    char l4_policy_str[16];

    unsigned long mem_latency;
} CacheConfig;

extern CacheConfig g_config;

void read_config(const char *filename);

void update_policy_lru(CacheSet *set, unsigned long line_index);
unsigned long find_victim_lru(CacheSet *set);

void update_policy_bip(CacheSet *set, unsigned long line_index);
unsigned long find_victim_bip(CacheSet *set);

void update_policy_random(CacheSet *set, unsigned long line_index);
unsigned long find_victim_random(CacheSet *set);

CacheLevel* init_cache_level(unsigned long cache_size, unsigned long associativity, unsigned long line_size, unsigned long access_latency, ReplacementPolicy policy);
void free_cache_level(CacheLevel *cache);

// simulator API calls
void init(void);
void start(void);
void end(void);
void deinit(void);
unsigned long simulate_memory_access(unsigned long vaddr, unsigned long paddr, unsigned long access_type);
unsigned long simulate_prefetch(unsigned long vaddr, unsigned long paddr, unsigned long access_type);
void flush_instruction(unsigned long paddr);
void flush_data(unsigned long paddr);
void invalidate(unsigned long paddr);
void invalidate_all(void);

// prefetch instructions
unsigned long simulate_prefetch_t0(unsigned long vaddr, unsigned long paddr, unsigned long access_type);
unsigned long simulate_prefetch_t1(unsigned long vaddr, unsigned long paddr, unsigned long access_type);
unsigned long simulate_prefetch_t2(unsigned long vaddr, unsigned long paddr, unsigned long access_type);
unsigned long simulate_prefetch_nta(unsigned long vaddr, unsigned long paddr, unsigned long access_type);
unsigned long simulate_prefetch_w(unsigned long vaddr, unsigned long paddr, unsigned long access_type);

extern CacheLevel *g_l1_data;
extern CacheLevel *g_l1_instr;
extern CacheLevel *g_l2;
extern CacheLevel *g_l3;
extern CacheLevel *g_l4;
extern unsigned long g_current_time;
extern unsigned long g_mem_accesses;
extern unsigned long g_instr_accesses;
extern unsigned long g_data_accesses;
extern unsigned long g_total_latency_instr;
extern unsigned long g_total_latency_data;
extern unsigned long g_counting;

#endif