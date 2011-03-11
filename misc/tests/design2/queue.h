typedef struct
{
    int size;
    int capacity;
    int head;
    int tail;
    int messageSize;
    char * data;
} queue_t;

queue_t * queue_init(int capacity, int messageSize)
{
    queue_t * q = calloc(1, sizeof(queue_t));
    assert(q);
    q->capacity = capacity;
    q->messageSize = messageSize;
    q->data = (char *)calloc(capacity, messageSize);
    assert(q->data);
    return q;
}

int queue_size(queue_t * q)
{
    return q->size;
}

int queue_get(queue_t * q, void * data)
{
    if(q->size == 0)
    {
        return -1;
    }
    memcpy(data, q->data + q->tail * q->messageSize,
           q->messageSize);
    q->tail = (q->tail + 1) % q->capacity;
    q->size--;
    return 0;
}

void queue_put(queue_t * q, void * data)
{
    assert(q->size != q->capacity);
    memcpy(q->data + q->head * q->messageSize,
           data, q->messageSize);
    q->head = (q->head + 1) % q->capacity;
    q->size++;
}
