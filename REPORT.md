# Report

## Semaphore Data Structure and Functions
All semaphores are represented by a struct with two element, a
queue named *wait* containing the threads waiting to be executed, 
and an integer named *count* representing the number of resources
available.


Initializing a semaphore with function *sem_create* result in
allocating memory for the semaphore structure, creating a queue
using *queue_create()* within the queue API, and setting
the integer value for the count.


Deallocating a semaphore with *sem_destroy* results in using
*queue_destroy* from the queue API on the semaphore struct's
queue, and then freeing the memory allocated for the
semaphore struct.


The function *sem_down* takes a resource from the semaphore
in the given parameter. It checks whether the semaphore is available.
If there is no resources available, meaning the *count* integer
of the given semaphore struct is equal to 0, it cause the caller thread 
to be blocked until the semaphore becomes available. Once resource is
available, removal of a resource means that the *count* integer
of the given semaphore struct is decremented by 1.


The function *sem_up* release a resource to the semaphore in the 
given parameter. It increment the *count* integer of the given 
semaphore and checks if the queue length of the *wait* queue is
0. If it is, then there are other threads waiting, and the first
(oldest) thread in the waiting list is unblocked.


The function *sem_getvalue* checks the semaphore's internal state, and
assign the given parameter pointer to the semaphore's *count* integer.


## Unprotected TPS
Each TPS is represented by a structure containing a pthread_t handle
to a thread *tid* and a pointer to a memory page. Each memory page is
represented as a structure containing a pointer to the *address* and
an integer representing the count.


A global queue is used to store all the TPS of threads. 


The function *tps_init* initializes the TPS queue using the *queue_create()*
API. 


The function *tps_create* uses the helper function *find_tps* to return
the TPS with the correct thread ID. With the correct thread ID, the function
can now check if the TPS already exists or not. A new TPS is made with
the helper function *make_new_tps* and this TPS's page element of its struct
is allocated with the helper function *make_new_page*. With those two, a new
TPS area is made and associated with the current thread and the newly
allocated TPS is enqueued within the global TPS list queue.


The function *tps_destroy* uses the helper function *find_tps* to return
the TPS with the correct thread ID, which is then deleted from the
global queue using *queue_delete* from the queue API. The deleted
TPS's memory page and struct is also freed.


The function *tps_read* once again uses the helper function *find_tps* to 
return the TPS with the correct thread ID. The function *mprotect* allows
the page to be readable and *memcpy* is used to copy the content into
the given buffer parameter. *mprotect* is again use to lock the page
and prevent readability after the byte of data from the current thread
TPS is read into the buffer.


The function *tps_write* uses the helper function *find_tps* to return
the TPS with the correct thread ID. Then, assuming its page's *ref_count*
is greater than 1, *mprotect* and *memcpy* is used to copy the content
of its page to a new page, with correct reference counter for each page.


## Testing
The testing of phrase 1 was done with *sem_count.c*, *sem_prime.c*,
and *sem_buffer.c*. The initial file tested synchronization, while
the other two tested application of the semaphore.


Most of the testing of phrase 2.1 was done using *mytps.c* which took
inspiration from the given *tps.c* file in the test directory. Asserts
were used to ensure that normal TPS functions like *tps_write* were
correctly working and returning the correct values. 



