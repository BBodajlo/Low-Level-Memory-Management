#include <unistd.h> // for size_t
#include "mypthread.h"

/* Debug */
//#define DEBUG 
#include <stdarg.h>
#ifndef UMALLOC_DEBUG_SETUP
#define UMALLOC_DEBUG_SETUP
void debug(char *s, ...);
#define DEBUG_FREE_FAIL 1
#define DEBUG_DOUBLE_FREE 2
int umalloc_set_debug(uint*);
int umalloc_set_debug_flag(uint);
int umalloc_stop_debug();
extern uint *umalloc_debug_flags;
#endif



/* Constants */
//#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#define PAGE_SIZE 4096
#define HEAP_SIZE 8388608
#define SWAP_SIZE 16777216
#define TOTAL_PAGES 2048 // Technically 2048, but changed this to 2041 since 2041 and past is reserved
#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define PAGE_HEADER_SIZE sizeof(page_header_t)
#define ACTIVATION_RECORD_SIZE 1590
#define INITIAL_THREAD_PAGES 3


#define THREAD 1

/* Structs */
/* Struct for a page header, stored in the reserved memory area
 * Note: This is not to be allocated, instead cast a pointer to
 * this struct.
 * As it is now, the reserved memory area will take 4 pages.
 */
typedef struct page_header {
    short threadID; // Thread ID of the thread that allocated this page
    short pageNum;  // Page number of this page in the thread's pages
    short usage;    // Number of bytes used in this page
    ushort loc;      // Index of the page in the page heap
    ushort spanOfPages;    // Number of pages after it uses (If span = 3 and pageNum = 3; This will use pages 3, 4, 5 for its malloc)
} page_header_t;

typedef struct header{ //Has size 6
    char isFree;
    unsigned short int sizeOfChunk;
    short int prev; //The amount of bytes to get to the last header
}header_t;
#define HEADER_SIZE sizeof(header_t)


#define PAGE_HEADER_SIZE sizeof(page_header_t)
#define RESERVED_PAGES 6
#define SCHEDULER_PAGE_START 2041 //Scheduler will take up Pages 2041, 2042, and 2043
#define SCHED_CONTEXT 1
// For malloc/free, whether it's a library call or not
#define THREADREQ 0
#define LIBRARYREQ 1

#define INITIAL_QUANTUM 500
#define SHARED_PAGES 4
#define SHALLOC_START_PAGE 2044
#define SHALLOC_THREAD_NUM -5
#define SWAP_META_PAGE 2048
#define SWAP_PAGES 1
#define SWAP_THREAD_ID -10
#define SWAP_PAGE_SIZE (PAGE_SIZE + sizeof(page_header_t))
#define SWAP_PAGE_MAX (SWAP_SIZE / (PAGE_SIZE + sizeof(page_header_t)))
#define SCHEDULER_PAGES 3
#define EVICT_START_PAGE 50
#define SWAP_PAGE_INDEX 5

#ifdef LIBRARY_MALLOC
#define malloc(x) mymalloc(x, __FILE__, __LINE__, LIBRARYREQ)
#define free(x) myfree(x, __FILE__, __LINE__, LIBRARYREQ)
#else
#define malloc(x) mymalloc(x, __FILE__, __LINE__, THREADREQ)
#define free(x) myfree(x, __FILE__, __LINE__, THREADREQ)
#endif
void *mymalloc(size_t size, char *file, int line, int lib);
void myfree(void *pointer, char *file, int line, int lib);
page_header_t* findMetaDataForPageIndex(int);
void printMetaData();
char* findPageForThread(int);
void printPageDebug(char*);
char* findFreePageUsingMetaData();
char* findPageForThreadGivenPageNum(int, int);
static void PageFaultHandlerFunction(int sig, siginfo_t *, void *);
static void StartPageFaultHandler();
page_header_t* findMetaDataForThreadPageIndex(int);
void swapPages(int , char* , page_header_t *);
void lockMemoryPages();
void unlockMemoryPages();
void unlockMemoryPagesForThread(int);
void loadLibraryPages(int);
void preemptiveThreadLoad(int);
void lockAllButThread(int);
page_header_t* findMetaDataGivenPageNum(int , int );
void printAllPageDebug();
void loadThreadStack(int );
void *shalloc(size_t);
char* makeShallocPages(int);
void* swapFindOnPageNum(int , int , int );
void putPageAndMetaInMemory(page_header_t *, char* );
void freeALL();

