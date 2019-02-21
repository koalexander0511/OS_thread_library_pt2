#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

//void * mmap (void *address, size_t length, int protect, int flags, int filedes, off_t offset)

/* TODO: Phase 2 */
struct tps{
	void * addr;
	pthread_t tid;
};

queue_t tps_list;

int find_tps_addr_iterator(void *data, void* arg)
{
	struct tps * curr_item = (struct tps *)data;

	return (curr_item->addr == arg);
}

static void segv_handler(int sig, siginfo_t *si, void *context)
{
    /*
     * Get the address corresponding to the beginning of the page where the
     * fault occurred
     */
    void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

    /*
     * Iterate through all the TPS areas and find if p_fault matches one of them
     */
    struct tps * t = NULL;
	queue_iterate(tps_list, find_tps_addr_iterator, (void *)&p_fault, (void**)&t);

    if (t != NULL)
        fprintf(stderr, "TPS protection error!\n");
    else
    	fprintf(stderr, "Regular segfault error!\n");

    /* In any case, restore the default signal handlers */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    /* And transmit the signal again in order to cause the program to crash */
    raise(sig);
}

int tps_init(int segv)
{
	if (segv) {
        struct sigaction sa;

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = segv_handler;
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGSEGV, &sa, NULL);
    }

	tps_list = queue_create();
	return 0;
}

static int find_tps_tid_iterator(void *data, void* arg)
{
	struct tps * curr_item = (struct tps *)data;
	pthread_t * target = (pthread_t *)arg;

	return (curr_item->tid == *target);
}

static struct tps * find_tps(pthread_t tid)
{
	struct tps * t = NULL;

	queue_iterate(tps_list, find_tps_tid_iterator, (void *)&tid, (void**)&t);

	return t;
}

static struct tps * make_new_tps()
{
	struct tps * new_tps = malloc(sizeof(struct tps));
	
	new_tps->addr = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, 0, 0);
	new_tps->tid = pthread_self();

	if(new_tps->addr == (caddr_t)-1){
		free(new_tps);
		printf("Failed to allocate TPS.\n");
		return NULL;
	}

	return new_tps;
}

int tps_create(void)
{
	struct tps * mytps = find_tps(pthread_self());
	
	if(mytps != NULL){
		printf("TPS for thread %lu already exists at address %p!\n",pthread_self(),(void *)mytps->addr);
		return -1;
	}

	struct tps* new_tps = make_new_tps();
	if(new_tps == NULL)
		return -1;

	queue_enqueue(tps_list, new_tps);
	printf("Allocated TPS for thread %lu at address %p\n",pthread_self(), (void *)new_tps->addr);
	return 0;
}

int tps_destroy(void)
{
	struct tps * mytps = find_tps(pthread_self());

	if(mytps == NULL)
	{
		printf("No TPS for this thread to destroy.\n");
		return -1;
	}

	if(munmap(mytps->addr,TPS_SIZE) == -1)
	{
		printf("Failed to deallocate memory\n");
		return -1;
	}
	
	queue_delete(tps_list, mytps);
	free(mytps);

	return 0;
}

int tps_read(size_t offset, size_t length, char *buffer)
{
	if( (offset+length > TPS_SIZE) || (buffer == NULL) )
		return -1;

	struct tps * mytps = find_tps(pthread_self());
	if(mytps == NULL)
	{
		printf("TPS for this thread does not exist!\n");
		return -1;
	}

	mprotect(mytps->addr+offset, length, PROT_READ);
	memcpy((void*)buffer,mytps->addr+offset,length);
	mprotect(mytps->addr+offset, length, PROT_NONE);

	return 0;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	if( (offset+length > TPS_SIZE) || (buffer == NULL) )
		return -1;

	struct tps * mytps = find_tps(pthread_self());
	if(mytps == NULL)
	{
		printf("TPS for this thread does not exist!\n");
		return -1;
	}

	mprotect(mytps->addr+offset, length, PROT_WRITE);
	memcpy(mytps->addr+offset,(void*)buffer,length);
	mprotect(mytps->addr+offset, length, PROT_NONE);

	return 0;
}

int tps_clone(pthread_t tid)
{
	struct tps * mytps = find_tps(pthread_self());
	if(mytps != NULL){
		printf("TPS for thread %lu already exists at address %p!\n",
			pthread_self(),(void *)mytps->addr);
		return -1;
	}

	struct tps* new_tps = make_new_tps();
	if(new_tps == NULL)
		return -1;

	struct tps* source_tps = find_tps(tid);
	if(source_tps == NULL){
		printf("TPS of source thread does not exist!\n");
		return -1;
	}

	mprotect(source_tps->addr,TPS_SIZE,PROT_READ); //allow read permission for source tps
	mprotect(new_tps->addr,TPS_SIZE,PROT_WRITE); //allow write permission for source tps
	
	memcpy(new_tps->addr,source_tps->addr,TPS_SIZE);
	
	//disable rw permission for source and new tps
	mprotect(source_tps->addr,TPS_SIZE,PROT_NONE); 
	mprotect(new_tps->addr,TPS_SIZE,PROT_NONE);  
	

	queue_enqueue(tps_list, new_tps);
	printf("Cloned TPS for thread %lu to address %p\n",pthread_self(), (void *)new_tps->addr);
	return 0;
}

