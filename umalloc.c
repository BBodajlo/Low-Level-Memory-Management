#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for size_t
#include <stddef.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include "umalloc.h"

void debug(char *s, ...) {
    #ifndef DEBUG
    return;
    #endif
    va_list args;
    va_start(args, s);
    vprintf(s, args);
    va_end(args);
}

uint* umalloc_debug_flags = NULL;

int umalloc_set_debug(uint* new_debug_flags) {
    umalloc_debug_flags = new_debug_flags;
    return 0;
}
int umalloc_set_debug_flag(uint flag) {
    if (umalloc_debug_flags == NULL) {
        return 1;
    }
    *umalloc_debug_flags |= flag;

    return 0;
}
int umalloc_stop_debug() {
    umalloc_debug_flags = NULL;
    return 0;
}


//ProtoTypes moved to mymalloc.h
void loadSpan(page_header_t* );
int writeDataToSwap(char* , page_header_t* );
int evictPageToSwapFile();
void initializeSwapFile();
//globals
struct sigaction PageFaultHandler; //signal handler for timer

static int MEMSIZE = PAGE_SIZE; //Changed to be page size of 4096 (same as regular mymalloc)
//char memoryArray[HEAP_SIZE];
char *memoryArray;

#define FIRST_MEMORY_PAGE ((char*)(memoryArray) + (RESERVED_PAGES * PAGE_SIZE))

//Initial Page Index will be MemoryArray[5] since the reserved space takes of 4 pages
int INITIAL_INDEX = 6;
int Thread_Page_Index = 1; // Used to know which page the functions are using/starting at (Should always be the first page of an allocation)
int PAGES_FOR_MALLOC;
int shallocInitialized = 0;
int isthisashalloc = 0;
char *shallocLocation = NULL;
int swapFileFD;
// Find previous header from header
header_t* findPrevHeader(header_t* header)
{
    return (header_t*)((char*)header - header->prev - HEADER_SIZE);
}

// Find next header from header
header_t* findNextHeader(header_t* header)
{
    return (header_t*)((char*)header + header->sizeOfChunk + HEADER_SIZE);
}

/* SHOULD BE DONE FOR PART A
*   Utilizes the first-fit method to search for an available chunk
*   It will start at the beginning of the heap and search every header until there is a chunk big enough for the given size
*   MODIFICATION: 
        Changed the parameters to include a heap (memory) that it will dynamically use (DONE)
        Made the while loop look between the start and Page_Size (basically same as before) (DONE)
        Update the meatadata in the reserved page as well 
*   char *memory is the start of a page/heap
*/
void *findFreeChunk(int size, char *memory)
{   
   
    unlockMemoryPages();
    header_t *pointer = (header_t*)&memory[0]; //Setting pointer to the beginning of heap
    void *location = NULL;
    int threadNum = getCurrentThreadId();
    int relativeThreadPage = Thread_Page_Index;
    debug("This is pointer for first page in findFreeChunk %p\n", memory);
    if(PAGES_FOR_MALLOC > 1 && threadNum != SCHED_CONTEXT && isthisashalloc == 0) //Want to line up span of pages
    {
        //Can't move the first page, so we have to move the span in right after it
        int positionOfPage = findPageOnFault(memory);
        page_header_t *headerForPage = findMetaDataForPageIndex(positionOfPage);
        debug("Spot %d thread: %d\n", headerForPage->loc, headerForPage->threadID);
        debug("Spot %d PageNum: %d\n", headerForPage->loc, headerForPage->pageNum);
        debug("Spot %d Usage: %d\n", headerForPage->loc, headerForPage->usage);
        debug("Spot %d Page Index: %d\n", headerForPage->loc, headerForPage->loc);
        debug("Spot %d Span: %d\n", headerForPage->loc, headerForPage->spanOfPages);
        debug("\n");
        debug("Size of first chunk in page %d\n", pointer->sizeOfChunk);
        debug("This is the first page's position %d\n", positionOfPage);

        //Load span into place
        loadSpan(headerForPage);
        
    }
    /* debug("This is pages for malloc %d\n", PAGES_FOR_MALLOC); */
    int UPPERBOUND = PAGE_SIZE * PAGES_FOR_MALLOC; //Upper bound for a heap 

    if(isthisashalloc == 1)
    {
        relativeThreadPage = 1;
    }
    
    debug("Before loop thread page index %d\n", relativeThreadPage);
    while((char*)pointer >= &memory[0] && (char*)pointer < &memory[UPPERBOUND]) // Check between the beginning of the page and the end of the page give by pageSize
    {
         /* debug("Upper bound for start of pages %d\n", UPPERBOUND);
        debug("Pointer for start of pages %p\n", &memory[0]);
        debug("Pointer for start of pages %p\n", &memory[UPPERBOUND]); */
        
        if(pointer->isFree == 1 && pointer->sizeOfChunk >= size) //Checks to see if a chunk is free and then if the size of big enough for the current allocation
        {
           // debug("Starting: ");
           //In here means it found a free chunk
            if(pointer->sizeOfChunk-size > HEADER_SIZE) //If the remaining portion of the chunk is greater than the size of a header, make a new header; else do not make a new header since it can't fit
            {
                 //debug("Page before malloc");
                    //printPageDebug(memory);
                //debug("Splitting chunk");
                header_t *next = (header_t*)((char*)pointer + size + HEADER_SIZE);
                next->prev = size;
                next->isFree = 1;
                next->sizeOfChunk = pointer->sizeOfChunk - size - HEADER_SIZE;
                pointer->sizeOfChunk = size;
                pointer->isFree = 0; //Set this new chunk to not free

                // We change the prev of the next, but if you insert between 2 chunks
                // you need to change the next next's prev as well to fit the new chunk
                header_t* next_next = findNextHeader(next);
                if((char*)next_next < &memory[UPPERBOUND])
                {
                    next_next->prev = next->sizeOfChunk;
                }
                
                //debug("After malloc chunk");
                //printPageDebug(memory);
                
                //PART A: Update the meta data size
                debug("Meta data before malloc of %ld including a new header for thread: %d\n", size+HEADER_SIZE, threadNum);
                page_header_t *pageMetaData = findMetaDataForThreadPageIndex(relativeThreadPage);
                pageMetaData->usage = pageMetaData->usage - size - HEADER_SIZE;
                debug("Meta data after malloc of %ld for thread: %d\n", size+HEADER_SIZE, threadNum);
                findMetaDataForThreadPageIndex(relativeThreadPage);
                debug("Page after malloc");
                //printPageDebug(memory);
               
            }
            else //Not making a new header inside of the chunk
            {
                //PART A: Update the meta data size
                debug("Meta data before malloc of %d no new header, can't fit, for thread: %d\n", size, threadNum);
                page_header_t *pageMetaData = findMetaDataForThreadPageIndex(relativeThreadPage);
                pageMetaData->usage = pageMetaData->usage - size;
                debug("Meta data after malloc of %ld for thread: %d\n", size+HEADER_SIZE, threadNum);
                findMetaDataForThreadPageIndex(relativeThreadPage);
                
                // //debug("Chunk cannot be split\t");
                // if(size <= HEADER_SIZE) //Catches edge cases where the chunk spans the whole heap; as in the first initialized header being completely allocated
                // {
                //     pointer->sizeOfChunk += size ; //If a chunk cannot be split, the amount of space that is left over is added on to the current chunk but will not be used until it is freed
                //     debug("Apples\t");
                // } 
                // else
                // {
                //     pointer->sizeOfChunk = pointer->sizeOfChunk +(pointer->sizeOfChunk-size); //when the first chunk is fully allocated, so say 4088 bytes + header = 4094 bytes
                //     debug("Here\t");
                // }

            }
            
            pointer->isFree = 0; //The chunk is now being used
            location = pointer+1; //The location of the start of the chunk for data
            break;
        }
        
        pointer = findNextHeader(pointer); //Going to the next chunk using the chunk size and the size of the header in char reference
    }
    //debug("Ending: %p\t", pointer);

    //PART B NEED TO ADD A CHECK HERE INSIDE OF THE IF STATEMENTS BELOW
    // NEED TO CHECK IF THE THREAD HAS ANOTHER PAGE THEY CAN TRY TO USE TO MALLOC TO
    // IF THEY DO, RUN FINDFREECHUNK AGAIN WITH THAT NEW PAGE FROM FINDFREEPAGE
    // IF THEY DON'T NEED TO TRY TO MAKE A NEW PAGE WITH FINDFREEPAGE
    // Find Free Page handles ^^^^^^^
    if(pointer == NULL) //Will return null if no sutible chunk was found
    {
        return NULL;
    }

    if (location == NULL) //Will return null if no sutible chunk was found
    {
        //debug("Error: No available chunk found\n");
        return NULL;
    }
    
    //Returning the pointer relative to the page, and not an offset from the first page
    if(isthisashalloc == 1)
    {      
        debug("In shalloc");
        isthisashalloc = 0;
        startiTimer(INITIAL_QUANTUM);
        //Need to lock the pages again before returning
        int threadNumber = getCurrentThreadId();
        lockAllButThread(threadNumber);
        return location;
    }
    debug("After shalloc");

     //returns the location of the available chunk inside of the page itself

    //Need to calculate how many bytes into the memory heap the location is
    ptrdiff_t locationOffset = (char*)location - memory;
    
    
    //Want to return the location as an offset from the memoryArray[4]
    void *finalLocation = FIRST_MEMORY_PAGE + (ptrdiff_t)((relativeThreadPage-1) * PAGE_SIZE);
    finalLocation = (char*)finalLocation + locationOffset;
    debug("This is First memory page offset %p\n", FIRST_MEMORY_PAGE);
    debug("This is thread page index %d (0 is first)\n", relativeThreadPage-1);
    debug("This is location offset %d\n", locationOffset);
    
    debug("This is final memory locatoin %p\n", finalLocation);

    debug("--Final location for malloc is--\n");
    debug("On page #%d for the thread\n", relativeThreadPage);
    debug("It is %d bytes into the page\n",(int)locationOffset);
    debug("Pointer is %d bytes into the Memory pages\n", (char*)finalLocation-FIRST_MEMORY_PAGE);
    debug("Which is on page %d of the MemoryPages (0 is first | not counting reserved)\n", ((char*)finalLocation-FIRST_MEMORY_PAGE)/4096);

    //Need to account for the stack pointer needing to be the bottom/end of the address
  /*   if(isStackAllocation() == 1)
    {
        finalLocation = (char*)finalLocation + STACK_SIZE;
        debug("Stack allocation so pointer is the end: %p\n", finalLocation);
    }      */

    return finalLocation;
}
/*  DONE FOR PART A
*   Checks if the heap is all zeros/nothing has been allocated yet
*   Returns 1 if all are zeros; Returns 0 if not all zeros
*   MODOFICATION: Changed params to give a page(memory) and size (to differentiate memoryArray and the pages)
*/
int checkIfAllZeros(char *memory, int size) 
{
    for(int i = 0; i < size; i++)
    {
        if(memory[i] != 0)
            return 0;
    }
    return 1;
}

/*  DONE FOR PART A
*   Initialize is called only once and it places a header at the beginning of the heap of size MEMSIZE-sizeof(header) [4096-6 = 4090]
*   No additional headers are added
*   MODIFICATION: Changed param to give a page
*/
void initialize(char *memory)
{
    memset(memory, 0 ,PAGE_SIZE);
    header_t *head = (header_t*)&memory[0];
    head->isFree = 1;
    head->sizeOfChunk = (MEMSIZE * PAGES_FOR_MALLOC)-sizeof(*head); // uses how many pages this malloc needs
    head->prev = 0;
}

