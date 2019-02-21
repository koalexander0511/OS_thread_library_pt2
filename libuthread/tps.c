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

struct page{
	void * addr;
	int ref_count;
};

struct tps{
	struct page * pg;
	pthread_t tid;
};

queue_t tps_list;

int find_tps_addr_iterator(void *data, void* arg)
{
	struct tps * curr_item = (struct tps *)data;

	return (curr_item->pg->addr == arg);
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
	queue_iterate(tps_list, find_tps_addr_iterator, (void *)p_fault, (void**)&t);

	fprintf(stderr, "The error occured at %p\n",p_fault);

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

static struct page * make_new_page()
{
	struct page * new_page = malloc(sizeof(struct page));
	new_page->addr = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, 0, 0);

	if(new_page->addr == (caddr_t)-1){
		free(new_page);
		printf("Failed to allocate page.\n");
		return NULL;
	}

	new_page->ref_count = 1;

	return new_page;

}

static struct tps * make_new_tps()
{
	struct tps * new_tps = malloc(sizeof(struct tps));
	
	new_tps->tid = pthread_self();
	new_tps->pg = NULL;

	return new_tps;
}

int tps_create(void)
{
	struct tps * mytps = find_tps(pthread_self());

	if(mytps != NULL){
		printf("TPS for thread %lu already exists at address %p!\n",pthread_self(),(void *)mytps->pg->addr);
		return -1;
	}

	struct tps* new_tps = make_new_tps();
	new_tps->pg = make_new_page();

	if(new_tps->pg == NULL)
		return -1;

	queue_enqueue(tps_list, new_tps);
	printf("Allocated TPS for thread %lu at address %p\n",pthread_self(), (void *)new_tps->pg->addr);
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

	if(munmap(mytps->pg->addr,TPS_SIZE) == -1)
	{
		printf("Failed to deallocate memory\n");
		return -1;
	}
	
	queue_delete(tps_list, mytps);
	free(mytps->pg);
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

	mprotect(mytps->pg->addr+offset, length, PROT_READ);
	memcpy((void*)buffer,mytps->pg->addr+offset,length);
	mprotect(mytps->pg->addr+offset, length, PROT_NONE);

	printf("Data read: %s",buffer);

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

	if(mytps->pg->ref_count > 1)
	{
		printf("CoW cloning initiated\n");
		struct page * new_page = make_new_page();
		struct page * source_page = mytps->pg;
		mytps->pg = new_page;

		mprotect(source_page->addr,TPS_SIZE,PROT_READ); //allow read permission for source tps
		mprotect(mytps->pg->addr,TPS_SIZE,PROT_WRITE); //allow write permission for source tps
	
		memcpy(mytps->pg->addr,source_page->addr,TPS_SIZE);
	
		//disable rw permission for source and new tps
		mprotect(source_page->addr,TPS_SIZE,PROT_NONE); 
		mprotect(mytps->pg->addr,TPS_SIZE,PROT_NONE);
		
		mytps->pg->ref_count = 1;
		source_page->ref_count--;

	}

	mprotect(mytps->pg->addr, TPS_SIZE, PROT_WRITE);
	memcpy(mytps->pg->addr+offset,(void*)buffer,length);
	mprotect(mytps->pg->addr, TPS_SIZE, PROT_NONE);


	printf("Data written: %s",buffer);
	return 0;
}

int tps_clone(pthread_t tid)
{
	struct tps * mytps = find_tps(pthread_self());
	if(mytps != NULL){
		printf("TPS for thread %lu already exists at address %p!\n",
			pthread_self(),(void *)mytps->pg->addr);
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

	new_tps->pg = source_tps->pg;
	new_tps->pg->ref_count++;

	queue_enqueue(tps_list, new_tps);
	printf("Cloned TPS for thread %lu to address %p\n",pthread_self(), (void *)new_tps->pg->addr);
	return 0;
}

