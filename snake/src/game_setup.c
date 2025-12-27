#include "game_setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

#include "game.h"
#include "common.h"

// Some handy dandy macros for decompression
#define E_CAP_HEX 0x45
#define E_LOW_HEX 0x65
#define G_CAP_HEX 0x47
#define G_LOW_HEX 0x67
#define S_CAP_HEX 0x53
#define S_LOW_HEX 0x73
#define W_CAP_HEX 0x57
#define W_LOW_HEX 0x77
#define DIGIT_START 0x30
#define DIGIT_END 0x39

/** Initializes the board with walls around the edge of the board, and a ring
 * of grass just inside the wall.
 *
 * Modifies values pointed to by cells_p, width_p, and height_p and initializes
 * cells array to reflect this default board.
 *
 * Returns INIT_SUCCESS to indicate that it was successful.
 *
 * Arguments:
 *  - cells_p: a pointer to a memory location where a pointer to the first
 *             element in a newly initialized array of cells should be stored.
 *  - width_p: a pointer to a memory location where the newly initialized
 *             width should be stored.
 *  - height_p: a pointer to a memory location where the newly initialized
 *              height should be stored.
 */
enum board_init_status initialize_default_board(int** cells_p, size_t* width_p,
                                                size_t* height_p) {
    *width_p = 20;
    *height_p = 10;
    int* cells = malloc(20 * 10 * sizeof(int));
    *cells_p = cells;
    for (int i = 0; i < 20 * 10; i++) {
        cells[i] = PLAIN_CELL;
    }

    // Set edge cells!
    // Top and bottom edges:
    for (int i = 0; i < 20; ++i) {
        cells[i] = FLAG_WALL;
        cells[i + (20 * (10 - 1))] = FLAG_WALL;
    }
    // Left and right edges:
    for (int i = 0; i < 10; ++i) {
        cells[i * 20] = FLAG_WALL;
        cells[i * 20 + 20 - 1] = FLAG_WALL;
    }

    // Set grass cells!
    // Top and bottom edges:
    for (int i = 1; i < 19; ++i) {
        cells[i + 20] = FLAG_GRASS;
        cells[i + (20 * (9 - 1))] = FLAG_GRASS;
    }
    // Left and right edges:
    for (int i = 1; i < 9; ++i) {
        cells[i * 20 + 1] = FLAG_GRASS;
        cells[i * 20 + 19 - 1] = FLAG_GRASS;
    }

    // Add snake
    cells[20 * 2 + 2] = FLAG_SNAKE;

    return INIT_SUCCESS;
}

/** Initialize variables relevant to the game board.
 * Arguments:
 *  - cells_p: a pointer to a memory location where a pointer to the first
 *             element in a newly initialized array of cells should be stored.
 *  - width_p: a pointer to a memory location where the newly initialized
 *             width should be stored.
 *  - height_p: a pointer to a memory location where the newly initialized
 *              height should be stored.
 *  - snake_p: a pointer to your snake struct (not used until part 3!)
 *  - board_rep: a string representing the initial board. May be NULL for
 * default board.
 */
enum board_init_status initialize_game(int** cells_p, size_t* width_p,
                                       size_t* height_p, snake_t* snake_p,
                                       char* board_rep) {
    
    
    // initialize snake
    snake_p->snake_dir = RIGHT;
    snake_p->snake_pos = NULL;
    size_t *start = malloc(sizeof(*start));
    *start = 42;
    ll_insert_first(&snake_p->snake_pos, start);


    // initialize board
    enum board_init_status status;

    if (board_rep == NULL) {
        status = initialize_default_board(cells_p, width_p, height_p);
        }

    // initialize board via decompression and find snake position
    else {
        status = decompress_board_str(cells_p, width_p, height_p, snake_p, board_rep);
        for (size_t i = 0; i < (*width_p) * (*height_p); i++) {
            if ((*cells_p)[i] & FLAG_SNAKE) {
                *start = i;
                break;
            }
        }
    }
    place_food(*cells_p, *width_p, *height_p);

    return status;
}

/** Takes in a string `compressed` and initializes values pointed to by
 * cells_p, width_p, and height_p accordingly. Arguments:
 *      - cells_p: a pointer to the pointer representing the cells array
 *                 that we would like to initialize.
 *      - width_p: a pointer to the width variable we'd like to initialize.
 *      - height_p: a pointer to the height variable we'd like to initialize.
 *      - snake_p: a pointer to your snake struct (not used until part 3!)
 *      - compressed: a string that contains the representation of the board.
 * Note: We assume that the string will be of the following form:
 * B24x80|E5W2E73|E5W2S1E72... To read it, we scan the string row-by-row
 * (delineated by the `|` character), and read out a letter (E, S or W) a number
 * of times dictated by the number that follows the letter.
 */
