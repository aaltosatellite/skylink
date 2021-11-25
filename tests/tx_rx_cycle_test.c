//
// Created by elmore on 2.11.2021.
//

#include "tx_rx_cycle_test.h"
#include "../src/skylink/elementbuffer.h"
#include "../src/skylink/arq_ring.h"
#include "../src/skylink/frame.h"
#include "../src/skylink/skylink.h"
#include "tst_utilities.h"
#include "tools/tools.h"

#define STATE_RX		0
#define STATE_TX		1






typedef struct {
	String* msg ;
	int target_peer;
	uint64_t idd;
	uint64_t ts_push;
	uint64_t ts_rcv;
	int push_retcode;
} Payload;


typedef struct {
	SkyRadioFrame frame;
	int target_peer;
	uint64_t tx_start;
	uint64_t tx_end;
	int rx_ok;
} EtherFrame;

typedef struct {
	EtherFrame* in_transit[1000];
	int n_in;
} Ether;

typedef struct {
	SkyHandle handle;
	uint8_t radio_state;
	uint64_t tx_end;
	String* under_send;
	double pl_rate;
} JobPeer;


typedef struct {
	EtherFrame** array;
	int n;
} EtherFrameList;

typedef struct {
	Payload** array;
	int n;
} PayloadList;

typedef struct {
	JobPeer peer1;
	JobPeer peer2;
	SkyRadioFrame frame;
	uint64_t now_ms;
	double byterate;
	double corrupt_rate;
	double loss_rate;
	int lag_ms;
	PayloadList* payloadList;
	EtherFrameList* lostFrames;
	EtherFrameList* missedFrames;
	EtherFrameList* receivedFrames;
	EtherFrameList* inEther;

} TXRXJob;

static void test1();
static void test1_round();
static void step_forward(int which, TXRXJob* job);
static void print_job_status(TXRXJob* job);



void txrx_tests(){
	test1();
}



static int string_same(String* s1, String* s2){
	if(s1->length != s2->length){
		return 0;
	}
	return memcmp(s1->data, s2->data, s1->length) == 0;
}


Payload* new_payload(int target, String* msg, uint64_t ts_push){
	Payload* payload = malloc(sizeof(Payload));
	payload->target_peer = target;
	payload->msg = msg;
	payload->idd = rand64();
	payload->ts_push = ts_push;
	payload->ts_rcv = 0;
	payload->push_retcode = -100;
	return payload;
}

Payload* new_random_payload(int target, uint64_t ts_push){
	String* msg = get_random_string(randint_u32(0, 172));
	return new_payload(target, msg, ts_push);
}

void destroy_payload(Payload* payload){
	destroy_string(payload->msg);
	free(payload);
}

EtherFrame* new_eframe(SkyRadioFrame frame, uint64_t tx_start, uint64_t tx_end, int target){
	EtherFrame* eframe = malloc(sizeof(EtherFrame));
	eframe->frame = frame;
	eframe->tx_start = tx_start;
	eframe->tx_end = tx_end;
	eframe->rx_ok = 1;
	eframe->target_peer = target;
	return eframe;
}

EtherFrameList* new_eframe_list(){
	EtherFrameList* elist = malloc(sizeof(EtherFrameList));
	elist->array = malloc(sizeof(EtherFrame*) * 4);
	elist->n = 0;
	return elist;
}

int eframe_list_append(EtherFrameList* elist, EtherFrame* etherFrame){
	elist->array[elist->n] = etherFrame;
	elist->n++;
	elist->array = realloc(elist->array, sizeof(EtherFrame*) * (elist->n + 2));
	return elist->n -1;
}

void eframe_list_pop(EtherFrameList* elist, int idx){
	if(idx >= elist->n){
		return;
	}
	elist->array[idx] = elist->array[elist->n];
	elist->n--;
	elist->array = realloc(elist->array, sizeof(EtherFrame*) * (elist->n + 2));
}


PayloadList* new_payload_list(){
	PayloadList* plist = malloc(sizeof(PayloadList));
	plist->array = malloc(sizeof(Payload*) * 4);
	plist->n = 0;
	return plist;
}

