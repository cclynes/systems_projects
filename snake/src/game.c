#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <assert.h>

#include "linked_list.h"
#include "mbstrings.h"
#include "common.h"

/** Updates the game by a single step, and modifies the game information
 * accordingly. Arguments:
 *  - cells: a pointer to the first integer in an array of integers representing
 *    each board cell.
 *  - width: width of the board.
 *  - height: height of the board.
 *  - snake_p: pointer to your snake struct (not used until part 3!)
 *  - input: the next input.
 *  - growing: 0 if the snake does not grow on eating, 1 if it does.
 */
void update(int* cells, size_t width, size_t height, snake_t* snake_p,
            enum input_key input, int growing) {
    // `update` should update the board, your snake's data, and global
    // variables representing game information to reflect new state. If in the
    // updated position, the snake runs into a wall or itself, it will not move
    // and global variable g_game_over will be 1. Otherwise, it will be moved
    // to the new position. If the snake eats food, the game score (`g_score`)
    // increases by 1. This function assumes that the board is surrounded by
    // walls, so it does not handle the case where a snake runs off the board.

    // TODO: implement!
    
    enum snake_direction old_dir = snake_p->snake_dir;
    size_t snake_len = (size_t)ll_length(snake_p->snake_pos);

    // change snake direction according to input
    switch (input) {
        case INPUT_NONE:
            break;
        case INPUT_DOWN:
            if (!(snake_len > 1) || (old_dir != UP)) {
                snake_p->snake_dir = DOWN;
            }
            break;
        case INPUT_UP:
            if (!(snake_len > 1) || (old_dir != DOWN)) {
                snake_p->snake_dir = UP;
            }
            break;
        case INPUT_LEFT:
            if (!(snake_len > 1) || (old_dir != RIGHT)) {
                snake_p->snake_dir = LEFT;
            }
            break;
        case INPUT_RIGHT:
            if (!(snake_len > 1) || (old_dir != LEFT)) {
                snake_p->snake_dir = RIGHT;
            }
            break;
    }

    // find next square
    size_t head_val = *(size_t *) ll_get_first(snake_p->snake_pos);
    size_t *new_cell_p = malloc(sizeof(*new_cell_p));
    if (!new_cell_p) { abort(); }

    switch (snake_p->snake_dir) {
        case UP:
            *new_cell_p = head_val - width;
            break;
        case DOWN:
            *new_cell_p = head_val + width;
            break;
        case RIGHT:
            *new_cell_p = head_val + 1;
            break;
        case LEFT:
            *new_cell_p = head_val - 1;
            break;
    }

    // end game if next square is a wall or snake
    size_t *tail = (size_t *) ll_get_last(snake_p->snake_pos);
    if ((cells[*new_cell_p] & FLAG_WALL) || ((cells[*new_cell_p] & FLAG_SNAKE) && (*tail != *new_cell_p))) {

        free(new_cell_p);
        g_game_over = 1;
        return;
    }

    // lengthen snake at head
    ll_insert_first(&(snake_p->snake_pos), new_cell_p);

    // fill cell at head
    cells[*new_cell_p] |= FLAG_SNAKE;

    // increment score and replace food if next square is food
    if ((cells[*new_cell_p] & FLAG_FOOD)) {
        g_score += 1;
        cells[*new_cell_p] &= ~FLAG_FOOD;
        place_food(cells, width, height);
        
        // if snake isn't growing, we need to remove its tail
        if (!growing) {
            size_t *last_cell = (size_t *) ll_remove_last(&(snake_p->snake_pos));

            size_t *first_cell = (size_t *) ll_get_first(&*snake_p->snake_pos);
            if (*first_cell != *last_cell) {
                cells[*last_cell] &= ~FLAG_SNAKE; // only remove snake flag if head isn't on that tile
            }
            free(last_cell); // free memory storing cell pos
        }

    }

    // otherwise, remove snake's tail
    else {
        size_t *last_cell = (size_t *) ll_remove_last(&(snake_p->snake_pos));

        size_t *first_cell = (size_t *) ll_get_first(&*snake_p->snake_pos);
        if (*first_cell != *last_cell) {
            cells[*last_cell] &= ~FLAG_SNAKE; // only remove snake flag if head isn't on that tile
        }
        
        free(last_cell); // free memory storing cell pos
    }

    return;
}

/** Sets a random space on the given board to food.
 * Arguments:
 *  - cells: a pointer to the first integer in an array of integers representing
 *    each board cell.
 *  - width: the width of the board
 *  - height: the height of the board
 */
void place_food(int* cells, size_t width, size_t height) {
    /* DO NOT MODIFY THIS FUNCTION */
    size_t max = width * height;
    size_t attempts = 0;

    while(attempts < max) {
        unsigned int food_index = generate_index(width * height);
        // check that the cell is empty or only contains grass
        if ((*(cells + food_index) == PLAIN_CELL) ||
            (*(cells + food_index) == FLAG_GRASS)) {
            *(cells + food_index) |= FLAG_FOOD;
            return;
        }
        attempts++;
    }
    assert(0 && "ERROR:  No empty or grass-only cells to place food on board.  Are you sure the board has been initialized correctly?");
    /* DO NOT MODIFY THIS FUNCTION */
}

/** Prompts the user for their name and saves it in the given buffer.
 * Arguments:
 *  - `write_into`: a pointer to the buffer to be written into.
 */
void read_name(char* write_into) {
    while (1) {
        printf("Name > ");
        fflush(stdout);

        ssize_t n = read(0, write_into, 999);
        if (n <= 0) {
            fprintf(stderr, "Name Invalid: must be longer than 0 characters.");
            fflush(stdin);
            fflush(stderr);
            continue;
        };

        write_into[n] = '\0';
        if (n > 0 && write_into[n-1] == '\n') write_into[n-1] = '\0';

        if (write_into[0] != '\0') return;
    }
}

/** Cleans up on game over — should free any allocated memory so that the
 * LeakSanitizer doesn't complain.
 * Arguments:
 *  - cells: a pointer to the first integer in an array of integers representing
 *    each board cell.
 *  - snake_p: a pointer to your snake struct. (not needed until part 3)
 */
void teardown(int* cells, snake_t* snake_p) {  
    // TODO: Free the board
    free(cells);

    if (!snake_p) return;

    while (snake_p->snake_pos) {
        void *data = ll_remove_first(&snake_p->snake_pos);
        free(data);
    }
    
    // TODO: implement! (part 3A)
}
