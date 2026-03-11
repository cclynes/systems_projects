/*
 * rtest003.c - basic writec 1
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "unit_tests.h"
#include "../io300.h"


int main() {
    test_init();
    assert(CACHE_SIZE == 8);

    struct io300_file* f = create_empty_file(TEST_FILE);

    char contents[3] = {0x00, 0x01, 0x03}; // File data can be any bytes, not just characters!

    for(int i = 0; i < 3; i++) {
	    io300_writec(f, contents[i]);
    }

    io300_close(f);

    // This is how you can check that a file matches, if the data isn't a string
    check_file_matches_bytes(TEST_FILE, contents, 3);
}
