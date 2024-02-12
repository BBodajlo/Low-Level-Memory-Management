#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Unity/src/unity.h"
#include "../umalloc.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

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

pthread_mutex_t test_1_mutex;
void* test_1_pointers[2];
char test_1_flag = 0;
void test_1_thread_func(void) {
    void *ptr = malloc(1);
    *(char *)ptr = 'a' + test_1_flag;
    printf("Thread %d: %p\n", test_1_flag, ptr);
    pthread_mutex_lock(&test_1_mutex);
    test_1_pointers[test_1_flag] = ptr;
    test_1_flag++;
    pthread_mutex_unlock(&test_1_mutex);
    while (test_1_flag < 3);
    printf("Thread %d: %c @ %p\n", *(char *)ptr - 'a', *(char *)ptr, ptr);
    free(ptr);
    pthread_exit(NULL);
}
/**
 * Test 1: Test two threads and pointer positions
 * Expected: Same pointer, different data
 */
void test_pointer_position(void) {
    msg("Test 1: Test two threads and pointer positions\n\tInput: Two identical malloc calls from two threads\n\tExpected: Same pointer, different data");
    pthread_t thread1, thread2;
    pthread_mutex_init(&test_1_mutex, NULL);
    pthread_create(&thread1, NULL, (void *)test_1_thread_func, NULL);
    pthread_create(&thread2, NULL, (void *)test_1_thread_func, NULL);
    while (test_1_flag < 2);
    pthread_mutex_destroy(&test_1_mutex);
    int diff = MAX(test_1_pointers[0], test_1_pointers[1]) - MIN(test_1_pointers[0], test_1_pointers[1]);
    printf("ptr1: %p, ptr2: %p, diff: %d\n", test_1_pointers[0], test_1_pointers[1], diff);
    test_1_flag++;
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    TEST_ASSERT_EQUAL_INT(diff, 0);
}

int test_2_flag = 0;
int test_2_page_fault = 0;
void test_2_thread_func(char *c) {
    printf("Thread %c: Mallocing 6000 bytes\n", *c);
    void *ptr = malloc(6000);
    int i = 0;
    while (test_2_flag < 2) {
        *(char *)(ptr + i) = *c + i % 26;
        i++;
        if (i == 6000) {
            i = 0;
        }
    }
    printf("Thread %c: Char at %p: %c, Char at %p + 5999: %c\n", *c, ptr, *(char *)ptr, ptr, *(char *)(ptr + 5999));
    free(ptr);
    pthread_exit(NULL);
}
/**
 * Test 2: Page fault handler
 * Expected: Page fault occurs, but data is still there
 */
void test_page_fault(void) {
    msg("Test 2: Page fault handler\n\tInput: Two threads malloc 6000 bytes, then write to them\n\tExpected: Page fault occurs, but data is still there");
    pthread_t thread1, thread2;
    char c = 'a';
    char d = 'b';
    pthread_create(&thread1, NULL, (void *)test_2_thread_func, &c);
    pthread_create(&thread2, NULL, (void *)test_2_thread_func, &d);
    sleep(2);
    test_2_flag = 2;
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

}

void* test_3_pointers[4];
void test_3_thread_func(int *c) {
    int thread_num = *c;
    int** ptr = malloc(20 * sizeof(int*));
    int i = 0;
    test_3_pointers[thread_num * 2] = ptr;
    while (i < 20) {
        ptr[i] = (int *) malloc(3000);
        *(ptr[i]) = thread_num * 20 + i;
        /* printf("Thread %d: Page %d @ %p: %d\n", thread_num, i, ptr[i], *(ptr[i])); */
        i++;
    }
    test_3_pointers[thread_num * 2 + 1] = ptr[i];
    pthread_exit(NULL);
}
/**
 * Test 3: Multiple allocations across threads
 * Expected: Variable change is reflected; Pointers for threads are the same
 */
void test_multiple_allocations(void) {
    msg("Test 3: Multiple allocations across threads\n\tInput: Two threads allocate variables, access and change them\n\tExpected: Page fault occurs, but data is still there");
    pthread_t thread1, thread2;
    int c = 0;
    int d = 1;
    pthread_create(&thread1, NULL, (void *)test_3_thread_func, &c);
    pthread_create(&thread2, NULL, (void *)test_3_thread_func, &d);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("Thread 0: First %p, last %p\n", test_3_pointers[0], test_3_pointers[1]);
    printf("Thread 1: First %p, last %p\n", test_3_pointers[2], test_3_pointers[3]);

    TEST_ASSERT_EQUAL_INT(test_3_pointers[0], test_3_pointers[2]);
    TEST_ASSERT_EQUAL_INT(test_3_pointers[1], test_3_pointers[3]);
}

int main(void) {
    UNITY_BEGIN();
    msg("Phase B: Virtual(ish) Memory");
    RUN_TEST(test_pointer_position);
    RUN_TEST(test_page_fault);
    RUN_TEST(test_multiple_allocations);
    msg("Finished running tests");
    return UNITY_END();
}

