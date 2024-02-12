    // File:	mypthread.c

// List all group members' names: Blake Bodajlo, Dean Chumash
// iLab machine tested on: cd.cs.rutgers.edu

/*
	1 -> scheduler context
	2 -> first thread
	3 -> Library
	4 -> Main context
*/

#include "mypthread.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <stdarg.h>
#include "umalloc.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE


#ifdef PSJF
	#define CHOSEN_ALGO PSJF_ALGO;
#else
	#define CHOSEN_ALGO RR_ALGO;
#endif

#define LIBRARYID -3
#define MAINID 3
#define SCHED_CONTEXT 1

int whatIsOurCurrentSchedulingAlgorithmInitiallyPlease = PSJF_ALGO; 

int schedulerInitialized = FALSE; // Used to see if p_threadcreate has been called yet
llnode_tcb_t* runQueueHead = NULL; 
int currentThreadId = 1;
atomic_flag schedulerHappeningQuestionMark = ATOMIC_FLAG_INIT; // Used to see if scheduler was in scheduler
atomic_flag areWeInTheLibraryQuestionMark = ATOMIC_FLAG_INIT; // Set to prevent scheduler from doing anything, unset tles the scheduler schetuler schedule
struct itimerval timer; //timer for scheduler
struct sigaction sa; //signal handler for timer
struct sigaction cleaningSA; //signal handler for timer
llnode_tcb_t* currentRunningThread;
mutex_list_t* listOfMutexes = NULL;
ucontext_t* scheduler;
ucontext_t* main_context;
int putMainInQUEUE = FALSE;
exitValues_t* exitValues = NULL;
int initialQuantum = 500;
int quantumIncrement = 100;
llnode_tcb_t* mutexQueueList = NULL;
int didweswitchcontextfrommainyet = FALSE;
int globalThreadIncrement = 1;
int inCreate = 0; //used to signify to malloc that we are in create
int inMutexInit = 0;
int inExit = 0;
int inLock = 0;
int inInsert = 0;
int allocatingStack = 0;
//Prototypes
static void startSignalHandler();
llnode_tcb_t* sched_RR();
void llnodePrintThreadList();
static exitValues_t* exitValueFindOnID(exitValues_t* , mypthread_t );
static void printExitValues();
llnode_tcb_t* sched_PSJF();
void printMutexQueue();

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg)
{
    // unlock all memory
   

    

   
	inCreate = 1;
	if(schedulerInitialized == FALSE)
	{	
		
		currentThreadId = SCHED_CONTEXT;
		debug("Allocating heap");
		allocatingStack = 1;
		stack_t* heap = malloc(STACK_SIZE);
		if (heap == NULL) {
		perror("Failed to allocate stack");
		return(EXIT_FAILURE);
		} 
		
		allocatingStack = 0;
		
		scheduler = malloc(sizeof(ucontext_t));
		debug("Pointer for ucontext in create %p\n", scheduler);
		debug("Malloce'd scheduler");
		loadLibraryPages(SCHED_CONTEXT);
		getcontext(scheduler);
		debug("got context");
		
		 scheduler->uc_stack.ss_sp = heap;
		scheduler->uc_stack.ss_size = STACK_SIZE;
		scheduler->uc_link = scheduler;
		makecontext(scheduler, (void(*)(void))schedule, 0, NULL); 
	
		
		schedulerInitialized = TRUE;
	}
	
	debug("After making scheduler context");
	//Need to lock all pages so the correct value can be given the threadID
	if(putMainInQUEUE == 0) //Has to be main calling create
	{
		printMetaData();
		lockAllButThread(MAINID);
		int temp = currentThreadId;
		currentThreadId = MAINID;
		*thread = temp + 1; //SHould always be equal to 2 if it gets to this point since SCHED_CONTEXT was set to 1
		currentThreadId = temp;
	}
	else
	{
		lockAllButThread(currentThreadId); //Current threadID should be tracked correctly
		debug("This is the thread number in check thread %d\n", globalThreadIncrement + 1);
		*thread = globalThreadIncrement + 1; //increment it
	}
	//Increment the threadID for the new thread being created

	
	unlockMemoryPages();

	globalThreadIncrement++; 
	
	currentThreadId = globalThreadIncrement;
	debug("Thread being created in create: %d\n", currentThreadId);

	
	//The current thread is the new thread being created 
	//*thread = currentThreadId;
	

	//Library Data
	currentThreadId = LIBRARYID;
	debug("making tcb for new thread %d, putting it in library: %d\n", globalThreadIncrement, LIBRARYID);
	tcb* newTCB = malloc(sizeof(tcb));	// Should be in library pages
	
	//Give thread a unique ID:
	currentThreadId = globalThreadIncrement;
	debug("Storing new thread id: %d in the tcb\n",currentThreadId);

	
	//Load the library page before altering this pointer/data
	loadLibraryPages(LIBRARYID);
	newTCB->threadId = currentThreadId; //In library pages

	
	
	
	
	
	

	currentThreadId = globalThreadIncrement;
	debug("Make heap for new thread: %d\n", currentThreadId);
	allocatingStack = 1;
	stack_t* heap = malloc(STACK_SIZE);
	if (heap == NULL) {
    perror("Failed to allocate stack");
    return(EXIT_FAILURE);
	}
	allocatingStack = 0;
	
	//Initialize ucontext by calling getcontext
	debug("Making ucontext for thread %d\n", currentThreadId);
	ucontext_t* newthread = malloc(sizeof(ucontext_t)); // Should be in the thread page 
	debug("Pointer for ucontext %p\n", newthread);
	debug("This is newthread char: %d\n", newthread);
	
	//Load the thread page since altering thread data
	
	loadLibraryPages(currentThreadId);
	getcontext(newthread);
	
	//Still have thread page loaded
	//loadLibraryPages(currentThreadId);
	newthread->uc_stack.ss_sp = heap; //in thread page
    newthread->uc_stack.ss_size = STACK_SIZE; //thread page
	newthread->uc_link = scheduler; //thread page

	// Make the context and give it the function and args
	makecontext(newthread, (void(*)(void))function, 1, arg); //altering thread page

	//Load library page since altering library data
	loadLibraryPages(LIBRARYID);

	
	newTCB->stack = heap; //all library data
	newTCB->threadContext = newthread; 
	newTCB->status = READY;
	newTCB->mutex = NULL;
	newTCB->waitingToEnd = NULL;
	newTCB->joinReturnLocation = NULL;
	newTCB->numQuantum = initialQuantum; // Seconds



	//Making the tcb node for the linked list
	currentThreadId = LIBRARYID;
	debug("Making newTCB Node for library %d\n", currentThreadId);
	llnode_tcb_t* newTCBNode = malloc(sizeof(llnode_tcb_t));
	newTCBNode->tcb = newTCB;
	newTCBNode->next = NULL;
	newTCBNode->tcb->status = READY;

	//Set back to the new thread number
	currentThreadId = globalThreadIncrement;

	//Putting thread onto the queue
	if(runQueueHead == NULL)
	{
		runQueueHead = newTCBNode;
		debug("This is run queue head thread id %d\n", runQueueHead->tcb->threadId);
	}
	else{
		if(whatIsOurCurrentSchedulingAlgorithmInitiallyPlease == RR_ALGO)
		{
			llNodeInsert(&runQueueHead, newTCBNode);
		}
		else
		{
			llNodeInsertSortedByQuantum(&runQueueHead, newTCBNode);
		}
		

		
	}


	//If this is first pthread_create, need to initialize the scheduler/timer
	if(putMainInQUEUE == FALSE){
		putMainInQUEUE = TRUE;
		//Set thread for malloc to main
		currentThreadId = MAINID;


		

		startSignalHandler();
		startCleaningSignalHandler();


		//Need to load main contexts' page
		//loadLibraryPages(MAINID);
		
		//Create a node for the main context
		debug("Making heap for main context: %d", currentThreadId);
		allocatingStack = 1;
		stack_t* heap2 = malloc(STACK_SIZE);
		if(heap2 == NULL)
		{
			perror("Main heap could not be allocated");
		}
		allocatingStack = 0;

		
		debug("Mallocing context for main thread: %d\n", currentThreadId);
		main_context = malloc(sizeof(ucontext_t));
		
		//Load library pages after making the new page for main
		
		loadLibraryPages(MAINID);
		
		main_context->uc_stack.ss_sp = heap2;
   	 	main_context->uc_stack.ss_size = STACK_SIZE;
		//NEED TO SET UC_LINK TO THE A CONTEXT
		main_context->uc_link = NULL;
		
		
		//startiTimer(initialQuantum);
		//Need to put main into the queue:
	
	
		

		currentThreadId = LIBRARYID;
		//Load library pages for library data
		loadLibraryPages(LIBRARYID);
		debug("Making main tcb node for library: %d", currentThreadId);
		llnode_tcb_t* mainNode = malloc(sizeof(llnode_tcb_t));
		debug("Making main tcb for library: %d", currentThreadId);
		tcb* mainTCB = malloc(sizeof(tcb));
		mainNode->tcb = mainTCB;
		mainNode->tcb->threadId = MAINID;
		mainNode->tcb->status = READY;
		mainNode->tcb->threadContext = main_context;
		mainNode->tcb->numQuantum = initialQuantum;
		mainNode->tcb->mutex = NULL;
		mainNode->tcb->waitingToEnd = NULL;
		
		
		debug("This is main node %d\n", mainNode->tcb->threadId);
		
		//setcontext(main_context);
		
		if(runQueueHead == NULL)
		{
			runQueueHead = mainNode;
			
		}
		else{
		//put main in front so it will the scheduler will put it to the back
			mainNode->next=runQueueHead;
			runQueueHead=mainNode;
		}
		
		//Still in main when we leave this create function
		//On first create() -> scheduler = 1; New thread = 2; Main = 3; when create() is run again, increment will be 4 to start
		globalThreadIncrement++; //increment this again to simulate main being created
		currentThreadId = MAINID;
		
		loadLibraryPages(MAINID);

		getcontext(main_context);

		
		startiTimer(initialQuantum);
		debug("leaving create with thread running of %d\n", currentThreadId);
		//currentThreadId = runQueueHead->tcb->threadId;
		//setcontext(&main_context);
	}

    

	inCreate = 0;
	debug("leaving create with thread running of %d\n", currentThreadId);
	debug("RunQueue's pointer location: %p\n", runQueueHead);
	//debug("Run queue head's tcb threadID: %d\n", runQueueHead->tcb->threadId);
	
	//printMetaData();
	loadLibraryPages(LIBRARYID);
	
	currentThreadId = runQueueHead->tcb->threadId;
	//debug("RunQueue's thread after create: %d\n", runQueueHead->tcb->threadId);
	// lock all but current page	
	debug("After current thread library");
	loadThreadStack(currentThreadId);
    lockAllButThread(currentThreadId);
	
	return 0;
};

