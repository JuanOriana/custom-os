// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <staticQueue.h>
#include <lib.h>

//https://www.tutorialspoint.com/data_structures_algorithms/queue_program_in_c.htm
void queuePeek(t_queue *queue, void *data)
{
      if (!(queueIsEmpty(queue)))
      {
            memcpy(data, (void *)((uint64_t)queue->queue + (queue->front * queue->dataSize)), queue->dataSize);
      }
}

void queueUpdateFirst(t_queue *queue, void *data)
{
      if (!(queueIsEmpty(queue)))
      {
            memcpy((void *)((uint64_t)queue->queue + (queue->front * queue->dataSize)), data, queue->dataSize);
      }
}

int queueIsEmpty(t_queue *queue)
{
      return queue->size == 0;
}

int queueIsFull(t_queue *queue)
{
      return queue->size == queue->dim;
}

int queueSize(t_queue *queue)
{
      return queue->size;
}

void queueInsert(t_queue *queue, void *data)
{
      if (!queueIsFull(queue))
      {
            if (queue->rear == queue->dim - 1)
            {
                  queue->rear = -1;
            }
            memcpy((void *)((uint64_t)queue->queue + ((++queue->rear) * queue->dataSize)), data, queue->dataSize);
            queue->size++;
      }
}

void queueRemoveData(t_queue *queue, void *data)
{
      if (queue->size != 0)
      {
            memcpy(data, (void *)((uint64_t)queue->queue + ((queue->front++) * queue->dataSize)), queue->dataSize);

            if (queue->front == queue->dim)
                  queue->front = 0;
            queue->size--;
      }
      else
            memset(data, 0, queue->dataSize);
}