// DONE FOR PART A  
// Coallesce the two pointers, return the header pointer to the new coalesced chunk
// Takes 2 adjacent header pointers that have already been checked if they have been free and in bounds
// MODIFICATION: Changed param to give a page
void* coalesce(header_t* leftPointer, header_t* rightPointer, char *file, int line, char *memory, int UPPERBOUND, page_header_t **header)
{

    if ((char*) leftPointer - memory < 0 || &memory[UPPERBOUND] - (char*) rightPointer <= 0){
        //debug("Error: Out of bounds. File: %s, Line: %d\n", file, line);
        
        return NULL;
    }

    // Check if the left pointer is free
    if (leftPointer->isFree == 0)
    {
        //debug("Error: Left pointer is not free. File: %s, Line: %d\n", file, line);
        return NULL;
    }

    // Check if the right pointer is free
    if (rightPointer->isFree == 0)
    {
        //debug("Error: Right pointer is not free. File: %s, Line: %d\n", file, line);
        return NULL;
    }

    // Set the size of the new chunk (keeping the left pointer)
    leftPointer->sizeOfChunk += rightPointer->sizeOfChunk + HEADER_SIZE;

    debug("This is size being added %d\n",  rightPointer->sizeOfChunk + HEADER_SIZE);
    //Update metadata for the reserved pages by adding the size + the header back
    (*header)->usage +=  HEADER_SIZE;
    // Set the prev of the next chunk
    header_t* nextHeaderPointer = findNextHeader(rightPointer);
    if ((char*) nextHeaderPointer < &memory[UPPERBOUND])
    {
        nextHeaderPointer->prev = leftPointer->sizeOfChunk;
    }

    // Set the header values to 0 of the old header in memory
    for (int i = 0; i < HEADER_SIZE; i++)
    {
        //((char*) rightPointer)[i] = 0;
    }

    return leftPointer;

}

