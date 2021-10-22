//
// Created by elmore loop_status 2.3.2021.
//

#include "tools.h"
#include <sys/epoll.h>
#include <time.h>


uint64_t recur_seed = 125;
uint32_t PRINT_COL = 0;
uint8_t fd_limit_risen__ = 0;

uint64_t real_microseconds(){
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64_t ts = t.tv_sec*1000000;
	ts += t.tv_nsec/1000;
	return ts;
}


uint64_t monotonic_microseconds(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	uint64_t ts = t.tv_sec*1000000;
	ts += t.tv_nsec/1000;
	return ts;
}



void sleepms(uint32_t ms){
	struct timespec to;
	struct timespec rm;
	rm.tv_sec = 0;
	rm.tv_nsec = 0;
	to.tv_nsec = (ms%1000L)*1000000L;
	to.tv_sec = (ms/1000);
	nanosleep(&to, &rm);
}


void sleepus(uint32_t us){
	struct timespec to;
	struct timespec rm;
	to.tv_nsec = ((int64_t)us % 1000000L)*1000L;
	to.tv_sec = (us/1000000);
	nanosleep(&to, &rm);
}


struct timespec realtime_since(struct timespec ts){
	struct timespec now;
	struct timespec diff;
	clock_gettime(CLOCK_REALTIME, &now);
	diff.tv_sec = (now.tv_sec - ts.tv_sec);
	int64_t nsec = (((int64_t)now.tv_nsec) - (int64_t)ts.tv_nsec);
	diff.tv_nsec = nsec;
	if(nsec < 0){
		diff.tv_sec--;
		diff.tv_nsec = 1000000000 + nsec;
	}
	return diff;
}


struct timespec monotime_since(struct timespec ts){
	struct timespec now;
	struct timespec diff;
	clock_gettime(CLOCK_MONOTONIC, &now);
	diff.tv_sec = (now.tv_sec - ts.tv_sec);
	int64_t nsec = (((int64_t)now.tv_nsec) - (int64_t)ts.tv_nsec);
	diff.tv_nsec = nsec;
	if(nsec < 0){
		diff.tv_sec--;
		diff.tv_nsec = 1000000000 + nsec;
	}
	return diff;
}


int64_t ms_since_mono(struct timespec ts){
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	int64_t secs = ((int64_t)now.tv_sec) - (int64_t)ts.tv_sec;
	int64_t nsecs = (((int64_t)now.tv_nsec)) - (int64_t)ts.tv_nsec;
	return secs*1000 + (nsecs/1000000);
}


int64_t ms_since_real(struct timespec ts){
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	int64_t secs = ((int64_t)now.tv_sec) - (int64_t)ts.tv_sec;
	int64_t nsecs = (((int64_t)now.tv_nsec)) - (int64_t)ts.tv_nsec;
	return secs*1000 + (nsecs/1000000);
}


int64_t timediff_ms(struct timespec t1, struct timespec t2){
	int64_t diff = (t2.tv_sec - t1.tv_sec)*1000;
	int64_t ndiff = (t2.tv_nsec - t1.tv_nsec);
	diff = diff + (ndiff/1000000);
	return diff;
}


int64_t timediff_us(struct timespec t1, struct timespec t2){
	int64_t diff = (t2.tv_sec - t1.tv_sec)*1000000;
	int64_t ndiff = (t2.tv_nsec - t1.tv_nsec);
	diff = diff + (ndiff/1000);
	return diff;
}


void reseed_random(){
	struct timespec tsr;
	struct timespec tsm;
	clock_gettime(CLOCK_REALTIME, &tsr);
	clock_gettime(CLOCK_MONOTONIC, &tsm);
	srandom((int)(tsr.tv_sec+tsr.tv_nsec+tsm.tv_sec+tsm.tv_nsec+recur_seed) );
	//for(uint64_t i=0;i<recur_seed;i++){
	//    random();
	//}
	//recur_seed = randint_u32(4,50);
}


//includes both ends.
uint32_t randint_u32(uint32_t a, uint32_t b){
	uint64_t d = b-a;
	uint64_t r = random();
	r = ((d+1)*r)/( (uint64_t)RAND_MAX+1);
	r = r+a;
	return (uint32_t)r;
}


int32_t randint_i32(int32_t a, int32_t b){
	int64_t d = b-a;
	int64_t r = random();
	r = ((d+1)*r)/( (int64_t)RAND_MAX+1);
	r = r+a;
	return (int32_t)r;
}


