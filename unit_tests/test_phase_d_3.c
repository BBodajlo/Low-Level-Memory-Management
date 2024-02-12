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

int* test_1_data;
void test_1_shalloc_func(void) {
    test_1_data = shalloc(sizeof(int));
    *test_1_data = 5;
    printf("test_1_data in allocating thread: %d\n", *test_1_data);
    pthread_exit(NULL);
}
void test_1_free_func(void) {
    free(test_1_data);
    printf("test_1_data in freeing thread after free: %d\n", *test_1_data);
    pthread_exit(NULL);
}
/**
 * Test 1: Data change is reflected from free
 * Expected: Pointer is NULL
 */
void test_data_free(void) {
    msg("Test 1: Data change is reflected from free\n\tExpected: Pointer is NULL");
    pthread_t thread;
    pthread_create(&thread, NULL, (void*)test_1_shalloc_func, NULL);
    pthread_join(thread, NULL);
    printf("test_1_data in main: %d\n", *test_1_data);
    TEST_ASSERT_EQUAL(5, *test_1_data);
    pthread_create(&thread, NULL, (void*)test_1_free_func, NULL);
    pthread_join(thread, NULL);
    printf("test_1_data in main: %d\n", *test_1_data);
    TEST_ASSERT_EQUAL(0, *test_1_data);
}

int main(void) {
    UNITY_BEGIN();
    msg("Phase D: Shalloc (Part 3)");
    RUN_TEST(test_data_free);
    msg("Finished running tests");
    return UNITY_END();
}

