#include <stdio.h>

#include "dequeue.h"

void initialize(dequeue *P)
{
	P->rear=-1;
	P->front=-1;
}
 
int empty(dequeue *P)
{
	if(P->rear==-1)
		return(1);
	
	return(0);
}
 
int full(dequeue *P)
{
	if((P->rear+1)%P->max==P->front)
		return(1);
	
	return(0);
}
 
void enqueueR(dequeue *P,int x)
{
	if(empty(P))
	{
		P->rear=0;
		P->front=0;
		P->data[0]=x;
	}
	else
	{
		P->rear=(P->rear+1)%P->max;
		P->data[P->rear]=x;
	}
}
 
void enqueueF(dequeue *P,int x)
{
	if(empty(P))
	{
		P->rear=0;
		P->front=0;
		P->data[0]=x;
	}
	else
	{
		P->front=(P->front-1+P->max)%P->max;
		P->data[P->front]=x;
	}
}
 
int dequeueF(dequeue *P)
{
	int x;
	
	x=P->data[P->front];
	
	if(P->rear==P->front)	//delete the last element
		initialize(P);
	else
		P->front=(P->front+1)%P->max;
	
	return(x);
}
 
int dequeueR(dequeue *P)
{
	int x;
	
	x=P->data[P->rear];
	
	if(P->rear==P->front)
		initialize(P);
	else
		P->rear=(P->rear-1+P->max)%P->max;
		
	return(x);
}
 
void print(dequeue *P)
{
	if(empty(P))
	{
		printf("\nQueue is empty!!");
		return;
	}
	
	int i;
	i=P->front;
	
	while(i!=P->rear)
	{
		printf("\n%d",P->data[i]);
		i=(i+1)%P->max;
	}
	
	printf("\n%d\n",P->data[P->rear]);
}
