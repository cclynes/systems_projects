#define DMALLOC_DISABLE 1
#include "dmalloc.hh"
#include <cassert>
#include <cstring>
#include <cstddef>
#include <map>

struct dmalloc_stats g_stats = {
    .nactive = 0,
    .active_size = 0,
    .ntotal = 0,
    .total_size = 0,
    .nfail = 0,
    .fail_size = 0,
    .heap_min = (uintptr_t)-1,
    .heap_max = 0,
};

static std::map<uintptr_t, alloc_info_t*> g_active; // tracks active memory

/**
 * dmalloc(sz,file,line)
 *      malloc() wrapper. Dynamically allocate the requested amount `sz` of memory and 
 *      return a pointer to it 
 * 
 * @arg size_t sz : the amount of memory requested 
 * @arg const char *file : a string containing the filename from which dmalloc was called 
 * @arg long line : the line number from which dmalloc was called 
 * 
 * @return a pointer to the heap where the memory was reserved
 */
void* dmalloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    constexpr size_t A = alignof(std::max_align_t);
    size_t hdr_sz = (sizeof(alloc_info_t) + A - 1) & ~(A - 1);
    size_t ftr_sz = sizeof(size_t);
    const size_t total = hdr_sz + sz + ftr_sz;

    // check for integer overflow
    if (sz > (size_t)-1 - hdr_sz - ftr_sz) {
        g_stats.nfail += 1;
        g_stats.fail_size += sz;
        return NULL;
    }

    // allocate and store metadata
    unsigned char *raw = static_cast<unsigned char *>(base_malloc(total));

    if (!raw) {
        g_stats.nfail += 1;
        g_stats.fail_size += sz;
        return NULL;
    }

    // update stats
    g_stats.nactive += 1;
    g_stats.active_size += sz;
    g_stats.ntotal += 1;
    g_stats.total_size += sz;

    auto *hdr = reinterpret_cast<alloc_info_t*>(raw);
    uintptr_t user = reinterpret_cast<uintptr_t>(raw + hdr_sz);

    // add to map of active allocations
    base_allocator_disable(true);
    g_active[user] = hdr;
    base_allocator_disable(false);

    // write boundary val into footer
    uint64_t canary = BOUND_VAL;
    memcpy(raw + hdr_sz + sz, &canary, ftr_sz);

    if (user < g_stats.heap_min) {
        g_stats.heap_min = user;
    }
    uintptr_t end = user + sz;
    if (end > g_stats.heap_max) {
        g_stats.heap_max = end;
    }

    hdr->ptr_val = user;
    hdr->alloc_size = sz;
    hdr->is_allocated = IS_ALLOC;
    hdr->padding = BOUND_VAL;
    hdr->file = file;
    hdr->line = line;

    return (void *)user;
}

/**
 * dfree(ptr, file, line)
 *      free() wrapper. Release the block of heap memory pointed to by `ptr`. This should 
 *      be a pointer that was previously allocated on the heap. If `ptr` is a nullptr do nothing. 
 * 
 * @arg void *ptr : a pointer to the heap 
 * @arg const char *file : a string containing the filename from which dfree was called 
 * @arg long line : the line number from which dfree was called 
 */
