#include "windows_queue.h"
#include <stdlib.h>
#include <string.h>

DataQueue* data_queue_create(size_t capacity) {
    DataQueue* queue = (DataQueue*)malloc(sizeof(DataQueue));
    if (!queue) return NULL;

    queue->data = (double*)malloc(sizeof(double) * capacity);
    if (!queue->data) {
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;
    queue->size = 0;
    queue->head = 0;
    queue->tail = 0;
    InitializeCriticalSection(&queue->lock);

    return queue;
}

void data_queue_destroy(DataQueue* queue) {
    if (!queue) return;

    DeleteCriticalSection(&queue->lock);
    free(queue->data);
    free(queue);
}

ErrorCode data_queue_put(DataQueue* queue, const double* data, size_t count) {
    if (!queue || !data || count == 0) return ERROR_INVALID_PARAM;
    
    EnterCriticalSection(&queue->lock);

    // Check if there's enough space
    if (queue->size + count > queue->capacity) {
        LeaveCriticalSection(&queue->lock);
        return ERROR_QUEUE_FULL;
    }

    // Copy data into queue
    for (size_t i = 0; i < count; i++) {
        queue->data[queue->tail] = data[i];
        queue->tail = (queue->tail + 1) % queue->capacity;
    }
    queue->size += count;

    LeaveCriticalSection(&queue->lock);
    return ERROR_NONE;
}

ErrorCode data_queue_get(DataQueue* queue, double* data, size_t* count) {
    if (!queue || !data || !count || *count == 0) return ERROR_INVALID_PARAM;

    EnterCriticalSection(&queue->lock);

    if (queue->size == 0) {
        LeaveCriticalSection(&queue->lock);
        *count = 0;
        return ERROR_QUEUE_EMPTY;
    }

    // Calculate how many items we can actually get
    size_t items_to_get = min(queue->size, *count);
    
    // Copy data from queue
    for (size_t i = 0; i < items_to_get; i++) {
        data[i] = queue->data[queue->head];
        queue->head = (queue->head + 1) % queue->capacity;
    }
    
    queue->size -= items_to_get;
    *count = items_to_get;

    LeaveCriticalSection(&queue->lock);
    return ERROR_NONE;
}

void data_queue_clear(DataQueue* queue) {
    if (!queue) return;

    EnterCriticalSection(&queue->lock);
    queue->size = 0;
    queue->head = 0;
    queue->tail = 0;
    LeaveCriticalSection(&queue->lock);
}

size_t data_queue_size(const DataQueue* queue) {
    if (!queue) return 0;

    EnterCriticalSection((CRITICAL_SECTION*)&queue->lock);
    size_t size = queue->size;
    LeaveCriticalSection((CRITICAL_SECTION*)&queue->lock);
    
    return size;
}