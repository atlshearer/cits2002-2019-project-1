#include "queue.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* CITS2002 Project 1 2019
   Name(s):             Alexander Shearer, Thomas Kinsella
   Student number(s):   22465777, 22177293
*/

struct queue* new_queue(int size) {
    struct queue *q = NULL;
    q = (struct queue*)malloc(sizeof(struct queue));

    if (q == NULL)
    {
        fprintf(stderr, "Queue ptr memeory allocation failed.");
        exit(EXIT_FAILURE);
    }
    

    q->array = (int*)malloc(size * sizeof(int));

    if (q->array == NULL)
    {
        fprintf(stderr, "Queue arry memeory allocation failed.");
        exit(EXIT_FAILURE);
    }

    q->max_size = size;
    q->front = 0;
    q->back = -1;
    q->curr_size = 0;

    return q;
}

void free_queue(struct queue *q) {
    free(q->array);
    free(q);
}

int size(struct queue *q) {
    return q->curr_size;
}

int is_empty(struct queue *q) {
    return size(q) == 0;
}

int front(struct queue *q) {
    if (is_empty(q))
    {
        fprintf(stderr, "Queue is empty, invalid operation\nTerminating program\n");
        exit(EXIT_FAILURE);
    }
    
    return q->array[q->front];
}

void enqueue(struct queue *q, int item) {
    if (size(q) == q->max_size)
    {
        fprintf(stderr, "Queue is full, unable to enqueue more items\nTerminating program\n");
        exit(EXIT_FAILURE);
    }
    
    q->back = (q->back + 1) % q->max_size;
    q->array[q->back] = item;
    q->curr_size++;
}

void dequeue(struct queue *q) {
    if (is_empty(q))
    {
        fprintf(stderr, "Queue is empty, invalid operation\nTerminating program\n");
        exit(EXIT_FAILURE);
    }
    
    q->front = (q->front + 1) % q->max_size;
    q->curr_size--;
}