/* current thread voluntarily surrenders its remaining runtime for other threads to use */
int mypthread_yield()
{
	// YOUR CODE HERE
		//Need to load the scheduler context in code in
		debug("In yield");
		unlockMemoryPages();
		
		
		//loadLibraryPages(LIBRARYID);
		
	// save context of this thread to its thread control block
		/* getcontext(scheduler);
		/* scheduler.uc_stack.ss_sp = malloc(8192);
		scheduler.uc_stack.ss_size = 8192;*/
		
		//scheduler->uc_link = scheduler;
		//debug("Pointer location for scheduler %p\n", scheduler);
		
		//makecontext(scheduler, (void(*)(void))schedule, 0, NULL); */
		/* char copyStack[STACK_SIZE];
		ucontext_t schedCopy;
		if (getcontext(&schedCopy) == -1)
		{
			perror("getcontext");
		}
		schedCopy.uc_stack.ss_sp = copyStack;
		schedCopy.uc_stack.ss_size = sizeof(STACK_SIZE);
		schedCopy.uc_link = scheduler; */
		//loadLibraryPages(SCHED_CONTEXT);
		getcontext(scheduler);
		makecontext(scheduler, (void(*)(void))schedule, 0, NULL);
		
		if(didweswitchcontextfrommainyet == FALSE)
		{
			debug("Switching from main");
			didweswitchcontextfrommainyet = TRUE;
			//Load main data so main is saved to library/correct context
			loadThreadStack(MAINID);

			swapcontext(main_context, scheduler);
		}
		else if(runQueueHead != NULL)
		{

			debug("Switching contexts");
			
			
			
			//Need to load library so we can get the pointer to the context that is running
			/* printMetaData();
			exit(1); */
			loadLibraryPages(LIBRARYID);
			
			
			debug("This is current running thread in sched %p\n", runQueueHead);
		debug("This is current running thread tcb in sched %p\n", runQueueHead->tcb);
		debug("This is current running thread id in sched %p\n", runQueueHead->tcb->threadId);
		debug("This is current running thread context in sched %p\n", runQueueHead->tcb->threadContext);
			debug("This is newthread char %d\n", runQueueHead->tcb->threadContext);
			

			//Get the pointer to the context that is running
			ucontext_t *threadToSwap = runQueueHead->tcb->threadContext;
			debug("This is thread to swap pointer %p\n", runQueueHead->tcb->threadContext);
			
			//Now load the running thread's pages
			int threadToLoad = runQueueHead->tcb->threadId;
			debug("This is thread id to load in yield %d\n", threadToLoad);
			loadThreadStack(threadToLoad);
			//printMetaData();
			//Now we can swap contexts
			//exit(1);
			swapcontext(threadToSwap, scheduler);
		}
		else
		{
			////////debug("setting context, should never do this");
			setcontext(main_context);
		}
		
	// switch from this thread's context to the scheduler's context

	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr)
{	
	debug("In exit");

	////debug("in exit with thread: %d", runQueueHead->tcb->threadId);
	// YOUR CODE HERE
	////////debug("in pthread exit");
	////////debug("value from thread: %d\n", (int)value_ptr);
	// preserve the return value pointer if not NULL
	// deallocate any dynamic memory allocated when starting this thread
	inExit = 1;
	currentThreadId = LIBRARYID;

	//Initialize the exitValues struct, need to keep track of every thread's exit status
	if(exitValues == NULL) //first exit call
	{
		debug("Exit values is null");
		
		
		
		exitValues = malloc(sizeof(exitValues_t));
		
		loadLibraryPages(LIBRARYID);

		exitValues->joinReturnValue = malloc(sizeof(value_ptr));
		*(exitValues->joinReturnValue) = value_ptr;
		debug("This is exit value %d for thread %d\n",value_ptr, runQueueHead->tcb->threadId);
		exitValues->threadID = runQueueHead->tcb->threadId;
		exitValues->next = NULL;
printExitValues();
		debug("Exit values is made");
		
	}
	else //insert it into the list
	{
		//Make pointer to head
		////////debug("THere was already an exit, putting this: %d into thread: %d\n",(int)value_ptr, runQueueHead->tcb->threadId);
		//unlockMemoryPages();
		loadLibraryPages(LIBRARYID);
		exitValues_t* ptr = exitValues;
		while(ptr->next != NULL) // Go to the end of the list
		{
			ptr = ptr->next;
		}
		exitValues_t* node = malloc(sizeof(exitValues_t));

		loadLibraryPages(LIBRARYID);

		node->next = NULL; // Make last node point to NULL
		node->threadID = runQueueHead->tcb->threadId; // give it the exiting thread's id #
		/* ////////debug("size: %d\n", sizeof(value_ptr));
		void *p = &value_ptr;
		node->joinReturnValue = malloc(sizeof(value_ptr));
		memcpy(node->joinReturnValue, p, sizeof(value_ptr)); */
		

		/* void *p = (void*)value_ptr;
		char value = (char)p;
		////////debug("This is the valumakee of p: %c\n", p); */
		////////debug("This is the value of p: %c\n", newVal);
		node->joinReturnValue = malloc(sizeof(value_ptr));

		loadLibraryPages(LIBRARYID);
		debug("This is exit value %d for thread %d\n",value_ptr, runQueueHead->tcb->threadId);
		*(node->joinReturnValue) = value_ptr;
		ptr->next = node;
		
		printExitValues();
		//ptr->joinReturnValue = value_ptr; //copy the svalue into the list
		
	}
		 
	
	
	//See if any threads were waiting on this one to end
	llnode_tcb_t* wasWaitingForThisThreadToEnd = llNodeFindWaitingForJoin(runQueueHead, runQueueHead);
	if(wasWaitingForThisThreadToEnd != NULL)
	{	
		////////debug("This was waiting for me to end %d:", wasWaitingForThisThreadToEnd->tcb->threadId);
		// A thread was waiting on this thread to end, and wants it's return value
		if(wasWaitingForThisThreadToEnd->tcb->joinReturnLocation != NULL)
		{
			//Give that exit value to the waitingThread's tcb;
			////debug("in exit, some one was waiting for me");
			////debug("This is the value ptr in exit %d \n", value_ptr);

			*(wasWaitingForThisThreadToEnd->tcb->joinReturnLocation) = value_ptr;
		}

		//That thread is no longer waiting for this thread to end
		wasWaitingForThisThreadToEnd->tcb->waitingToEnd = NULL;
	}
	//free the tcb
	//Take node out of the list
	//free the node
	//////////debug("in pthread exit");
	debug("Exiting with thread: %d\n", runQueueHead->tcb->threadId);
	debug("current id  thread: %d\n", currentThreadId);
	llNodeDelete(&runQueueHead, runQueueHead);
	//schedule();
	//setcontext(scheduler);
	////////debug("\n\n\n\n\nPRINTING EXIT VALUES NOW THANK YOU\n");
	
	inExit = 0;
	return;
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr)
{	
	unlockMemoryPages();
	loadLibraryPages(LIBRARYID);
	// YOUR CODE HERE
	debug("IN JOIN \n\n\n\n");
	//printMetaData();
	debug("This is current thread id %d\n",currentThreadId);
	debug("This thread given in join %d\n",thread);
	//Can't wait on multiple threads to end
	if(runQueueHead->tcb->waitingToEnd != NULL)
	{
		debug("Already waiting for a thread to end");
		return 0;
	}
	if(runQueueHead->tcb->threadId == thread)
	{
		debug("Can't wait for self");
		return 0;
	}
	//This thread was never created/existed
	if(thread > globalThreadIncrement)
	{
		debug("ErESRCH");
		return(ESRCH);
	}
	//Find thread based on it's threadID
	llnode_tcb_t* threadToWaitFor = llNodeFindOnThreadID(runQueueHead, thread);
	llnodePrintThreadList();
	debug("This is thread to wait for %p\n", threadToWaitFor);
	//Check if thread is still on the run queue (Not terminated)
	
	if(threadToWaitFor == NULL)
	{
		debug("join is null");
		// If that thread has already terminated, then mypthread_join() returns immediately
		////debug("returning immediately from join");
		//Need to give it the return value by finding the return value in the list based on past ID:
		//changeincaseofbreakage
		////debug("Priting exit values");
		//printExitValues();
		////////debug("This is the return value: %d\n", *(exitValueFindOnID(exitValues, thread)->joinReturnValue));
		////////debug("Thread ID: %d\n", runQueueHead->tcb->threadId);
		//debug("Exit Value: %d\n", *(exitValueFindOnID(exitValues, thread)->joinReturnValue)); 
		if(runQueueHead->tcb->joinReturnLocation == NULL && (exitValueFindOnID(exitValues, thread)->joinReturnValue) != NULL)
		{
			debug("is null");
		}
		else
		{
			debug("is not null");
			 //Need to split this up so we can write to the correct location/page
			char **addressToWriteTo = runQueueHead->tcb->joinReturnLocation; //Get the pointer to the pointer of the return location
			printExitValues();
			//Get the exit value from the ended thread
			void *value = *(exitValueFindOnID(exitValues, thread)->joinReturnValue);
			
			debug("This is value %d\n", value);
			//Load the thread's stack and lock all pages but for it again
			loadThreadStack(currentThreadId);
			lockAllButThread(currentThreadId);
			//Write to the address, should trigger the seg fault handler
			*addressToWriteTo = value; 
			
			//*runQueueHead->tcb->joinReturnLocation = *(exitValueFindOnID(exitValues, thread)->joinReturnValue);
		}

			loadThreadStack(currentThreadId);
			lockAllButThread(currentThreadId);
		return 0;
	}
	else
	{
		debug("join is not null");
		
		llnode_tcb_t* waitingToJoin = llNodeFindWaitingForJoin(runQueueHead, threadToWaitFor);
		// Need to make sure a thread isn't already waiting for the given thread
		if(waitingToJoin == NULL) // no thread is waiting for this one
		{
			debug("no thread is waiting for this one\n");
			// Let the running thread wait on this thread
			runQueueHead->tcb->waitingToEnd = threadToWaitFor;

			debug("Thread id that is waiting: %d fort thread: %d\n", runQueueHead->tcb->threadId, thread);

			//Need to store in runQueue TCB the location for return value
			runQueueHead->tcb->joinReturnLocation = value_ptr;
			
			//debug("This is total from main %d\n",	(*value_ptr));
			//NEED TO SCHEDULE?
			// NEED TO RETURN 0, BUT ALSO NEED TO STOP THIS THREAD'S EXECUTION?
			//schedule();
			debug("Thread %d is waiting for thread %d\n",	runQueueHead->tcb->threadId, threadToWaitFor->tcb->threadId);
			debug("Exit values:");
			printExitValues();
			//llnodePrintThreadList();
			mypthread_yield();
			//swapcontext(runQueueHead->tcb->threadContext,scheduler);
			return 0;
		}
		else if(waitingToJoin->tcb->waitingToEnd == runQueueHead) //Tried to wait on each other causing deadlock
		{
			debug("Er1");
			return(EDEADLK);
		}
		else // there is a thread waiting for this one already
		{
			debug("Er2");
			return(EINVAL);
		}
	}


	// deallocate any dynamic memory created by the joining thread
	debug("At bottom of join");
	//Have to free the thread's node here
	loadThreadStack(currentThreadId);
	lockAllButThread(currentThreadId);
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	_Bool prev = atomic_flag_test_and_set(&areWeInTheLibraryQuestionMark);
	inMutexInit = 1;
	currentThreadId = LIBRARYID;

	
	// YOUR CODE HERE
	////////debug("in mutex init");
	//mutex = (mypthread_mutex_t*)malloc(sizeof(mypthread_mutex_t));
	////////debug("mutex address: %p\n", mutex);
	if(mutex == NULL)
	{
		perror("Error making mutex");
		return 1;
	}
	//initialize data structures for this mutex
	// Initialize the atomic flag
    
	//mutex->flag = ATOMIC_FLAG_INIT;
	atomic_flag_clear(&(mutex->flag));
	mutex->owner = NULL;
	//mutex->flag = ATOMIC_FLAG_INIT;

	// If the mutex list hasn't been made yet
	debug("Before allocating space for list of mutexes");
	if(listOfMutexes == NULL)
	{
		////////debug("adding mutex to list");
		listOfMutexes = malloc(sizeof(mutex_list_t)); 

		loadLibraryPages(LIBRARYID); //load library pages so its written to the correct page

		listOfMutexes->next = NULL; 
		listOfMutexes->mutex = mutex;

		mutexQueueList = malloc(sizeof(llnode_tcb_t));
		
		loadLibraryPages(LIBRARYID);

		mutexQueueList->next = NULL;
		mutexQueueList->tcb = NULL;
		debug("This is list of mutexes pointer in init %p\n", listOfMutexes);
		
	}
	else // If there is an existing list made, insert the node into the list
	{

		loadLibraryPages(LIBRARYID);

		mutexListInsert(&listOfMutexes, mutex);

	}

	debug("After allocating space for list of mutexes");

	
	
	//May need to alter the currentThreadID Here
	inMutexInit = 0;
	atomic_flag_clear(&areWeInTheLibraryQuestionMark);
	debug("This is pointer for list of mutexes in init %p\n", listOfMutexes);
	if(runQueueHead == NULL)
	{
		currentThreadId = MAINID;
	}
	else
	{
		currentThreadId = runQueueHead->tcb->threadId; //Get the current running thread
		loadThreadStack(currentThreadId); //Load its stack
		lockAllButThread(currentThreadId); //Lock all pages but it's pages again
	}
	
	

	return 0;
};

