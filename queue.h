struct queue {
    int *array;
    int max_size;
    int front;
    int back;
    int curr_size;
};

struct queue* new_queue(int size);

void free_queue(struct queue *q);

int size(struct queue *q);

int is_empty(struct queue *q);

int front(struct queue *q);

void enqueue(struct queue *q, int item);

void dequeue(struct queue *q);