# Project 3 Report
---
---

# 1. Semaphore API
## Introduction
Semaphores are a type of lock that allows threads to share a limited number of
resources.
It can be used for synchronization, or mutual exclusion.
For the implementation, a queue with thread IDs were used to keep track of
blocked threads, and a integer counter was used to indicate the current number
of available resources.
Also, all queue related operations were done between
`enter_critical_section()` and `exit_critical_section()` to ensure safety.

## Data structure
The semaphore data structre has two items: wait queue, and count.
The wait queue stores the thread ID of all the threads that are waiting to be
executed. The count shows the number of resources available.
```c
struct semaphore {
	queue_t wait;
	unsigned int count;
};
```

## Initialization
In `sem_create()`, the semaphore is created by allocation memory space for it.
The count is set by the user, and the wait queue is created as well.

## Taking a resource
The thread takes a resource by calling `sem_down()`.
The main part of the function is a while loop, where it checks if the number
of resources is equal to zero.
If it is zero, the runnig thread's ID gets pushed into the wait queue, and the
thread blocks itself.
When the thread gets woken up, it goes to the top of the loop and checks any
resources are available.
If there is a resource available(count > 0), the function decreases the number
of resources and exits.
```c
while (sem->count == 0){
		queue_enqueue(sem->wait,(void*)tid_self);
		thread_block();
	}
	sem->count -= 1;
```
## Giving up a resource
The thread gives up a resource by calling `sem_up()`
First, the function increases `sem->count` to indicate that it gave up its
resource.
Then, it checks if there are any threads waiting to be executed in the thread
queue.
If so, the function dequeues an item(= thread ID of the next therad in line)
from the wait queue.
Using the tid, it unblockes the next thread scheduled to run.

```c
	sem->count += 1;
	if(queue_length(sem->wait) != 0){
		queue_dequeue(sem->wait, (void**)&tid);
		thread_unblock(*tid);
	}
```

# 2. TPS API
## Intruduction 
Thread Protected Storage(TPS) allows individual threads to have its own memory
space, undisturbed by other threads.
The memory spaces were allocated using `mmap()`, and `mprotect()` was used to
change the memory's permissions in order to read and write into the memory
space when appropriate.

## Data Structures
The TPS object consists of two items: a page structure and thread ID.
The page structure stores the beginning of the TPS's memory address, and the
number of threads pointing to the memory address.

```c
struct page{
	void * addr;
	int ref_count;
};

struct tps{
	struct page * pg;
	pthread_t tid;
};
```
## Initialization
When the user calls `tps_init()`, the TPS is initialized.
If segv = 1, then it creates a signal handler that catches SIGBUS and 
SIGSEGV errors. Then a handler is called to determine if the error was
caused because of a TPS protection error or not.
Also, the queue that stores all of the TPS structures, `queue_t tps_list`
is initialized.

## Creating a TPS
A TPS for a thread can be created by allocating memory that is empty, or
cloning a page from an existing thread.

***Creating a new TPS***
Creating a new TPS is done by calling `tps_create()`
It calls two helper functions `make_new_tps()` and `make_new_page()`
The first helper function allocated memory space for the TPS structure,
assigns its own thread ID, and returns the created TPS;
The second helper function is where the tps memory allocation happens.
`make_new_page()` allocates space for a page structure first.
Then, using `mmap()`, it creates a memory page, and stores the memory address 
into the newly created page structure.
For `mmap()`, the parameter `PROT_NONE` is used to indicate that the memory 
space has no access rights when it is created. Also, the `MAP_PRIVATE` and 
`MAP_ANON` parameters were used to create a private, anonymous memory space.
Finally, the reference counter was set to 1, and the new page is returned.
The new TPS structure points to the new page structure, and the entire thing
is pushed into the `tps_list` queue.

***Cloning a TPS from an existing thread***
Cloning a TPS is done by calling `tps_clone()`.
The operation is quite simple; first, it finds the approptiate TPS structure
that is associated with the thread in which it wants to clone from.
This is achieved by another helper function, `find_tps()`. This function takes
a thread ID, and, using `queue_iterate()`, finds the TPS associated with that
thread ID.
Once `tps_clone()` finds the appropriate TPS, the page pointer of the newly
created TPS points to the page address of the TPS that was found.
The reference counter is increamented, and the new TPS is enqueued.

## Writing into a TPS
Writing into a TPS is done by `tps_write()`
If the TPS's page only had one thread pointing to it, the function simply
changes the page's permissions by calling `mprotect()` with `PROT_WRITE` as a 
parameter. Then `memcpy()` is used to copy the contents of the buffer into the
memory page, and `mprotect()` is used again to block any writing operations.
If the memory page associated with the current TPS has more than one thread 
pointing to it, directly writing on it would result in changing data for
other threads as well. To prevent this, this API uses a Copy-on-Write
technique.
When more than one reference count is detected for the current memory page,
first, a entirely new page is created by calilng `make_new_page()`. Then, 
using `memcpy()`, the contents of the memory page is copied into the new
memory page. The current TPS now points to this newly created memory page,
instead of the previous one. After this operation is done, regular writing
operations are conducted.

## Reading from a TPS
Reading from a TPS is quite simple; it changes the permissions by calling
`mprotect()` with paramter `PROT_READ`, and uses `memcpy()` to copy the
contents in the memory page into the buffer. Then, the memory page is locked
using `PROT_NONE` as a parameter.

## Destroying a TPS
To destroy a TPS, the TPS associated with the current thread is deleted from
`tps_list` queue. Then, if the page associated with the TPS is only used by
this thread, then it is deleted as well. If not, the reference counter for 
that page is decremented. Finally, the TPS structed is deleted.

# 3. Testing
## Semaphore API
All tests for the semaphore API was done by using the provided testers,
`sem_count.c`, `sem_buffer.c`, and `sem_prime.c`. No errors were detected.

## TPS API
In addition to the provided `tps.c` tester, two more testers, `mytps.c` and 
`test_tpsfault.c` were used for testing.
The `mytps.c` tester was used for unit testing. It tested if all of the
invalid operations listed in the API would actually cause an error. It also
tested the reading and writing capabilities for different parts of the memory
page. 
Finally, it tested if a TPS was able to successfully be cloned, and if the CoW
technique was properly implemented.
The `test_tpsfault.c` tester was used to test if the error handler was working
properly.