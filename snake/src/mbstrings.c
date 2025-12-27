#include "mbstrings.h"

#include <stddef.h>

/* mbslen - multi-byte string length
 * - Description: returns the number of UTF-8 code points ("characters")
 * in a multibyte string. If the argument is NULL or an invalid UTF-8
 * string is passed, returns -1.
 *
 * - Arguments: A pointer to a character array (`bytes`), consisting of UTF-8
 * variable-length encoded multibyte code points.
 *
 * - Return: returns the actual number of UTF-8 code points in `src`. If an
 * invalid sequence of bytes is encountered, return -1.
 *
 * - Hints:
 * UTF-8 characters are encoded in 1 to 4 bytes. The number of leading 1s in the
 * highest order byte indicates the length (in bytes) of the character. For
 * example, a character with the encoding 1111.... is 4 bytes long, a character
 * with the encoding 1110.... is 3 bytes long, and a character with the encoding
 * 1100.... is 2 bytes long. Single-byte UTF-8 characters were designed to be
 * compatible with ASCII. As such, the first bit of a 1-byte UTF-8 character is
 * 0.......
 *
 * You will need bitwise operations for this part of the assignment!
 */
size_t mbslen(const char* bytes) {

    if (bytes == NULL) {
        return -1;
    }
    
    size_t num_chars = 0;

    int one_byte_code = 0b0; // right shift by seven for this
    int two_byte_code = 0b110; // right shift by five
    int three_byte_code = 0b1110; // right shift by four
    int four_byte_code = 0b11110; // right shift by three
    int cont_code = 0b10; // right shift by six

    size_t curr = 0;

    int are_remaining_bytes_valid(size_t *curr_p, size_t num_to_check) {
        unsigned char curr_byte = (unsigned char) bytes[*curr_p];

        if (num_to_check == 0) return 1;

        if (curr_byte >> 6 == cont_code) {
            *curr_p += 1;
            return are_remaining_bytes_valid(curr_p, num_to_check - 1);
        }
        else {
            return 0;
        }
    }

    while (bytes[curr] != '\0') {
        unsigned char curr_byte = (unsigned char) bytes[curr];
        
        // decode curr_byte
        if (curr_byte >> 7 == one_byte_code) {
            num_chars += 1;
            curr += 1;
            continue;
        }
        else if (curr_byte >> 5 == two_byte_code) {
            num_chars += 1;
            curr += 1;
            if (!are_remaining_bytes_valid(&curr, 1)) {
                return -1;
            }
            continue;
        }
        else if (curr_byte >> 4 == three_byte_code) {
            num_chars += 1;
            curr += 1;
            if (!are_remaining_bytes_valid(&curr, 2)) {
                return -1;
            }
            continue;
        }
        else if (curr_byte >> 3 == four_byte_code) {
            num_chars += 1;
            curr += 1;
            if (!are_remaining_bytes_valid(&curr, 3)) {
                return -1;
            }
            continue;
        }
        else {
            return -1;
        }
    }
    return num_chars;
}
