// File:	mypthread_t.h

// List all group members' names: Blake Bodajlo, Dean Chumash
// iLab machine tested on: cd.cs.rutgers.edu

#ifndef MYTHREAD_T_H

#define MYTHREAD_T_H

#define _GNU_SOURCE

/* in order to use the built-in Linux pthread library as a control for benchmarking, you have to comment the USE_MYTHREAD macro */
#define USE_MYTHREAD 1

#define RR_ALGO 0
#define PSJF_ALGO 1


#define STACK_SIZE 8186
#define TRUE  1
#define FALSE 0

#define RUNNING 1
#define READY 0
#define queued_mutex 3

#define EMPTY -1
#define ERROR -10


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>

typedef uint mypthread_t;

struct mypthread_mutex_t;

// Feel free to add your own auxiliary data structures (linked list or queue etc...)
typedef struct llnode_tcb
{
	struct llnode_tcb* next;
	struct threadControlBlock* tcb;
} llnode_tcb_t;

/* mutex struct definition */
typedef struct mypthread_mutex_t
{

	// YOUR CODE HERE
		//da owner
		llnode_tcb_t* owner;
		//da flag bit
		atomic_flag flag;
	
} mypthread_mutex_t;

/* mutex struct definition */
typedef struct mutex_list
{
	mypthread_mutex_t* mutex;
	struct mutex_list* next;
	
} mutex_list_t;

typedef struct mutexQueue
{
	mypthread_mutex_t* mutex;
	struct llnode_tcb_t** threadPointer;
	struct mutexQueue* nextQueuedThread;
	struct mutexQueue* nextMutexQueue;

}mutexQueue_t;


	/* add important states in a thread control block */
typedef struct threadControlBlock
{
	// YOUR CODE HERE	
	
	// thread Id
	int threadId;
	
	// thread status
	//Possibly make a macro for different status states (running, queued, mutex....)
	int status;

	// thread context
	ucontext_t* threadContext;
	// thread stack
	void* stack;
	// thread priority
	int threadPriority;
	//Mutex
	mypthread_mutex_t* mutex;
	// And more ...

	//Pthread_Join waiting for this thread
	llnode_tcb_t* waitingToEnd;

	//Return value for join
	void** joinReturnLocation;

	//Quantum for running
	int numQuantum;


}tcb;

typedef struct exitValues
{
	// YOUR CODE HERE	
	
	//Exit value for thread
	void** joinReturnValue;

	//threadID
	int threadID;

	//Next
	struct exitValues* next;


}exitValues_t;

// Inserts a node at the end of the linked list
void llNodeInsert(llnode_tcb_t**, llnode_tcb_t*);

// Free TCB block params
// TODO: May need to alter later

/* Function Declarations: */
void freeTCBParams(llnode_tcb_t*);


llnode_tcb_t* llNodePopHead(llnode_tcb_t** );
void printHello();
static void schedule();
llnode_tcb_t* llNodeFindRunning(llnode_tcb_t*);
void setThreadToRunning(llnode_tcb_t *);
void setThreadToReady(llnode_tcb_t *);
llnode_tcb_t* llNodeFindThread(llnode_tcb_t* , llnode_tcb_t* );
llnode_tcb_t* llNodeFindWaitingForJoin(llnode_tcb_t* , llnode_tcb_t* );
llnode_tcb_t* llNodeFindOnThreadID(llnode_tcb_t* , mypthread_t);
void llNodeDelete(llnode_tcb_t**, llnode_tcb_t* );
void cleanMemory();
static void startCleaningSignalHandler();
void mutexListInsert(mutex_list_t**, mypthread_mutex_t* );
void llNodeInsertSortedByQuantum(llnode_tcb_t** , llnode_tcb_t* );
llnode_tcb_t* mqFindOnID(llnode_tcb_t* head, int id);
void printMutexQueue();
void llnodePrintThreadList();

//Getters

/*
	Returns the current thread id number
*/
int getCurrentThreadId();
void startiTimer(int);
void stopiTimer();
int isStackAllocation();
int isSchedulerContextMade();
void restInits();
/* create a new thread */

int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);


/* current thread voluntarily surrenders its remaining runtime for other threads to use */
int mypthread_yield();

/* terminate a thread */
void mypthread_exit(void *value_ptr);

/* wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr);

/* initialize a mutex */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire a mutex (lock) */
int mypthread_mutex_lock(mypthread_mutex_t *mutex);

/* release a mutex (unlock) */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex);

/* destroy a mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex);

#ifdef USE_MYTHREAD
#define pthread_t mypthread_t
#define pthread_mutex_t mypthread_mutex_t
#define pthread_create mypthread_create
#define pthread_exit mypthread_exit
#define pthread_join mypthread_join
#define pthread_mutex_init mypthread_mutex_init
#define pthread_mutex_lock mypthread_mutex_lock
#define pthread_mutex_unlock mypthread_mutex_unlock
#define pthread_mutex_destroy mypthread_mutex_destroy
#endif

#endif
