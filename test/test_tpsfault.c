#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>


void *latest_mmap_addr; // global variable to make the address returned by mmap accessible

void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
    latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
    return latest_mmap_addr;
}

void *my_thread()
{
    char *tps_addr;

    /* Create TPS */
    tps_create();

    /* Get TPS page address as allocated via mmap() */
    tps_addr = latest_mmap_addr;

    /* Cause an intentional TPS protection error */
    tps_addr[0] = '\0';

    return tps_addr;
}

int main(){
	printf("This program should cause a TPS protection error\n");

	pthread_t tid;

	tps_init(1);

	pthread_create(&tid, NULL, my_thread, NULL);
	pthread_join(tid, NULL);

	return 0;
}