uint32_t randint_u64(uint64_t a, uint64_t b){
	uint64_t span = b - a;
	if (span > 32000000){
		uint64_t mid = a + span/2;
		if(random() % 2 == 0){
			return randint_u64(a, mid);
		}
		return randint_u64(mid+1, b);
	}
	return a + (rand64() % (span+1));
}


uint64_t rand64(){
	uint64_t i = 0;
	fillrand((uint8_t*)(&i), sizeof(uint64_t));
	return i;
}


double randomd(double a, double b){
	double span = b - a;
	uint64_t ur = random();
	double dr = (double)ur;
	dr = dr/((double)RAND_MAX+1);
	return a + (dr*span);
}


uint64_t fillrand(uint8_t* tgt, uint64_t leng){
	FILE* f = fopen("/dev/urandom", "r\0");
	uint64_t rd = fread(tgt, 1, leng, f);
	fclose(f);
	return rd;
}




int32_t* get_n_unique_random_integers(uint32_t N, int32_t from, int32_t to){
	if(from >= to){
		return NULL;
	}
	if((to-from) < ((int32_t)N)){
		return NULL;
	}
	if(N == 0){
		return NULL;
	}
	int32_t* arr = x_alloc(N * sizeof(int32_t));
	uint32_t n = 0;
	uint32_t L = to-from;
	while (n < N){
		int32_t r = ((int32_t) randint_u32(0, L)) + from;
		uint8_t found = 0;
		for (uint32_t i=0;i<n;i++){
			if (arr[i] == r){
				found = 1;
				break;
			}
		}
		if(found){continue;}
		arr[n] = r;
		n++;
	}
	return arr;
}

int32_t* shuffled_order(int32_t n){
	int32_t* order = x_alloc(sizeof(int32_t)* n);
	for (int i = 0; i < n; ++i) {
		order[i] = i;
	}
	for (int i = 0; i < n*4; ++i) {
		int idx1 = randint_i32(0, n - 1);
		int idx2 = randint_i32(0, n - 1);
		int32_t a = order[idx1];
		int32_t b = order[idx2];
		order[idx1] = b;
		order[idx2] = a;
	}
	return order;
}





//=== MIN MAX ==========================================================================================================
int max(int num1, int num2){
	return (num1 > num2 ) ? num1 : num2;
}
int min(int num1, int num2){
	return (num1 > num2 ) ? num2 : num1;
}
uint32_t u32_max(uint32_t num1, uint32_t num2){
	return (num1 > num2 ) ? num1 : num2;
}
uint32_t u32_min(uint32_t num1, uint32_t num2){
	return (num1 > num2 ) ? num2 : num1;
}
int32_t i32_max(int32_t num1, int32_t num2){
	return (num1 > num2 ) ? num1 : num2;
}
int32_t i32_min(int32_t num1, int32_t num2){
	return (num1 > num2 ) ? num2 : num1;
}
int64_t i64_max(int64_t num1, int64_t num2){
	return (num1 > num2 ) ? num1 : num2;
}
int64_t i64_min(int64_t num1, int64_t num2){
	return (num1 > num2 ) ? num2 : num1;
}
uint64_t u64_max(uint64_t num1, uint64_t num2){
	return (num1 > num2 ) ? num1 : num2;
}
uint64_t u64_min(uint64_t num1, uint64_t num2){
	return (num1 > num2 ) ? num2 : num1;
}
//=== MIN MAX ==========================================================================================================





int init_semaphore(sem_t* sem, uint32_t initial_value){
	int ret = sem_init(sem, 0, initial_value);
	return ret;
}


// ===========================================================================================================
// ===========================================================================================================
int x_in_u8_arr(uint8_t x, uint8_t* arr, uint32_t length){// NOLINT(readability-non-const-parameter)
	for (uint32_t i = 0; i < length; ++i) {
		if(arr[i] == x){
			return (int)i;
		}
	}
	return -1;
}
int x_in_u32_arr(uint32_t x, uint32_t* arr, uint32_t length){// NOLINT(readability-non-const-parameter)
	for (uint32_t i = 0; i < length; ++i) {
		if(arr[i] == x){
			return (int)i;
		}
	}
	return -1;
}
int x_in_i32_arr(int32_t x, int32_t* arr, uint32_t length){ // NOLINT(readability-non-const-parameter)
	for (uint32_t i = 0; i < length; ++i) {
		if(arr[i] == x){
			return (int)i;
		}
	}
	return -1;
}