/*
    Given a thread and the page it wants, will give the first page in the span
    Methodology: Count up from the pages (Which are loaded into place already)
    Everytime we find a page with a span matching the one given
        Save the first page of the span's pointer
        Count up the amount of the span
            If we find the page we want within that span, that page pointer is the one we want
*/
char* findSpanStart(int threadNum, int pageThreadWants, int pageSpan, page_header_t** header)
{   
    int count = 1; //Start at one, go up to the span number
    char* pointer;
    for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        //Go through each page, only care about the ones that have the same span as the one we want
        if(metaData->threadID == threadNum && metaData->spanOfPages == pageSpan) 
        {
            debug("MetaData information for span search");
            debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
            debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
            debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
            debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
            debug("\n"); 

            //If this is the first of the span, save its pointer location
            if(count == 1)
            {
                pointer = memoryArray + (pageIndex * PAGE_SIZE);
                *(header) = metaData;
            }

            //If the page number matches the one we are looking for, return the first page we noted for the span
            if(metaData->pageNum == pageThreadWants)
            {
                return pointer;
            }

            //Increase the counter for this span
            count++;    

        }    
        //If we increased the counter past the span length we are looking for, the next page we search is a new span
        if(count > pageSpan)
        {
            count = 1;
            pointer = NULL;
        }
    }
}
// Free the memory pointed to by pointer, coalesce if possible
void myfree(void *pointer, char *file, int line, int lib)
{
    //FOR PART A: NEED TO ADD A FIND PAGE FOR THREAD FUNCTION
    //AND HAVE IT RETURN A CHAR *MEMORY POINTER TO THE BEGINNING OF THE PAGE

    //ThreadNumber will be a global from the library
    stopiTimer();
    int threadNum;

    /* debug("Shalloc lower bound %p\n", shallocLocation + HEADER_SIZE);
    debug("Shalloc upper bound %p\n", shallocLocation + (SHARED_PAGES * PAGE_SIZE));
    debug("Pointer location %p\n", pointer); */


    if(shallocLocation != NULL && pointer >= shallocLocation + HEADER_SIZE && pointer < shallocLocation + (SHARED_PAGES * PAGE_SIZE)) //we have a shalloc pointer
    {
        threadNum = SHALLOC_THREAD_NUM;
    }
    else
    {
        threadNum = getCurrentThreadId();
    }
    /* if(lib == LIBRARYREQ)
    {
       threadNum = getCurrentThreadId();
    }
    else
    {
        threadNum = 1;
    } */

    //Make sure the inital pointer is not NULL
    if (pointer == NULL)
    {
        debug("Error: Null pointer. File: %s, Line: %d\n", file, line);
        umalloc_set_debug_flag(DEBUG_FREE_FAIL);
        return;
    }

    // Check if pointer is in range
    if(pointer < &memoryArray[0] || pointer > &memoryArray[HEAP_SIZE])
    {
        debug("Error: Pointer is not in bounds of memory. File: %s, Line: %d\n", file, line);
        //debug("Pointer: %p, Memory: %p, Memsize: %p\n", charPointer, &memory[0], &memory[MEMSIZE]);
        umalloc_set_debug_flag(DEBUG_FREE_FAIL);
        return;
    }
    

    //Given the pointer, need to find the pageNumber it is on
    //If it is, get the page index from findPageOnFault()
    //Load all pages for a thread so that they are lined up correctly
    debug("This is thread number in free %d\n", threadNum);
    char *memory;
    header_t *head;
    page_header_t *pageHeaderForLoad;
     int UPPERBOUND; //Upper bound for a heap
    if(threadNum != SHALLOC_THREAD_NUM)
    {
        loadLibraryPages(threadNum);
        int absolutePageNum = findPageOnFault(pointer);
        
        debug("This is the page number the free pointer was pointing to %d\n", absolutePageNum);
        
        int pageThreadWants = (absolutePageNum - RESERVED_PAGES) + 1;
        debug("This is the page index where the free is trying to occur %d\n", pageThreadWants);

        //Check to make sure they have this page
        char *pageToLoad = findPageForThreadGivenPageNum(threadNum, pageThreadWants);

        
        if(pageToLoad != NULL) //They did have the page number they wanted
        {
            //Get the metadata for the page the thread wants
            pageHeaderForLoad = findMetaDataGivenPageNum(threadNum, pageThreadWants);
        }

        debug("thread id: %d\n",pageHeaderForLoad->threadID);

        //Page is loaded into the correct spot, calculate the upperbound based on the span of pages
       
        if(pageHeaderForLoad->spanOfPages == 1)
        {
            UPPERBOUND = PAGE_SIZE; //Upper bound for a heap
        }
        else
        {
            UPPERBOUND = PAGE_SIZE * pageHeaderForLoad->spanOfPages; //Upper bound for a heap

            page_header_t *thisHeader; //Header for the start of the span
            //To find the start of the span, need the threadnum, the page it wants, and the span
            pageToLoad = findSpanStart(threadNum, pageThreadWants, pageHeaderForLoad->spanOfPages, &thisHeader);

            pageHeaderForLoad = thisHeader;
            
            
        }
    
        //printMetaData();
        debug("Header for page in free \n");
        debug("Spot %d thread: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->threadID);
        debug("Spot %d PageNum: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->pageNum);
        debug("Spot %d Usage: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->usage);
        debug("Spot %d Page Index: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->loc);
        debug("Spot %d Span: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->spanOfPages);
        debug("\n");
        memory = pageToLoad;
        head = (header_t*)memory;
        debug("Page Before Free\n");
        printPageDebug(memory);
    }
    else
    {
        memory = shallocLocation;
        head = (header_t*)memory;
        page_header_t *metaData = (page_header_t *)(memoryArray + SHALLOC_START_PAGE * PAGE_HEADER_SIZE);
        pageHeaderForLoad = metaData;
        UPPERBOUND = UPPERBOUND = PAGE_SIZE * SHARED_PAGES;
        debug("Header for page in free \n");
        debug("Spot %d thread: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->threadID);
        debug("Spot %d PageNum: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->pageNum);
        debug("Spot %d Usage: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->usage);
        debug("Spot %d Page Index: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->loc);
        debug("Spot %d Span: %d\n", pageHeaderForLoad->loc, pageHeaderForLoad->spanOfPages);
        debug("\n");
        //printPageDebug(memory);
    }

    
    //FOR PART B: SINCE THE POINTERS WILL ALL BE BASED ON MEMORYARRAY[1] AS THE START
    // WE CAN CALCULATE WHICH PAGE THE POINTER IS ON DOING THE CIELING OF INTEGER DIVISON OF 
    // POINTER LOCATION / MEMORYARRAY[1] WHICH WILL GIVE A PAGE LIKE 2.5 OR .5. WHEN
    // WE ROUND UP, IT WILL BE THE INDEX OF THE PAGE THAT POINTER IS LOCATED IN
    // THAT LOCATION WILL HAVE THE HEAD AND THE CHUNK SIZE (IN THE HEADER) SO WE WILL KNOW HOW MANY
    // PAGES WE NEED TO SUPPLY THIS FUNCTION AS THE CHAR* MEMORY POINTER
    // SO WE WILL GIVE THIS FUNCTION CHAR *MEMORY AND CHAR SIZE (WHERE SIZE IS IN INTERVALS OF PAGE_SIZE SO 4096, 8192 ....)
    // THIS SHOULD MAKE FREE WORK WITHOUT ISSUEA
    char* charPointer = (char*)pointer; // Cast to char pointer like we did before 
    // Check if Null pointer
    if (charPointer == NULL)
    {
        //debug("Error: Null pointer. File: %s, Line: %d\n", file, line);
        umalloc_set_debug_flag(DEBUG_FREE_FAIL);
        return;
    }

    // Check if pointer is in range
    if(charPointer < &memory[0] || charPointer > &memory[UPPERBOUND])
    {
        debug("Error: Pointer is not in bounds of memory. File: %s, Line: %d\n", file, line);
        //debug("Pointer: %p, Memory: %p, Memsize: %p\n", charPointer, &memory[0], &memory[MEMSIZE]);
        umalloc_set_debug_flag(DEBUG_FREE_FAIL);
        return;
    }
    debug("After range check");
    // Check if pointer is a valid pointer
    char *tempPointer = &memory[0];
    while(tempPointer < &memory[UPPERBOUND])
    {
        if (tempPointer + HEADER_SIZE == charPointer)
        {
            break;
        }

        tempPointer += HEADER_SIZE + ((header_t*)tempPointer)->sizeOfChunk;
    }

    if (tempPointer >= &memory[UPPERBOUND])
    {

        char *tempPointer = &memory[0];
        while(tempPointer < &memory[UPPERBOUND]) // THIS IS FOR DEBUGGING
        {
            if (tempPointer + HEADER_SIZE == charPointer)
            {
                break;
            }

           //debug("\nPointer: %d, Size: %d, Prev: %d, IsFree: %d", tempPointer - memory, ((header_t*)tempPointer)->sizeOfChunk, ((header_t*)tempPointer)->prev, ((header_t*)tempPointer)->isFree);
            tempPointer += HEADER_SIZE + ((header_t*)tempPointer)->sizeOfChunk;
        }

        debug("Error: Pointer is not a valid pointer. File: %s, Line: %d\n", file, line);
       // debug("Pointer: %p, Pointer Number: %d, Memory: %p, Memsize: %p\n", charPointer, charPointer - memory, &memory[0], &memory[MEMSIZE]);
        //exit(1);
        umalloc_set_debug_flag(DEBUG_FREE_FAIL);
        return;
    }

    // We have a valid pointer
    // Check if the pointer is already free
    header_t* pointerHeader = (header_t*) charPointer-1;
    debug("This is pointer header %d\n", pointerHeader->isFree);
    if (pointerHeader->isFree == 1)
    {
        printf("Error: Pointer is already free. File: %s, Line: %d\n", file, line);
        umalloc_set_debug_flag(DEBUG_DOUBLE_FREE);
        return;
    }

    pointerHeader->isFree = 1; // Dont change size or prev until coalescing
    
    // Set all values in memory to 0
    tempPointer = charPointer;
    for (int i = 0; i < pointerHeader->sizeOfChunk; i++)
    {
        tempPointer[i] = 0;
    }

    pageHeaderForLoad->usage += pointerHeader->sizeOfChunk;
    // Coalesce with previous header
    header_t* tempPointerHeader = coalesce(findPrevHeader(pointerHeader), pointerHeader, file, line, memory, UPPERBOUND, &pageHeaderForLoad);
    if (tempPointerHeader != NULL)
    {
        pointerHeader = tempPointerHeader;
    }
    
    // Coalesce with next header
    coalesce(pointerHeader, findNextHeader(pointerHeader), file, line, memory, UPPERBOUND, &pageHeaderForLoad);
    
    /* MEGA OUTDATED SAD */
    // Check if the pointer is the first or last header
    // char* previousHeaderPointer = ((char*) pointerHeader) - pointerHeader->prev - HEADER_SIZE;
    // char* nextHeaderPointer = ((char*) pointerHeader) + HEADER_SIZE + pointerHeader->sizeOfChunk; 
    // int isFirstHeader = previousHeaderPointer < &memory[0];
    // int isLastHeader = nextHeaderPointer >= &memory[MEMSIZE];

    // debug("\nFreeing pointer: %d\t", charPointer - memory);

    // Now we coalesce the adjacent memory

    // if (!isLastHeader && ((header_t*)nextHeaderPointer)->isFree == 1)
    // {
    //     // Coalesce with next header
    //     coalesce((char*) pointerHeader, (char*) nextHeaderPointer);
    //     debug("NEW HEADER: %d -> %d\t", ((char*) nextHeaderPointer + HEADER_SIZE)-memory, ((char*) pointerHeader + HEADER_SIZE) - memory);

    // }

    // if (!isFirstHeader && ((header_t*)previousHeaderPointer)->isFree == 1)
    // {
    //     // Coalesce with previous header
    //     pointerHeader = (header_t*) coalesce((char*) previousHeaderPointer, (char*) pointerHeader); 
    //     debug("NEW HEADER: %d -> %d\t", charPointer-memory, ((char*) pointerHeader + HEADER_SIZE) - memory);

    //     // We reset the pointerHeader here to the previousHeaderPointer
    // }

    //debug("Size: %d, Prev: %d, IsFree: %d\n", pointerHeader->sizeOfChunk, pointerHeader->prev, pointerHeader->isFree);
    head = (header_t*)memory;
    //Update the pageMetaData
    //printMetaData();
    debug("Page After Free\n");
    printPageDebug(memory);
    //exit(1);
    return;
}

/*
* Will initialize the reserved memory page on the first call to malloc
* Functionality: Place the page_header struct in the first 4 spots of memoryArray[0]
* This will have threadID = -1, pageNum = -1 usage = -1, loc = 0 (loc = 1, loc = 2, loc = 3)
* for metadata for the reserved pages
*/
void initializeMetaData()
{
    debug("pointer for memoryArray %p\n", (void *)memoryArray);
    //Set all of the values to 0
   /*  page_header_t *metaData = (page_header_t*)&memoryArray[0];
    metaData->threadID = -1;
    metaData->pageNum = -1;
    metaData->usage = -1;
    metaData->loc = 0 */

    // Allocating metadata for the first 4 pages
    for(int pageIndex = 0; pageIndex < RESERVED_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t*)(memoryArray + (PAGE_HEADER_SIZE * pageIndex)); //Pointer math based on the page size
        metaData->threadID = -1; //Indicate no thread is here
        metaData->pageNum = -1; //Indicate this is not a standard value for this metadata
        metaData->usage = 0; //We don't care about the space of the metadata pages
        metaData->loc = pageIndex; //We do want to keep track of the page indexes
    }

    // DEBUG: Print the initial values for the metadata pages
    /*  for (int pageIndex = 0; pageIndex < 4; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
        debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
        debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
        debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
        debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
        debug("\n");
    }  */
}

/*
    Used to find the metadata for a given page index
    Returns a pointer to page_header_t
*/
page_header_t* findMetaDataForPageIndex(int pageIndex)
{
    
    page_header_t *metaData = (page_header_t*)(memoryArray + (PAGE_HEADER_SIZE * pageIndex)); //Pointer math based on the page size

    debug("MetaData information");
    debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
    debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
    debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
    debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
    debug("\n"); 
    return metaData; 
    
}

page_header_t* findMetaDataForThreadPageIndex(int threadPageIndex)
{   

    int threadNumber;
    if(isthisashalloc == 0)
    {
     threadNumber = getCurrentThreadId();
    }
    else
    {
        threadNumber = SHALLOC_THREAD_NUM;
    }

    
    //debug("This is the thread's page index %d\n", threadPageIndex);
    //printMetaData();
    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
        
        if(metaData->threadID == threadNumber && metaData->pageNum == threadPageIndex) 
        {
            debug("Found a page for thread %d at index %d\n", threadNumber, pageIndex);
            debug("MetaData information");
            debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
            debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
            debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
            debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
            debug("\n"); 
            return(metaData);
            
        }    
    }
    
}

/*
    Used to find the page for a given thread
    COULD ALTER TO LOOK FOR NEXT PAGE BASED ON THE Thread_Page_Index GLOBAL
    Returns a pointer to a page's heap/memory
*/
char* findPageForThread(int threadNumber)
{
    
    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->threadID == threadNumber) 
        {
            debug("Found a page for thread %d at index %d\n", threadNumber, pageIndex);
            Thread_Page_Index = pageIndex;
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset;

            return pagePointer; //returns a pointer to the page location
        }    
    }

    //Return null if can't find a page
    //Could be for swap file check indication (???)
    return NULL;
}

/*
    Will tell if there are enough free pages given how many are desired
    True if there are enough
    False if there aren't
*/
int findNumberOfFreePages(int pagesNeeded)
{
    
    for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->threadID == 0) 
        {
            pagesNeeded--;
        }    

        if(pagesNeeded == 0)
        {
            return TRUE;
        }
    }


    return FALSE;
}

/*
    Finds a free page for a given thread
    PART A: Will only allow a thread to have one page
    Assumes the threadID is passed from the library for now

    Return: A pointer to the start of the free page
    Returns NULL if there are no free pages left

    May need to alter later to set an external index of the page number
    for virtualizing
    PART B: NEED TO ADD A WAY TO CHECK FOR THE NEXT PAGE
    SO NEED TO USE THE Thread_Page_Index GLOBAL AND BASICALLY LOOK FOR THE ONE AFTER IT
*/
char* findFreePage(int threadID, int size)
{

    unlockMemoryPages();
    //Need to check the metadata for every single page for the thread first before making a new page
    //Should only make a new page unless absolutely necessary
    //Should keep track of the number of pages for this thread already
    debug("Looking for thread's pages");
    int threadPageCount = 1;
    for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->threadID == threadID) //If its the thread's page,
        { 
             /* debug("This is the usage for meta data %d\n",metaData->usage);
             debug("This is the size %d\n", size); */
             
            threadPageCount++;
            if(size <= metaData->usage) //and there is enough size to allocate to
            {
                threadPageCount--; //Go back to the page that was just confirmed
                //debug("Already found a page for thread %d, it has enough space for this malloc of %d in page %d\n", threadID, size, threadPageCount);
                Thread_Page_Index = threadPageCount;
                //Offset into the memoryArray
                int offset = pageIndex * PAGE_SIZE;

                //The actual pointer to the start of the free page
                char *pagePointer = memoryArray + offset;
                

                return pagePointer; //returns a pointer to the page location
            }
        }
        
    
    }

    debug("This thread had %d pages already\n", threadPageCount-1);
    
    if(threadPageCount > 2035)
    {
        debug("Max allocation allowed for pages");
        return NULL;
    }
    //Add a check to see if there are enough free pages for the thread's need
    int enoughPages = findNumberOfFreePages(PAGES_FOR_MALLOC);
    //CAN LATER MODIFY TO KEEP COUNT OF HOW MANY PAGES IT ALREADY HAS
    
    //If we didn't find a sutible page for the thread before, we should make a new page for it
    //We should find the first available page
   
    if(enoughPages == TRUE && PAGES_FOR_MALLOC == 1)
    {
        int spanOfPages = PAGES_FOR_MALLOC; //This is how many pages this one allocation uses/accounts for
        debug("Thread didn't have pages or enough space, looking for only one page");
        for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
        {   
            page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
            if(metaData->threadID == 0) // Checking each page to see if its free (0 means its free)
            {
                //If the page is occupied, need to check if its the same thread (For part a this should indicate NULL )
                //PART A: If the thread has a page, then return the pagePointer so malloc can try mallocing it
                //FOR PART B, HAVE THIS KEEP TRACK OF THE PAGE INDEX
                //THE POINTER TO RETURN SHOULD BE MEMORYARRAY(INDEX OFFSET)
                //THEN USE THE PAGE WE FOUND HERE TO INDICATE IT IS TAKEN/THE METADATA FOR IT
                debug("Found free page at location %d\n", pageIndex);

                //Update the metadata to indicate this thread has it
                metaData->threadID = threadID;
                metaData->pageNum = threadPageCount; //Should always be pageNum 1 here since there was no page found for it before
                metaData->usage = PAGE_SIZE - HEADER_SIZE; // Set the size it has to account for the malloc header size
                metaData->loc = pageIndex; //The page index is based on the loop and where it was found in the meta data
                metaData->spanOfPages = spanOfPages;
                
                debug("New metadata for the thread");
                debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
                debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
                debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
                debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
                debug("Spot %d Page Index: %d\n", pageIndex, metaData->spanOfPages);
                debug("\n");
                //Need to initialize the new heap/page by giving it the page's space/heap
                //This page's heap will be a pointer to the first location of the page

                //Offset into the memoryArray
                //Still want it to write into the page
                int offset = pageIndex * PAGE_SIZE;

                //The actual pointer to the start of the free page
                //Still want to write it to the page itself
                char *pagePointer = memoryArray + offset;

                //Initialize the malloc block for this heap
                initialize(pagePointer);

                //puts("Initialized page pointer");

                //Set the global page index tracker
                //Thread_Page_Index = pageIndex; No longer based on the page location inside of the memoryArray
                Thread_Page_Index = threadPageCount; //should be based on the location relative to the thread

                //findMetaDataForPageIndex(Thread_Page_Index);

                return pagePointer;

                
            }
            else // Couldn't find a free page (THIS COULD BE WHERE WE THEN USE THE SWAP FILE LOGIC)
            {
                //Want to evict a page from the memoryArray so we can run this function again
                

            }
        }
    }
    else if(PAGES_FOR_MALLOC == 1 && enoughPages == FALSE)
    {   
        if (evictPageToSwapFile() == 0) {
            return NULL;
        }
        
        return findFreePage(threadID, size);
    }
    else if(enoughPages == TRUE && PAGES_FOR_MALLOC > 1)
    {
        //Want a page pointer holder
        char *pagePointer;
        int numAllocated = 0; // Need an indexer to keep track of how many pages we've gotten already
        debug("Thread didn't have pages or enough space, looking for multiple pages");
        for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
        {   
            page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
            if(metaData->threadID == 0 && numAllocated < PAGES_FOR_MALLOC) // Checking each page to see if its free (0 means its free) and we've allocated the correct amount
            {
                //If the page is occupied, need to check if its the same thread (For part a this should indicate NULL )
                //PART A: If the thread has a page, then return the pagePointer so malloc can try mallocing it
                //FOR PART B, HAVE THIS KEEP TRACK OF THE PAGE INDEX
                //THE POINTER TO RETURN SHOULD BE MEMORYARRAY(INDEX OFFSET)
                //THEN USE THE PAGE WE FOUND HERE TO INDICATE IT IS TAKEN/THE METADATA FOR IT
                debug("Found free page at location %d\n", pageIndex);

                //Update the metadata to indicate this thread has it
                metaData->threadID = threadID;
                metaData->pageNum = threadPageCount + numAllocated; //Should always be pageNum 1 here since there was no page found for it before
                
                if(numAllocated == 0)
                {
                    metaData->usage = (PAGES_FOR_MALLOC * PAGE_SIZE) - HEADER_SIZE; // Set the size it has to account for the malloc header size
                }
                else
                {
                    metaData->usage = -1; // indicates this page is an auxillary for one before it
                }
                metaData->loc = pageIndex; //The page index is based on the loop and where it was found in the meta data
                metaData->spanOfPages = PAGES_FOR_MALLOC;
                
                debug("New metadata for the thread");
                debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
                debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
                debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
                debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
                debug("Spot %d Page Index: %d\n", pageIndex, metaData->spanOfPages);
                debug("\n");
                //Need to initialize the new heap/page by giving it the page's space/heap
                //This page's heap will be a pointer to the first location of the page

                //Mprotect should make it so the pages don't have to be continuous 
                if(numAllocated == 0) //Only want the first page to be initialized and only want pointer to the first page
                {
                    //Offset into the memoryArray
                    //Still want it to write into the page
                    int offset = pageIndex * PAGE_SIZE;

                    //The actual pointer to the start of the free page
                    //Still want to write it to the page itself
                    pagePointer = memoryArray + offset;

                    //Initialize the malloc block for this heap
                    initialize(pagePointer);

                    //Set the global page index tracker
                    //Thread_Page_Index = pageIndex; No longer based on the page location inside of the memoryArray
                    Thread_Page_Index = threadPageCount; //should be based on the location relative to the thread
                    debug("This is thread page count in loop %d\n", Thread_Page_Index);
                    //findMetaDataForPageIndex(Thread_Page_Index);

                    
                }
                
                numAllocated++;
                
            }
            else // Couldn't find a free page (THIS COULD BE WHERE WE THEN USE THE SWAP FILE LOGIC)
            {
            

            }
        }
        return pagePointer;

    }
    
    // Returns NULL indicating that there are no free pages left
    return NULL;

}


/*
    Allocate scheduler pages 
    Will be pages 2041, 2042 (Stack), 2043 (Ucontext Pointer)
*/
char* makeShallocPages(int size)
{

    //CAN LATER MODIFY TO KEEP COUNT OF HOW MANY PAGES IT ALREADY HAS
    
    //If we didn't find a sutible page for the thread before, we should make a new page for it
    //We should find the first available page
   
    
    
    debug("Making shalloc pages ");

    //Make metadata for the first shalloc page
    page_header_t *metaData = (page_header_t *)(memoryArray + SHALLOC_START_PAGE * PAGE_HEADER_SIZE);

    //Update the metadata to indicate this thread has it
    metaData->threadID = -5;
    metaData->pageNum = 1; //Should always be pageNum 1 here since there was no page found for it before
    metaData->usage = (SHARED_PAGES * PAGE_SIZE) - HEADER_SIZE; // Set the size it has to account for the malloc header size
    metaData->loc = (SHALLOC_START_PAGE); //The page index is based on the loop and where it was found in the meta data
    metaData->spanOfPages = SHARED_PAGES;
    
    debug("New metadata1 for the scheduler stack");
    debug("Spot %d thread: %d\n", (SHALLOC_START_PAGE), metaData->threadID);
    debug("Spot %d PageNum: %d\n", (SHALLOC_START_PAGE), metaData->pageNum);
    debug("Spot %d Usage: %d\n", (SHALLOC_START_PAGE), metaData->usage);
    debug("Spot %d Page Index: %d\n", (SHALLOC_START_PAGE), metaData->loc);
    debug("Spot %d Page Index: %d\n", (SHALLOC_START_PAGE), metaData->spanOfPages);
    debug("\n");
    
    //Make the second shalloc page
    page_header_t *metaDataPage2 = (page_header_t *)(memoryArray + (SHALLOC_START_PAGE+1) * PAGE_HEADER_SIZE); //page 2042


    metaDataPage2->threadID = -5;
    metaDataPage2->pageNum = -5; //Should always be pageNum 1 here since there was no page found for it before
    metaDataPage2->usage = -1; // Set the size it has to account for the malloc header size
    metaDataPage2->loc = (SHALLOC_START_PAGE+1); //The page index is based on the loop and where it was found in the meta data
    metaDataPage2->spanOfPages = SHARED_PAGES; 

    debug("New metadata2 for the scheduler stack");
    debug("Spot %d thread: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->threadID);
    debug("Spot %d PageNum: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->pageNum);
    debug("Spot %d Usage: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->usage);
    debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->loc);
    debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->spanOfPages);
    debug("\n");

    //Make the third shalloc page
    page_header_t *metaDataPage3 = (page_header_t *)(memoryArray + (SHALLOC_START_PAGE+2) * PAGE_HEADER_SIZE); //page 2042


    metaDataPage3->threadID = -5;
    metaDataPage3->pageNum = -5; //Should always be pageNum 1 here since there was no page found for it before
    metaDataPage3->usage = -1; // Set the size it has to account for the malloc header size
    metaDataPage3->loc = (SHALLOC_START_PAGE+2); //The page index is based on the loop and where it was found in the meta data
    metaDataPage3->spanOfPages = SHARED_PAGES; 

    debug("New metadata2 for the scheduler stack");
    debug("Spot %d thread: %d\n", (SHALLOC_START_PAGE+2), metaDataPage2->threadID);
    debug("Spot %d PageNum: %d\n", (SHALLOC_START_PAGE+2), metaDataPage2->pageNum);
    debug("Spot %d Usage: %d\n", (SHALLOC_START_PAGE+2), metaDataPage2->usage);
    debug("Spot %d Page Index: %d\n", (SHALLOC_START_PAGE+2), metaDataPage2->loc);
    debug("Spot %d Page Index: %d\n", (SHALLOC_START_PAGE+2), metaDataPage2->spanOfPages);
    debug("\n");

    //Make the fourth shalloc page
    page_header_t *metaDataPage4 = (page_header_t *)(memoryArray + (SHALLOC_START_PAGE+3) * PAGE_HEADER_SIZE); //page 2042


    metaDataPage4->threadID = -5;
    metaDataPage4->pageNum = -5; //Should always be pageNum 1 here since there was no page found for it before
    metaDataPage4->usage = -1; // Set the size it has to account for the malloc header size
    metaDataPage4->loc = (SHALLOC_START_PAGE+3); //The page index is based on the loop and where it was found in the meta data
    metaDataPage4->spanOfPages = SHARED_PAGES; 

    debug("New metadata2 for the scheduler stack");
    debug("Spot %d thread: %d\n", (SHALLOC_START_PAGE+3), metaDataPage2->threadID);
    debug("Spot %d PageNum: %d\n", (SHALLOC_START_PAGE+3), metaDataPage2->pageNum);
    debug("Spot %d Usage: %d\n", (SHALLOC_START_PAGE+3), metaDataPage2->usage);
    debug("Spot %d Page Index: %d\n", (SHALLOC_START_PAGE+3), metaDataPage2->loc);
    debug("Spot %d Page Index: %d\n", (SHALLOC_START_PAGE+3), metaDataPage2->spanOfPages);
    debug("\n");

    
    //Offset into the memoryArray
    //Still want it to write into the page
    int offset = SHALLOC_START_PAGE * PAGE_SIZE;

    //The actual pointer to the start of the free page
    //Still want to write it to the page itself
    char *pagePointer = memoryArray + offset;

    //Initialize the malloc block for this heap
    initialize(pagePointer);

    //Set the global page index tracker
    //Thread_Page_Index = pageIndex; No longer based on the page location inside of the memoryArray
    Thread_Page_Index = 1; //should be based on the location relative to the thread
    
    //findMetaDataForPageIndex(Thread_Page_Index);
    return pagePointer;
            
        
    
    
}
/*
    Allocate scheduler pages 
    Will be pages 2041, 2042 (Stack), 2043 (Ucontext Pointer)
*/
char* findSchedulerPages(int threadID, int size)
{

    //CAN LATER MODIFY TO KEEP COUNT OF HOW MANY PAGES IT ALREADY HAS
    
    //If we didn't find a sutible page for the thread before, we should make a new page for it
    //We should find the first available page
   
    if(size < STACK_SIZE) //This is for allocating the ucontext page
    {
        int spanOfPages = 1; //This is how many pages this one allocation uses/accounts for
        debug("Making ucontext page");
          
            page_header_t *metaData = (page_header_t *)(memoryArray + (SCHEDULER_PAGE_START+2) * PAGE_HEADER_SIZE);
           
                //If the page is occupied, need to check if its the same thread (For part a this should indicate NULL )
                //PART A: If the thread has a page, then return the pagePointer so malloc can try mallocing it
                //FOR PART B, HAVE THIS KEEP TRACK OF THE PAGE INDEX
                //THE POINTER TO RETURN SHOULD BE MEMORYARRAY(INDEX OFFSET)
                //THEN USE THE PAGE WE FOUND HERE TO INDICATE IT IS TAKEN/THE METADATA FOR IT
                debug("Scheduler Ucontext at page:%d\n", (SCHEDULER_PAGE_START+2));

                //Update the metadata to indicate this thread has it
                metaData->threadID = threadID;
                metaData->pageNum = 3; //Should always be pageNum 1 here since there was no page found for it before
                metaData->usage = PAGE_SIZE - HEADER_SIZE; // Set the size it has to account for the malloc header size
                metaData->loc = (SCHEDULER_PAGE_START+2); //The page index is based on the loop and where it was found in the meta data
                metaData->spanOfPages = 1;
                
                debug("New metadata for the thread");
                debug("Spot %d thread: %d\n", (SCHEDULER_PAGE_START+2), metaData->threadID);
                debug("Spot %d PageNum: %d\n", (SCHEDULER_PAGE_START+2), metaData->pageNum);
                debug("Spot %d Usage: %d\n", (SCHEDULER_PAGE_START+2), metaData->usage);
                debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START+2), metaData->loc);
                debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START+2), metaData->spanOfPages);
                debug("\n");
                //Need to initialize the new heap/page by giving it the page's space/heap
                //This page's heap will be a pointer to the first location of the page

                //Offset into the memoryArray
                //Still want it to write into the page
                int offset = (SCHEDULER_PAGE_START+2) * PAGE_SIZE;

                //The actual pointer to the start of the free page
                //Still want to write it to the page itself
                char *pagePointer = memoryArray + offset;

                //Initialize the malloc block for this heap
                initialize(pagePointer);

                //Set the global page index tracker
                //Thread_Page_Index = pageIndex; No longer based on the page location inside of the memoryArray
                Thread_Page_Index = 3; //should be based on the location relative to the thread

                //findMetaDataForPageIndex(Thread_Page_Index);
                return pagePointer;
        
    }
    else
    {
        int numAllocated = 0;
        PAGES_FOR_MALLOC = 2;
        debug("Making scheduler stack pages");
        page_header_t *metaData = (page_header_t *)(memoryArray + SCHEDULER_PAGE_START * PAGE_HEADER_SIZE);
                if(numAllocated < PAGES_FOR_MALLOC) // Checking each page to see if its free (0 means its free) and we've allocated the correct amount
                {
                    //If the page is occupied, need to check if its the same thread (For part a this should indicate NULL )
                    //PART A: If the thread has a page, then return the pagePointer so malloc can try mallocing it
                    //FOR PART B, HAVE THIS KEEP TRACK OF THE PAGE INDEX
                    //THE POINTER TO RETURN SHOULD BE MEMORYARRAY(INDEX OFFSET)
                    //THEN USE THE PAGE WE FOUND HERE TO INDICATE IT IS TAKEN/THE METADATA FOR IT
                    //debug("Found free page at location %d\n", pageIndex);

                    //Update the metadata to indicate this thread has it
                    metaData->threadID = threadID;
                    metaData->pageNum = 1; //Should always be pageNum 1 here since there was no page found for it before
                    metaData->usage = (2 * PAGE_SIZE) - HEADER_SIZE; // Set the size it has to account for the malloc header size
                    metaData->loc = (SCHEDULER_PAGE_START); //The page index is based on the loop and where it was found in the meta data
                    metaData->spanOfPages = 2;
                    
                    debug("New metadata1 for the scheduler stack");
                    debug("Spot %d thread: %d\n", (SCHEDULER_PAGE_START), metaData->threadID);
                    debug("Spot %d PageNum: %d\n", (SCHEDULER_PAGE_START), metaData->pageNum);
                    debug("Spot %d Usage: %d\n", (SCHEDULER_PAGE_START), metaData->usage);
                    debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START), metaData->loc);
                    debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START), metaData->spanOfPages);
                    debug("\n");
                    

                    page_header_t *metaDataPage2 = (page_header_t *)(memoryArray + (SCHEDULER_PAGE_START+1) * PAGE_HEADER_SIZE); //page 2042


                    metaDataPage2->threadID = threadID;
                    metaDataPage2->pageNum = 2; //Should always be pageNum 1 here since there was no page found for it before
                    metaDataPage2->usage = -1; // Set the size it has to account for the malloc header size
                    metaDataPage2->loc = (SCHEDULER_PAGE_START+1); //The page index is based on the loop and where it was found in the meta data
                    metaDataPage2->spanOfPages = 2; 

                    debug("New metadata2 for the scheduler stack");
                    debug("Spot %d thread: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->threadID);
                    debug("Spot %d PageNum: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->pageNum);
                    debug("Spot %d Usage: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->usage);
                    debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->loc);
                    debug("Spot %d Page Index: %d\n", (SCHEDULER_PAGE_START+1), metaDataPage2->spanOfPages);
                    debug("\n");

                    
                    //Offset into the memoryArray
                    //Still want it to write into the page
                    int offset = SCHEDULER_PAGE_START * PAGE_SIZE;

                    //The actual pointer to the start of the free page
                    //Still want to write it to the page itself
                    char *pagePointer = memoryArray + offset;

                    //Initialize the malloc block for this heap
                    initialize(pagePointer);

                    //Set the global page index tracker
                    //Thread_Page_Index = pageIndex; No longer based on the page location inside of the memoryArray
                    Thread_Page_Index = 1; //should be based on the location relative to the thread
                    
                    //findMetaDataForPageIndex(Thread_Page_Index);
                    return pagePointer;
                        
                    
                }
    }
}

