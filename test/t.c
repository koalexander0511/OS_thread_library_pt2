#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>


int main(int argc, char **argv)
{
	tps_init(1);
	tps_create();
	tps_create();
	return 0;
}