int x_in_u64_arr(uint64_t x, uint64_t* arr, uint32_t length){// NOLINT(readability-non-const-parameter)
	for (uint32_t i = 0; i < length; ++i) {
		if(arr[i] == x){
			return (int)i;
		}
	}
	return -1;
}
// ===========================================================================================================
String* new_string(uint8_t* data, int32_t length){
	String* str = x_alloc(sizeof(String));
	str->length = length;
	str->data = x_alloc(length);
	memcpy(str->data, data, length);
	return str;
}
void destroy_string(String* str){
	free(str->data);
	free(str);
}
String* clone_string(String* str){
	return new_string(str->data, str->length);
}
int strings_equal(String* str1, String* str2){
	if(str1->length != str2->length){
		return 0;
	}
	return memcmp(str1->data, str2->data, str1->length) == 0;
}
// ===========================================================================================================
// ===========================================================================================================



// === X =========================================================================================================
// ===============================================================================================================
void x_init_condition(pthread_cond_t* condition){
	memset(condition, 0, sizeof(pthread_cond_t));
	int ret = 0;
	pthread_condattr_t cond_attr;
	ret = ret | pthread_condattr_init(&cond_attr);
	ret = ret | pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_PRIVATE); //is default anyway
	//ret = ret | pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
	ret = ret | pthread_cond_init(condition, &cond_attr);
	pthread_condattr_destroy(&cond_attr);
	if(ret != 0){
		quick_exit(-800);
	}
}

int x_eventfd(uint32_t count, int flags){
	int ev = eventfd(count, flags); //eventfd is used when queue is present in a polling array.
	if(ev < 0){
		quick_exit(-801);
	}
	return ev;
}

void x_init_mutex(pthread_mutex_t* q_mutex){
	memset(q_mutex, 0, sizeof (pthread_mutex_t));
	//note: destroying uninitialized mutex seems to always at least return.
	int ret = 0;
	pthread_mutexattr_t mutex_attr1;
	ret = ret | pthread_mutexattr_init(&mutex_attr1);
	ret = ret | pthread_mutexattr_setrobust(&mutex_attr1, PTHREAD_MUTEX_ROBUST);
	ret = ret | pthread_mutex_init(q_mutex, &mutex_attr1);
	pthread_mutexattr_destroy(&mutex_attr1);
	if(ret != 0){
		quick_exit(-802);
	}
}

void* x_alloc(uint64_t n){
	void* p = malloc(n);
	if(p == NULL){
		quick_exit(-803);
	}
	return p;
}

void* x_calloc(uint64_t n){
	void* p = calloc(n, 1);
	if(p == NULL){
		quick_exit(-804);
	}
	return p;
}

void* x_realloc(void* p0, uint64_t new_len){
	void* p = realloc(p0, new_len);
	if(p == NULL){
		quick_exit(-805);
	}
	return p;
}

int x_epoll_create(){
	int fd = epoll_create(100);
	if(fd<0){
		quick_exit(-806);
	}
	return fd;
}

void x_pthread_create(pthread_t* thread_ptr, void*(*routine)(void*), void* arg){
	int r = pthread_create(thread_ptr, NULL, routine, arg);
	if(r != 0){
		quick_exit(-807);
	}
}
// === X =========================================================================================================
// ===============================================================================================================





int set_fd_limit(uint32_t number){
	struct rlimit rl_fd_max;
	getrlimit(RLIMIT_NOFILE, &rl_fd_max);
	rl_fd_max.rlim_cur = number;
	int r = setrlimit(RLIMIT_NOFILE, &rl_fd_max);
	return r;
}



/* reverse:  reverse string s in place */
void reverse(char s[])
{
	int i, j;
	char c;

	for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}


/* itoa:  convert n to characters in s */
void itoa(int n, char s[])
{
	int i, sign;

	if ((sign = n) < 0){  /* record sign */
		n = -n;          /* make n positive */
	}
	i = 0;
	do {       /* generate digits in reverse order */
		s[i++] = (n % 10) + '0';   /* get next digit */
	} while ((n /= 10) > 0);     /* delete it */
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
}



void PRINTFF(uint32_t col, const char* restrict fmt, ...){
	char buffer[4096];
	col = min((int)col, 4000);
	uint32_t c = 0;
	if(col > PRINT_COL){
		c = col-PRINT_COL;
		for(uint32_t i=0;i<c;i++){
			buffer[i] = ' ';
		}
	}
	va_list args;
	va_start(args, fmt);
	int rc = vsnprintf(buffer+c, sizeof(buffer), fmt, args);
	va_end(args);
	buffer[rc+c]='\0';
	for(uint32_t i=0;i<c+rc;i++){
		if(buffer[i] == '\0'){break;}
		PRINT_COL++;
		if(buffer[i] == '\n'){PRINT_COL = 0;}
	}
	buffer[4095] = '\0';
	printf("%s",buffer);
	fflush(stdout);
}