/* aquire a mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex)
{		
		inLock = 1;
		stopiTimer();
		debug("In mutex lock");
		_Bool prev = atomic_flag_test_and_set(&areWeInTheLibraryQuestionMark);
		 ////////debug("in mutex lock, set flag to %d", prev);
		//stopiTimer();
		// YOUR CODE HERE
		//debug("in mutex lock");
		if(mutex == NULL)
		{
			//Didn't touch anything, don't need to load or set running thread
			inLock = 0;
			return 1;
		}

		//Set the thread to be library and load it's pages since need to make changes
		currentThreadId = LIBRARYID;
		loadLibraryPages(LIBRARYID);

		if(atomic_flag_test_and_set(&(mutex->flag)) == 0)
		{
			
			// Mutex given to owner and flag is set
			if(mutex->owner == NULL)
			{
				debug("owner is null");
				debug("I am the thread getting the mutex %d\n", runQueueHead->tcb->threadId);
				mutex->owner = runQueueHead;
				runQueueHead->tcb->mutex = mutex;
				atomic_flag_test_and_set(&(mutex->flag));
				debug("This thread has the mutex: %d\n", mutex->owner->tcb->threadId);
				_Bool prev = atomic_flag_test_and_set(&(mutex->flag));
				////////debug("This was the flag's value after setting: %d\n", prev);
				////////debug("This was the flag's value after setting: %d\n", prev);
				
			}
			else
			{
				debug("flag is not set, but owner is not null?");
				mutex->owner = runQueueHead;
			}
			//startiTimer();
			

			//Make this thread the head of the mutexQueue (should only happen on empty-ish queue)
			/* llnode_tcb_t* newQueueNode = malloc(sizeof(llnode_tcb_t));
			newQueueNode->tcb = malloc(sizeof(tcb));
			memcpy(newQueueNode->tcb, runQueueHead->tcb, sizeof(tcb));
			newQueueNode->next = NULL;
			//Put it in front
			newQueueNode->next = mutexQueueList;
			mutexQueueList= newQueueNode;
			printMutexQueue(); */
			currentThreadId = runQueueHead->tcb->threadId;
			loadThreadStack(currentThreadId);
			lockAllButThread(currentThreadId);
			inLock = 0;
			startiTimer(INITIAL_QUANTUM);
			atomic_flag_clear(&areWeInTheLibraryQuestionMark);
			return 0;
			
		}
		// if mutex was locked
		else
		{
			debug("mutex was locked");
			
			//Owner tried to lock own mutex
			if(mutex->owner == runQueueHead)
			{
				currentThreadId = runQueueHead->tcb->threadId;
				loadThreadStack(currentThreadId);
				lockAllButThread(currentThreadId);
				inLock = 0;
				atomic_flag_clear(&areWeInTheLibraryQuestionMark);
				return 1;
			}
			else //let the thread wait for it and add it to the queue
			{


				runQueueHead->tcb->mutex = mutex; //signify this thread is waiting for this mutex
				//debug("putting thread into the list");
				//Add this thread to the corresponding mutex queue list
				// Make a new node for the queue
				debug("10");
				if(mutexQueueList == NULL)
				{	
					debug("20");
					mutexQueueList = malloc(sizeof(llnode_tcb_t));
					mutexQueueList->tcb = malloc(sizeof(tcb));

					loadLibraryPages(LIBRARYID);

					mutexQueueList->next = NULL;
					
					//debug("This is the tcb address %p\n", runQueueHead->tcb);
					memcpy(mutexQueueList->tcb, runQueueHead->tcb, sizeof(tcb));
					//mutexQueueList->tcb = runQueueHead->tcb;
					//debug("This is the threadid %d\n", mutexQueueList->tcb->threadId);
				}
				else if(mutexQueueList->tcb == NULL) //changed this to an else if
				{
					debug("30");
					mutexQueueList->next = NULL;
					mutexQueueList->tcb = malloc(sizeof(tcb));

					loadLibraryPages(LIBRARYID);
					//debug("This is the tcb address %p\n", runQueueHead->tcb);
					memcpy(mutexQueueList->tcb, runQueueHead->tcb, sizeof(tcb));
					
					//mutexQueueList->tcb = runQueueHead->tcb;
					debug("This is the threadid %d\n", mutexQueueList->tcb->threadId);
					
					//printMutexQueue();
				}
				else
				{
					debug("40");
					llnode_tcb_t* newQueueNode = malloc(sizeof(llnode_tcb_t));
					newQueueNode->tcb = malloc(sizeof(tcb));

					loadLibraryPages(LIBRARYID);

					memcpy(newQueueNode->tcb, runQueueHead->tcb, sizeof(tcb));
					//newQueueNode->tcb = runQueueHead->tcb;
					newQueueNode->next = NULL;
					llNodeInsert(&mutexQueueList, newQueueNode);
					//debug("This is the threadid %d\n", newQueueNode->tcb->threadId);
					/* //debug("Head address %p\n", &mutexQueueList);
					//debug("Head %d\n", mutexQueueList->tcb->threadId);
					//debug("Next After head %d\n", mutexQueueList->next->tcb->threadId); */
				}
				
				atomic_flag_clear(&areWeInTheLibraryQuestionMark);
				//debug("This is a mutex Queue pointer %p\n", &mutexQueueList);
				//debug("\n\n\nPRINTING MUTEX QUUEE\n\n\n\n");
				//printMutexQueue();
				inLock = 0;

				//Don't need to load thread or lock pages since yield will handle it from here
				mypthread_yield();
			}

			
			
		}
		// use the built-in test-and-set atomic function to test the mutex
		// if the mutex is acquired successfully, return
		// if acquiring mutex fails, put the current thread on the blocked/waiting list and context switch to the scheduler thread
		loadLibraryPages(LIBRARYID);
		currentThreadId = runQueueHead->tcb->threadId;
		loadThreadStack(currentThreadId);
		lockAllButThread(currentThreadId);
		inLock = 0;
		startiTimer(INITIAL_QUANTUM);
		atomic_flag_clear(&areWeInTheLibraryQuestionMark);
		return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex)
{
	// YOUR CODE HERE	
		//Mutex doesn't exit
		debug("in mutex unlock");
		

		if(mutex == NULL)
		{
			perror("Tried unlocking null mutex");
			return 1;
		}
		if(atomic_flag_test_and_set(&mutex->flag) == 0) // If the flag wasn't set, do nothing
		{
			////////debug("no flag was set, was already 0");
			atomic_flag_clear(&mutex->flag);
			return 0;
		}
		else //The flag was set
		{
			debug("Here in unlock");
			//exit(1)
			loadLibraryPages(LIBRARYID);
			if(runQueueHead == mutex->owner) // If the running thread called this
			{	
				////////debug("unlocking mutex");	
				//_Bool prev = atomic_flag_test_and_set(&runQueueHead->tcb->mutex->flag);
				////////debug("This is the flag value in unlock %d\n\n", prev);
				atomic_flag_clear(&mutex->flag);											//and they are the owner
				mutex->owner = NULL;
				runQueueHead->tcb->mutex = NULL;

				if(runQueueHead == NULL)
				{
					return 0;
				}
				else
				{
					loadThreadStack(currentThreadId);
					lockAllButThread(currentThreadId);
					return 0;
				}
				
			}
			else
			{
				loadThreadStack(currentThreadId);
				lockAllButThread(currentThreadId);
				return 1;
			}
		}
	// update the mutex's metadata to indicate it is unlocked
	// put the thread at the front of this mutex's blocked/waiting queue in to the run queue
	loadThreadStack(currentThreadId);
	lockAllButThread(currentThreadId);
	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex)
{
	// YOUR CODE HERE
	if(atomic_flag_test_and_set(&mutex->flag) == 0)
	{

		return 0;
	}
	else
	{
		//Determine error codes

		return EBUSY;
	}



	
	// deallocate dynamic memory allocated during mypthread_mutex_init

	return 0;
};

