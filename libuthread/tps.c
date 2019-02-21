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

int tps_init(int segv)
{
	/* TODO: Phase 2 */

	tps_list = queue_create();
	return 0;
}

int find_item_iterator(void *data, void* arg)
{
	struct tps * curr_item = (struct tps *)data;
	pthread_t * target = (pthread_t *)arg;

	if(curr_item->tid == *target)
		return 1;
	else
		return 0;
}

struct tps * find_mytps()
{
	pthread_t mytid = pthread_self();
	struct tps * mytps = NULL;

	queue_iterate(tps_list, find_item_iterator, (void *)&mytid, (void**)&mytps);

	return mytps;
}

struct tps * make_new_tps()
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
	struct tps * mytps = find_mytps();
	
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
	struct tps * mytps = find_mytps();

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
	
	return 0;
}

int tps_read(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	return 0;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	return 0;
}

int tps_clone(pthread_t tid)
{
	struct tps * mytps = find_mytps();
	
	if(mytps != NULL){
		printf("TPS for thread %lu already exists at address %p!\n",pthread_self(),(void *)mytps->addr);
		return -1;
	}

	struct tps* new_tps = make_new_tps();
	if(new_tps == NULL)
		return -1;

//	memcpy();

	queue_enqueue(tps_list, new_tps);
	printf("Allocated TPS for thread %lu at address %p\n",pthread_self(), (void *)new_tps->addr);
	return 0;
}

