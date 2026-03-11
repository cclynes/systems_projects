#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../io300.h"

/*
 *  student.c - Your implementation
 */

#ifndef CACHE_SIZE
#define CACHE_SIZE 8
#endif

#if (CACHE_SIZE < 4)
#error "internal cache size should not be below 4."
#error "if you changed this during testing, that is fine."
#error "when handing in, make sure it is reset to the provided value"
#error "if this is not done, the autograder will not run"
#endif

// Prototypes for helper functions defined later
int fetch(struct io300_file* const f);
int flush(struct io300_file* const f);

/*
 * This macro enables/disables the dbg() function. Use it to silence your
 * debugging info.
 * Use the dbg() function instead of printf debugging if you don't want to
 * hunt down 30 printfs when you want to hand in
 */
#define DEBUG_PRINT 0
#define DEBUG_STATISTICS 1

struct io300_file {
    /* read,write,seek all take a file descriptor as a parameter */
    int fd;
    /* this will serve as our cache */
    char* cache;

    off_t cache_pos; // position in file of the start of the cache
    size_t cache_idx; // cache-relative read/write head
    size_t cache_valid; // start of invalid portion of the cache - used if fetch doesn't write into the whole cache

    size_t dirty_start;
    size_t dirty_end;

    /* Used for debugging, keep track of which io300_file is which */
    char* description;
    /* To tell if we are getting the performance we are expecting */
    struct io300_statistics {
        int read_calls;
        int write_calls;
        int seeks;
    } stats;
};

/*
 *  Assert the properties that you would like your file to have at all times.
 *  Call this function frequently (like at the beginning of each function) to
 *   catch logical errors early on in development.
 */
static void check_invariants(struct io300_file* f) {
    assert(f != NULL);
    assert(f->cache != NULL);
    assert(f->fd >= 0);
    assert(f->dirty_end <= f->cache_valid); // we should never have unwritten portions of the cache that are also invalid

    // TODO: Add more invariants
}

/*
 *  Wrapper around printf that provides information about the
 *  given file. You can turn off messages printed by this function
 *  by changing the DEBUG_PRINT macro above.
 */
static void dbg(struct io300_file* f, char* fmt, ...) {
    (void)f;
    (void)fmt;
#if (DEBUG_PRINT == 1)
    static char buff[300];
    size_t const size = sizeof(buff);
    int n = snprintf(buff, size,
                     "{desc:%s, } -- ", f->description);
    int const bytes_left = size - n;
    va_list args;
    va_start(args, fmt);
    vsnprintf(&buff[n], bytes_left, fmt, args);
    va_end(args);
    printf("%s", buff);
#endif
}

struct io300_file* io300_open(const char* const path, int mode, char* description) {
    if (path == NULL) {
        fprintf(stderr, "error: null file path\n");
        return NULL;
    }

    // Set flags for how file is created [DO NOT CHANGE THIS]
    int flags = O_CREAT | O_SYNC;
    switch(mode) {
    case MODE_READ:
	flags |= O_RDONLY;
	break;
    case MODE_WRITE:
	flags |= O_RDWR | O_TRUNC;
	break;
    case (MODE_READ|MODE_WRITE):
	flags |= O_RDWR;
	break;
    default:
	fprintf(stderr, "error: invalid file mode %02x\n", mode);
	return NULL;
    }

    int const fd = open(path, flags, S_IRUSR | S_IWUSR); // DO NOT CHANGE THIS LINE
    if (fd == -1) {
        fprintf(stderr, "error: could not open file: `%s`: %s\n", path,
                strerror(errno));
        return NULL;
    }

    struct io300_file* const ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        fprintf(stderr, "error: could not allocate io300_file\n");
        close(fd);
        return NULL;
    }

    ret->fd = fd;
    ret->cache = malloc(CACHE_SIZE);
    if (ret->cache == NULL) {
        fprintf(stderr, "error: could not allocate file cache\n");
        close(ret->fd);
        free(ret);
        return NULL;
    }
    // Initialize debugging info
    ret->description = description;
    ret->stats.read_calls = 0;
    ret->stats.write_calls = 0;
    ret->stats.seeks = 0;

    // initialize metadata
    ret->cache_pos = 0;
    ret->cache_idx = 0;
    ret->cache_valid = CACHE_SIZE + 1;
    
    ret->dirty_start = 0;
    ret->dirty_end = 0;

    // initialize the cache
    int bytes_fetched = fetch(ret);
    if (bytes_fetched < 0) {
        fprintf(stderr, "error: could not fetch from file %d", ret->fd);
        return NULL; }
    if (bytes_fetched < CACHE_SIZE) { ret->cache_valid = bytes_fetched; }

    check_invariants(ret);
    dbg(ret, "Just finished initializing file from path: %s\n", path);
    return ret;
}

