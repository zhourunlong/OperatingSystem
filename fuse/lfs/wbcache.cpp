#include <unistd.h>
#include <fcntl.h>
#include "wbcache.h"

std::map <int, int> m;
std::priority_queue <pii, std::vector <pii>, std::greater <pii> > heap;
cacheline_metadata metablocks[NUM_CACHELINE];
std::bitset <NUM_CACHELINE> inheap;
char cache[CACHE_SIZE];
int T;

int read_block_through_cache(void* buf, int block_addr) {
std::lock_guard <std::mutex> guard(io_lock);
    int cacheline_idx = block_addr / BLOCKS_PER_CACHELINE, i;
    if (m.find(cacheline_idx) != m.end()) i = m[cacheline_idx];
    else {
        i = evict(1);
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
    int i = evict(SEGMENT_SIZE / CACHELINE_SIZE);
    memcpy(cache + i * CACHELINE_SIZE, buf, SEGMENT_SIZE * sizeof(char));
    for (int j = 0; j < SEGMENT_SIZE / CACHELINE_SIZE; ++j) {
        int cacheline_idx = segment_addr * SEGMENT_SIZE / CACHELINE_SIZE + j;
        m[cacheline_idx] = i + j;
        metablocks[i + j] = (cacheline_metadata) {cacheline_idx, ++T, true};
        if (!inheap[i + j]) {
            inheap[i + j] = 1;
            heap.push(std::make_pair(T, i + j));
        }
    }
    return SEGMENT_SIZE;
}

int evict(int len) {
    int r;
    if (len == 1) {
        while (1) {
            std::pair <int, int> u = heap.top();
            heap.pop();
            if (metablocks[u.second].timestamp != u.first) {
                heap.push(std::make_pair(metablocks[u.second].timestamp,
                                         u.second));
                continue;
            }
            r = u.second;
            inheap[r] = 0;
            break;
        }
    } else {
        long long sum = 0;
        r = 0;
        for (int i = 0; i < len; ++i)
            sum += metablocks[i].timestamp;
        long long mi = sum;
        for (int i = 1; i <= NUM_CACHELINE - len; ++i) {
            sum += metablocks[i + len - 1].timestamp;
            sum -= metablocks[i - 1].timestamp;
            if (sum < mi) {
                mi = sum;
                r = i;
            }
        }
    }

    int file_handle = open(lfs_path, O_RDWR);
    for (int i = 0; i < len; ++i) {
        int file_offset = metablocks[r + i].cacheline_idx * CACHELINE_SIZE;
        if (metablocks[r + i].dirty)
            pwrite(file_handle, cache + (r + i) * CACHELINE_SIZE,
                   CACHELINE_SIZE, file_offset);
        m.erase(metablocks[r + i].cacheline_idx);
    }
    fsync(file_handle);
    close(file_handle);
    return r;
}

void init_cache() {
    T = 0;
    m.clear();
    while (heap.size()) heap.pop();
    for (int i = 0; i < NUM_CACHELINE; ++i) {
        metablocks[i] = (cacheline_metadata) {-1, 0, false};
        heap.push(std::make_pair(0, i));
    }
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