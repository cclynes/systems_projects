#include <stdio.h>
#include <stdlib.h>

#include "../io300.h"
#include "unit_tests.h"

// calling lseek repeatedly works as expected
int main() {
    test_init();

    assert(CACHE_SIZE == 8);

    char contents[] = "hello world";
    struct io300_file *in = create_file_from_string(TEST_FILE, contents);
    struct io300_file *out = create_empty_file(TEST_FILE_2);

    for (off_t i=0; i < 11; i++) {
        int rrc = io300_seek(in, i); if (rrc < 0) { return -1; }
        int ch = io300_readc(in);
        int wrc = io300_writec(out, ch); if (wrc < 0) { return -1; }
    }

    io300_close(in);
    io300_close(out);

    check_file_matches_string(TEST_FILE_2, "hello world");
}