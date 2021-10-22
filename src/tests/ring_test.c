//
// Created by elmore on 16.10.2021.
//

#include "ring_test.h"
#include <assert.h>

void test1();
void test2();

void ring_tests(){
	test1();
	//test1();


}

String* random_string(int len){
	uint8_t* buff = x_alloc(len);
	fillrand(buff, len);
	String* s = new_string(buff, len);
	free(buff);
	return s;
}

void test1(){
	SkylinkArray* array = new_skylink_array(64, 1500, 12, 12, 1, 4, 4);
	String** strings1 = x_calloc(sizeof(String*)* 40);
	String** strings2 = x_calloc(sizeof(String*)* 40);


	assert(skyArray_packets_to_tx(array) == 0);
	String* s = random_string(randint_i32(0,160));
	int r = skyArray_push_packet_to_send(array, s->data, s->length);
	assert(skyArray_packets_to_tx(array) == 1);
	uint8_t* tgt = x_alloc(s->length+2);
	r = skyArray_recall(array, 0, &tgt);
	assert(r == -1);

}






void test2(){
	int elesize = randint_i32(60,80);
	int elecount = randint_i32(1500,3000);
	int rcv_ring_len = randint_i32(8,35);
	int send_ring_len = randint_i32(10,35);
	int n_recall = randint_i32(0, send_ring_len-3);
	int horizon = randint_i32(0, rcv_ring_len-3);
	SkylinkArray* array = new_skylink_array(elesize, elecount, rcv_ring_len, send_ring_len, 1, n_recall, horizon);

	int N = randint_i32(100,9000);
	for (int i = 0; i < N; ++i) {

	}


}













