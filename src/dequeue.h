#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dequeue
{
	int rear,front;
    int max;
    int data[];
} dequeue;

extern void initialize(dequeue *p);
extern int empty(dequeue *p);
extern int full(dequeue *p);
extern void enqueueR(dequeue *p,int x);
extern void enqueueF(dequeue *p,int x);
extern int dequeueF(dequeue *p);
extern int dequeueR(dequeue *p);
extern void print(dequeue *p);

#ifdef __cplusplus
}
#endif