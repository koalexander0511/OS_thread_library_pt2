#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "queue.h"
#include "sem.h"
#include "thread.h"

//Semaphore structure
//has a queue of threads waiting to be executed
//and a count for the number of resources available
struct semaphore {
	queue_t wait;
	unsigned int count;
};

sem_t sem_create(size_t count)
{
	//initialize sem structure
	sem_t sem = malloc(sizeof(struct semaphore));
	sem->count = count;
	sem->wait = queue_create();

	return sem;
}

int sem_destroy(sem_t sem)
{
	//error if sem is NULL or still something in waiting queue
	if(sem == NULL || queue_length(sem->wait) != 0)
		return -1;

	//free memory allocated for the queue and semaphore struct
	queue_destroy(sem->wait);
	free(sem);

	return 0;
}

int sem_down(sem_t sem)
{
	if(sem == NULL)
		return -1;

	enter_critical_section();
	
	pthread_t *tid_self = malloc(sizeof(pthread_t));
	*tid_self = pthread_self();

	//while there are no resources available
	while (sem->count == 0)
	{
		//go back into the wait queue and go to sleep
		queue_enqueue(sem->wait,(void*)tid_self);
		thread_block();
	}

	//reaching here means there is a resource. take it
	sem->count -= 1;

	exit_critical_section();

	return 0;
}

int sem_up(sem_t sem)
{
	
	if(sem == NULL)
		return -1;

	enter_critical_section();

	pthread_t *tid_self = malloc(sizeof(pthread_t));
	*tid_self = pthread_self();

	pthread_t* tid;

	//give up resource
	sem->count += 1;

	//if there are other threads waiting
	if(queue_length(sem->wait) != 0)
	{
		//unblock the first thread in the waiting queue
		queue_dequeue(sem->wait, (void**)&tid);
		thread_unblock(*tid);
	}

	exit_critical_section();
	
	return 0;
}

int sem_getvalue(sem_t sem, int *sval)
{
	if(sem == NULL || sval == NULL)
		return -1;

	*sval = sem->count;

	return 0;
}

