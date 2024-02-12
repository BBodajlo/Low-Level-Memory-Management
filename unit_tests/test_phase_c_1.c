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

#define TEST_1_NUM_THREADS 4
pthread_mutex_t test_1_mutex;
atomic_int test_1_thread_count = ATOMIC_VAR_INIT(TEST_1_NUM_THREADS);
atomic_int test_1_page_count = ATOMIC_VAR_INIT(0);
void test_1_thread_func(int *s) {
    int thread_num = atomic_fetch_add(&test_1_thread_count, 1);
    // Until malloc returns null
    while (1) {
        // Malloc a page-worth of memory
        void *ptr = malloc(3000);


        if (ptr == NULL) {
            printf("\b\b\b\b\b\b\b\b\b\bThread %d received null\nLoading   ", thread_num);
            break;
        }
        atomic_fetch_add(&test_1_page_count, 1);
        //printf("Thread %d malloced a page, %d\n", thread_num, test_1_page_count);
    }
    atomic_fetch_sub(&test_1_thread_count, 1);
    pthread_exit(NULL);
}
void test_1_loading(atomic_int* count) {
    int i = 0;
    char* dots2[8] = {"⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷" };
    char dots[4] = {'|', '/', '-', '\\'};
    printf("Loading   ");
    int last_count = *count;
    while (count > 0) {
        printf("\b\b%s ", dots2[i]);
        fflush(stdout);
        usleep(100000);
        i = (i + 1) % 8;

    }
    printf(" ");
    fflush(stdout);
    thrd_exit(0);
}
/**
 * Test 1: Max out pages
 * Expected: The maximum number of pages available
 */
void max_number_of_pages(void) {
    msg("Test 1: Max out pages");

    pthread_mutex_init(&test_1_mutex, NULL);

    pthread_t threads[TEST_1_NUM_THREADS];
    for (int i = 1; i <= TEST_1_NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, (void *)test_1_thread_func, NULL);
    }
    printf("Created all threads\n");
    // use different threading library to print loading dots
    thrd_t loading_dots_thread;
    thrd_create(&loading_dots_thread, (void *)test_1_loading, &test_1_thread_count);
    thrd_detach(loading_dots_thread);
    for (int i = 1; i <= TEST_1_NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf("Joined thread %d\n", i);
    }
    printf("Joined all threads\n");

    printf("Number of pages: %d\n", test_1_page_count);
    pthread_mutex_destroy(&test_1_mutex);
    printf("Heap part %d\n", TOTAL_PAGES - RESERVED_PAGES - SHARED_PAGES - SCHEDULER_PAGES);
    printf("Swapfile part %lu\n",  (SWAP_SIZE / (PAGE_SIZE + sizeof(page_header_t))) );
    printf("thread's pages part: %d\n", (TEST_1_NUM_THREADS ) * INITIAL_THREAD_PAGES);
    int expected = (TOTAL_PAGES - RESERVED_PAGES - SHARED_PAGES - SCHEDULER_PAGES) + (SWAP_SIZE / (PAGE_SIZE + sizeof(page_header_t))) - ((TEST_1_NUM_THREADS ) * INITIAL_THREAD_PAGES);
    printf("Number of expected %d\n", expected);
    TEST_ASSERT_EQUAL_INT(expected, test_1_page_count);
}

int main(void) {
    UNITY_BEGIN();
    msg("Phase C: Swap File (Part 1)");
    RUN_TEST(max_number_of_pages); 
    msg("Finished running tests");
    return UNITY_END();
}