/* scheduler */
static void schedule()
{
	// YOUR CODE HERE
	
	//Stop the timer in the scheduler
 	debug("I am doing something");
	
	//Load the library pages since we only need the library here
	loadLibraryPages(LIBRARYID); 
	//unlockMemoryPages(); Already done from yield

	//llnodePrintThreadList();
	//printMetaData();
	debug("This is current running thread in sched %p\n", runQueueHead);
	debug("This is current running thread tcb in sched %p\n", runQueueHead->tcb);
	debug("This is current running thread id in sched %p\n", runQueueHead->tcb->threadContext);
	

	if (atomic_flag_test_and_set(&areWeInTheLibraryQuestionMark) == 1) {
		////////debug("in the library");
		if(runQueueHead != NULL)
		{
			currentRunningThread = runQueueHead;
			//timer was stopped by yield, restart the timer
			atomic_flag_clear(&areWeInTheLibraryQuestionMark);
			startiTimer(initialQuantum);

			setcontext(runQueueHead->tcb->threadContext);
		}
		else{
			////////debug("No thread to run");
			return; //should just return
		}
		return;
	}
	atomic_flag_clear(&areWeInTheLibraryQuestionMark);
	// 
	////////debug("here in scheduler");
	_Bool prev = atomic_flag_test_and_set(&schedulerHappeningQuestionMark);
	//_Bool prev = 0;
	stopiTimer(); 
	////llnodePrintThreadList();
	
	
	//If the scheduler inturrupts itself
	if(prev == 1)
	{	
		
		debug("in test an set");
		if(runQueueHead != NULL)
		{
			
			currentRunningThread = runQueueHead;
			if(currentRunningThread == NULL)
			{
				if(whatIsOurCurrentSchedulingAlgorithmInitiallyPlease == RR_ALGO)
				{
					currentRunningThread = sched_RR();
				}
				else
				{
					currentRunningThread = sched_PSJF();
				}
				
				////llnodePrintThreadList();
				currentRunningThread->tcb->status = RUNNING;
				atomic_flag_clear(&schedulerHappeningQuestionMark);
				startiTimer(currentRunningThread->tcb->numQuantum);
				setcontext(currentRunningThread->tcb->threadContext);
			}
			atomic_flag_clear(&schedulerHappeningQuestionMark);
			startiTimer(initialQuantum);
			setcontext(currentRunningThread->tcb->threadContext);
		}
			
		else{
			////////debug("No thread to run");
		}
	}	
	else
	{
		debug("Making schudeler decision");
		debug("This is chosen algo: %d\n", whatIsOurCurrentSchedulingAlgorithmInitiallyPlease);
		//Use the RR algo
		if(whatIsOurCurrentSchedulingAlgorithmInitiallyPlease == RR_ALGO)
		{
			debug("In RR ALGO");
			//If there is a current running thread
			if(runQueueHead != NULL){
				
				// Get the threadToRun from sched algo
				////////debug("before threadToRun");
				llnode_tcb_t* scheduledThread = sched_RR();
				ucontext_t* threadToRun = scheduledThread->tcb->threadContext;
				////////debug("after threadToRun");
				// Get the current running thread
				// set that thread to ready
				//setThreadToReady(llNodeFindRunning(runQueueHead));
				//set thread to be run to RUNNING
				//setThreadToRunning(scheduledThread);
				scheduledThread->tcb->status = RUNNING;
				//currentRunningThread = llNodeFindRunning(runQueueHead);
				startiTimer(scheduledThread->tcb->numQuantum);
				// Want to use swap context to save the other context that was running
				atomic_flag_clear(&schedulerHappeningQuestionMark);
				////////debug("swapping context");
				////llnodePrintThreadList();
				setcontext(threadToRun);
			}
			// There is no running thread
			else{
				//don't know what to do here
				////////debug("no threads in the queue");
				startiTimer(initialQuantum);
				atomic_flag_clear(&schedulerHappeningQuestionMark);
				mypthread_yield();
				return;
				}
			}


		else if(whatIsOurCurrentSchedulingAlgorithmInitiallyPlease == PSJF_ALGO)
		{
			debug("In PSJF");
			//If there is a current running thread
			if(runQueueHead != NULL){
				
				// Get the threadToRun from sched algo
				debug("before threadToRun");
				llnode_tcb_t* scheduledThread = sched_PSJF();
				
				int threadToLoad =  scheduledThread->tcb->threadId;
				debug("This is thread to load: %d\n", threadToLoad);
				ucontext_t* threadToRun = scheduledThread->tcb->threadContext; //This will be a pointer location in the thread's page
				
				//ucontext_t* threadToRun = scheduledThread->tcb->threadContext;
				debug("after threadToRun");
				// Get the current running thread
				debug("This is current running thread in sched %p\n", scheduledThread);
				debug("This is current running thread tcb in sched %p\n", scheduledThread->tcb);
				debug("This is current running thread id in sched %p\n", scheduledThread->tcb->threadId);
				debug("This is current running thread context in sched %p\n", scheduledThread->tcb->threadContext);
				debug("This is thread context data %d\n", scheduledThread->tcb->threadContext);
				

				
				int quntumToRun = scheduledThread->tcb->numQuantum;
				
				debug("Timer started");
				// Want to use swap context to save the other context that was running
				atomic_flag_clear(&schedulerHappeningQuestionMark);
				debug("Flag cleared");
				////////debug("swapping context");
				////llnodePrintThreadList();
				//preemptiveThreadLoad(scheduledThread->tcb->threadId);
				//lockAllButThread(scheduledThread->tcb->threadId);
				
				//Now want to load the thread's pages
				debug("This is run queue head pionter in sched %p\n", runQueueHead);
				debug("This is current running thread in sched %d\n", runQueueHead->tcb->threadId);
				//debug("This is next running thread in sched %d\n", runQueueHead->next->tcb->threadId);
				debug("This is current running thread in sched %p\n", runQueueHead->tcb->threadContext);
				//debug("This is next running thread in sched %p\n", runQueueHead->next->tcb->threadContext); 
				//printMetaData();
				
				//printAllPageDebug();
				int threadIDToRUN = runQueueHead->tcb->threadId;
				currentThreadId = runQueueHead->tcb->threadId;
				debug("Before lock in shecudler");
				loadThreadStack(threadIDToRUN);
				
				//printMetaData();
				lockAllButThread(threadIDToRUN);
				
				//lockMemoryPages();
				//printMetaData();
				
				//printAllPageDebug();
				
				/* printMetaData();
				exit(1); */
				//Want to lock all pages but that thread's
				//lockAllButThread(threadToLoad);

				//loadLibraryPages(LIBRARYID);
				//*threadToRun; //Trigger the seg fault handler to load in the ucontext page
				debug("This is address of thread to run %p\n", threadToRun);
				//debug("This is address of thread to run data %d\n", *threadToRun);
				
				startiTimer(quntumToRun);
				debug("This is after timer set");
				
				setcontext(threadToRun);
				
			}
			// There is no running thread
			else{
				//don't know what to do here
				debug("no threads in the queue");
				startiTimer(initialQuantum);
				atomic_flag_clear(&schedulerHappeningQuestionMark);
				mypthread_yield();
				return;
				}
			}
							
		
			
	

	
	}
	// each time a timer signal occurs your library should switch in to this context
	
	// be sure to check the SCHED definition to determine which scheduling algorithm you should run
	//   i.e. RR, PSJF or MLFQ
	////////debug("Returning from scheudler");
	return;
}

