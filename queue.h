struct queue {
    int *array;
    int max_size;
    int front;
    int back;
    int curr_size;
};

extern struct queue* new_queue(int size);

extern void free_queue(struct queue *q);

extern int size(struct queue *q);

extern int is_empty(struct queue *q);

extern int front(struct queue *q);

extern void enqueue(struct queue *q, int item);

extern void dequeue(struct queue *q);