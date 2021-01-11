#include <unistd.h>
#include <fcntl.h>
#include "wbcache.h"

std::map <int, int> m;
std::priority_queue <pii, std::vector <pii>, std::greater <pii> > heap;
cacheline_metadata metablocks[NUM_CACHELINE];
int inheap[NUM_CACHELINE];
char cache[CACHE_SIZE];
int T;

int read_block_through_cache(void* buf, int block_addr) {
std::lock_guard <std::mutex> guard(io_lock);
    int cacheline_idx = block_addr / BLOCKS_PER_CACHELINE, i;
    if (m.find(cacheline_idx) != m.end()) {
        i = m[cacheline_idx];
        metablocks[i].timestamp = ++T;
    } else {
        i = evict();

        int file_handle = open(lfs_path, O_RDWR);
        int file_offset = cacheline_idx * CACHELINE_SIZE;
        int read_length = pread(file_handle, cache + i * CACHELINE_SIZE,
                                CACHELINE_SIZE, file_offset);
        close(file_handle);
        m[cacheline_idx] = i;
        metablocks[i] = (cacheline_metadata) {cacheline_idx, ++T, false};
        if (!inheap[i]) {
            inheap[i] = 1;
            heap.push(std::make_pair(T, i));
        }
    }

    memcpy(buf, cache + i * CACHELINE_SIZE
              + block_addr % BLOCKS_PER_CACHELINE * BLOCK_SIZE,
              BLOCK_SIZE * sizeof(char));
    return BLOCK_SIZE;
}

int write_block_through_cache(void* buf, int block_addr) {
std::lock_guard <std::mutex> guard(io_lock);
    // not used, do nothing
    return 0;
}

int read_segment_through_cache(void* buf, int segment_addr) {
std::lock_guard <std::mutex> guard(io_lock);
    // not used, do nothing
    return 0;
}

int write_segment_through_cache(void* buf, int segment_addr) {
std::lock_guard <std::mutex> guard(io_lock);
    int first_cacheline_idx = CACHELINES_PER_SEGMENT * segment_addr;
    for (int j = 0; j < CACHELINES_PER_SEGMENT; ++j) {
        int cacheline_idx = first_cacheline_idx + j;
        int i;
        if (m.find(cacheline_idx) != m.end()) {
            i = m[cacheline_idx];
            metablocks[i].timestamp = ++T;
            metablocks[i].dirty = true;
        } else {
            i = evict();
            m[cacheline_idx] = i;
            metablocks[i] = (cacheline_metadata) {cacheline_idx, ++T, true};
            if (!inheap[i]) {
                inheap[i] = 1;
                heap.push(std::make_pair(T, i));
            }
        }
        memcpy(cache + i * CACHELINE_SIZE, buf + j * CACHELINE_SIZE, CACHELINE_SIZE);
    }
    return SEGMENT_SIZE;
}

int evict() {
    int r;
    while (1) {
        std::pair <int, int> u = heap.top();
        heap.pop();
        if (metablocks[u.second].timestamp != u.first) {
            if (inheap[u.second] == 1)
                heap.push(std::make_pair(metablocks[u.second].timestamp,
                                            u.second));
            else
                --inheap[u.second];
            continue;
        }
        r = u.second;
        --inheap[r];
        break;
    }
    if (metablocks[r].dirty) {
        int file_handle = open(lfs_path, O_RDWR);
        int file_offset = metablocks[r].cacheline_idx * CACHELINE_SIZE;
        pwrite(file_handle, cache + r * CACHELINE_SIZE, CACHELINE_SIZE, file_offset);
        close(file_handle);
        fsync(file_handle);
    }
    m.erase(metablocks[r].cacheline_idx);
    return r;
}

void init_cache() {
    m.clear();
    while (heap.size()) heap.pop();
    for (int i = 0; i < NUM_CACHELINE; ++i) {
        inheap[i] = 1;
        metablocks[i] = (cacheline_metadata) {-1, i, false};
        heap.push(std::make_pair(0, i));
    }
    T = NUM_CACHELINE;
}

void flush_cache() {
    int file_handle = open(lfs_path, O_RDWR);
    for (int i = 0; i < NUM_CACHELINE; ++i) {
        int file_offset = metablocks[i].cacheline_idx * CACHELINE_SIZE;
        if (metablocks[i].dirty)
            pwrite(file_handle, cache + i * CACHELINE_SIZE,
                   CACHELINE_SIZE, file_offset);
    }
    fsync(file_handle);
    close(file_handle);
}