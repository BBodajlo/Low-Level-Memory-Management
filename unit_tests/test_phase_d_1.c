#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
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

/**
 * Test 1: Bounds checking
 * Expected: Shalloc returns NULL when size is 0, or when size is greater than the remaining memory
 */
void test_shalloc_bounds(void) {
    msg("Test 1: Bounds checking\n\tExpected: Shalloc returns NULL when size is 0, or when size is greater than the remaining memory");
    void* ptr = shalloc(0);
    TEST_ASSERT_NULL(ptr);
    ptr = shalloc(PAGE_SIZE * 5);
    TEST_ASSERT_NULL(ptr);
}

atomic_int test_2_thread_num = ATOMIC_VAR_INIT(0);
void* test_2_pointers[4];
void test_2_thread_func(void) {
    int thread_id = atomic_fetch_add(&test_2_thread_num, 1);
    test_2_pointers[thread_id] = shalloc(4);
    *(int*)test_2_pointers[thread_id] = thread_id;

    pthread_exit(NULL);
}
/**
 * Test 2: Pointer is different across threads
 * Expected: Pointer is different across threads
 */
void test_ptr_different(void) {
    msg("Test 2: Pointer is different across threads\n\tExpected: Pointer is different across threads");
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, (void*)test_2_thread_func, NULL);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    for (int i = 0; i < 4; i++) {
        for (int j = i+1; j < 4; j++) {
            TEST_ASSERT_NOT_EQUAL(test_2_pointers[i], test_2_pointers[j]);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    msg("Phase D: Shalloc (Part 1)");
    RUN_TEST(test_shalloc_bounds);
    RUN_TEST(test_ptr_different);
    msg("Finished running tests");
    return UNITY_END();
}

