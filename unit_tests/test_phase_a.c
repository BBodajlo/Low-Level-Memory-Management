#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Unity/src/unity.h"
#include "../umalloc.h"

uint debug_flag = 0;

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
    debug_flag = 0;
    umalloc_set_debug(&debug_flag);
}

void tearDown(void) {
    msg("Tearing down tests");
    umalloc_stop_debug();
}


/**
 * Test 1: Test an invalid input to malloc
 * Expected: NULL
 */
void test_malloc_invalid_input(void) {
    msg("Test 1: Test an invalid input to malloc\n\tInput: 0\n\tExpected: NULL");
    void *ptr = malloc(0);
    TEST_ASSERT_NULL(ptr);
}

/**
 * Test 2: Test a valid input to malloc
 * Expected: A pointer to a block of memory
 */
void test_malloc_valid_input(void) {
    msg("Test 2: Test a valid input to malloc\n\tInput: 1\n\tExpected: A pointer to a block of memory");
    void *ptr = malloc(1);
    free(ptr);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_NOT_EQUAL(0, ptr);
}

/**
 * Test 3: Test an invalid input to free
 * Expected: Nothing happens
 */
void test_free_invalid_input(void) {
    msg("Test 3: Test an invalid input to free\n\tInput: 0\n\tExpected: Nothing happens");
    free(0);
    TEST_ASSERT_BITS_HIGH(DEBUG_FREE_FAIL, debug_flag);
}

/**
 * Test 4: Test an invalid input to free
 * Expected: Expected: First Free - Nothing; Second Free - 0
 */
void test_free_invalid_pointer(void) {
    msg("Test 4: Test an invalid input to free\n\tInput: Pointer from malloc + 2\n\tExpected: First Free - Nothing; Second Free - 0");
    int *ptr = malloc(4);
    *ptr = 4;
    free(ptr + 2);
    TEST_ASSERT_EQUAL(4, *ptr);
    printf("debug_flag: %d\n", debug_flag);
    TEST_ASSERT_BITS_HIGH(DEBUG_FREE_FAIL, debug_flag);
    debug_flag = 0;
    free(ptr);
    TEST_ASSERT_EQUAL(0, *ptr);
    printf("debug_flag: %d\n", debug_flag);
    TEST_ASSERT_BITS_LOW(DEBUG_FREE_FAIL, debug_flag);
}

/**
 * Test 5: Test a valid input to free
 * Expected: PTR is 0
 */
void test_free_valid_input(void) {
    msg("Test 5: Test a valid input to free\n\tInput: 1\n\tExpected: PTR is 0");
    int *ptr = malloc(4);
    *ptr = 5;
    TEST_ASSERT_EQUAL(5, *ptr);
    free(ptr);
    TEST_ASSERT_EQUAL(0, *ptr);
    printf("debug_flag: %d\n", debug_flag);
    TEST_ASSERT_BITS_LOW(DEBUG_FREE_FAIL, debug_flag);
}

/**
 * Test 6: Double free
 * Expected: PTR still 0
 */
void test_double_free(void) {
    msg("Test 6: Double free\n\tInput: 1\n\tExpected: PTR still 0 and Error Message");
    int *ptr = malloc(4);
    *ptr = 5;
    TEST_ASSERT_EQUAL(5, *ptr);
    free(ptr);
    TEST_ASSERT_EQUAL(0, debug_flag);
    free(ptr);
    TEST_ASSERT_EQUAL(0, *ptr);
    printf("debug_flag: %d\n", debug_flag);
    TEST_ASSERT_BITS_HIGH(DEBUG_DOUBLE_FREE, debug_flag);
}

/**
 * Test 7: Malloc very large size
 * Expected: Some pointer
 */
void test_malloc_large_size(void) {
    msg("Test 7: Malloc very large size\n\tInput: PAGE_SIZE + (PAGE_SIZE % 4) + 4\n\tExpected: Some pointer");
    int *ptr = malloc(PAGE_SIZE + (PAGE_SIZE % 4) + 4);
    memset(ptr, 0, PAGE_SIZE + (PAGE_SIZE % 4) + 4);
    free(ptr);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_NOT_EQUAL(0, ptr);
}

/**
 * Test 8: Malloc in order, free in order
 * Expected: 3 different pointers
 */
void test_malloc_free_in_order(void) {
    msg("Test 8: Malloc in order, free in order\n\tInput: 3 * 4 bytes\n\tExpected: 3 different pointers, all freed");
    int *ptr1 = malloc(4);
    int *ptr2 = malloc(4);
    int *ptr3 = malloc(4);
    *ptr1 = 1;
    *ptr2 = 2;
    *ptr3 = 3;
    free(ptr1);
    free(ptr2);
    free(ptr3);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);
    TEST_ASSERT_NOT_EQUAL(0, ptr1);
    TEST_ASSERT_NOT_EQUAL(0, ptr2);
    TEST_ASSERT_NOT_EQUAL(0, ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr2, ptr3);
}

/**
 * Test 9: Malloc in order, free out of order
 * Expected: 3 different pointers
 */
void test_malloc_free_out_of_order(void) {
    msg("Test 9: Malloc in order, free out of order\n\tInput: 3 * 4 bytes\n\tExpected: 3 different pointers, all freed");
    int *ptr1 = malloc(4);
    int *ptr2 = malloc(4);
    int *ptr3 = malloc(4);
    *ptr1 = 1;
    *ptr2 = 2;
    *ptr3 = 3;
    free(ptr2);
    free(ptr3);
    free(ptr1);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);
    TEST_ASSERT_NOT_EQUAL(0, ptr1);
    TEST_ASSERT_NOT_EQUAL(0, ptr2);
    TEST_ASSERT_NOT_EQUAL(0, ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr2, ptr3);
}

int main(void) {
    UNITY_BEGIN();
    msg("Phase A: Direct-Mapped Memory");
    RUN_TEST(test_malloc_invalid_input);
    RUN_TEST(test_malloc_valid_input);
    RUN_TEST(test_free_invalid_input);
    RUN_TEST(test_free_invalid_pointer);
    RUN_TEST(test_free_valid_input);
    RUN_TEST(test_double_free);
    RUN_TEST(test_malloc_large_size);
    RUN_TEST(test_malloc_free_in_order);
    RUN_TEST(test_malloc_free_out_of_order);
    msg("Finished running tests");
    return UNITY_END();
}