int payload_list_append(PayloadList* payloadList, Payload* payload){
	payloadList->array[payloadList->n] = payload;
	payloadList->n++;
	payloadList->array = realloc(payloadList->array, sizeof(Payload*) * (payloadList->n + 2));
	return payloadList->n -1;
}

void payload_list_mark_as_received(PayloadList* payloadList, void* msg, int msg_len, int target, uint64_t ts_now){
	assert(msg_len >= 0);
	String* ref_string = new_string(msg ,msg_len);
	for (int i = 0; i < payloadList->n; ++i) {
		Payload* pl = payloadList->array[i];
		if(pl->ts_rcv > 0){
			continue;
		}
		if(target != pl->target_peer){
			continue;
		}
		if(string_same(ref_string, pl->msg)){
			pl->ts_rcv = ts_now;
			break;
		}
	}
	destroy_string(ref_string);
}

int payload_list_count_unreceived(PayloadList* payloadList){
	int unreceived = 0;
	for (int i = 0; i < payloadList->n; ++i) {
		Payload* pl = payloadList->array[i];
		assert(pl->ts_push > pl->ts_rcv);
		if(pl->ts_rcv == 0){
			unreceived++;
		}
	}
	return unreceived;
}


static int strings_same_comparison(String** arr1, String** arr2, int n){
	for (int i = 0; i < n; ++i) {
		String* s1 = arr1[i];
		String* s2 = arr2[i];
		int same = string_same(s1,s2);
		if(!same){
			return 0;
		}
	}
	return 1;
}


static int roll_chance(double const chance){
	int r = rand(); // NOLINT(cert-msc50-cpp)
	double rd = (double)r;
	double rM = (double)RAND_MAX;
	double rr = rd/rM;
	return rr < chance;
}




