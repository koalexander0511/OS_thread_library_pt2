#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <tps.h>
#include <sem.h>

#define MSG_SIZE 32

static char msg1[MSG_SIZE] = "Hello World!";
static char msg2[MSG_SIZE] = "It's a beutiful day!";

void * clone_test(void *args)
{
	char *buffer = malloc(MSG_SIZE);
	memset(buffer, 0, MSG_SIZE);

	pthread_t * main_tid = (pthread_t *)args;

	printf("tps_clone() unit test...");
	assert(tps_clone(*main_tid) == 0); //correctly cloned
	assert(tps_clone(*main_tid) == -1); //error: thread already has TPS

	//do the same read operations as the main thread, see if TPS is cloned
	assert(tps_read(0,MSG_SIZE,buffer) == 0);
	assert(memcmp(buffer,msg1,MSG_SIZE) == 0);
	assert(tps_read(MSG_SIZE,MSG_SIZE,buffer) == 0);
	assert(memcmp(buffer,msg2,MSG_SIZE) == 0);

	printf("ok!\n");

	tps_destroy();

	return NULL;	
}

//this function is called after the main thread deleted its TPS
//it will try to clone TPS from main thread but will not be successfull
void * clone_from_noexist_TPS(void *args)
{
	pthread_t * main_tid = (pthread_t *)args;

	printf("Trying to clone TPS from main thread...");
	assert(tps_clone(*main_tid) == -1);
	printf("not gonna happen!\n");

	return NULL;
}

int main(int argc, char **argv)
{

	char *buffer = malloc(MSG_SIZE);
	memset(buffer, 0, MSG_SIZE);

	printf("test read/write on nonexistent TPS...");
	assert(tps_write(0, MSG_SIZE, msg1) == -1); //error: no TPS for this thread
	assert(tps_read(0, MSG_SIZE, buffer) == -1); // same as above
	printf("correct error values!\n");

	printf("tps_init() unit test...");
	assert(tps_init(0) == 0); //correctly initialize
	assert(tps_init(0) == -1); //error: duplicate initialization
	printf("ok!\n");

	printf("tps_create() unit test...");
	assert(tps_create() == 0); //correct creation
	assert(tps_create() == -1); //error: duplicate TPS
	printf("ok!\n");

	printf("tps_write() unit test...");
	assert(tps_write(0, TPS_SIZE*2, msg1) == -1); //error: out of bounds
	assert(tps_write(0, MSG_SIZE, NULL) == -1); //error: buffer NULL
	assert(tps_write(0, MSG_SIZE, msg1) == 0); //correct writing operation
	assert(tps_write(MSG_SIZE, MSG_SIZE, msg2) == 0); //correct writing op
	printf("ok!\n");

	printf("tps_read() unit test...");
	assert(tps_read(0,TPS_SIZE*2,buffer) == -1); //error: out of bounds
	assert(tps_read(0,MSG_SIZE,NULL) == -1); //error: buffer NULL
	assert(tps_read(0,MSG_SIZE,buffer) == 0); //correct reading operation
	assert(memcmp(buffer,msg1,MSG_SIZE) == 0); //compare results

	//reading where msg2 should be stored
	//testing if read works with different offset
	assert(tps_read(MSG_SIZE,MSG_SIZE,buffer) == 0);
	assert(memcmp(buffer,msg2,MSG_SIZE) == 0);
	printf("ok!\n");

	pthread_t main_tid = pthread_self();
	pthread_t tid;
	pthread_create(&tid, NULL, clone_test, (void *)&main_tid);
	pthread_join(tid, NULL);

	printf("tps_destroy() unit test...");
	assert(tps_destroy() == 0); //correct deletion
	assert(tps_destroy() == -1); //no TPS to delete
	printf("ok!\n");

	pthread_create(&tid, NULL, clone_from_noexist_TPS, (void *)&main_tid);
	pthread_join(tid, NULL);

	printf("\nAll unit tests passed! Yay!\n");

	return 0;
}