size_t min(size_t a, size_t b) {
    if (a < b) { return a; }
    return b;
}

size_t max(size_t a, size_t b) {
    if (a > b) { return a; }
    return b;
}

int io300_seek(struct io300_file* const f, off_t const pos) {
    check_invariants(f);
    f->stats.seeks++;

    if (pos < 0) { return -1; }

    // if we can seek within the cache, do so
    if (f->cache_pos <= (off_t) pos && (off_t) pos < f->cache_pos + (off_t) min(f->cache_valid, CACHE_SIZE)) {
        f->cache_idx = (size_t) pos - f->cache_pos;
        return (int) pos;
    }

    // update metadata
    flush(f);
    off_t block_pos = pos - (pos % CACHE_SIZE);
    f->cache_pos = block_pos;
    f->cache_idx = (size_t) (pos % CACHE_SIZE);

    // if seek is beyond EOF, fill bytes from EOF to seek point with 0x00
    off_t file_end = lseek(f->fd, 0, SEEK_END);
    if (file_end < 0) { return file_end; }
    
    off_t distance_to_end = file_end - pos;
    if (distance_to_end < 0) {

        size_t num_bytes_to_fill = (size_t) (pos - file_end);
        char buf[num_bytes_to_fill];
        memset(buf, 0, num_bytes_to_fill);

        ssize_t write_rc = io300_write(f, (const char *)buf, num_bytes_to_fill); // io300_write updates f->cache_valid, so we don't have to
        if (write_rc <= 0) { return -1; }
    }

    // otherwise, just seek and fetch from file, and update metadata
    else {
        off_t seek_res = lseek(f->fd, pos, SEEK_SET);
        if (seek_res < 0) { return seek_res; }

        if (distance_to_end >= CACHE_SIZE) {
            f->cache_valid = CACHE_SIZE + 1;
        }
        else {
            f->cache_valid = distance_to_end;
        }
        
        int fetch_res = fetch(f);
        if (fetch_res < 0) {return fetch_res; }
    }
    return pos;
}

int io300_close(struct io300_file* const f) {
    check_invariants(f);

#if (DEBUG_STATISTICS == 1)
    printf("stats: {desc: %s, read_calls: %d, write_calls: %d, seeks: %d}\n",
           f->description, f->stats.read_calls, f->stats.write_calls,
           f->stats.seeks);
#endif

    flush(f);

    close(f->fd);
    free(f->cache);
    free(f);
    return 0;
}

/*
 * io300_filesize: Get the size of a file This function is part of the
 * io300 library: some tests will use it to get the file's current
 * size.  You may also use it as a helper for your implementation, or
 * modify it to better fit your design, but this is not required.
 *
 * WARNING:  this function makes a system call (fstat)!
 */
off_t io300_filesize(struct io300_file* const f) {
    check_invariants(f);
    struct stat s;
    int const r = fstat(f->fd, &s);   // system call!
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}

int io300_readc(struct io300_file* const f) {
    check_invariants(f);

    unsigned char ch;
    ssize_t res = io300_read(f, (char *) &ch, 1);
    if (res < 0) {
        return -1;
    }
    return (int) ch;
}

int io300_writec(struct io300_file* f, int ch) {
    check_invariants(f);

    unsigned char c = (unsigned char)ch;
    if (io300_write(f, (const char *) &c, 1) == 1) {
        return (int)c;
    } else {
        return -1;
    }
}

