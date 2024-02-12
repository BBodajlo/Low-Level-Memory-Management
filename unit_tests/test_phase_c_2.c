#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <threads.h>
#include <stdatomic.h>
#include "Unity/src/unity.h"
#include "../umalloc.h"

void msg(char *s) {
    printf("\n");
    int i = 0;
    while (s[i] != '\0') {
        printf("%c", (i %2 == 0) ? '-' : '=');
        i++;
    }
    printf("\n%s\n", s);
    i = 0;
    while (s[i] != '\0') {
        printf("%c", (i %2 == 0) ? '-' : '=');
        i++;
    }
    printf("\n");
}

void setUp(void) {
    msg("Setting up tests");
}

void tearDown(void) {
    msg("Tearing down tests");
}

atomic_int test_2_thread_count = ATOMIC_VAR_INIT(0);
void test_2_thread_func(void) {
    int thread_num = atomic_fetch_add(&test_2_thread_count, 1);
    int i = 0;
    for (i = 0; i < 512; i++) {
        //puts("mallocing");
        void *ptr = malloc(3000);
        if (ptr == NULL) {
            printf("Thread %d received null\n", thread_num);
            break;
        }
    }
    pthread_exit(NULL);
}
void test_2_thread_func_1()
{
    for(int i = 0; i < 700; i++)
    {
        int *a = malloc(3000);
    }
    pthread_exit(NULL);
}
/**
 * Test 2: Malloc after filling the heap
 * Expected: Pointer is not null
 */
void malloc_after_filling_heap(void) {
    msg("Test 2: Malloc after filling the heap");
    pthread_t threadA;
    int returnVal = pthread_create(&threadA, NULL, (void*)test_2_thread_func_1, (void*)'a'); 
    
    pthread_t threadB;
    returnVal = pthread_create(&threadB, NULL, (void*)test_2_thread_func_1, (void*)'b');   
    
    pthread_t threadC;
    returnVal = pthread_create(&threadC, NULL, (void*)test_2_thread_func_1, (void*)'c'); 
      
    pthread_join(threadA, NULL);
    puts("Thread 1 done mallocing");
    pthread_join(threadB, NULL);
    puts("Thread 2 done mallocing");
    pthread_join(threadC, NULL);
    puts("Thread 3 done mallocing");

    int *value = malloc(3000);
    printf("This is pointer for c %p\n",value);
    *value = 4;
    printf("This is c value %d\n", *value);
    puts("In main exiting");
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(4, 4);
}

int main(void) {
    UNITY_BEGIN();
    msg("Phase C: Swap File (Part 2)");
    RUN_TEST(malloc_after_filling_heap); 
    msg("Finished running tests");
    return UNITY_END();
}