/* Round Robin scheduling algorithm */
//Return: the current thread node in the list to be run
llnode_tcb_t* sched_RR()
{
	// YOUR CODE HERE

	// Your own implementation of RR
	//Want to find current running thread and take
	//If no thread is running, and there is currently a queue formed
	// OR If there is only one thread in the queue
	////////debug("Before find");
	//(llNodeFindRunning(runQueueHead) == NULL && runQueueHead != NULL) || (
	if(runQueueHead != NULL && runQueueHead->next == NULL)
	{
		////////debug("in 1");
		//If there is no current running thread, pick the head as the current running thread
		//This is an edge case of the scheduler first starting
		//Shouldn't need mutex checking
		//Just set head to running
		////////debug("Thread ID: %d\n", runQueueHead->tcb->threadId);
		return(runQueueHead);
	}
	//Need to implement mutex checking logic
	else if(runQueueHead != NULL)
	{
		////////debug("in 2");
		//Want to find the thread that should be run next and return that one
		//Without checking for mutexes, just return the head
		//popping the head off
		////////debug("before pop");
		////llnodePrintThreadList();
		llnode_tcb_t* pop = llNodePopHead(&runQueueHead);
		//makeing sure pop isn't null
		
		//inserting the head into the end of the queue

		llNodeInsert(&runQueueHead,pop);
		////debug("after insert");
		//llnodePrintThreadList();
		////////debug("Thread ID: %d\n", runQueueHead->tcb->threadId);
		//returning the head which is the next thread to run

		// Need to check mutex
		
		// Not waiting for a mutex AND not waiting for a thread to end
		if(runQueueHead->tcb->mutex == NULL && runQueueHead->tcb->waitingToEnd == NULL)
		{
			////debug("no mutex");
			return(runQueueHead);
		}
		else
			
		{
			
			////////debug("\n\n\nPRINTING MUTEX QUUEE FROM SCHEUDLSER\n\n\n\n");
			//////printMutexQueue();
			////////debug("There is a mutex/join for threadID: %d\n", runQueueHead->tcb->threadId);
			llnode_tcb_t* threadToBeRun = NULL;
			//Need to loop through until find one
			////debug("In mutex loop");
			////llnodePrintThreadList();
	////////debug("This is current thread %d\n", runQueueHead->tcb->threadId);
			 while(threadToBeRun == NULL)
			 {	
				////debug("In mutex loop");
				//llnodePrintThreadList();
				//printMutexQueue();
				////////debug("1");
				//sleep(2);
				////////debug("\n\n\nPrinting mutex Queue\n");
				//////printMutexQueue();
				// waiting for mutex or thread to end?
				// no: run this thread
				if(runQueueHead->tcb->mutex == NULL && runQueueHead->tcb->waitingToEnd == NULL)
				{
					////debug("2")
					return(runQueueHead);
				}
				// yes: is mutex locked AND NOT waiting for thread to end?
				else if(runQueueHead->tcb->waitingToEnd == NULL)
				{	

					//Check to see if mutexQueue lead is still in runQueue
					//////debug("here in unlock");
					/* llnode_tcb_t* deletedThread = mqFindOnID(&mutexQueueList, runQueueHead->tcb->threadId);
					if( deletedThread != NULL)
					{
						llNodeDelete(&mutexQueueList, deletedThread);
					} */
					////debug("Printing mutex queue list");
					//printMutexQueue();
					if(mutexQueueList != NULL && mutexQueueList->tcb != NULL)
					{
						llnode_tcb_t* deletedThread = mqFindOnID(mutexQueueList, runQueueHead->tcb->threadId);
						if( deletedThread != NULL)
						{
							// //debug("delete copy in RR (1)\n");
							llNodeDeleteCopy(&mutexQueueList, deletedThread);
						} 
					}
					

					
					////debug("3");
					//////////debug("waiting for thread with id: %d", runQueueHead->tcb->waitingToEnd->tcb->threadId);
					// Mutex was not free,
						//////////debug("You are at the front of the queue: %d\n", mqFindNext(mutexQueueList, runQueueHead->tcb->mutex) );
						//Need to determing if it was next in the queue
						////////debug("checking mutex queue in sched");
						//Mutex was free, need to check if they are next in line for it
						//////debug("\nThis is the mutex owner thread: %d\n", runQueueHead->tcb->mutex->owner->tcb->threadId);
						if(runQueueHead == runQueueHead->tcb->mutex->owner)
						{
							////debug("they are the owner");
							if(mutexQueueList != NULL && mutexQueueList->tcb != NULL)
							{
								llnode_tcb_t* deletedThread = mqFindOnID(mutexQueueList, runQueueHead->tcb->threadId);
								if( deletedThread != NULL)
								{
									llNodeDelete(&mutexQueueList, deletedThread);
								} 
							}
							return(runQueueHead);
						} 
						else if(mqFindNext(mutexQueueList, runQueueHead->tcb->mutex) == EMPTY) //There is not queue for this mutex, let thread have it
						{
							////debug("5");
							////////debug("Mutex Queue was empty, thread can have it");
							atomic_flag_clear(&runQueueHead->tcb->mutex->flag);
							////////debug("Clearing mutex owner");
							runQueueHead->tcb->mutex->owner = NULL;
							////////debug("Current thread %d\n", runQueueHead->tcb->threadId);
							mypthread_mutex_lock(runQueueHead->tcb->mutex);
							return(runQueueHead);
						}
						else if(runQueueHead->tcb->mutex->owner == NULL && mqFindNext(mutexQueueList, runQueueHead->tcb->mutex) == runQueueHead->tcb->threadId)
						{
							////debug("6");
							////////debug("you are in front of the queue, you get to run");

							//Need to take this thread out of the queue
							llnode_tcb_t* deleteFromQueue = mqFindOnID(mutexQueueList, runQueueHead->tcb->threadId);
			

							// //debug("delete copy in RR (2)\n");
							llNodeDeleteCopy(&mutexQueueList,deleteFromQueue);
							//Let it have the mutex
							atomic_flag_clear(&runQueueHead->tcb->mutex->flag);
							////////debug("Current thread %d\n", runQueueHead->tcb->threadId);
							mypthread_mutex_lock(runQueueHead->tcb->mutex);
							runQueueHead->tcb->numQuantum += quantumIncrement;
							return(runQueueHead);
						}
						else // There is a queue form, and they are in it somewhere
						{
							////debug("7");
							// Send to the back of that mutex's queue
							////////debug("There is a queue formed for this mutex: wait your turn");

	

							pop = llNodePopHead(&runQueueHead);
							pop->tcb->numQuantum += quantumIncrement;
							llNodeInsert(&runQueueHead,pop);
						}
						
					
					// Mutex was free: send to back
					
				}
				else // Didn't have a mutex, but is waiting for a thread
				{
					////debug("8");
					// Check if that thread is in the runQueue still:
					if(llNodeFindThread(runQueueHead, runQueueHead->tcb->waitingToEnd) == NULL)
					{
						runQueueHead->tcb->waitingToEnd = NULL;
						return(runQueueHead);
					}
					else //that thread is still in the queue:
					{
						////////debug("Waiting for this to end: %d", runQueueHead->tcb->waitingToEnd->tcb->threadId);
						pop = llNodePopHead(&runQueueHead);
						llNodeInsert(&runQueueHead,pop);
					}
				}
			 }
				
					
			
						// no: run this thread
							// give it the mutex
						// yes: 

		
			// Want to check if mutex is free and give it the mutex and return it
			
			// If mutex is taken then pop and re insert
		}
		////////debug("returning from RR");
		//////debug("returning from psjf");
		return(runQueueHead);
	}



	// (feel free to modify arguments and return types)
	
}

