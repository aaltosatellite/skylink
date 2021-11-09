//
// Created by elmore loop_status 2.3.2021.
//

#ifndef SUREFIRE_TOOLS_H
#define SUREFIRE_TOOLS_H

#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <semaphore.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <assert.h>


struct string_ss {
	int32_t length;
	uint8_t* data;
} __attribute__((aligned(16)));

typedef struct string_ss String;


// === TIME ==================================================================================================
uint64_t real_microseconds();
uint64_t monotonic_microseconds();
void sleepms(uint32_t ms);
void sleepus(uint32_t us);
struct timespec realtime_since(struct timespec ts);
struct timespec monotime_since(struct timespec ts);
int64_t ms_since_mono(struct timespec ts);
int64_t ms_since_real(struct timespec ts);
int64_t timediff_ms(struct timespec t1, struct timespec t2);
int64_t timediff_us(struct timespec t1, struct timespec t2);
// ===========================================================================================================



// === RANDOM ================================================================================================
void reseed_random();
uint32_t randint_u32(uint32_t a, uint32_t b);
int32_t randint_i32(int32_t a, int32_t b);
uint32_t randint_u64(uint64_t a, uint64_t b);
uint64_t rand64();
int32_t* get_n_unique_random_integers(uint32_t N, int32_t from, int32_t to);
double randomd(double a, double b);
uint64_t fillrand(uint8_t* tgt, uint64_t leng);
int32_t* shuffled_order(int32_t n);
// ===========================================================================================================



// === MIN MAX ===============================================================================================
int max(int num1, int num2);
int min(int num1, int num2);
uint32_t u32_max(uint32_t num1, uint32_t num2);
uint32_t u32_min(uint32_t num1, uint32_t num2);
int32_t i32_max(int32_t num1, int32_t num2);
int32_t i32_min(int32_t num1, int32_t num2);
int64_t i64_max(int64_t num1, int64_t num2);
int64_t i64_min(int64_t num1, int64_t num2);
uint64_t u64_max(uint64_t num1, uint64_t num2);
uint64_t u64_min(uint64_t num1, uint64_t num2);
// ===========================================================================================================

// ===========================================================================================================
int x_in_u8_arr(uint8_t x, uint8_t* arr, uint32_t length);
int x_in_u32_arr(uint32_t x, uint32_t* arr, uint32_t length);
int x_in_i32_arr(int32_t x, int32_t* arr, uint32_t length);
int x_in_u64_arr(uint64_t x, uint64_t* arr, uint32_t length);
String* new_string(uint8_t* data, int32_t length);
void destroy_string(String* str);
String* clone_string(String* str);
int strings_equal(String* str1, String* str2);
String* get_random_string(int leng);
// ===========================================================================================================

// ===========================================================================================================
int x_eventfd(uint32_t count, int flags);
void x_init_condition(pthread_cond_t* condition);
void* x_alloc(uint64_t n);
void* x_calloc(uint64_t n);
void* x_realloc(void* p0, uint64_t new_len);
void x_init_mutex(pthread_mutex_t* q_mutex);
int x_epoll_create();
void x_pthread_create(pthread_t* thread_ptr, void*(*routine)(void*), void* arg);
// ===========================================================================================================

// ===========================================================================================================
int set_fd_limit(uint32_t number);
// ===========================================================================================================

// ===========================================================================================================
void itoa(int n, char s[]);
// ===========================================================================================================

// ===========================================================================================================
void PRINTFF(uint32_t col, const char* restrict fmt, ...);
// ===========================================================================================================




#endif //SUREFIRE_TOOLS_H