void *shalloc(size_t size)
{

    stopiTimer();
    size = (int) size; // Added this to change the size_t to an int (like we did originally)
    debug("In shalloc");
    PAGES_FOR_MALLOC = SHARED_PAGES;

    if(size < 1 || size > (SHARED_PAGES * PAGE_SIZE) - HEADER_SIZE)
    {
        debug("Shalloc was toooo bigs :(");
        return NULL;
    }
    if (memoryArray == NULL)  //need to see if the memoryArray has been initialized/first call to shalloc
    {
        StartPageFaultHandler(); // Used to initalize the sig seg handler


        debug("Memaligning array");
        posix_memalign((void*)&memoryArray, PAGE_SIZE, HEAP_SIZE);
         if (memoryArray == NULL) {
            perror("memalign");
            exit(EXIT_FAILURE);
        } 
        
        debug("pointer for memoryArray %p\n", memoryArray);
        
        debug("MemoryArray had all zeros/was not initialized yet");
        
        //Need to initialize the first page for metadata
        initializeMetaData();
        //printMetaData();
        unlockMemoryPages();
        debug("Going to find a free page");

        //Need to check if this is scheduler malloc
        char* pageStart;
        
        pageStart = makeShallocPages(size);
        shallocLocation = pageStart;
        debug("This is scheduler stack page pointer %p\n", pageStart);
        printMetaData();
        //exit(1);
        startiTimer(INITIAL_QUANTUM);
        isthisashalloc = 1;
        shallocInitialized = 1;
        return findFreeChunk(size, pageStart);
        // printAllPageDebug();
        
        //return pageStart; //returning the actual pointer to the page and not in reference to the first memory page
        
    }
    else
    {   
        unlockMemoryPages();
        //If shalloc hasn't been called yet, need to set up the meta data for it
        if(shallocInitialized == 0)
        {
            char* pageStart;
        
            pageStart = makeShallocPages(size); //Make the meta data and get the pointer to the first page
            shallocLocation = pageStart; //Save the pointer globally so we can refer to it on subsequent calls
            debug("This is shalloc page pointer %p\n", pageStart);
            
            isthisashalloc = 1; //This is a shalloc so findFreeChunk will give out the location inside of shalloc page
            shallocInitialized = 1; //Note that shalloc stuff is already made (have meta data and pointer already)
            startiTimer(INITIAL_QUANTUM);
            return findFreeChunk(size, pageStart); 
        }
        else
        {
            isthisashalloc = 1;
            startiTimer(INITIAL_QUANTUM);
            return findFreeChunk(size, shallocLocation);
        }
        
    } 
   return NULL;

}
void *mymalloc(size_t size, char *file, int line, int lib)
{   
    stopiTimer();
    //Get the thread number from the thread library
    debug("Before num");
    int threadNumber = getCurrentThreadId();
    Thread_Page_Index = 1;
    debug("After num");
    
    debug("Getting if is stack allocation");

    
    /* if(isStackAllocation() == 1)
    {
        size += ACTIVATION_RECORD_SIZE;
    } */
   /*  if(lib == LIBRARYREQ)
    {
        threadNumber = getCurrentThreadId();
    }
    else
    {
        threadNumber = 1;
    } */
    

    size = (int) size; // Added this to change the size_t to an int (like we did originally)
    if(size == STACK_SIZE) //Accounting for the heap allocation
    {
        PAGES_FOR_MALLOC = 2;
        //size += ACTIVATION_RECORD_SIZE;
    }
    else
    {
         PAGES_FOR_MALLOC = (size / (PAGE_SIZE - PAGE_HEADER_SIZE)) + 1; //Represents the ceiling based on the size account for headers
    }
   
    
    void *location;
    header_t *head; //Used to get size of the header, don't know how to get 24 other wise: sizeof(heater_t*) = 8
    debug("Before check");
    if(size > HEAP_SIZE || size < 1) // 24 needs to be the size of the struct
    {
        debug("In size check");
        return NULL;
    } 
   //else if (checkIfAllZeros(memoryArray, HEAP_SIZE) == 1)
    if (memoryArray == NULL)  //need to see if the memoryArray has been initialized/first call to malloc
    {
        StartPageFaultHandler(); // Used to initalize the sig seg handler


        debug("Memaligning array");
        posix_memalign((void*)&memoryArray, PAGE_SIZE, HEAP_SIZE);
         if (memoryArray == NULL) {
            perror("memalign");
            exit(EXIT_FAILURE);
        } 
        //memset(memoryArray, 0, HEAP_SIZE); //make sure heap is 0'd out
        debug("pointer for memoryArray %p\n", memoryArray);
        
       /*     if (mprotect(memoryArray, HEAP_SIZE-1, PROT_NONE) == -1) {
            perror("mprotect");
            exit(EXIT_FAILURE);
        }     */
        
        
        debug("MemoryArray had all zeros/was not initialized yet");
        
        //Need to initialize the first page for metadata
        initializeMetaData();
        //printMetaData();

        //Also need to make the swapfile
        initializeSwapFile();
        //exit(1);
        //After initializing need to find a free page for this allocation
            //TODO: Make function to find a free page in metadataA
            //(also make sure the thread doesn't already have a page)
        debug("Going to find a free page");

        //Need to check if this is scheduler malloc
        char* pageStart;
        if(threadNumber == SCHED_CONTEXT)
        {
            pageStart = findSchedulerPages(threadNumber, size);
            debug("This is scheduler stack page pointer %p\n", pageStart);
            //printMetaData();
            findFreeChunk(size, pageStart);
           // printAllPageDebug();
            startiTimer(INITIAL_QUANTUM);
            return pageStart; //returning the actual pointer to the page and not in reference to the first memory page
        }
        else
        {
            pageStart = findFreePage(threadNumber, size);
        }
         

        if(pageStart == NULL) //Indicates that this thread already has a page/there is no room left
        {
            return NULL;
        }

        //Find the next free block to allocate to inside of the heap
        //Find free chunk need to update the metadata in the reserved pages
        debug("Going to malloc on the page");
        startiTimer(INITIAL_QUANTUM);
        
        return findFreeChunk(size, pageStart);
    }
    else
    {   

        debug("Regular check");
        //lockAllButThread(threadNumber);
        char* pageStart;
        if(threadNumber == SCHED_CONTEXT)
        {
            pageStart = findSchedulerPages(threadNumber, size);
            debug("This is ucontext page pointer %p\n", pageStart);
            
            //printMetaData();
            findFreeChunk(size, pageStart);
           printAllPageDebug();
            startiTimer(INITIAL_QUANTUM);
            return pageStart; //returning the actual pointer to the page and not in reference to the first memory page
        }
        else
        {
            pageStart = findFreePage(threadNumber, size);
        }

        if(pageStart == NULL) //could not find a page
        {
            return NULL;
        }
        
        debug("Before findFreeChunk thread page count %d\n", Thread_Page_Index);
        void* ptr = findFreeChunk(size, pageStart);
        debug("After malloc");
        startiTimer(INITIAL_QUANTUM);
        debug("After timer start");

        //If a thread calls malloc, we don't want to keep every page unlocked
        if(lib == THREADREQ)
        {
            lockAllButThread(threadNumber);
            debug("After lock all");
        }
        
        return ptr;
    } 
    //debug("This is x address: %p\n", location);
    startiTimer(INITIAL_QUANTUM);
     if(lib == THREADREQ)
    {   
        lockAllButThread(threadNumber);
    }
    return location;

}


    /*
    Used to print all of the metadata that currently have a thread there
    DEBUG
*/
void printMetaData()
{
    debug("Printing meta data");
     for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if((metaData + 1)->threadID != 0 || metaData->threadID != 0) // Checking each page to see if its free (0 means its free)
        {
             debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
            debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
            debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
            debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
            debug("Spot %d Span: %d\n", pageIndex, metaData->spanOfPages);
            debug("\n"); 
        }
    }
    debug("After meta data");
}

