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

atomic_int test_1_thread_num = ATOMIC_VAR_INIT(0);
int* test_1_data;
void test_1_thread_func(void) {
    int thread_id = atomic_fetch_add(&test_1_thread_num, 1);
    (*test_1_data)++;
    printf("Thread %d: test_1_data: %d\n", thread_id, *test_1_data);
    pthread_exit(NULL);
}
/**
 * Test 1: Data change is shared
 * Expected: Values are the same across threads
 */
void test_data_shared(void) {
    msg("Test 1: Data change is shared\n\tExpected: Values are the same across threads");
    test_1_data = shalloc(sizeof(int));
    printf("test_1_data in main: %d\n", *test_1_data);
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, (void*)test_1_thread_func, NULL);
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("test_1_data in main: %d\n", *test_1_data);
    TEST_ASSERT_EQUAL(4, *test_1_data);

}

int main(void) {
    UNITY_BEGIN();
    msg("Phase D: Shalloc (Part 2)");
    RUN_TEST(test_data_shared);
    msg("Finished running tests");
    return UNITY_END();
}