// reads from the file via cache
// assumes buff points to at least sz bytes of accessible memory
ssize_t io300_read(struct io300_file* const f, char* const buff,
                   size_t const sz) {
    check_invariants(f);

    // check how much we can read from the cache
    size_t bytes_until_cache_end = CACHE_SIZE - f->cache_idx;
    size_t readable_bytes = min(sz, bytes_until_cache_end);

    size_t bytes_until_invalid = f->cache_valid - f->cache_idx;
    readable_bytes = min(readable_bytes, bytes_until_invalid);

    // if we've hit EOF, return without reading
    if (bytes_until_invalid == 0) {
        return -1;
    }

    // read whatever we can from the cache
    if (readable_bytes > 0) {
        memcpy(buff, f->cache + f->cache_idx, readable_bytes);
    }

    // if we've read everything requested or hit file end, update metadata and return
    if (readable_bytes == sz || readable_bytes == bytes_until_invalid) {
        f->cache_idx += readable_bytes;
        return readable_bytes;
    }

    // otherwise, fetch the next segment
    int fetch_res = fetch(f); // note that fetch also flushes if needed
    if (fetch_res < 0) { return fetch_res; }

    // fetch updates the metadata, so now we can just continue with the read
    return io300_read(f, buff + readable_bytes, sz - readable_bytes);
}
ssize_t io300_write(struct io300_file* const f, const char* buff,
                    size_t const sz) {
    check_invariants(f);

    size_t bytes_until_cache_end = CACHE_SIZE - f->cache_idx;
    size_t writable_bytes = min(sz, bytes_until_cache_end);

    int bytes_until_clean = (ssize_t) f->dirty_end - (ssize_t) f->cache_idx; // if < 0, we don't have to worry about dirty reads
    size_t bytes_until_dirty;
    if (bytes_until_clean <= 0) {
        bytes_until_dirty = sz;
    }
    else {
        bytes_until_dirty = max(0, (ssize_t) f->dirty_start - (ssize_t) f->cache_idx);
    }
    writable_bytes = min(writable_bytes, bytes_until_dirty);

    // write whatever we can to the cache and update dirty bounds, cache_idx, cache_valid
    if (writable_bytes > 0) {
        memcpy(f->cache + f->cache_idx, buff, writable_bytes);

        if (f->dirty_end <= f->dirty_start) {
            f->dirty_start = f->cache_idx;
            f->dirty_end = f->cache_idx + writable_bytes;
        }
        else {
            f->dirty_start = min(f->dirty_start, f->cache_idx);
            f->dirty_end = max(f->dirty_end, f->cache_idx + writable_bytes);
        }

        f->cache_valid = max(f->cache_valid, f->cache_idx + writable_bytes);
        if (f->cache_valid == CACHE_SIZE) {
            f->cache_valid += 1;
        }
        f->cache_idx += writable_bytes;
    }

    // if we've written over EOF, extend cache_valid
    int are_over_eof = (f->cache_idx > f->cache_valid);
    if (are_over_eof) {
        f->cache_valid = f->cache_idx;
        if (f->cache_idx == CACHE_SIZE) {
            f->cache_valid += 1;
        }
    }

    // if we've written everything requested, update metadata and return
    if (writable_bytes == sz) {
        return (ssize_t) writable_bytes;
    }

    // if not over EOF, fetch the next segment (which also flushes the current one and updates metadata)
    if (!are_over_eof) {
        int fetch_rc = fetch(f);
        if (fetch_rc < 0) { return fetch_rc; }
    }

    // if over EOF, flush the segment but don't fetch; update metadata
    else {
        int flush_rc = flush(f); if (flush_rc < 0) { return flush_rc; }
        memset(f->cache, 0, CACHE_SIZE);
    }

    return writable_bytes + io300_write(f, buff + writable_bytes, sz - writable_bytes);
}

// flushes cache
// upon returning, should restore kernel offset to its value at time of call
int flush(struct io300_file* const f) {
    check_invariants(f);
    

    if (f->dirty_end <= f->dirty_start) {
        return 0;
    }

    // store offset and jump to the proper place in file
    off_t logical = f->cache_pos + f->cache_idx;
    if (logical < 0) { return -1; }
    off_t offset = lseek(f->fd, f->cache_pos + f->dirty_start, SEEK_SET); // TODO: exchange with io300 seek
    if (offset < 0) {
        return (int)offset;
    }

    // make sys call to write
    size_t count = 0;
    size_t dirty_size = f->dirty_end - f->dirty_start;
    while (count < dirty_size) {
        ssize_t bytes_written = write(f->fd, f->cache + f->dirty_start + count, dirty_size - count);

        if (bytes_written <= 0) {
            lseek(f->fd, logical, SEEK_SET);
            return -1;
        }

        count += bytes_written;
    }

    // reset kernel offset and dirty pointers
    off_t new_idx = lseek(f->fd, logical, SEEK_SET); // TODO: exchange with io300 seek
    if (new_idx < 0) { return -1; }
    f->dirty_start = 0;
    f->dirty_end = 0;

    return 0;
}

// fetches data from the file
// increments kernel offset by no. bytes fetched
int fetch(struct io300_file* const f) {
    check_invariants(f);
    
    // flush cache first, if dirty
    if (f->dirty_start < f->dirty_end) {
        int res = flush(f);
        if (res < 0) { return -1; }
    }

    off_t start = f->cache_pos;
    if (start < 0) {return -1; }
    
    // read from file
    size_t count = 0;
    while (count < CACHE_SIZE) {
        off_t seek_rc = lseek(f->fd, start + count, SEEK_SET); if (seek_rc < 0) { return -1; }
        ssize_t res = read(f->fd, f->cache + count, CACHE_SIZE - count);
        if (res < 0) {
            return res;
        }
        if (res == 0) {
            break;
        }
        count += res;
    }

    // update metadata
    f->cache_valid = count;
    if (f->cache_valid == CACHE_SIZE) {
        f->cache_valid += 1;
    }
    f->cache_pos = start;
    f->cache_idx = 0;

    return count;
}