void dfree(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    if (ptr == NULL) {
        return;
    }

    // get position of header
    constexpr size_t A = alignof(std::max_align_t);
    size_t hdr_sz = (sizeof(alloc_info_t) + A - 1) & ~(A - 1);
    uintptr_t hdr_pos = (uintptr_t)ptr - (uintptr_t)hdr_sz;

    // check that pointer alignment is valid
    if (hdr_pos % 16 != 0) {
        fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not allocated\n", file, line, ptr);
        abort();
    }

    // check that pointer is in heap
    if ((uintptr_t)ptr < g_stats.heap_min || (uintptr_t)ptr >= g_stats.heap_max) {
        fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not in heap\n", file, line, ptr);
        abort();
    }

    // get position of header
    alloc_info_t *hdr = (alloc_info_t *)hdr_pos;
    alloc_info_t info = *(alloc_info_t *)hdr;

    // check that address is valid
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);

    // check that memory is allocated (hasn't been freed)
    if (info.ptr_val == (uintptr_t)ptr && info.is_allocated != IS_ALLOC) {
        fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, double free\n", file, line, ptr);
        abort();
    }

    if ((info.ptr_val) != (uintptr_t)ptr || !g_active.count(p)) {
        
        fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not allocated\n", file, line, ptr);

        // check if address is inside another previously allocated memory block
        auto it = g_active.upper_bound(p);
        if (it != g_active.begin()) {
            auto jt = std::prev(it);

            uintptr_t start = jt->first;
            alloc_info_t* a = jt->second;
            uintptr_t end = start + a->alloc_size;

            if (p > start && p < end) {
                uintptr_t offset = p - start;
                fprintf(stderr, "%s:%ld: %p is %zu bytes inside a %zu byte region allocated here\n", a->file, a->line, ptr, offset, a->alloc_size);
            }

        }

        abort();
    }

    // check for boundary errors
    uint64_t to_check;
    auto ftr_pos = reinterpret_cast<const unsigned char*>(hdr_pos) + hdr_sz + hdr->alloc_size;
    memcpy(&to_check, ftr_pos, sizeof(to_check));
    if (to_check != BOUND_VAL || hdr->padding != BOUND_VAL) {
        fprintf(stderr, "MEMORY BUG: %s:%ld: detected wild write during free of pointer %p\n", file, line, ptr);
    }

    // update metadata and free the memory
    hdr->is_allocated = 0;
    base_free(hdr);

    base_allocator_disable(true);
    g_active.erase(p); // erase user ptr from the active list
    base_allocator_disable(false);

    // update stats
    g_stats.nactive -= 1;
    g_stats.active_size -= info.alloc_size;
}

/**
 * dcalloc(nmemb, sz, file, line)
 *      calloc() wrapper. Dynamically allocate enough memory to store an array of `nmemb` 
 *      number of elements with wach element being `sz` bytes. The memory should be initialized 
 *      to zero  
 * 
 * @arg size_t nmemb : the number of items that space is requested for
 * @arg size_t sz : the size in bytes of the items that space is requested for
 * @arg const char *file : a string containing the filename from which dcalloc was called 
 * @arg long line : the line number from which dcalloc was called 
 * 
 * @return a pointer to the heap where the memory was reserved
 */
void* dcalloc(size_t nmemb, size_t sz, const char* file, long line) {

    // check for integer overflow
    if (!(nmemb == 0 || sz == 0) && sz > (size_t)-1 / nmemb) {
        g_stats.nfail += 1;
        g_stats.fail_size = (size_t)-1;
        return NULL;
    } 

    // Your code here (to fix test014).
    void* ptr = dmalloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}

/**
 * get_statistics(stats)
 *      fill a dmalloc_stats pointer with the current memory statistics  
 * 
 * @arg dmalloc_stats *stats : a pointer to the the dmalloc_stats struct we want to fill
 */
void get_statistics(dmalloc_stats* stats) {
    // Stub: set all statistics to enormous numbers

    *stats = g_stats;
    return;
}

/**
 * print_statistics()
 *      print the current memory statistics to stdout       
 */
void print_statistics() {
    dmalloc_stats stats;
    get_statistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

/**  
 * print_leak_report()
 *      Print a report of all currently-active allocated blocks of dynamic
 *      memory.
 */
void print_leak_report() {
    for (auto it = g_active.rbegin(); it != g_active.rend(); it++) {
        void* ptr = (void *)it->first;
        alloc_info_t *info = it->second;
        fprintf(stdout, "LEAK CHECK: %s:%ld: allocated object %p with size %ld\n", info->file, info->line, ptr, info->alloc_size);
    }
    return;
}