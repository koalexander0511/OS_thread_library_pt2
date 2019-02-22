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

//Memory page structure
struct page{
	void * addr;
	int ref_count;
};

//TPS structure
struct tps{
	struct page * pg;
	pthread_t tid;
};

//queue structure to store all the TPSs of threads
queue_t tps_list;

//callback function that matches TPS by its page address. Used for queue_iterate()
static int find_tps_addr_iterator(void *data, void* arg)
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
	queue_iterate(tps_list, find_tps_addr_iterator, p_fault, (void**)&t);

    if (t != NULL)
        fprintf(stderr, "TPS protection error!\n");

    /* In any case, restore the default signal handlers */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    /* And transmit the signal again in order to cause the program to crash */
    raise(sig);
}

int tps_init(int segv)
{
	//error if tps_list already exists (tps_init was alrady called)
	if(tps_list != NULL) 
		return -1;

	//modifies segfault handler to distinguish between TPS protection error
	//and regular segmentation fault
	if (segv) {
        struct sigaction sa;

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = segv_handler;
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGSEGV, &sa, NULL);
    }

    //initialize the TPS queue
	tps_list = queue_create();
	
	//error if queue initialization failed
	if(tps_list == NULL)
		return -1;

	return 0;
}

//callback function that matches TPS by its tid. Used for queue_iterate()
static int find_tps_tid_iterator(void *data, void* arg)
{
	struct tps * curr_item = (struct tps *)data;
	pthread_t * target = (pthread_t *)arg;

	return (curr_item->tid == *target);
}

//finds the TPS with thread ID = tid in tps_list
//If found, returns the address of the TPS. Returns NULL otherwise
static struct tps * find_tps(pthread_t tid)
{
	struct tps * t = NULL;
	
	enter_critical_section();
	queue_iterate(tps_list, find_tps_tid_iterator, (void *)&tid, (void**)&t);
	exit_critical_section();

	return t;
}

//creates and returns a new page by reserving memory with no access rights
static struct page * make_new_page()
{
	struct page * new_page = malloc(sizeof(struct page));
	new_page->addr = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, 0, 0);

	if(new_page->addr == (caddr_t)-1)
	{
		free(new_page);
		return NULL;
	}

	new_page->ref_count = 1;

	return new_page;

}

//creates a new TPS structure with its own thread ID. Page obj. is NULL
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

	if(mytps != NULL) //error if TPS already exists
		return -1;

	struct tps* new_tps = make_new_tps();
	new_tps->pg = make_new_page();

	if(new_tps->pg == NULL) //erro`r if unable to allocate page
		return -1;
	enter_critical_section();
	queue_enqueue(tps_list, new_tps);
	exit_critical_section();

	return 0;
}

int tps_destroy(void)
{
	struct tps * mytps = find_tps(pthread_self());

	if(mytps == NULL) //error is TPS not found
		return -1;

	//error is unable to deallocate memory page
	if(munmap(mytps->pg->addr,TPS_SIZE) == -1)
		return -1;
	
	//delete TPS from queue
	enter_critical_section();
	queue_delete(tps_list, mytps);
	exit_critical_section();

	//free the memories of their shackles
	if(mytps->pg->ref_count > 1)
		mytps->pg->ref_count--;
	else
		free(mytps->pg);
	free(mytps);

	return 0;
}

int tps_read(size_t offset, size_t length, char *buffer)
{
	//error if going out of bounds, or buffer is NULL
	if( (offset+length > TPS_SIZE) || (buffer == NULL) )
		return -1;

	struct tps * mytps = find_tps(pthread_self());
	if(mytps == NULL) //error if TPS not found
		return -1;

	//make the page readable and put content into buffer
	mprotect(mytps->pg->addr, TPS_SIZE, PROT_READ);
	memcpy((void*)buffer,mytps->pg->addr+offset,length);

	//lock the page again
	mprotect(mytps->pg->addr, TPS_SIZE, PROT_NONE);

	return 0;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	//error if going out of bounds, or buffer is NULL
	if( (offset+length > TPS_SIZE) || (buffer == NULL) )
		return -1;

	struct tps * mytps = find_tps(pthread_self());
	if(mytps == NULL) //error if TPS not found
		return -1;

	//Copy-on-Write operation
	if(mytps->pg->ref_count > 1)
	{
		struct page * new_page = make_new_page();
		struct page * source_page = mytps->pg;
		mytps->pg = new_page;

		//copy the contents of the page to new page
		mprotect(source_page->addr,TPS_SIZE,PROT_READ); 
		mprotect(mytps->pg->addr,TPS_SIZE,PROT_WRITE);
		memcpy(mytps->pg->addr,source_page->addr,TPS_SIZE);
	
		//disable rw permission for source and new tps
		mprotect(source_page->addr,TPS_SIZE,PROT_NONE); 
		mprotect(mytps->pg->addr,TPS_SIZE,PROT_NONE);
		
		//set correct reference counters for each page
		mytps->pg->ref_count = 1;
		source_page->ref_count--;

	}

	//write the data into new page
	mprotect(mytps->pg->addr, TPS_SIZE, PROT_WRITE);
	memcpy(mytps->pg->addr+offset,(void*)buffer,length);
	mprotect(mytps->pg->addr, TPS_SIZE, PROT_NONE);


	return 0;
}

int tps_clone(pthread_t tid)
{
	struct tps * mytps = find_tps(pthread_self());
	if(mytps != NULL) //error if TPS already exists
		return -1;

	struct tps* new_tps = make_new_tps();
	if(new_tps == NULL) //error if failed to make new TPS
		return -1;

	struct tps* source_tps = find_tps(tid);
	if(source_tps == NULL) //error if target thread has no TPS
		return -1;

	//make the new TPS point to the target page; shallow copy
	new_tps->pg = source_tps->pg;
	new_tps->pg->ref_count++;

	enter_critical_section();
	queue_enqueue(tps_list, new_tps);
	exit_critical_section();

	return 0;
}

