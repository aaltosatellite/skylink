
#include "../skylink/elementbuffer.h"
#include "tools/tools.h"
#include "elebuffer_tests.h"


static void test1_pass(int verbose);
static void test1();
static void test2();
static void test_ratios();


void elebuffer_tests(){
	test1();
	test2();
	if(randint_i32(100,200) < 5){
		test_ratios();
	}
}





static void test1(){
	PRINTFF(0, "[ELEMENT BUFFER TEST 1]\n");
	test1_pass(1);
	for (int i = 0; i < 30; ++i) {
		test1_pass(0);
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


static void test1_pass(int verbose){
	reseed_random();
	int elecount = 1700;
	ElementBuffer* sbuffer = new_element_buffer(64, elecount);
	sbuffer->last_write_index = randint_i32(0, elecount-1);

	//generate N_pl test messages
	if(verbose){
		PRINTFF(0, "\tmaking test datas\n");
	}
	int32_t N_pl = randint_i32(90,400);
	uint8_t** datas = malloc(sizeof(uint8_t*)*N_pl);
	int32_t* lengths = malloc(sizeof(int32_t)*N_pl);
	int32_t* indexes = malloc(sizeof(int32_t)*N_pl);
	for (int i = 0; i < N_pl; ++i) {
		int32_t len = randint_i32(0, 300);
		datas[i] = malloc(len);
		lengths[i] = len;
		fillrand(datas[i], len);
	}
	int32_t* order1 = shuffled_order(N_pl);
	uint8_t* tgt = malloc(2000);

	//push the test messages into ebuffer
	if(verbose){
		PRINTFF(0, "\tStoring...\n");
	}
	for (int i = 0; i < N_pl; ++i) {
		sbuffer->last_write_index = randint_i32(0, elecount-1);
		int idx = element_buffer_store(sbuffer, datas[i], lengths[i]);
		indexes[i] = idx;
		assert(idx >= 0);
		int valid = element_buffer_entire_buffer_is_ok(sbuffer);
		assert(valid);
	}

	//read all the messages back, and compare that they match
	if(verbose){
		PRINTFF(0, "\tReading...\n");
	}
	for (int i = 0; i < N_pl; ++i) {
		int leng_r_fail = element_buffer_read(sbuffer, tgt, indexes[i], lengths[i] - 1);
		int leng_r = element_buffer_read(sbuffer, tgt, indexes[i], lengths[i]);
		int leng_r2 = element_buffer_get_data_length(sbuffer, indexes[i]);
		assert(leng_r_fail == EBUFFER_RET_TOO_LONG_PAYLOAD);
		assert(leng_r == lengths[i]);
		assert(leng_r2 == lengths[i]);
		assert(memcmp(tgt, datas[i], leng_r) == 0);
	}



	//delete all messages.
	if(verbose){
		PRINTFF(0, "\tDeleting...\n");
	}
	for (int i = 0; i < N_pl; ++i) {
		int j = order1[i];
		int idx = indexes[j];
		int leng = lengths[j];
		int slot_req = element_buffer_element_requirement_for(sbuffer, leng);
		int old_free = (int)sbuffer->free_elements;
		int new_free_prediction = old_free + slot_req;

		int leng_r = element_buffer_read(sbuffer, tgt, idx, leng);
		int leng_r2 = element_buffer_get_data_length(sbuffer, idx);
		assert(leng_r == leng);
		assert(leng_r2 == leng);
		assert(memcmp(tgt, datas[j], leng_r) == 0);

		int r = element_buffer_delete(sbuffer, idx);
		assert(r == 0);
		assert(sbuffer->free_elements == new_free_prediction);
		int valid = element_buffer_entire_buffer_is_ok(sbuffer);
		assert(valid);

		int rd = element_buffer_read(sbuffer, tgt, idx, 1000);
		assert(rd == EBUFFER_RET_INVALID_INDEX);
	}


	if(verbose){
		PRINTFF(0,"\t[");
		for (int i = 0; i < 10; ++i) {
			PRINTFF(0, "%d ", order1[i]);
		}
		PRINTFF(0,"]\n");
	}


	for (int i = 0; i < N_pl; ++i) {
		free(datas[i]);
	}
	free(datas);
	free(lengths);
	free(indexes);
	free(tgt);
	free(order1);
	destroy_element_buffer(sbuffer);
}
//======================================================================================================================
//======================================================================================================================




//== TEST 2 ============================================================================================================
//======================================================================================================================
struct testmsg{
	String* data;
	uint8_t in_buff;
	int32_t index;
};
typedef struct testmsg TestMsg;

struct testjob{
	TestMsg** msgs;
	int n_msgs;
	int n_in;
	ElementBuffer* buffer;
	int general_direction;
	int total_stored_content;
};
typedef struct testjob TestJob;

#define DIR_FORWARD		1
#define DIR_BACKWARD	(-1)


static void confirm_msg_i_in_buffer(ElementBuffer* buffer, TestMsg* msg){
	uint8_t* tgt = x_alloc(msg->data->length + 2);
	int leng = element_buffer_read(buffer, tgt, msg->index, msg->data->length);
	assert(leng == msg->data->length);
	assert(memcmp(tgt, msg->data->data, leng) == 0);
	free(tgt);
}


static TestMsg* new_random_msg(int32_t minlen, int32_t maxlen){
	TestMsg* msg = x_alloc(sizeof(TestMsg));
	msg->in_buff = 0;
	msg->index = -1;
	int32_t len = randint_i32(minlen, maxlen);
	uint8_t* data = x_alloc(len);
	fillrand(data, len);
	msg->data = new_string(data, len);
	free(data);
	return msg;
}



static int get_one_to_add(TestMsg** msgs, int n_msgs){
	int i = randint_i32(0, n_msgs-1);
	while (msgs[i]->in_buff == 1){
		i = randint_i32(0, n_msgs-1);
	}
	return i;
}



static int get_one_to_remove(TestMsg** msgs, int n_msgs){
	int i = randint_i32(0, n_msgs-1);
	while (msgs[i]->in_buff == 0){
		i = randint_i32(0, n_msgs-1);
	}
	return i;
}



static int pick_direction(TestJob* job){
	if(job->n_in == 0){
		return DIR_FORWARD;
	}
	if(job->n_in == job->n_msgs){
		return DIR_BACKWARD;
	}
	if(job->general_direction == DIR_FORWARD){
		if(randint_i32(1, 100) > 30){
			return DIR_FORWARD;
		}
		return DIR_BACKWARD;
	}
	if(randint_i32(1, 100) > 30){
		return DIR_BACKWARD;
	}
	return DIR_FORWARD;
}



static TestJob new_job(int minlen, int maxlen, int elecount){
	TestJob job;
	job.n_msgs = elecount * 2;
	job.general_direction = DIR_FORWARD;
	job.n_in = 0;
	job.buffer = new_element_buffer(64, elecount);
	job.msgs = x_alloc(sizeof(TestMsg*) * job.n_msgs);
	job.total_stored_content = 0;
	for (int i = 0; i < job.n_msgs; ++i) {
		job.msgs[i] = new_random_msg(minlen, maxlen);
	}
	return job;
}


static void destroy_job(TestJob* job){
	for (int i = 0; i < job->n_msgs; ++i) {
		TestMsg* msg = job->msgs[i];
		destroy_string(msg->data);
		free(msg);
	}
	free(job->msgs);
	free(job->buffer->pool);
	free(job->buffer);
}



static int up_cycle(TestJob* job) {
	int i = get_one_to_add(job->msgs, job->n_msgs);
	assert(job->msgs[i]->in_buff == 0);
	TestMsg *msg = job->msgs[i];
	int required = element_buffer_element_requirement_for(job->buffer, msg->data->length);
	int old_free = (int) job->buffer->free_elements;
	int ret = element_buffer_store(job->buffer, msg->data->data, msg->data->length);
	if (old_free < required) {
		assert(job->buffer->free_elements == old_free);
		assert(ret == EBUFFER_RET_NO_SPACE);
	} else {
		assert(job->buffer->free_elements == (old_free - required));
		assert(ret >= 0);
		assert(ret < job->buffer->element_count);
		msg->in_buff = 1;
		msg->index = ret;
		job->n_in++;
		job->total_stored_content += msg->data->length;
		confirm_msg_i_in_buffer(job->buffer, msg);
	}
	int valid = element_buffer_entire_buffer_is_ok(job->buffer);
	assert(valid);
	return ret > -1;
}


static int down_cycle(TestJob* job){
	int i = get_one_to_remove(job->msgs, job->n_msgs);
	TestMsg* msg = job->msgs[i];
	assert(job->msgs[i]->in_buff == 1);
	confirm_msg_i_in_buffer(job->buffer, msg);
	int required = element_buffer_element_requirement_for(job->buffer, msg->data->length);
	int old_free = (int)job->buffer->free_elements;
	int ret = element_buffer_delete(job->buffer, job->msgs[i]->index);
	assert(job->buffer->free_elements == (old_free + required));
	assert(ret == 0);
	msg->in_buff = 0;
	msg->index = -1;
	job->n_in--;
	job->total_stored_content -= msg->data->length;
	int valid = element_buffer_entire_buffer_is_ok(job->buffer);
	assert(valid);
	return 1;
}

static void random_confirm(TestJob* job, int n){
	for (int i = 0; i < n; ++i) {
		int idx = randint_i32(0, job->n_msgs-1);
		TestMsg* msg = job->msgs[idx];
		if(msg->in_buff){
			confirm_msg_i_in_buffer(job->buffer, msg);
		}
	}
}

static void cycling(TestJob* job, int break_switch){
	int failed_adds_in_row = 0;
	int switchs = 0;
	int c = 0;
	while(1){
		c++;
		int dir = pick_direction(job);
		assert ((dir == DIR_FORWARD) || (dir == DIR_BACKWARD));
		assert ((job->general_direction == DIR_FORWARD) || (job->general_direction == DIR_BACKWARD));

		if(dir == 1){
			int added = up_cycle(job);
			if(added){
				failed_adds_in_row = 0;
			} else {
				failed_adds_in_row++;
			}
		}
		if(dir == DIR_BACKWARD){
			down_cycle(job);
		}

		random_confirm(job, 20);


		//if((c % 1000) == 0){
		//	PRINTFF(0,"\t\t[free elements: %d]\n", job->buffer->free_elements);
		//}


		if((job->general_direction == DIR_FORWARD) && (failed_adds_in_row > 3)){
			job->general_direction = DIR_BACKWARD;
			double space = (double)job->buffer->element_count * (double)job->buffer->element_size;
			double filled = (double)job->total_stored_content;
			double ff = filled / space;
			PRINTFF(0, "\t[Downhill] [fill fraction: %lf]\n",ff);
			assert(ff > 0.77);

			switchs++;
		}

		if((job->general_direction == DIR_BACKWARD) && (job->n_in == 0)){
			job->general_direction = DIR_FORWARD;

			PRINTFF(0, "\t[Uphill]\n");
			switchs++;
		}

		if(switchs > break_switch){
			break;
		}
	}
}



static void test2(){
	PRINTFF(0, "[ELEMENT BUFFER TEST 2]\n");
	reseed_random();
	int elecount = randint_i32(5000,8000);
	PRINTFF(0, "[elecount: %d ]\n", elecount);
	TestJob job = new_job(0, 350, elecount);

	cycling(&job, 6);

	destroy_job(&job);
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}
//======================================================================================================================
//======================================================================================================================




//==== STORAGE SPACE UTILIZATION RATIO TEST ============================================================================
//======================================================================================================================
static double obtain_ratio(int elecount, int elesize, int pl_minsize, int pl_maxsize){
	ElementBuffer* buffer = new_element_buffer(elesize, elecount);
	int n_strings = 0;
	int content  = 0;
	uint8_t* payload = x_alloc(pl_maxsize+20);
	fillrand(payload, pl_maxsize+20);
	while(1){
		int size = randint_i32(pl_minsize, pl_maxsize);
		n_strings++;
		int store_idx = element_buffer_store(buffer, payload, size);
		if(store_idx >= 0){
			content += size;
		}
		else {
			break;
		}
	}
	free(payload);
	double fill = (double) content;
	double total = (double) (elecount * buffer->element_size);
	double ratio = fill / total;
	destroy_element_buffer(buffer);
	return ratio;
}

static double obtain_ratio_avg(int n, int elecount, int elesize, int pl_minsize, int pl_maxsize){
	double sum = 0.0;

	for (int i = 0; i < n; ++i) {
		double ratio_ = obtain_ratio(elecount, elesize, pl_minsize, pl_maxsize);
		sum = sum + ratio_;
	}
	double dn = (double)n;
	return sum / dn;
}


static void test_ratios(){
	int elecount = 9000;
	int pl_minsize = 40;
	int pl_maxsize = 255;

	int elesize_min = 28;
	int elesize_max = 50;
	int n_size_tests = elesize_max - elesize_min + 1;
	int avg_over	= 5000;
	int* elesizes = x_alloc(sizeof(int)*n_size_tests);
	double span = (double)elesize_max - (double)elesize_min;
	double step = span / (double)(n_size_tests-1);
	for (int i = 0; i < n_size_tests; ++i) {
		double sz = (double)elesize_min + (double)i * step;
		elesizes[i] = (int)sz;
	}

	for (int i = 0; i < n_size_tests; ++i) {
		int size = elesizes[i];
		PRINTFF(0, "[size: %d]:",size);
		double ratio = obtain_ratio_avg(avg_over, elecount, size, pl_minsize, pl_maxsize);
		PRINTFF(0, "\t %lf ",ratio);
		/*
		if(ratio > old_ratio){
			PRINTFF(0,"+");
		} else {
			PRINTFF(0,"-");
		}
		*/
		int k = (int)(900*(ratio-0.77));
		for (int j = 0; j < k; ++j) {
			PRINTFF(0,":");
		}
		PRINTFF(0,"\n");
	}
}