/* Preemptive PSJF (STCF) scheduling algorithm */
llnode_tcb_t* sched_PSJF()
{
	// YOUR CODE HERE

	// Your own implementation of RR
	//Want to find current running thread and take
	//If no thread is running, and there is currently a queue formed
	// OR If there is only one thread in the queue
	////////debug("Before find");
	//(llNodeFindRunning(runQueueHead) == NULL && runQueueHead != NULL) || (
	if(runQueueHead != NULL && runQueueHead->next == NULL)
	{
		//debug("in 1");
		//If there is no current running thread, pick the head as the current running thread
		//This is an edge case of the scheduler first starting
		//Shouldn't need mutex checking
		//Just set head to running
		//debug("Thread ID: %d\n", runQueueHead->tcb->threadId);
		runQueueHead->tcb->numQuantum += quantumIncrement;
		return(runQueueHead);
	}
	//Need to implement mutex checking logic
	else if(runQueueHead != NULL)
	{
		//debug("in 2");
		//Want to find the thread that should be run next and return that one
		//Without checking for mutexes, just return the head
		//popping the head off
		debug("before pop");
		debug("This is runQueueHead pointer %p\n", runQueueHead);
		debug("This is runQueueHead pointer threadid %d\n", runQueueHead->tcb->threadId);
		debug("This is runQueueHead pointer threadid %p\n", runQueueHead->tcb->threadContext);

		debug("This is runQueueHead pointer next %p\n", runQueueHead->next);
		debug("This is runQueueHead pointer next threadid %d\n", runQueueHead->next->tcb->threadId);
		debug("This is runQueueHead pointer threadid %p\n", runQueueHead->next->tcb->threadContext);

		llnodePrintThreadList();
		
		llnode_tcb_t* pop = llNodePopHead(&runQueueHead);
		
		//makeing sure pop isn't null
		pop->tcb->numQuantum += quantumIncrement;
		
		//inserting the head into the end of the queue

		llNodeInsertSortedByQuantum(&runQueueHead,pop);
		debug("after insert");
		debug("This is runQueueHead pointer %p\n", runQueueHead);
		debug("This is runQueueHead pointer threadid %d\n", runQueueHead->tcb->threadId);
		debug("This is runQueueHead pointer threadid %p\n", runQueueHead->tcb->threadContext);

		debug("This is runQueueHead pointer next %p\n", runQueueHead->next);
		debug("This is runQueueHead pointer next threadid %d\n", runQueueHead->next->tcb->threadId);
		debug("This is runQueueHead pointer threadid %p\n", runQueueHead->next->tcb->threadContext);

		
		llnodePrintThreadList();

		
		//debug("Thread ID: %d\n", runQueueHead->tcb->threadId);
		//returning the head which is the next thread to run

		// Need to check mutex
		
		// Not waiting for a mutex AND not waiting for a thread to end
		if(runQueueHead->tcb->mutex == NULL && runQueueHead->tcb->waitingToEnd == NULL)
		{
			////debug("no mutex");
			runQueueHead->tcb->numQuantum += quantumIncrement;
			return(runQueueHead);
		}
		else
			
		{
			
			/* //debug("\n\n\nPRINTING MUTEX QUUEE FROM SCHEUDLSER\n\n\n\n");
			printMutexQueue(); */
			//debug("There is a mutex/join for threadID: %d\n", runQueueHead->tcb->threadId);
			llnode_tcb_t* threadToBeRun = NULL;
			//Need to loop through until find one
	////////debug("This is current thread %d\n", runQueueHead->tcb->threadId);
			 while(threadToBeRun == NULL)
			 {	
				//debug("In mutex loop");
				//llnodePrintThreadList();
				//printMutexQueue();
				////////debug("1");
				//sleep(2);
				////////debug("\n\n\nPrinting mutex Queue\n");
				//////printMutexQueue();
				// waiting for mutex or thread to end?
				// no: run this thread
				if(runQueueHead->tcb->mutex == NULL && runQueueHead->tcb->waitingToEnd == NULL)
				{
					//debug("2");
					runQueueHead->tcb->numQuantum += quantumIncrement;
					return(runQueueHead);
				}
				// yes: is mutex locked AND NOT waiting for thread to end?
				else if(runQueueHead->tcb->waitingToEnd == NULL)
				{	

					//Check to see if mutexQueue lead is still in runQueue
					//////debug("here in unlock");
					/* llnode_tcb_t* deletedThread = mqFindOnID(&mutexQueueList, runQueueHead->tcb->threadId);
					if( deletedThread != NULL)
					{
						llNodeDelete(&mutexQueueList, deletedThread);
					} */
					////debug("Printing mutex queue list");
					//printMutexQueue();
					/* if(mutexQueueList != NULL && mutexQueueList->tcb != NULL)
					{
						llnode_tcb_t* deletedThread = mqFindOnID(mutexQueueList, runQueueHead->tcb->threadId);
						if( deletedThread != NULL)
						{
							// //debug("delete copy in PSJF (1)\n");
							llNodeDeleteCopy(&mutexQueueList, deletedThread);
						} 
					} */
					

					
					debug("3");
					//////////debug("waiting for thread with id: %d", runQueueHead->tcb->waitingToEnd->tcb->threadId);
					// Mutex was not free,
						//////////debug("You are at the front of the queue: %d\n", mqFindNext(mutexQueueList, runQueueHead->tcb->mutex) );
						//Need to determing if it was next in the queue
						////////debug("checking mutex queue in sched");
						//Mutex was free, need to check if they are next in line for it
						//////debug("\nThis is the mutex owner thread: %d\n", runQueueHead->tcb->mutex->owner->tcb->threadId);
						//Check to see if mutexQueue lead is still in runQueue
						if(runQueueHead == runQueueHead->tcb->mutex->owner)
						{
							debug("they are the owner");
							//debug("This is the thread %d\n", runQueueHead->tcb->threadId);
							//debug("This is the mutex owner %d\n", runQueueHead->tcb->mutex->owner->tcb->threadId);
							if(mutexQueueList != NULL && mutexQueueList->tcb != NULL)
							{
								llnode_tcb_t* deletedThread = mqFindOnID(mutexQueueList, runQueueHead->tcb->threadId);
								if( deletedThread != NULL)
								{
									llNodeDelete(&mutexQueueList, deletedThread);
								} 
								printMutexQueue();
							}
							return(runQueueHead);
						}
						else if(mqFindNext(mutexQueueList, runQueueHead->tcb->mutex) == EMPTY) //There is not queue for this mutex, let thread have it
						{
							debug("5 in sched");
							//debug("Mutex Queue was empty, thread can have it");
							atomic_flag_clear(&runQueueHead->tcb->mutex->flag);
							//debug("Clearing mutex owner");
							runQueueHead->tcb->mutex->owner = NULL;
							//debug("Current thread %d\n", runQueueHead->tcb->threadId);
							mypthread_mutex_lock(runQueueHead->tcb->mutex);

							loadLibraryPages(LIBRARYID);

							runQueueHead->tcb->numQuantum += quantumIncrement;
							return(runQueueHead);
						}
						//Mutex doesn't have an owner and they are next in queue
						else if(runQueueHead->tcb->mutex->owner == NULL && mqFindNext(mutexQueueList, runQueueHead->tcb->mutex) == runQueueHead->tcb->threadId)
						{
							debug("6");
							//debug("you are in front of the queue, you get to run");

							//Need to take this thread out of the queue
							llnode_tcb_t* deleteFromQueue = mqFindOnID(mutexQueueList, runQueueHead->tcb->threadId);
			

							//debug("delete copy in PSJF (2)\n");
							llNodeDeleteCopy(&mutexQueueList,deleteFromQueue);
							//Let it have the mutex
							atomic_flag_clear(&runQueueHead->tcb->mutex->flag);
							debug("Current thread %d\n", runQueueHead->tcb->threadId);

							mypthread_mutex_lock(runQueueHead->tcb->mutex);
							//Have library pages loaded
							loadLibraryPages(LIBRARYID);
							runQueueHead->tcb->numQuantum += quantumIncrement;
							return(runQueueHead);
						}
						else // There is a queue form, and they are in it somewhere
						{
							debug("7");
							// Send to the back of that mutex's queue
							//debug("There is a queue formed for this mutex: wait your turn");

	

							pop = llNodePopHead(&runQueueHead);
							pop->tcb->numQuantum += quantumIncrement;
							llNodeInsertSortedByQuantum(&runQueueHead,pop);
						}
						
					
					// Mutex was free: send to back
					
				}
				else // Didn't have a mutex, but is waiting for a thread
				{
					debug("8");
					// Check if that thread is in the runQueue still:
					if(llNodeFindThread(runQueueHead, runQueueHead->tcb->waitingToEnd) == NULL)
					{
						runQueueHead->tcb->waitingToEnd = NULL;
						runQueueHead->tcb->numQuantum += quantumIncrement;
						
						return(runQueueHead);
					}
					else //that thread is still in the queue:
					{
						//debug("Waiting for this to end: %d\n", runQueueHead->tcb->waitingToEnd->tcb->threadId);
						pop = llNodePopHead(&runQueueHead);
						pop->tcb->numQuantum += quantumIncrement;
						llNodeInsertSortedByQuantum(&runQueueHead,pop);
					}
				}
			 }
				
					
			
						// no: run this thread
							// give it the mutex
						// yes: 

		
			// Want to check if mutex is free and give it the mutex and return it
			
			// If mutex is taken then pop and re insert
		}
		////////debug("returning from RR");
		//////debug("returning from psjf");
		return(runQueueHead);
	}

	
}