enum board_init_status set_dimensions(size_t* width_p, size_t* height_p, char* compressed) {
    int row_dim = 0;
    int col_dim = 0;
    bool on_rows = true;
    
    // get dimension specs
    for (size_t i=0; compressed[i] != '|'; i++) {
        if (compressed[i] == 'B') { continue; }
        if (compressed[i] == 'x') { on_rows = false; continue; }

        // check that char is a numerical character
        if ((compressed[i] > '9') || (compressed[i] < '0')) {
            return INIT_ERR_INCORRECT_DIMENSIONS;
        }

        if (on_rows) {
            row_dim = row_dim * 10 + (compressed[i] - 0x30);
        }
        else {
            col_dim = col_dim * 10 + (compressed[i] - 0x30);
        }
    }

    // set dimensions and return error status
    *width_p = col_dim;
    *height_p = row_dim;

    return INIT_SUCCESS;
}

enum board_init_status decompress_board_str(int** cells_p, size_t* width_p,
                                            size_t* height_p, snake_t* snake_p,
                                            char* compressed) {
    // TODO: implement!

    // set dimensions
    enum board_init_status set_dims_status = set_dimensions(width_p, height_p, compressed);
    if (set_dims_status != INIT_SUCCESS) { return set_dims_status; }

    // allocate memory for board
    *cells_p = malloc((*width_p) * (*height_p) * sizeof(int));
/*
    // gets position in cells array given the row and col
    size_t get_cell_pos(size_t row, size_t col) {
        if (row >= *(height_p)) {
            printf("Error: row can\'t be greater than dim");
            return 0;
        }
        if (col >= *(width_p)) {
            printf("Error: row can\'t be greater than dim");
            return 0;
        }

        return (*width_p) * row + col;
    }
*/
    int num_snakes = 0;

    // fill cells with the given flag
    enum board_init_status fill_cells(size_t start_ind, size_t num_cells, char flag) {
        for (size_t j = start_ind; j < start_ind + num_cells; j++) {
            if (j >= (*width_p)*(*height_p)) {
                printf("filled cells returned incorrect dimensions\n");
                return INIT_ERR_INCORRECT_DIMENSIONS;
            }

            switch (flag) {
                case 'S':
                    (*cells_p)[j] = FLAG_SNAKE;
                    num_snakes += 1;
                    break;
                case 'E':
                    (*cells_p)[j] = PLAIN_CELL;
                    break;
                case 'G':
                    (*cells_p)[j] = FLAG_GRASS;
                    break;
                case 'W':
                    (*cells_p)[j] = FLAG_WALL;
                    break;
                default:
                    printf("fill_cells got bad char %c\n", flag);
                    return INIT_ERR_BAD_CHAR;
            }

        }
        return INIT_SUCCESS;
    }

    // populate the board with the run-length encoding
    char* compression_str = compressed;
    strtok(compression_str, "|");

    size_t curr_cell = 0;
    size_t curr_row;

    for (curr_row = 0; ; curr_row++) {
        // get next (compressed) row
        char* row_token = strtok(NULL, "|");

        // break after the last row
        if (row_token == NULL) {
            if (curr_row != *height_p) { return INIT_ERR_INCORRECT_DIMENSIONS; }
            break;
        }
        
        size_t num_cells_in_row = 0;
        for (size_t ind = 0; row_token[ind] != '\0'; ) {

            char c = row_token[ind++];

            int count = 0;
            while (('0' <= row_token[ind]) && (row_token[ind] <= '9')) {
                count = 10 * count + (row_token[ind++] - '0');
            }

            enum board_init_status status = fill_cells(curr_cell, count, c);
            if (status != INIT_SUCCESS) {
                printf("fill_cells returned error %d\n", status);
                return status;
            }

            curr_cell += count;
            num_cells_in_row += count;
        }
        
        // check that row has correct number of cells encoded
        if (num_cells_in_row != (*width_p)) {
            printf("wrong number of cells in row error triggered\n");
            return INIT_ERR_INCORRECT_DIMENSIONS;
        }
    }

    if (num_snakes != 1) {
        return INIT_ERR_WRONG_SNAKE_NUM;
    }

    if (curr_row != *height_p) {
        return INIT_ERR_INCORRECT_DIMENSIONS;
    }

    return INIT_SUCCESS;
}