void printPageDebug(char* memory)
{   
    header_t *pointer = (header_t*)&memory[0]; //Setting pointer to the beginning of heap
    while((char*)pointer >= &memory[0] && (char*)pointer < &memory[MEMSIZE * PAGES_FOR_MALLOC])
    {
        debug("Chunk:");
        debug("Size: %d\n", pointer->sizeOfChunk);
        debug("Is free: %d\n", pointer->isFree);
        debug("Prev: %d\n", pointer->prev);
        debug("\n");
        pointer = findNextHeader(pointer);
    }
}

void printAllPageDebug()
{   

    unlockMemoryPages();
    debug("Printing All page data");
    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if((metaData + 1)->threadID != 0 || metaData->threadID != 0) // Checking each page to see if its free (0 means its free)
        {
            debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
            debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
            debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
            debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
            debug("Spot %d Span: %d\n", pageIndex, metaData->spanOfPages);
            debug("\n");
        header_t *pointer = (header_t*)&memoryArray[PAGE_SIZE*pageIndex]; //Setting pointer to the beginning of heap
        while((char*)pointer >= &memoryArray[PAGE_SIZE*pageIndex] && (char*)pointer < &memoryArray[PAGE_SIZE*pageIndex + PAGE_SIZE] && (metaData->threadID != -1 && metaData->threadID != 0) &&  metaData->usage != -1)
        {
            debug("Chunk:");
            debug("Size: %d\n", pointer->sizeOfChunk);
            debug("Is free: %d\n", pointer->isFree);
            debug("\n");
            pointer = findNextHeader(pointer);
        }
        }
    }
}
    


/*
    Will return the page that this pointer is in based
    on the inital reserved memory page (page 0) (Memory[0])
*/

 int findPageOnFault(char *pointer)
{

    //This will be the offset from the beginning of the 
    int pageNum = pointer - memoryArray;
    //After that int divide by 4096
    pageNum = pageNum / PAGE_SIZE;

    // P1   P2  P3  P4  P5  P6
    // 100 200 300 400 500 600

    //650 as pointer
    // 650 - 100 = 550

    // 550/100 = 5

    return pageNum;
}

/*
    Will return the total number of pages that a thread has inside of 
    the memoryArray
    Return: Page number relative to the entire memoryArray
*/
int findTotalPagesForThread()
{   
    int totalPages = 0;
    int threadNum = getCurrentThreadId();
    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->threadID == threadNum) 
        {
            totalPages++;
        }    
    }
    return totalPages;
}

/*
    Signal handler that will run when a thread hit a page fault
*/
static void StartPageFaultHandler(){

    PageFaultHandler.sa_flags = SA_SIGINFO;
    sigemptyset(&PageFaultHandler.sa_mask);
    PageFaultHandler.sa_sigaction = PageFaultHandlerFunction;

    if (sigaction(SIGSEGV, &PageFaultHandler, NULL) == -1)
    {
        debug("Fatal error setting up signal handler\n");
        exit(EXIT_FAILURE);    //explode!
    }


}

