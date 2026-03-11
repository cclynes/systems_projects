#include <stdio.h>
#include <stdlib.h>

#include "../io300.h"
#include "unit_tests.h"

// check string reversal - seeking to successively earlier points works as expected
int main() {
    test_init();

    assert(CACHE_SIZE == 8);

    // Create and open a test file containing the string "hello world"
    struct io300_file* in = create_file_from_string(TEST_FILE, "hello world");
    struct io300_file* out = create_empty_file(TEST_FILE_2);

    // Do some readc operations
    for (int i = 10; i >= 0; i--) {
        io300_seek(in, (off_t) i);
        char ch = io300_readc(in);
        io300_writec(out, ch);
    }

    // Make sure the results from readc are what we expect

    // Close the files
    io300_close(in);
    io300_close(out);

    // Make sure the input file wasn't modified
    // (In tests using write/writec, you can use this to assert that the file contains specific data)
    check_file_matches_string(TEST_FILE_2, "dlrow olleh");

    return 0;
}