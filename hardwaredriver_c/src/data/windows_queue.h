#ifndef WINDOWS_QUEUE_H
#define WINDOWS_QUEUE_H

#include <stdio.h>
#include <windows.h>
#include "src/core/error_codes.h"

typedef struct {
    double* data;
    size_t capacity;
    size_t size;
    size_t head;
    size_t tail;
    CRITICAL_SECTION lock;
} DataQueue;

DataQueue* data_queue_create(size_t capacity);
void data_queue_destroy(DataQueue* queue);
ErrorCode data_queue_put(DataQueue* queue, const double* data, size_t count);
ErrorCode data_queue_get(DataQueue* queue, double* data, size_t* count);
void data_queue_clear(DataQueue* queue);
size_t data_queue_size(const DataQueue* queue);

#endif // WINDOWS_QUEUE_H