/*
    Goal: Load the page they are trying to access into the correct spot for that thread
*/
static void PageFaultHandlerFunction(int sig, siginfo_t *sigInfo, void *unused)
{   
    //To get the address
    stopiTimer();
    char* addressFaulted = sigInfo->si_addr;
    if(sigInfo == NULL)
    {
        debug("Is null");
    }
    else
    {
        debug("is not null");
        debug("Code is %d\n",sigInfo->si_addr);
    }
    //fprintf(stderr, "Segmentation fault occurred at address %p\n", sigInfo->si_addr);
    //debug("Got SIGSEGV at address: 0x%lx\n",(long) sigInfo->si_addr);
    debug("Address faulted = %p\n", addressFaulted);
    //int threadNum = runQueueHead->tcb->threadId;
    //Check if seg fault is inside of our array
    //printMetaData();
    //exit(1);
    int threadNum = getCurrentThreadId();
    if((char*)addressFaulted >= &memoryArray[RESERVED_PAGES+1] && (char*)addressFaulted < &memoryArray[HEAP_SIZE]) 
    {  
         //If it is, get the page index from findPageOnFault()
        int absolutePageNum = findPageOnFault(addressFaulted);
        
        debug("This is the page number that should be loaded %d\n", absolutePageNum);
        
        int pageThreadWants = (absolutePageNum - RESERVED_PAGES) + 1;
        debug("This is the page index that the thread wants %d\n", pageThreadWants);
        
        
        //Check to make sure they have this page
        char *pageToLoad = findPageForThreadGivenPageNum(threadNum, pageThreadWants);

        if(pageToLoad != NULL) //They did have the page number they wanted
        {
            //Get the metadata for the page the thread wants
            page_header_t *pageHeaderForLoad = findMetaDataGivenPageNum(threadNum, pageThreadWants);
            swapPages(absolutePageNum, pageToLoad, pageHeaderForLoad);

            //Lock the pages for all but current thread 
            lockAllButThread(threadNum);
        }
        else
        {
            debug("Thread didn't have the page number they were trying to access");
            exit(EXIT_FAILURE); 
        }

    }
    else
    {
        exit(EXIT_FAILURE);  
    }
   
        //If its in reserved memory array terminated

        //Function that counts the n
        startiTimer(INITIAL_QUANTUM);

}


/*
    Will return the page pointer for a page given the threadID and page number
*/
char* findPageGivenPageNum(int pageNum)
{
   // printMetaData();   
    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
        
        if(metaData->loc == pageNum) 
        {
            debug("Found a page at index %d which blongs to thread %d\n", pageIndex, metaData->threadID);
            //debug("Found a page number %d at index %d\n", pageNum, pageIndex);
            Thread_Page_Index = pageIndex;
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset;

            return pagePointer; //returns a pointer to the page location
        }    
    }

    //Return null if can't find a page
    //Could be for swap file check indication (???)
    return NULL;
}

char* findPageForThreadGivenPageNum(int threadNumber, int pageNum)
{
    
    for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->pageNum == pageNum && metaData->threadID == threadNumber) 
        {
            debug("Found a page for thread %d at index %d\n", threadNumber, pageIndex);
            debug("Found a page number %d for thread %d at index %d\n", pageNum, threadNumber, pageIndex);
            Thread_Page_Index = pageIndex;
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset;

            return pagePointer; //returns a pointer to the page location
        }    
    }

    //Return null if can't find a page
    //Could be for swap file check indication (???)

    //Now search the swap file for the page
    return swapFindOnPageNum(threadNumber, pageNum, 1) != NULL; //Will return NULL if it can't find it
}




/*
    Will return the page header pointer for a page given the threadID and page number
*/
page_header_t* findMetaDataGivenPageNum(int threadNumber, int pageNum)
{
    
    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->pageNum == pageNum && metaData->threadID == threadNumber) 
        {
            debug("MetaData information for thread given its pageNum");
            debug("Spot %d thread: %d\n", pageIndex, metaData->threadID);
            debug("Spot %d PageNum: %d\n", pageIndex, metaData->pageNum);
            debug("Spot %d Usage: %d\n", pageIndex, metaData->usage);
            debug("Spot %d Page Index: %d\n", pageIndex, metaData->loc);
            debug("\n");
            

            return metaData; //returns a pointer to the page location
        }    
    }

    //Return null if can't find a page
    //Could be for swap file check indication (???)
     return swapFindOnPageNum(threadNumber, pageNum, 0) != NULL; //Will return NULL if it can't find it
}

/*
    Will return a pointer to the the first free page
    Returns: Pointer to the page
*/
char* findFreePageUsingMetaData()
{
    
    for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
    {
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

        if(metaData->threadID == 0) 
        {
            debug("Found a free page at index %d\n", pageIndex);
            Thread_Page_Index = pageIndex;
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset;

            return pagePointer; //returns a pointer to the page location
        }    
    }

    //If it can't find one, evict a page and redo the search
    if(evictPageToSwapFile() != 0)
    {
        for (int pageIndex = 0; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
        {
            page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);

            if(metaData->threadID == 0) 
            {
                debug("Found a free page at index %d\n", pageIndex);
                Thread_Page_Index = pageIndex;
                //Offset into the memoryArray
                int offset = pageIndex * PAGE_SIZE;

                //The actual pointer to the start of the free page
                char *pagePointer = memoryArray + offset;

                return pagePointer; //returns a pointer to the page location
            }    
        }
    }
    else //This means we are trying to swap when the swapfile is full
    {
        //Need to return the dedicated swap file location
        int offset = SWAP_PAGE_INDEX * PAGE_SIZE;

        //The actual pointer to the start of the free page
        char *pagePointer = memoryArray + offset;

        return pagePointer; //returns a pointer to the page location of the swap spot
    }
    return NULL;
}

/*
    On a page fault, is used to swap two pages of memory
    Params: 
    absolutePageNum -> position page is being loaded into
    pageToLoad -> page heap we are loading
    headerForLoad -> head we are loading in metadata to that spot (absolutePageNum)
*/
void swapPages(int absolutePageNum, char* pageToLoad, page_header_t *headerForLoad)
{   
    unlockMemoryPages();
    debug("In swap");
    //First Check if that spot to see if it already has a page
    int pageToLoadLocation = (int)(pageToLoad - memoryArray) / PAGE_SIZE;
    //Page is already in the proper location
    
    
    char *freePage = findFreePageUsingMetaData();
    if(freePage == NULL)
    {
        debug("Swapfile was full");
        return NULL;
    }
    char *pageToEvict = findPageGivenPageNum(absolutePageNum);
    page_header_t *headerToEvict = findMetaDataForPageIndex(absolutePageNum);

    debug("This is pageLocation %d\n", pageToLoadLocation);
    debug("This is header loc for page %d\n", headerForLoad->loc);

    //If the page is already in the correct location, don't move it
    if(pageToLoadLocation == headerToEvict->loc)
    {
        //printMetaData();
        return;
    }

    int pageToEvictLocation = (int)(pageToEvict- memoryArray) / PAGE_SIZE;
    if(headerToEvict->threadID != 0) //Means there was a page loaded in that spot
    {
        //We can swap
        debug("Swapping with filled page");
        /* debug("Before swap");
        printMetaData(); */
        //Swap evicted page into the free page
        int freePageLocation = (int)(freePage - memoryArray) / PAGE_SIZE;
        page_header_t *freePageHeader = findMetaDataForPageIndex(freePageLocation);
        
        
         
        //Copy the page itself
        memcpy(freePage, pageToEvict, PAGE_SIZE);
        
        //Copy the page header 
        memcpy(freePageHeader, headerToEvict, PAGE_HEADER_SIZE);

        //Update the location inside of the memoryArray
        freePageHeader->loc = freePageLocation;

        //Move page to load into the evicted spot
        memcpy(pageToEvict, pageToLoad, PAGE_SIZE);

        //Move header to the corresponding spot
        memcpy(headerToEvict, headerForLoad, PAGE_HEADER_SIZE);

        //Update loaded page with spot it was loaded into
        headerToEvict->loc = absolutePageNum;

        //Zero out the loaded spot for malloc
        //memset(pageToLoad, 0, PAGE_SIZE);

        //Need to clear meta data for spot that was just loaded in
        memset(headerForLoad, 0, PAGE_HEADER_SIZE);        

        //Set that header to the correct location
        headerForLoad->loc = pageToLoadLocation;

        //Means we had to use the dedicated swap location to write to, need to move this to the swap file now
        if(freePageLocation == SWAP_PAGE_INDEX)
        {
            writeDataToSwap(freePage, freePageHeader);
            //Mark the free page header as a non valid location
            freePageHeader->threadID = -1;
            freePageHeader->loc = SWAP_PAGE_INDEX;
        }
        debug("After swap");
       //printMetaData();

    }
    else // Means the page was free and can just load the page into the spot without swapping
    {

        debug("Swapping with empty page");
        /* debug("Before swap");
        printMetaData(); */
        //Move page to load into the evicted spot
        memcpy(pageToEvict, pageToLoad, PAGE_SIZE);

        //Move header to the corresponding spot
        memcpy(headerToEvict, headerForLoad, PAGE_HEADER_SIZE);
        
        //Update loaded page with spot it was loaded into
        headerToEvict->loc = absolutePageNum;

        //Zero out the loaded spot for malloc
        //memset(pageToLoad, 0, PAGE_SIZE);

        //Need to clear meta data for spot that was just loaded in
        memset(headerForLoad, 0, PAGE_HEADER_SIZE);        

        //Set that header to the correct location
        headerForLoad->loc = pageToLoadLocation;
        /*  debug("After swap");
       printMetaData(); */
    }
}

/*
    Used to lock the entire heap
    Primarily for when running a new thread, want to reset state/lock all of memory
    before unlocking that thread's pages
*/
void lockMemoryPages()
{
    debug("In lock");
    //printMetaData();
    //printAllPageDebug();
    for (int pageIndex = INITIAL_INDEX; pageIndex < SCHEDULER_PAGE_START; pageIndex++)
    {   
        debug("Getting meta data");
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
        
          debug("Found free page not belonging to thread %d | index %d | at location %d\n",metaData->threadID, pageIndex, pageIndex);
            debug("Going to lock this page");  
        //Offset into the memoryArray
        int offset = pageIndex * PAGE_SIZE;

        //The actual pointer to the start of the free page
        char *pagePointer = memoryArray + offset;
        
        debug("Before mprotect");
        debug("This is page pointer %p\n", pagePointer);
        
        //want to unlock this page specifically
        if (mprotect(pagePointer, PAGE_SIZE, PROT_NONE) == -1) 
        {
            perror("mprotect");
            exit(EXIT_FAILURE);
        } 
        debug("After mprotect");
        
    }
    
}

/*
    Used to unlock the entire heap
    Primarily for library functions
    Both librarys should be able to access all data if necessary
    Moreso malloc, will need to see about mypthread.lib
*/
void unlockMemoryPages()
{

    /* for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
        //
        //Offset into the memoryArray
        int offset = pageIndex * PAGE_SIZE;

        //The actual pointer to the start of the free page
        char *pagePointer = memoryArray + offset;

        //want to unlock this page specifically
        if (mprotect(pagePointer, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) 
        {
            perror("mprotect");
            exit(EXIT_FAILURE);
        } 
        
    } */

    if (mprotect(memoryArray, HEAP_SIZE, PROT_READ | PROT_WRITE) == -1) 
        {
            perror("mprotect");
            exit(EXIT_FAILURE);
        } 
    
}

/*
    Used to unlock all of pages for a given threadID
    Will primarily be used before switching contexts in
    the threading library
*/
void unlockMemoryPagesForThread(int threadID)
{

    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);
        if(metaData->threadID == threadID) // Checking to see if page belongs to the thread
        {
            
            debug("Found free page belonging to thread %d at location %d\n", threadID, pageIndex);
            
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset;

            //want to unlock this page specifically
            if (mprotect(pagePointer, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) 
            {
                perror("mprotect");
                exit(EXIT_FAILURE);
            } 
            
        }
    }
    
}