/* Preemptive MLFQ scheduling algorithm */
/* Graduate Students Only */
static void sched_MLFQ() {
	// YOUR CODE HERE
	
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	return;
}

// Feel free to add any other functions you need

// YOUR CODE HERE
void llNodeInsert(llnode_tcb_t** head, llnode_tcb_t* node){
	//Make pointer to head
	llnode_tcb_t* ptr = *head;

	// If the list is empty, set first node to the head
	if(head == NULL){
		*head = node;
	}
	else{
		while(ptr->next != NULL) // Go to the end of the list
		{
			ptr = ptr->next;
		}
		ptr->next = node; // Insert the tcb at the end of the list
		ptr->next->next = NULL; // Make last node point to NULL
	}

}

void llNodeInsertSortedByQuantum(llnode_tcb_t** head, llnode_tcb_t* node) {
	llnode_tcb_t* ptr = *head;
	llnode_tcb_t* prev = NULL;

	//List is null

	//Thread is not waiting on anything to end and should be inserted normally

		// List is null
    // Thread is not waiting on anything to end and should be inserted normally
    if (ptr == NULL) {
        *head = node;
        node->next = NULL;
    }
    // If the current node quantum is less than the head
     else if (node->tcb->numQuantum <= (*head)->tcb->numQuantum) {
        node->next = (*head)->next;
        (*head)->next = node;
    } 
    // Insert into the middle or end of the list
    else {
        while (ptr != NULL && node->tcb->numQuantum >= ptr->tcb->numQuantum) {
            prev = ptr;
            ptr = ptr->next;
        }
        prev->next = node;
        node->next = ptr;
    }
}

//Pre change to enter the node into the second position
/* void llNodeInsertSortedByQuantum(llnode_tcb_t** head, llnode_tcb_t* node) {
	llnode_tcb_t* ptr = *head;
	llnode_tcb_t* prev = NULL;

	//List is null

	//Thread is not waiting on anything to end and should be inserted normally

		// List is null
    // Thread is not waiting on anything to end and should be inserted normally
    if (ptr == NULL) {
        *head = node;
        node->next = NULL;
    }
    // If the current node quantum is less than the head
    else if (node->tcb->numQuantum <= (*head)->tcb->numQuantum) {
        node->next = *head;
        *head = node;
    }
    // Insert into the middle or end of the list
    else {
        while (ptr != NULL && node->tcb->numQuantum >= ptr->tcb->numQuantum) {
            prev = ptr;
            ptr = ptr->next;
        }
        prev->next = node;
        node->next = ptr;
    }
} */



		
		
		
	


// Free TCB block params
// TODO: May need to alter later
void freeLLNode(llnode_tcb_t* node)
{
	// //debug("In freeLLNode\n");
	// //debug("Freeing threadcontext of threadID: %d, address %p\n", node->tcb->threadId, node->tcb->threadContext);
	// //debug("Freeing stack of threadID: %d, address %p\n", node->tcb->threadId, node->tcb->stack);
	// //debug("Freeing TCB of threadID: %d, address %p\n", node->tcb->threadId, node->tcb);
	// //debug("Freeing node, address %p\n", node);
	/* free(node->tcb->threadContext);
	free(node->tcb->stack);
	free(node->tcb);
	free(node);  */
}

void freeLLNodeCopy(llnode_tcb_t* node) {
	// //debug("In freeLLNodeCopy\n");
	// //debug("NOT Freeing threadcontext of threadID: %d, address %p\n", node->tcb->threadId, node->tcb->threadContext);
	// //debug("NOT Freeing stack of threadID: %d, address %p\n", node->tcb->threadId, node->tcb->stack);
	// //debug("Freeing TCB of threadID: %d, address %p\n", node->tcb->threadId, node->tcb);
	// //debug("Freeing node, address %p\n", node);
	/* free(node->tcb);
	free(node); */
}

// Returns the head, shifts the list down
llnode_tcb_t* llNodePopHead(llnode_tcb_t** head)
{

	if(*head == NULL)
	{
		return NULL;
	}

	llnode_tcb_t* ptr = *head;
	*head = (*head)->next;
	////////debug("Popping theadID: %d\n", ptr->tcb->threadId);
	return (ptr);
	//freeTCBParams(ptr);
	//free(ptr);
}


void llnodePrintThreadList()
{	
	
	debug("Printing Thread List:");
	if(runQueueHead == NULL)
	{
		debug("No threads");
	}
	else
	{
		loadLibraryPages(LIBRARYID);
		debug("This is runQueueHead pointer %p\n",runQueueHead);
		debug("This is runQueueHead id %d\n",runQueueHead->tcb->threadId);
		llnode_tcb_t* ptr = runQueueHead;
		while(ptr != NULL)
		{
	debug("|ID: %d, Status: %d, Qauntum: %d |", ptr->tcb->threadId, ptr->tcb->status, ptr->tcb->numQuantum);
			 if(ptr != NULL && ptr->tcb->mutex != NULL)
			{
				debug("<-has mutex/waiting");
				debug("This is pointer for the mutex: %p\n",listOfMutexes);
				 _Bool prev = atomic_flag_test_and_set(&(listOfMutexes->mutex->flag));
				if(prev = 0)
				{
					atomic_flag_clear(&(listOfMutexes->mutex->flag));
			debug(" <-Mutex Status--: %d\n", 0);
				}
				else
				{
			debug(" <-Mutex Status--: %d\n", 1);
				} 
				
			} 
			debug("\n");
			ptr = ptr->next;
		}
		debug("\n");
	}
	
}

//Find the current running thread via the TCB and running queue
llnode_tcb_t* llNodeFindRunning(llnode_tcb_t* head){
	llnode_tcb_t* ptr = head;
	// TODO MAYBE: Convert to bit operations
	if(head == NULL)
	{
		return NULL;
	}
	while(ptr->tcb->status != RUNNING)
	{	
		ptr = ptr->next;
		if(ptr == NULL)
		{
			break;
		}
	}
	return ptr;
}

//Used to see if findThis is still in the runQueue
llnode_tcb_t* llNodeFindThread(llnode_tcb_t* head, llnode_tcb_t* findThis){
	llnode_tcb_t* ptr = head;
	// TODO MAYBE: Convert to bit operations
	while(ptr != findThis)
	{	
		ptr = ptr->next;
		if(ptr == NULL)
		{
			break;
		}
	}
	return ptr;
}
//This function is used to find a thread in the list based on its ID
//Returns the thread on successful find
//Return NULL on can't find
llnode_tcb_t* llNodeFindOnThreadID(llnode_tcb_t* head, mypthread_t id){
	llnode_tcb_t* ptr = head;
	// TODO MAYBE: Convert to bit operations
	while(ptr->tcb->threadId != id)
	{	
		ptr = ptr->next;
		if(ptr == NULL)
		{
			break;
		}
	}
	return ptr;
}


//This function is used to see if any thread in the runqueue is waiting to join on the given thread
llnode_tcb_t* llNodeFindWaitingForJoin(llnode_tcb_t* head, llnode_tcb_t* waitingFor){
	llnode_tcb_t* ptr = head;
	// TODO MAYBE: Convert to bit operations
	if(ptr == NULL)
	{
		return NULL;
	}
	
	while(ptr->tcb->waitingToEnd != waitingFor)
	{	
		ptr = ptr->next;
		if(ptr == NULL)
		{
			break;
		}
	}
	return ptr;
}

//Delete node from thread queue and free its stuff
void llNodeDelete(llnode_tcb_t** head, llnode_tcb_t* node){
	//Make pointer to head
	////////debug("in delete");
	llnode_tcb_t* ptr = *head;
	////////debug("Deleting node with ID: %d\n", node->tcb->threadId);	//Node before node being removed
	//llnode_tcb_t* beforeNode = *head;
		////////debug("deleting node");
		if(node == ptr) //if the one wanting to delete is head
		{
			*head = (*head)->next;
		}
		else if(node->next == NULL) //if da one wanting to delete is tail
		{
			while(ptr->next != node) // Go to the end of the list
			{
			ptr = ptr->next;
			}
			ptr->next = NULL;
		}
		else
		{
			//Find the node before the current node
			while(ptr->next != node) // Go to the end of the list
			{
				ptr = ptr->next;
			}
			ptr->next = node->next;

			
		}

	freeLLNode(node);
}


