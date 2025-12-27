#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stddef.h>

// struct for a node in a doubly linked list
typedef struct node {
    void* data;

    struct node* next;
    struct node* prev;
} node_t;

// function declarations
int ll_length(node_t* head_list);
void* ll_get_first(node_t* head_list);
void* ll_get_last(node_t* head_list);
void ll_insert_first(node_t** head_list, void* to_add);
void ll_insert_last(node_t** head_list, void* to_add);
void* ll_get(node_t* head_list, int index);
void* ll_remove_element(node_t** head_list, void* to_remove, size_t size);
int ll_remove_and_free_element(node_t** head_list, void* to_remove, size_t size);
void ll_reverse(node_t** head_list);
void* ll_remove_first(node_t** head_list);
void* ll_remove_last(node_t** head_list);

#endif