/*
    Used to load library pages for the thread library into front of memory
    Library will need to access its memory when running so load all of it in
    Params: libraryID, id for the library thread
*/
void loadLibraryPages(int libraryID)
{
    
    //Need to unlock all of memory
    unlockMemoryPages();
    stopiTimer();
    //Need to find all of the library pages
    debug("In load library");
    //printMetaData();
    //Loop can go through all of the pages
        //When it finds one, it will reset back to the first memory page spot
        //Once it returns NULL/Can't find another page, it should be done loading the pages
    int startPageIndexForThread = 1; //need to look for the first page first
    int positionLoadTo = RESERVED_PAGES;
    for (int pageIndex = RESERVED_PAGES; pageIndex < SCHEDULER_PAGE_START; pageIndex++) //Starts at Page 1 of memory (page 5 in terms of absolute pages)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        //debug("This is startPageIndexForThread %d\n",startPageIndexForThread);
        if(metaData->threadID == libraryID && metaData->pageNum == startPageIndexForThread && pageIndex != positionLoadTo) // Checking to see if page belongs to the thread and its the pageNum that we want and that its not already in place
        {

            /* debug("Found page belonging to thread %d | Page %d | at location %d\n", libraryID, startPageIndexForThread, pageIndex);
            debug("Swapping this page with location %d and loading this page into location %d\n", pageIndex, positionLoadTo); */
            /*
                In here means we found the page and we want to swap it to the front (startPageIndex)
                Need: 
                the pageNumber we are loading into (positionLoadTo)
                the page we just found (metaData)
                metadata for the page we just found (pagePointer)
            */
            
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset; //the page we just found
            //printPageDebug(pagePointer);
            //We have all of the info to swap, so swap the page
            swapPages(positionLoadTo, pagePointer, metaData);

            /*
                After swapping, we need to update:
                    positionLoadTo by one so if a new page is found, it will load into the next slot
                    pageIndex should be reset to start from the beginning again
                    startPageIndexForThread should be incremented by one as well to look for the next page for the thread
            */
           //Basically want to do the search again but look for the next page and load it into the next spot
           positionLoadTo++;
           pageIndex = RESERVED_PAGES;
           startPageIndexForThread++;
          // debug("After swap meta data");
        }
        else if(pageIndex == positionLoadTo && metaData->threadID == libraryID && pageIndex == positionLoadTo) //alread in position look for the next
        {
            positionLoadTo++;
           pageIndex = RESERVED_PAGES;
           startPageIndexForThread++;
        }
       


    }
    startiTimer(INITIAL_QUANTUM);
}

/*
    This will line up a span of pages in relation to the first page of the span
    So if page 5 is in location 12 with a span of 3
    Pages 6 and 7 will be loaded into location 13 and 14
    Params: 
        firstInSpan: Head to the first page of a span
*/
void loadSpan(page_header_t* firstInSpan)
{
    //Need to unlock all of memory
    unlockMemoryPages();

    //Need to find all of the library pages
    debug("In load span");
    //printMetaData();
    
    int pagesLoaded = 1; //Number of pages we loaded already
    int spanPageIndex = firstInSpan->pageNum + 1; //should always start at 1 and count up to the number in the span
    int positionLoadTo = firstInSpan->loc + 1; //Want to load into the spot after the span's location
    for (int pageIndex = RESERVED_PAGES; pageIndex < TOTAL_PAGES; pageIndex++) //Starts at Page 1 of memory (page 5 in terms of absolute pages)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        
        //Checking to see if page belongs to thread, the page number is the next one we want, the page is not the first one in the span, and this page is not already in place
        if(metaData->threadID == firstInSpan->threadID && metaData->pageNum == spanPageIndex && pageIndex != firstInSpan->loc && metaData->loc != positionLoadTo) // Checking to see if page belongs to the thread and its the pageNum that we want and that its not already in place
        {

            debug("Found span page belonging to thread %d | Page %d | at location %d\n", firstInSpan->threadID, spanPageIndex, pageIndex);
            debug("Loading this page into location %d\n",  positionLoadTo);
            /*
                In here means we found the page and we want to swap it to the front (startPageIndex)
                Need: 
                the pageNumber we are loading into (positionLoadTo)
                the page we just found (metaData)
                metadata for the page we just found (pagePointer)
            */
            
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset; //the page we just found

            //We have all of the info to swap, so swap the page
            swapPages(positionLoadTo, pagePointer, metaData);

            /*
                After swapping, we need to update:
                    positionLoadTo by one so if a new page is found, it will load into the next slot
                    pageIndex should be reset to start from the beginning again
                    startPageIndexForThread should be incremented by one as well to look for the next page for the thread
            */
           //Basically want to do the search again but look for the next page and load it into the next spot
           positionLoadTo++;
           pageIndex = RESERVED_PAGES;
           spanPageIndex++;
            pagesLoaded++;

            //Means we've loaded all of span into place
            if(pagesLoaded == firstInSpan->spanOfPages)
            {
                break;
            }
           //Check to see if we've gotten all of a span
           
          // debug("After swap meta data");
        }

    }
    //printMetaData();
}


/*
    Used to load the first page of a given thread into the first memory spot
    Params:
    threadID -> thread's id to load first page
*/
void preemptiveThreadLoad(int threadID)
{
    //Need to unlock all of memory
    unlockMemoryPages();

    //Need to find all of the library pages

    //Loop can go through all of the pages
        //When it finds one, it will reset back to the first memory page spot
        //Once it returns NULL/Can't find another page, it should be done loading the pages
    int startPageIndexForThread = 1; //need to look for the first page first
    int positionLoadTo = RESERVED_PAGES;
    for (int pageIndex = RESERVED_PAGES; pageIndex < TOTAL_PAGES; pageIndex++) //Starts at Page 1 of memory (page 5 in terms of absolute pages)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        if(metaData->threadID == threadID && metaData->pageNum == startPageIndexForThread) // Checking to see if page belongs to the thread and its the pageNum that we want
        {
            
            debug("Found free page belonging to thread %d | index %d | at location %d\n", threadID, startPageIndexForThread, pageIndex);
            debug("Swapping this with free location %d and loading page into location %d\n", pageIndex, positionLoadTo);
            /*
                In here means we found the page and we want to swap it to the front (startPageIndex)
                Need: 
                the pageNumber we are loading into (positionLoadTo)
                the page we just found (metaData)
                metadata for the page we just found (pagePointer)
            */
            
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset; //the page we just found

            //We have all of the info to swap, so swap the page
            swapPages(positionLoadTo, pagePointer, metaData);

            /*
                After swapping, we need to update:
                    positionLoadTo by one so if a new page is found, it will load into the next slot
                    pageIndex should be reset to start from the beginning again
                    startPageIndexForThread should be incremented by one as well to look for the next page for the thread
            */
           //We only want to swap one page so we stop here
           break;
           
        }

    }
    
}

/*
    Will lock all of the memory pages except for the given thread's pages
    Methodology: Will go through and lock every page that doesn't belong to this thread
    Cannot just lock everything and just unlock the thread's since we need to access that data
*/
/* void lockAllButThread2(int threadID)
{   
    //Make sure everything is unlocked
    stopiTimer();
    //unlockMemoryPages();
    debug("Before locking memory pages");
    lockMemoryPages();
    debug("after locking memory pages");
    
   
   
    //printAllPageDebug();
    debug("Pointer for memoryArray %p\n",memoryArray);
    debug("Going to lock this pages but for thread %d\n", threadID); 
    //Go through all of the pages in metadata (excluding the reserved pages themselves)
    for (int pageIndex = INITIAL_INDEX; pageIndex < SCHEDULER_PAGE_START; pageIndex++) //Starts at Page 1 of memory (page 5 in terms of absolute pages)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        //debug("meta data %d\n", metaData->threadID);
        if(metaData->threadID == threadID) // Need to get all of the pages that don't belong to this thread and lock it
        {
            
             debug("Found free page belonging to thread %d | index %d | at location %d\n", metaData->threadID, pageIndex, pageIndex);
            debug("Going to unlock this page");
        
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *page = memoryArray + offset; //the page we just found

            /*
                We have the page that we want to lock, we should lock that page
            */
           /* debug("In lock page pointer: %p\n", pagePointer);
        debug("Which is on page %d of the MemoryPages (0 is first | not counting reserved)\n", ((char*)pagePointer-memoryArray)/4096); */
           //Mprotect the page so nothing else can access it
     /*       if (mprotect(page, PAGE_SIZE, PROT_WRITE | PROT_READ) == -1) 
            {
                perror("mprotect");
                exit(EXIT_FAILURE);
            } 
           
        }
    }

    startiTimer(5000);
} */
 
void lockAllButThread(int threadID)
{   
    //Make sure everything is unlocked
    stopiTimer();
    unlockMemoryPages();
  
    debug("Pointer for memoryArray %p\n",memoryArray);
    debug("Going to lock this pages but for thread %d\n", threadID); 
    //Go through all of the pages in metadata (excluding the reserved pages themselves)
    for (int pageIndex = INITIAL_INDEX; pageIndex < SCHEDULER_PAGE_START; pageIndex++) //Starts at Page 1 of memory (page 5 in terms of absolute pages)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        //debug("meta data %d\n", metaData->threadID);
        if(metaData->threadID != threadID) // Need to get all of the pages that don't belong to this thread and lock it
        {
            
             /* debug("Found free page belonging to thread %d | index %d | at location %d\n", metaData->threadID, pageIndex, pageIndex);
            debug("Going to lock this page"); */
        
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *page = memoryArray + offset; //the page we just found

            /*
                We have the page that we want to lock, we should lock that page
            */
           /* debug("In lock page pointer: %p\n", pagePointer);
        debug("Which is on page %d of the MemoryPages (0 is first | not counting reserved)\n", ((char*)pagePointer-memoryArray)/4096); */
           //Mprotect the page so nothing else can access it
           if (mprotect(page, PAGE_SIZE, PROT_NONE) == -1) 
            {
                perror("mprotect");
                exit(EXIT_FAILURE);
            } 
           
        }
    }

    startiTimer(INITIAL_QUANTUM);
}



void loadThreadStack(int libraryID)
{
    
    //Need to unlock all of memory
    unlockMemoryPages();
    stopiTimer();
    //Need to find all of the library pages
    debug("In load stack");
    //printMetaData();
    //Loop can go through all of the pages
        //When it finds one, it will reset back to the first memory page spot
        //Once it returns NULL/Can't find another page, it should be done loading the pages
    int startPageIndexForThread = 1; //need to look for the first page first
    int positionLoadTo = RESERVED_PAGES;
    for (int pageIndex = RESERVED_PAGES; pageIndex < SCHEDULER_PAGE_START; pageIndex++) //Starts at Page 1 of memory (page 5 in terms of absolute pages)
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        
        if(metaData->threadID == libraryID && metaData->pageNum == startPageIndexForThread && pageIndex != positionLoadTo) // Checking to see if page belongs to the thread and its the pageNum that we want and that its not already in place
        {

            //debug("Found page belonging to thread %d | Page %d | at location %d\n", libraryID, startPageIndexForThread, pageIndex);
            //debug("Swapping this page with location %d and loading this page into location %d\n", pageIndex, positionLoadTo);
            /*
                In here means we found the page and we want to swap it to the front (startPageIndex)
                Need: 
                the pageNumber we are loading into (positionLoadTo)
                the page we just found (metaData)
                metadata for the page we just found (pagePointer)
            */
            
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset; //the page we just found
            //printPageDebug(pagePointer);
            //We have all of the info to swap, so swap the page
            swapPages(positionLoadTo, pagePointer, metaData);

            /*
                After swapping, we need to update:
                    positionLoadTo by one so if a new page is found, it will load into the next slot
                    pageIndex should be reset to start from the beginning again
                    startPageIndexForThread should be incremented by one as well to look for the next page for the thread
            */
           //Basically want to do the search again but look for the next page and load it into the next spot
           positionLoadTo++;
           pageIndex = RESERVED_PAGES;
           startPageIndexForThread++;
          // debug("After swap meta data");
          
          if(startPageIndexForThread > ((STACK_SIZE + HEADER_SIZE)/ PAGE_SIZE) + 1) //Means we'eve loaded all stack pages and the ucontext page
          {
            break;
          }
        }

    }
    startiTimer(INITIAL_QUANTUM);
}