//Delete node from thread queue and free its stuff WITHOUT freeing the context
void llNodeDeleteCopy(llnode_tcb_t** head, llnode_tcb_t* node){
	//Make pointer to head
	////////debug("in delete");
	llnode_tcb_t* ptr = *head;
	////////debug("Deleting node with ID: %d\n", node->tcb->threadId);	//Node before node being removed
	//llnode_tcb_t* beforeNode = *head;
		////////debug("deleting node");
		if(node == ptr) //if the one wanting to delete is head
		{
			*head = (*head)->next;
		}
		else if(node->next == NULL) //if da one wanting to delete is tail
		{
			while(ptr->next != node) // Go to the end of the list
			{
			ptr = ptr->next;
			}
			ptr->next = NULL;
		}
		else
		{
			//Find the node before the current node
			while(ptr->next != node) // Go to the end of the list
			{
				ptr = ptr->next;
			}
			ptr->next = node->next;

			
		}

	freeLLNodeCopy(node);
}

void freeMutexList()
{
	if(listOfMutexes != NULL)
	{
		

		while(listOfMutexes->next != NULL)
		{
			mutex_list_t* ptr = listOfMutexes;
			listOfMutexes = listOfMutexes->next;
			free(ptr);
		}
	}
}

//When the main program exits, we need to clean memory (thread still allocated but main finished...)
void cleanMemory()
{
	if(runQueueHead != NULL)
	{
		llnode_tcb_t* ptr = runQueueHead;
		while(ptr != NULL)
		{
			llnode_tcb_t* thisNode = ptr;
			ptr = ptr->next;
			freeLLNode(thisNode);
			
			
		}
	}

	if(listOfMutexes != NULL)
	{
		freeMutexList();
	}

	free(scheduler->uc_stack.ss_sp);

}


//Used to set the thread's state to running
void setThreadToRunning(llnode_tcb_t *thread)
{
	thread->tcb->status = RUNNING;
}
//Used to set the thread's state to Ready
void setThreadToReady(llnode_tcb_t *thread)
{
	thread->tcb->stack = READY;
}

// On first p_thread_create need to initialize the signal handler
static void startSignalHandler(){

    sa.sa_handler = mypthread_yield;
    sigaction(SIGALRM, &sa, NULL);

}

static void startCleaningSignalHandler(){

    cleaningSA.sa_handler = cleanMemory;
    sigaction(SIGTERM, &cleaningSA, NULL);

}

// Inserting a new mutex into the mutex list
// No regard to queue order since this is just for memory addresses to free later
void mutexListInsert(mutex_list_t** head, mypthread_mutex_t* addingMutex)
{
	inInsert = 1;
	currentThreadId = LIBRARYID;
	// Make a new node
	mutex_list_t* newMutexListNode = malloc(sizeof(mutex_list_t));

	//Add node to be the after the head of the list
	newMutexListNode->mutex = addingMutex;
	newMutexListNode->next = (*head);
	head = newMutexListNode;
	//(*head)->next = newMutexListNode;
	inInsert = 0;
}

static exitValues_t* exitValueFindOnID(exitValues_t* head, mypthread_t threadID)
{
	exitValues_t* ptr = head;
	if(ptr == NULL)
	{
		////////debug("No exit values ERROR");
		return NULL;
	}
	while(ptr != NULL && ptr->threadID != threadID)
	{
		ptr = ptr->next;
	}
	return ptr;
	
}

static void printExitValues()
{	
	////////debug("Printing Thread List:");
	if(runQueueHead == NULL)
	{
		debug("No exits values");
	}
	else
	{
		exitValues_t* ptr = exitValues;
		while(ptr != NULL)
		{
			debug("Thread: %d returns: %d, ", ptr->threadID,  *(ptr->joinReturnValue));
			ptr = ptr->next;
		}
		debug("\n");
	}
}



int mqFindNext(llnode_tcb_t* head, mypthread_mutex_t* mutex)
{
	if(head == NULL)
	{
		return EMPTY;
	}
	else{

		llnode_tcb_t* ptr = head;

		while(ptr != NULL)
		{
			
			if(ptr != NULL)
			{
				/* ////debug("thread mutex address %p\n", ptr->tcb->mutex);
				////debug("mutex param address %p\n", mutex); */
				if(ptr->tcb->mutex == mutex)
				{
				return ptr->tcb->threadId;
				}	
			}
			
			ptr = ptr->next;
		}

	}
	return ERROR;
}

llnode_tcb_t* mqFindOnID(llnode_tcb_t* head, int id)
{
	if(head == NULL)
	{
		//////debug("error");
	}
	else
	{
		

		llnode_tcb_t* ptr = head;
		

		while(ptr != NULL)
		{
			if(ptr->tcb == NULL)
		{
			//////debug("is null");
		}
		else
		{
	////////debug("sz of this thitng %d\n", sizeof(ptr->tcb));
	////////debug("This is the pointer tcb address %p\n", ptr->tcb);
			
		}
			if(ptr->tcb->threadId == id)
			{
				////debug("returning thread in findonID %d\n", ptr->tcb->threadId);
				return ptr;
			}
			ptr = ptr->next;
		}
	}
	return NULL;
}
void printMutexQueue()
{
	llnode_tcb_t* ptr = mutexQueueList;
	if(ptr != NULL)
	{
	if(ptr->tcb == NULL)
	{
		debug("Mutex queue is empty");
	}
	else
	{	
		debug("THREAD Mutex QUEUE");
		while(ptr != NULL && ptr->tcb != NULL)
		{	
			debug("|ID: %d |", ptr->tcb->threadId);
			ptr = ptr->next;
		}
	}
	}
	//debug("\n");
}

//Inserts into the thread list given that you give it the thread list (Always call with mutexQueue->threadQueue)



// On first p_thread_create need to initialize the iTimer (set to 3 seconds)
// Will need seperate function to possibly change timer for quantum value
/* static void startiTimer(){

	////////debug("starting timer");
	// Set the initial timer value to 1 millisecond
    timer.it_value.tv_sec = 0;      // 0 seconds
    timer.it_value.tv_usec = 50000;  // 1 millisecond

    // Set the interval to 1 millisecond to repeat the timer every 1 ms
    timer.it_interval.tv_sec = 0;      // 0 seconds
    timer.it_interval.tv_usec = 50000;  // 1 millisecond

    // Start the timer
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("Error setting timer");
	}

} */

void startiTimer(int Quantum){
    if (schedulerInitialized == FALSE) {
        return;
    }

	////////debug("starting timer");
	// Set the initial timer value to 1 millisecond
    timer.it_value.tv_sec = 0;      // 0 seconds
    timer.it_value.tv_usec = Quantum;  // 1 millisecond

    // Set the interval to 1 millisecond to repeat the timer every 1 ms
    timer.it_interval.tv_sec = 0;      // 0 seconds
    timer.it_interval.tv_usec = Quantum;  // 1 millisecond

    // Start the timer
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("Error setting timer");
	}

}
//Ability to stop the timer when entering the scheduler so it can finish its job
void stopiTimer(){
    if (schedulerInitialized == FALSE) {
        return;
    }

	// May want to set it to cycles and not seconds (?)
    /* timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0; */
	////////debug("stopping timer");
	// Set the initial timer value to 1 millisecond
    timer.it_value.tv_sec = 0;      // 0 seconds
    timer.it_value.tv_usec = 0;  // 1 millisecond

    // Set the interval to 1 millisecond to repeat the timer every 1 ms
    timer.it_interval.tv_sec = 0;      // 0 seconds
    timer.it_interval.tv_usec = 0;  // 1 millisecond

    setitimer(ITIMER_REAL, &timer, NULL);


}


int getCurrentThreadId(){
	
	
	debug("In getcurrentthreadid");
	

	//This indicates the threading library has yet to be called
	if(inCreate == 1 || inMutexInit == 1 || inExit == 1 || inLock == 1 || inInsert == 1) //use the logic from pthread_create to handle the initialization process of threadIDS
	{
		if(inCreate == 1)
		debug("gettting current running thread for malloc: %d\n", currentThreadId);
		return currentThreadId;
	}
	else if(runQueueHead == NULL) //Means no threads are running so use the main thread's space
	{
		return MAINID;
	}
	else
	{
		unlockMemoryPages();
		//Load library code
		debug("Getting the runqueue head id number");
	/* loadLibraryPages(LIBRARYID);

	debug("Getting the runqueue head id number");
	//Get the current running thread
	int runningThread = runQueueHead->tcb->threadId;
	debug("This is the running thread %d\n", runningThread); */
	return currentThreadId;
	}

	
}

int isStackAllocation()
{
	return allocatingStack;
}

int isSchedulerContextMade()
{
	return schedulerInitialized;
}

void restInits()
{
	schedulerInitialized = FALSE; // Used to see if p_threadcreate has been called yet
	putMainInQUEUE = FALSE;
	didweswitchcontextfrommainyet = FALSE;
	globalThreadIncrement = 1;
	currentThreadId = 1;
	scheduler = NULL;
	main_context = NULL;
	mutexQueueList = NULL;
}
