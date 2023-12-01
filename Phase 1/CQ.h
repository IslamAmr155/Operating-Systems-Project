#include <stdlib.h>
#include <unistd.h>

struct Node
{
  struct pcb *data;
  struct Node *next, *prev;
};

struct circularQueue
{
  struct Node *front, *rear;
};

void insertNode(struct circularQueue *cq, struct pcb *data)
{
  struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
  newNode->data = data;

  if (cq->front == NULL)
  {
    cq->front = cq->rear = newNode;
    return;
  }

  newNode->next = cq->front;
  // newNode->prev = cq->rear;
  cq->rear->next = newNode;
  cq->rear = newNode;
}

struct Node *deleteNode(struct circularQueue *cq, struct Node *node)
{
  if (!node)
    return NULL;

  struct Node *ret = node->next;
  if (cq->front == cq->rear)
  {
    cq->front = NULL;
    cq->rear = NULL;
    return NULL;
  }
  else if (cq->front == node)
  {
    cq->front = cq->front->next;
    cq->rear->next = cq->front;
    cq->front->prev = cq->rear;
  }
  else if (cq->rear == node)
  {
    cq->rear = cq->rear->prev;
    cq->rear->next = cq->front;
    cq->front->prev = cq->rear;
  }
  else
  {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  free(node);
  return ret;
}