#ifndef wbcache_h
#define wbcache_h

#include <bits/stdc++.h>
#include "utility.h"

typedef std::pair <int, int> pii;

// outer APIs

int read_block_through_cache(void* buf, int block_addr);

// not used
int write_block_through_cache(void* buf, int block_addr);

// not used
int read_segment_through_cache(void* buf, int segment_addr);

int write_segment_through_cache(void* buf, int segment_addr);

// inner functions

const int CACHE_SIZE = 4194304; // 4MB
const int BLOCKS_PER_CACHELINE = 8;
const int NUM_BLOCK = CACHE_SIZE / BLOCK_SIZE;
const int CACHELINE_SIZE = BLOCK_SIZE * BLOCKS_PER_CACHELINE;
const int NUM_CACHELINE = CACHE_SIZE / CACHELINE_SIZE;

struct cacheline_metadata {
    int cacheline_idx;
    int timestamp;                  // for LRU
    bool dirty;                     // for segment buffer
};

extern std::map <int, int> m;       // blockaddr -> cache block idx

extern std::priority_queue <pii, std::vector <pii>, std::greater <pii> > heap;
                                    // pair <timestamp, cache block idx>
                                    // implement LRU

extern cacheline_metadata metablocks[NUM_CACHELINE];

extern std::bitset <NUM_CACHELINE> inheap;
                                    // each cache block in heap or not

extern char cache[CACHE_SIZE];

extern int T;                       // counter

int evict(int n);                   // n: # contiguous blocks to evict

void init_cache();

void flush_cache();

#endif