void initializeSwapFile()
{
    swapFileFD = open("swapfile2.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);

    debug("This is FD for swap %d\n",swapFileFD);
    if(swapFileFD > 0)
    {
        //No meta data for it since the size 
        debug("Swap file created");
    } 
    else
    {
        debug("Error making swap file");
        exit(1);
    }
}

/*
    This function will find a free page in the memoryArray and put it in the swap file
    Page -> Has to be a span of one
    Methodology: Look through the meta data of memoryArray start after the thread stack and ucontext pages (using an aribtrary position like page 50)
    Go through the meta data and look for the first page with a span of 1
    This page gets written to the swap file
    The page gets zero'd out
    The meta data for that page is reset to default of all 0s (except the location)
    PARAM: isaload: used to determine if it should use a swap spot
    RETURN: The page location that is now free
    RETURNS 0 if it couldn't free up a spot (Swap file is full)
*/
int evictPageToSwapFile(int isaload)
{   
    unlockMemoryPages();
    //Get the thread num
    int threadNum = getCurrentThreadId();

    //Start at page 50
    for (int pageIndex = EVICT_START_PAGE; pageIndex < SCHEDULER_PAGE_START; pageIndex++) //Starts at Page 50 of memory
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        
        //Look for a single page, not a span of multiple so we can keep track of them
        if(metaData->spanOfPages == 1 && metaData->threadID != threadNum) // Checking to see if page is a single page
        {
            
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset; //the page we just found
            
            //We have the page and meta data to write to the swap file
            if(writeDataToSwap(pagePointer, metaData) == 1)
            {
                //Update metadata
                metaData->threadID = 0; //Back to 0 as an initial state
                metaData->pageNum = 0; 
                metaData->usage = 0; 
                metaData->spanOfPages = 0;

                memset(pagePointer, 0, PAGE_SIZE);

                return pageIndex;
            }
            else
            {   
                //Means the swap file is full, but we are trying to load a page
                if(isaload == 1)
                {

                }
                return 0;
            }
            
        }
    }

    return 0;
}

/*
    Will write the given page and meta data to the swap file in a free location
    Methodology: Index through the swap file at PAGE_SIZE+METADATA size and look for a free spot
    The upper bound will be a determined size (4086) based on the page size and meta data size
    If it finds a free spot, write it into the file and return 1 indicating it was written successfully
    RETURN 0: If there was no free spot in the swap file
*/
int writeDataToSwap(char* pagePointer, page_header_t* metadata)
{   
    //Reset the pointer to the beginning of the file
    int offset = lseek(swapFileFD, 0, SEEK_SET);
    if(offset == -1)
    {
        debug("%s\n",strerror(errno));
        exit(1);
    }
    char buffer[2];
    for(int pageIndex = 0; pageIndex < SWAP_PAGE_MAX; pageIndex++)
    {   
        buffer[0] = 0; //Reset the buffer
       // debug("This is pageIndex*SWAPPAGESIZE %d\n",(pageIndex*SWAP_PAGE_SIZE));
        
        //For debug
        //debug("This is the swap file offset %d for swap page index %d\n", offset, pageIndex);
        
        //Read the first byte of the metadata for this page (threadID)
        
        
        if(read(swapFileFD, buffer, 1) == 0) //Means this spot wasn't written to yet
        {
            //We found a spot that is not occupied by a page since no thread can be 0
            if((short)buffer[0] == 0)
            {
                //Go back to the beginning of the page
                lseek(swapFileFD, -1, SEEK_CUR);
                
                // Write the meta data to the page
                int w = write(swapFileFD, metadata, sizeof(PAGE_HEADER_SIZE));
                //debug("This was write amount %d\n",w);
                if(w == -1)
                {
                    debug("%s\n",strerror(errno));
                    exit(1);
                }

                //Debug
                /* lseek(swapFileFD, PAGE_HEADER_SIZE, SEEK_CUR);
                page_header_t *meta; 
                read(swapFileFD, meta, PAGE_HEADER_SIZE); */
            
                //Write the page to the page
                w =  write(swapFileFD, pagePointer, PAGE_SIZE);
                //debug("This was write amount %d\n",w);
                if(w == -1)
                {
                    debug("%s\n",strerror(errno));
                    exit(1);
                }
                
                return 1;
            }
        }
        else
        {   
            //debug("here");
            //debug("This was buff %d\n", (short)buffer[0]);
            if((short)buffer[0] == 0) //This means a no thread is in this spot since it can't be 0
            {
                //Go back to the beginning of the page
                lseek(swapFileFD, -1, SEEK_CUR);
                
                // Write the meta data to the page
                int w = write(swapFileFD, metadata, sizeof(PAGE_HEADER_SIZE));
                //debug("This was write amount %d\n",w);
                if(w == -1)
                {
                    debug("%s\n",strerror(errno));
                    exit(1);
                }

                //Debug
                /* lseek(swapFileFD, PAGE_HEADER_SIZE, SEEK_CUR);
                page_header_t *meta; 
                read(swapFileFD, meta, PAGE_HEADER_SIZE); */
            
                //Write the page to the page
                w =  write(swapFileFD, pagePointer, PAGE_SIZE);
                //debug("This was write amount %d\n",w);
                if(w == -1)
                {
                    debug("%s\n",strerror(errno));
                    exit(1);
                }
                
                return 1;
            }
        }

        //If the page wasn't free, need to go back one spot
        //Go back to the beginning of the page
        lseek(swapFileFD, -1, SEEK_CUR);

        //Set the cursor to the next page
        offset = lseek(swapFileFD, PAGE_SIZE, SEEK_CUR);
        //debug("This is the offset %d\n", offset);
        
    }

    return 0;

}

/*
    Will search the swap file for a page given the thread number and page number
    Can be specified to give either the page or the meta data to reuse the function
    Methodology: Search each first bit of the meta data for each page
        If it matches the threadNum then we check the pageNum
            If we get the pageNum we want, return the head or page based on the param
    PARAMS: 0 for head
            1 for page
    RETURNS NULL if can't find
*/

void* swapFindOnPageNum(int threadNum, int pageNum, int headORPage)
{
    //Reset the pointer to the beginning of the file
    int offset = lseek(swapFileFD, 0, SEEK_SET);
    if(offset == -1)
    {
        debug("%s\n",strerror(errno));
        exit(1);
    }
    char buffer[2];
    for(int pageIndex = 0; pageIndex < SWAP_PAGE_MAX; pageIndex++)
    {   
        buffer[0] = 0; //Reset the buffer
        debug("This is pageIndex*SWAPPAGESIZE %d\n",(pageIndex*SWAP_PAGE_SIZE));
        
        //For debug
        debug("This is the swap file offset %d for swap page index %d\n", offset, pageIndex);
        
        //Read the first byte of the metadata for this page (threadID)
        
        
        if(read(swapFileFD, buffer, 2) == 0) //Means this spot wasn't written to yet
        {
            //We found a spot that is not occupied by a page since no thread can be 0
            if((short)buffer == 0)
            {
                debug("Found nothing in this spot");
            }
        }
        else //Read actual data from the spot
        {   
            debug("This was buff %d\n", (short)buffer);
            if((short)buffer == threadNum) //This means we found a page for the thread
            {
                memset(buffer, 0, 2);
                //We then want to check if the pageNum matches
                if(read(swapFileFD, buffer, 2) != 0) 
                {
                    if((short)buffer == pageNum) //Means we found the correct pageNum and this is the page we want
                    {
                        //Save the page and header before trying to evict a free page so we keep the pointer here
                        lseek(swapFileFD, -4, SEEK_CUR); //Go back the 4 spots of the 2 shorts

                        page_header_t *metaData;

                        if(read(swapFileFD, metaData, sizeof(metaData)) != sizeof(metaData))
                        {
                            perror("Read error");
                            exit(1);
                        }
                        debug("Header for page in from swap file \n");
                        debug("Spot %d thread: %d\n", metaData->loc, metaData->threadID);
                        debug("Spot %d PageNum: %d\n", metaData->loc, metaData->pageNum);
                        debug("Spot %d Usage: %d\n", metaData->loc, metaData->usage);
                        debug("Spot %d Page Index: %d\n", metaData->loc, metaData->loc);
                        debug("Spot %d Span: %d\n", metaData->loc, metaData->spanOfPages);
                        debug("\n");

                        char* pagePointer;
                        if(read(swapFileFD, pagePointer, PAGE_SIZE) != PAGE_SIZE)
                        {
                            perror("Read error");
                            exit(1);
                        }

                        if(evictPageToSwapFile != 0) //Means we evicted a page and can write to the memoryArray
                        {
                            //Put the data in the memoryArray
                            putPageAndMetaInMemory(metaData, pagePointer);

                            if(headORPage == 0)
                            {
                                return metaData;
                            }
                            else
                            {
                                return pagePointer;
                            }
                        }
                        else
                        {
                            debug("Swap was full");
                            return NULL;
                        }
                    }
                }
                else
                {
                    debug("Read error");
                    exit(1);
                }
            }
        }
        //If the page wasn't free, need to go back one spot
        //Go back to the beginning of the page
        lseek(swapFileFD, -2, SEEK_CUR);

        //Set the cursor to the next page
        offset = lseek(swapFileFD, PAGE_SIZE, SEEK_CUR);
        debug("This is the offset %d\n", offset);
        
    }

    return NULL;
}

/*
    Puts a page and meta data into the freed up spot by evict
    Methodology: Go through the meta data to look for the evicted spot
    
*/
void putPageAndMetaInMemory(page_header_t *meta, char* page)
{

    for (int pageIndex = INITIAL_INDEX; pageIndex < SCHEDULER_PAGE_START; pageIndex++) 
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        
        //Look for a single page
        if(metaData->threadID == 0) // Checking to see if page is empty
        {
            
            //Set the meta data to the given para
            memcpy(metaData, meta, sizeof(page_header_t));
            metaData->loc = pageIndex; // Keep the location that it was put in
            //Offset into the memoryArray
            int offset = pageIndex * PAGE_SIZE;

            //The actual pointer to the start of the free page
            char *pagePointer = memoryArray + offset; //the page we just found
            
            //set the pagePointer to the page from the param;
            memcpy(pagePointer, page, PAGE_SIZE);
            
        }
    }


}

void freeALL()
{
    /*
    unlockMemoryPages();

    for (int pageIndex = 0; pageIndex < TOTAL_PAGES; pageIndex++) 
    {   
        page_header_t *metaData = (page_header_t *)(memoryArray + pageIndex * PAGE_HEADER_SIZE);  //metadata for the page we just found
        
        //Offset into the memoryArray
        int offset = pageIndex * PAGE_SIZE;

        //The actual pointer to the start of the free page
        char *pagePointer = memoryArray + offset; //the page we just found
        
        //Free the meta data
        memset(metaData, 0, sizeof(page_header_t));
        
        //Free the page pointer
        memset(pagePointer, 0, PAGE_SIZE);
            
    }
    */
        
        stopiTimer();
        debug("Memaligning array");
        posix_memalign((void*)&memoryArray, PAGE_SIZE, HEAP_SIZE);
         if (memoryArray == NULL) {
            perror("memalign");
            exit(EXIT_FAILURE);
        } 
        //memset(memoryArray, 0, HEAP_SIZE); //make sure heap is 0'd out
        debug("pointer for memoryArray %p\n", memoryArray);
        
       /*     if (mprotect(memoryArray, HEAP_SIZE-1, PROT_NONE) == -1) {
            perror("mprotect");
            exit(EXIT_FAILURE);
        }     */
        
        
        debug("MemoryArray had all zeros/was not initialized yet");
        
        //Need to initialize the first page for metadata
        initializeMetaData();
        //printMetaData();

        //Also need to make the swapfile
        initializeSwapFile();
        restInits();
        shallocInitialized = 0;

}

