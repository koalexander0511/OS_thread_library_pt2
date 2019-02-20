#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "queue.h"
#include "sem.h"
#include "thread.h"

struct semaphore {
	queue_t wait;
	unsigned int count;
};

sem_t sem_create(size_t count)
{
	sem_t sem = malloc(sizeof(struct semaphore));
	sem->count = count;
	sem->wait = queue_create();

	return sem;
}

int sem_destroy(sem_t sem)
{
	if(sem == NULL || queue_length(sem->wait) != 0)
		return -1;

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
	//printf("\nAcquiring semaphore!(%lu)\n",*tid_self%10);

	while (sem->count == 0)
	{
		
		//printf("Not available! Going to sleep!(%lu)\n",*tid_self%10);
		queue_enqueue(sem->wait,(void*)tid_self);
		thread_block();
	}

	sem->count -= 1;

	//printf("\nAcquired semaphore!yay!(%lu)\n",*tid_self%10);
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
	//printf("\nReleasing semaphore!(%lu)\n",*tid_self%10);

	pthread_t* tid;

	sem->count += 1;

	if(queue_length(sem->wait) != 0)
	{
		queue_dequeue(sem->wait, (void**)&tid);
		//printf("Unblock thread %lu!(%lu)\n",*tid%10,*tid_self%10);
		thread_unblock(*tid);
	}

	//printf("Released semaphore!(%lu)\n",*tid_self%10);
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