void test1(){
	PRINTFF(0,"[TX-RX Test 1: basic case]\n");
	int N = 200;
	for (int i = 0; i < N; ++i) {

		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}






void test1_round(){
	SkyConfig* config1 = new_vanilla_config();
	SkyConfig* config2 = new_vanilla_config();
	config1->identity[0] = 1;
	config2->identity[0] = 2;

	SkyHandle handle1 = new_handle(config1);
	SkyHandle handle2 = new_handle(config2);

	SkyRadioFrame* sendFrame = new_frame();
	SkyRadioFrame* recvFrame = new_frame();

	mac_shift_windowing(handle1->mac, rand()%12000);
	mac_shift_windowing(handle2->mac, rand()%12000);

	TXRXJob job;
	job.peer1.handle = handle1;
	job.peer2.handle = handle1;
	job.peer1.radio_state = STATE_RX;
	job.peer2.radio_state = STATE_RX;
	job.peer1.under_send = NULL;
	job.peer2.under_send = NULL;
	job.peer1.tx_end = 0;
	job.peer2.tx_end = 0;
	job.peer1.pl_rate 	= 1.0; //param
	job.peer2.pl_rate 	= 1.0; //param
	job.now_ms = 0;
	job.corrupt_rate	= 0.00; //param
	job.loss_rate 		= 0.00; //param
	job.lag_ms 			= 3; //param
	job.byterate 		= 2100.0; //param
	job.payloadList 	= new_payload_list();
	job.inEther 		= new_eframe_list();
	job.lostFrames 		= new_eframe_list();
	job.receivedFrames 	= new_eframe_list();

	uint64_t N_RUN = 1000 * 60 * 5;
	N_RUN = 2;

	for (uint64_t i = 0; i < N_RUN; ++i) {
		step_forward(1, &job);
		step_forward(2, &job);
		if( i % 1000 == 0){
			print_job_status(&job);
		}

		job.now_ms++;
	}

	destroy_config(config1);
	destroy_config(config2);
	destroy_frame(recvFrame);
	destroy_frame(sendFrame);
	destroy_handle(handle1);
	destroy_handle(handle2);
}

static void step_forward(int which, TXRXJob* job){
	uint8_t tgt[1000];
	int target	  = (which == 1) ? 2 : 1;
	JobPeer* peer = (which == 1) ? &job->peer1 : &job->peer2;


	//generate new pl to send queue
	if(roll_chance(peer->pl_rate/1000.0)){
		Payload* pl = new_random_payload(target, job->now_ms);
		int push_ret = skyArray_push_packet_to_send(peer->handle->arrayBuffers[0], pl->msg->data, pl->msg->length);
		pl->push_retcode = push_ret;
		payload_list_append(job->payloadList, pl);
	}


	//receive (or miss) transmissions from ether, and collect payloads.
	for (int i = 0; i < job->inEther->n; ++i) {
		EtherFrame* eframe = job->inEther->array[i];
		//targeted to this peer?
		if(eframe->target_peer != which){
			continue;
		}
		assert(job->inEther->n < 50);
		assert(eframe->tx_start <= job->now_ms);
		assert((eframe->tx_end + job->lag_ms) >= job->now_ms);
		//even if not yet ready, check if device will miss this by speaking over the packet.
		int in_reception = ((eframe->tx_start + job->lag_ms) <= job->now_ms) && ((eframe->tx_end + job->lag_ms) >= job->now_ms);
		if(in_reception && (peer->radio_state == STATE_TX)){
			eframe->rx_ok = 0;
		}
		//at end just now?
		int arrives_now = (eframe->tx_end + job->lag_ms) == job->now_ms;
		if(!arrives_now){
			continue;
		}
		if(eframe->rx_ok){
			eframe->frame.rx_time_ms = job->now_ms;
			sky_rx(peer->handle, &eframe->frame, 1);
			if(skyArray_count_readable_rcv_packets(peer->handle->arrayBuffers[0])){
				int seq = -1;
				int red = skyArray_read_next_received(peer->handle->arrayBuffers[0], tgt, &seq);
				payload_list_mark_as_received(job->payloadList, tgt, red, which, job->now_ms);
			}
			eframe_list_append(job->receivedFrames, eframe);
			eframe_list_pop(job->inEther, i);
		}
		else {
			eframe_list_append(job->missedFrames, eframe);
			eframe_list_pop(job->inEther, i);
		}
	}


	//transmit new packet if sky_tx says so. Also corrup and maybe lose entirely.
	int r_tx = sky_tx(peer->handle, &job->frame, 1, job->now_ms);
	if(r_tx){
		uint64_t tx_end = job->now_ms + ((job->frame.length * 1000) / job->byterate) + 1;
		EtherFrame* eframe = new_eframe(job->frame, job->now_ms, tx_end, target);
		corrupt_bytearray(eframe->frame.raw, eframe->frame.length, job->corrupt_rate);
		int lost = roll_chance(job->loss_rate);
		if(lost){
			eframe_list_append(job->inEther, eframe);
		} else{
			eframe_list_append(job->lostFrames, eframe);
		}
		peer->radio_state = STATE_TX;
		peer->tx_end = tx_end;
	}

	if(peer->radio_state == STATE_TX){
		assert(peer->tx_end >= job->now_ms);
	}
	if((peer->radio_state == STATE_TX) && (peer->tx_end == job->now_ms)){
		peer->radio_state = STATE_RX;
	}
}



static void print_job_status(TXRXJob* job){
	PRINTFF(0, "\n=====================================\n");
	PRINTFF(0,"Frames created: %d\n", job->inEther->n + job->lostFrames->n + job->receivedFrames->n);
	PRINTFF(0,"Frames lost: %d\n", job->lostFrames->n);
	PRINTFF(0,"Frames in ether: %d\n", job->inEther->n);
	PRINTFF(0,"Frames received: %d\n", job->receivedFrames->n);
	PRINTFF(0,"Payloads created: %d\n", job->payloadList->n);
	int unchecked = payload_list_count_unreceived(job->payloadList);
	PRINTFF(0,"Payloads acked: %d\n", job->payloadList->n - unchecked);
	PRINTFF(0,"Payloads not acked: %d\n", unchecked);
	PRINTFF(0, "=====================================\n");
}






