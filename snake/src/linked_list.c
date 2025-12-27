#include "linked_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * In this file, you will find a partial implementation of common doubly linked
 * list functions
 *
 * Your first task is to debug some of the functions
 *
 * After you have found all of the bugs, you will be writing two doubly linked
 * list functions and testing them.
 *
 * See the comments for each function for a description of how it
 * should operate.
 */

/**
 * ll_length:  find and return the length of the list
 * Arguments:
 *  - head_list:  Pointer to the head of the list
 *
 * Returns:  size of the list
 */
int ll_length(node_t* head_list) {
    int len = 0;
    if (!head_list) {
        return len;
    }
    node_t* current = head_list;
    while (current) {
        len++;
        current = current->next;
    }
    return len;
}

/**
 * ll_get_first: Return the first element of a the list, given a
 * pointer to the head of the list. Return NULL for empty lists.
 */
void* ll_get_first(node_t* head_list) {
    
    if (head_list == NULL) {
        return NULL;
    }

    return head_list->data;
}

/**
 * ll_get_last: Return the last element of the list, given a pointer
 * to the head of the list
 */
void* ll_get_last(node_t* head_list) {
    if (!head_list) {
        return NULL;
    }
    node_t* curr = head_list;
    while (curr->next) {
        curr = curr->next;
    }
    return curr->data;
}

/**
 * ll_insert_first: Insert a new element at the beginning of the list
 *
 * Warning: the user of this function is responsible for making sure that
 * the data for the list element remains valid while it is in the
 * list.  If the data is freed or goes out of scope while the list
 * still refers to it, this will result in undefined behavior!
 *
 * Arguments:
 * - head_list: Pointer to a pointer that refers to the head node of
     the list
 * - to_add: void pointer representing data to be added
 *
 * Returns:  nothing
 */
void ll_insert_first(node_t** head_list, void* to_add) {
    if (head_list == NULL) {
        return;
    }
    
    node_t* new_list = malloc(sizeof(node_t));
    if (new_list == NULL) {
        return;
    }

    new_list->data = to_add;
    new_list->prev = NULL;
    new_list->next = *head_list;

    if (*head_list != NULL) {
        (*head_list)->prev = new_list;
    }

    *head_list = new_list;
}

/**
 * ll_insert_last: Insert a new element at the beginning of the list
 *
 * Warning: the user of this function is responsible for making sure that
 * the data for the list element remains valid while it is in the
 * list.  If the data is freed or goes out of scope while the list
 * still refers to it, this will result in undefined behavior!
 *
 * Arguments:
 * - head_list: Pointer to a pointer that refers to the head node of
     the list
 * - to_add: void pointer representing data to be added
 *
 * Returns:  nothing
 */
void ll_insert_last(node_t** head_list, void* to_add) {
    if (!to_add) {
        return;
    }
    node_t* new_element = (node_t*)malloc(sizeof(node_t));

    new_element->data = to_add;

    new_element->prev = NULL;
    new_element->next = NULL;

    if (!(*head_list)) {  // if the list is empty
        *head_list = new_element;
        return;
    }

    node_t* curr = *head_list;
    while (curr->next) {
        curr = curr->next;
    }

    curr->next = new_element;
    new_element->prev = curr;
}

/**
 * ll_get: gets an element from the linked list, given its index.  If
 * the index is negative or invalid (i.e., out ot bounds), this
 * function should return NULL.
 *
 * Arguments:
 * - head_list: pointer to the head of the list
 * - index:  Index to get
 *
 * Returns: pointer to element of the list given by index, NULL if
 * index is invalid
 */
void* ll_get(node_t* head_list, int index) {
    node_t *curr = head_list;
    if (index < 0) {
        return NULL;
    }
    while (curr) {
        if (index == 0) {
            return curr->data;
        }
        index -= 1;
        curr = curr->next;
    }
    return NULL;
}

/**
 * ll_remove_element: remove the first matching element from the list,
 * given the data to be removed.
 *
 * Arguments:
 * - head_list: Pointer to pointer referring to the head of the list
 * - to_remove: void pointer referencing data to be removed (NOTE:
 *   data may not exist in the list!)
 * - size: Size of the data pointed to by to_remove
 *
 * Returns: a pointer to the data if the element was found and removed
 * from the list, null on error (e.g., if the data was not found in
 * the list)
 */
void* ll_remove_element(node_t** head_list, void* to_remove, size_t size) {
    if (!(*head_list)) {
        return 0;  // element doesn't exist
    }

    node_t* curr = *head_list;

    while (curr) {
        if (!memcmp(curr->data, to_remove,
                    size)) {  // found the element to remove
            if (curr->next) {
                curr->next->prev = curr->prev;
            }
            if (curr == *head_list) {
                *head_list = curr->next;
            } else {
                curr->prev->next = curr->next;
            }
            void* data = curr->data;
	    free(curr);
	    return data;
        }
        curr = curr->next;
    }

    return NULL;
}


/*
 * reverse_helper: helper function for ll_reverse
 *
 * (Fun fact: this function does not start with ll_ or appear in the
 * header file: this makes it a "private" function, because it's only
 * usable within this file.)
 */
void reverse_helper(node_t** head_list) {
    node_t* curr = *head_list;
    node_t* placeholder = NULL;
    if (!curr) {
        return;
    }
    else if (curr->next) {
        while (curr) {
            if (!curr->next) {
                *head_list = curr;
            }
            curr->prev = curr->next;
            curr->next = placeholder;
            placeholder = curr;
            curr = curr->prev;
        }
    }
}

/**
 * ll_reverse:  reverse elements in a list
 *
 * Arguments:
 * - head_list: pointer to a pointer referring to the head of the list
 *
 * Returns:  nothing
 */
void ll_reverse(node_t** head_list) {
    if (head_list) {
        reverse_helper(head_list);
    }
}

/**
 * ll_remove_first: Remove the first element of the list, given a
 * pointer to the head of the list
 *
 * Returns: pointer to the element removed; NULL if the list is empty
 */
void* ll_remove_first(node_t** head_list) {
    if (!(*head_list)) {
        return NULL;
    }
    node_t* curr = *head_list;
    *(head_list) = (*head_list)->next;

    if (*head_list) {
        (*head_list)->prev = NULL;
    }

    void *data = curr->data;

    free(curr);

    return data;
}

/**
 * ll_remove_last: Remove the last element of the list, given a
 * pointer to the head of the list
 *
 * Returns: pointer to the element removed; NULL if the list is empty
 */
void* ll_remove_last(node_t** head_list) {
    if (head_list == NULL || *head_list == NULL) {
        return NULL;
    }

    node_t* curr = *head_list;

    if (curr->next == NULL) {
        void* data = curr->data;
        free(curr);
        *head_list = NULL;
        return data;
    }

    while (curr->next != NULL) {
        curr = curr->next;
    }

    // curr is now the last element in the list
    curr->prev->next = NULL;
    void* data = curr->data;
    free(curr);
    return data;
}

