#include <stdio.h>
#include <stdlib.h>

#include "../io300.h"
#include "unit_tests.h"

// multiple out-of-bounds writes don't overwrite each other
int main() {
    test_init();

    assert(CACHE_SIZE == 8);

    struct io300_file *f = create_file_from_string(TEST_FILE, "abcdefgh");

    // seek to after EOF
    io300_seek(f, 10);
    io300_writec(f, 'i');
    io300_writec(f, 'j');

    // check that file is formed as expected
    io300_seek(f, 8);
    char c8 = io300_readc(f);
    char c9 = io300_readc(f);
    char c10 = io300_readc(f);
    char c11 = io300_readc(f);
    
    assert(c8 == 0x00);
    assert(c9 == 0x00);
    assert(c10 == 'i');
    assert(c11 == 'j');

    io300_close(f);
    check_file_matches_string(TEST_FILE, "abcdefgh\0\0ij");
}