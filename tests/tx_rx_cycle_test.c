//
// Created by elmore on 2.11.2021.
//


#include "../src/skylink/elementbuffer.h"
#include "../src/skylink/arq_ring.h"
#include "../src/skylink/skylink.h"
#include "tst_utilities.h"
#include "tools/tools.h"
#include <math.h>

#define STATE_RX		0
#define STATE_TX		1






typedef struct {
	String* msg ;
	int target_peer;
	uint64_t idd;
	uint64_t ts_push;
	uint64_t ts_rcv;
	int assigned_sequence;
} Payload;


typedef struct {
	SkyRadioFrame frame;
	int target_peer;
	uint64_t tx_start;
	uint64_t tx_end;
	int rx_ok;
} EtherFrame;


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
	int lag_ms;
	double loss_rate0;
	double spin_rate_rpm;
	double silent_section;
	int spin_on;
	PayloadList* payloadList1;
	PayloadList* payloadList2;
	EtherFrameList* inEther;
	EtherFrameList* lostFrames;
	EtherFrameList* receivedFrames;
	EtherFrameList* missedFrames;
} TXRXJob;


static void test1();
static void test1_round(uint64_t NN, int print_on);
static void step_forward(int which, TXRXJob* job);
static void print_job_status(TXRXJob* job);
static void test_get_loss_chance();


int main(){
	reseed_random();
	PRINTFF(0,"\n\t(testing loss function...");
	for (int i = 0; i < 1000; ++i) {
		test_get_loss_chance();
	}
	PRINTFF(0," ...ok)\n");
	test1();
}

void test1(){
	PRINTFF(0,"[TX-RX Test 1]\n");
	int N = 1;
	for (int i = 0; i < N; ++i) {
		test1_round(3600*1000, 1);
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}



double get_loss_chance(uint64_t now_ms, double rate0, double spin_rpm, double silent_section, int spin_on){
	if(!spin_on){
		return rate0;
	}
	double ms_per_r = 60.0 * 1000.0 / spin_rpm;
	int l_silence = (int)round(ms_per_r * silent_section);
	int mspr = (int)round(ms_per_r);
	if( ((int64_t)now_ms % mspr) < l_silence){
		return 1.0;
	}
	return rate0;
}


static void test_get_loss_chance(){
	uint64_t nowms = 1;
	double rate0 = 0.1;
	double spin_rpm = randomd(0.5, 80.0);
	double silent_section = randomd(0.0, 0.99);
	int spin_on = 1;
	int N = (int) round( 1000000.0 / spin_rpm);
	double N_on = 0.0;
	double N_off = 0.0;
	for (int i = 0; i < N; ++i) {
		double r = get_loss_chance(nowms, rate0, spin_rpm, silent_section, spin_on);
		if(r == 1.0){
			N_off += 1.0;
		}
		if(r == rate0){
			N_on += 1.0;
		}
		nowms++;
	}
	double observed_silent_section = N_off / (N_on+N_off);
	double _rel = observed_silent_section - silent_section;

	//PRINTFF(0,"\n");
	//PRINTFF(0,"Ratio rel: %lf\n", observed_silent_section/silent_section);
	//PRINTFF(0,"Ratio target: %lf\n", silent_section);
	//PRINTFF(0,"Silent ratio: %lf\n", observed_silent_section);
	assert(fabs(_rel) < 0.02);
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
	payload->assigned_sequence = -100;
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

void destroy_eframe_list(EtherFrameList* etherFrameList){
	for (int i = 0; i < etherFrameList->n; ++i) {
		free(etherFrameList->array[i]);
	}
	free(etherFrameList->array);
	free(etherFrameList);
}

int eframe_list_append(EtherFrameList* elist, EtherFrame* etherFrame){
	elist->array[elist->n] = etherFrame;
	elist->n++;
	elist->array = realloc(elist->array, sizeof(EtherFrame*) * (elist->n + 3));
	return elist->n -1;
}

void eframe_list_pop(EtherFrameList* elist, int idx){
	if(idx >= elist->n){
		return;
	}
	memmove(&elist->array[idx], &elist->array[idx+1], sizeof(EtherFrame*)*(elist->n - idx));
	//elist->array[idx] = elist->array[elist->n];
	elist->n--;
	elist->array = realloc(elist->array, sizeof(EtherFrame*) * (elist->n + 3));
}


PayloadList* new_payload_list(){
	PayloadList* plist = malloc(sizeof(PayloadList));
	plist->array = malloc(sizeof(Payload*) * 4);
	plist->n = 0;
	return plist;
}

void destroy_payload_list(PayloadList* payloadList){
	for (int i = 0; i < payloadList->n; ++i) {
		destroy_payload(payloadList->array[i]);
	}
	free(payloadList->array);
	free(payloadList);
}

int payload_list_append(PayloadList* payloadList, Payload* payload){
	payloadList->array[payloadList->n] = payload;
	payloadList->n++;
	payloadList->array = realloc(payloadList->array, sizeof(Payload*) * (payloadList->n + 2));
	return payloadList->n -1;
}

void payload_list_mark_as_received(PayloadList* payloadList, void* msg, int msg_len, int sequence, int target, uint64_t ts_now){
	assert(msg_len >= 0);
	int found = 0;
	String* ref_string = new_string(msg ,msg_len);
	for (int i = 0; i < payloadList->n; ++i) {
		Payload* pl = payloadList->array[i];
		if(found){
			assert(pl->ts_rcv == 0);
		}
		//if(!found){
		//	assert(pl->ts_rcv > 0);
		//}
		if(pl->ts_rcv > 0){
			continue;
		}
		if(target != pl->target_peer){
			continue;
		}
		if(string_same(ref_string, pl->msg) && (pl->assigned_sequence == sequence) ){
			assert(pl->ts_rcv == 0);
			pl->ts_rcv = ts_now;
			found++;
		}
	}
	destroy_string(ref_string);
	if(found != 1){
		PRINTFF(0, "found: %d\n",found);
	}
	assert(found == 1);
}

int payload_list_count_unreceived(PayloadList* payloadList){
	int unreceived = 0;
	for (int i = 0; i < payloadList->n; ++i) {
		Payload* pl = payloadList->array[i];
		if(pl->ts_rcv == 0){
			unreceived++;
		} else {
			assert(pl->ts_rcv > pl->ts_push);
		}
	}
	return unreceived;
}

















void test1_round(uint64_t NN, int print_on){
	SkyConfig* config1 = new_vanilla_config();
	SkyConfig* config2 = new_vanilla_config();
	config1->identity[0] = 1;
	config2->identity[0] = 2;

	SkyHandle handle1 = new_handle(config1);
	SkyHandle handle2 = new_handle(config2);

	mac_shift_windowing(handle1->mac, rand()%3200);
	mac_shift_windowing(handle2->mac, rand()%3200);

	handle1->hmac->sequence_tx[0] = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	handle1->hmac->sequence_rx[0] = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	handle2->hmac->sequence_tx[0] = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	handle2->hmac->sequence_rx[0] = randint_i32(0, HMAC_CYCLE_LENGTH-1);

	TXRXJob job;
	job.peer1.handle = handle1;
	job.peer2.handle = handle2;
	job.peer1.radio_state = STATE_RX;
	job.peer2.radio_state = STATE_RX;
	job.peer1.under_send = NULL;
	job.peer2.under_send = NULL;
	job.peer1.tx_end = 0;
	job.peer2.tx_end = 0;
	job.now_ms = 9;
	job.byterate 		= 2*1200.0; //param
	job.lag_ms 			= 4; 		//param
	job.peer1.pl_rate 	= 4.0; 		//param
	job.peer2.pl_rate 	= 2.0; 		//param
	job.corrupt_rate	= 0.05; 	//param
	job.loss_rate0 		= 0.11; 	//param (0.12)
	job.spin_rate_rpm	= 6;		//param
	job.silent_section	= 0.24;		//param
	job.spin_on			= 1;		//param
	job.payloadList1 	= new_payload_list();
	job.payloadList2 	= new_payload_list();
	job.inEther 		= new_eframe_list();
	job.lostFrames 		= new_eframe_list();
	job.receivedFrames 	= new_eframe_list();
	job.missedFrames 	= new_eframe_list();

	skyArray_wipe_to_arq_init_state(handle1->arrayBuffers[0], job.now_ms);

	for (uint64_t i = 0; i < NN; ++i) {
		step_forward(1, &job);
		step_forward(2, &job);
		if((i % 60000 == 0) && print_on){
			PRINTFF(0,"\ni:%d \n",i);
			PRINTFF(0,"%dh %dmin %ds \n",i/(3600*1000),  (i%(3600*1000))/60000,  (i%60000)/1000);
			print_job_status(&job);
		}
		job.now_ms++;
	}

	//sleepms(2000);

	destroy_payload_list(job.payloadList1);
	destroy_payload_list(job.payloadList2);
	destroy_eframe_list(job.inEther);
	destroy_eframe_list(job.lostFrames);
	destroy_eframe_list(job.receivedFrames);
	destroy_eframe_list(job.missedFrames);

	destroy_config(config1);
	destroy_config(config2);
	destroy_handle(handle1);
	destroy_handle(handle2);
}













static void step_forward(int which, TXRXJob* job){
	uint8_t tgt[1000];
	int target	  = (which == 1) ? 2 : 1;
	JobPeer* peer = (which == 1) ? &job->peer1 : &job->peer2;
	PayloadList* plList = (which == 1) ? job->payloadList1 : job->payloadList2;
	PayloadList* peers_plList = (which == 1) ? job->payloadList2 : job->payloadList1;


	//generate new pl to send queue
	if((job->now_ms > 10000) && roll_chance(peer->pl_rate/1000.0) && !skyArray_send_buffer_is_full(peer->handle->arrayBuffers[0])){
		Payload* pl = new_random_payload(target, job->now_ms);
		int push_ret = skyArray_push_packet_to_send(peer->handle->arrayBuffers[0], pl->msg->data, pl->msg->length);
		assert(push_ret != RING_RET_RING_FULL);
		assert(push_ret != RING_RET_BUFFER_FULL);
		pl->assigned_sequence = push_ret;
		payload_list_append(plList, pl);
	}

	if(job->now_ms > 15500){
		int state_on1 = job->peer1.handle->arrayBuffers[0]->arq_state_flag == ARQ_STATE_ON;
		int state_on2 = job->peer2.handle->arrayBuffers[0]->arq_state_flag == ARQ_STATE_ON;
		if(!state_on1 || !state_on2){
			PRINTFF(0,"now:%ld   states_on:%d %d\n", job->now_ms, state_on1, state_on2);
		}
		assert(state_on1);
		assert(state_on2);
		int hmac_diff1 = job->peer2.handle->hmac->sequence_tx[0] - job->peer1.handle->hmac->sequence_rx[0];
		int hmac_diff2 = job->peer2.handle->hmac->sequence_rx[0] - job->peer1.handle->hmac->sequence_tx[0];
		hmac_diff1 = hmac_diff1 * hmac_diff1;
		hmac_diff2 = hmac_diff2 * hmac_diff2;
		//if((hmac_diff1 > 150) || (hmac_diff2 > 150)){
		//	PRINTFF(0,"HMAC DIFFS : %d  %d !\n", hmac_diff1, hmac_diff2);
		//}
		//assert(hmac_diff1 < 150);
		//assert(hmac_diff2 < 150);
	}

	//receive (or miss) transmissions from ether, and collect payloads.
	int i = -1;
	while ((i+1) < job->inEther->n) {
		i++;
		EtherFrame* eframe = job->inEther->array[i];
		//targeted to this peer?
		if(eframe->target_peer != which){
			continue;
		}

		assert(job->inEther->n < 60);
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
				assert(red >= 0);
				assert(seq >= 0);
				payload_list_mark_as_received(peers_plList, tgt, red, seq, which, job->now_ms);
			}
			eframe_list_append(job->receivedFrames, eframe);
			eframe_list_pop(job->inEther, i);
		}
		if(!(eframe->rx_ok)) {
			eframe_list_append(job->missedFrames, eframe);
			eframe_list_pop(job->inEther, i);
		}
	}


	//transmit new packet if sky_tx says so. Also corrup and maybe lose entirely.
	if(peer->radio_state != STATE_TX){
		int r_tx = sky_tx(peer->handle, &job->frame, 1, job->now_ms);
		if(r_tx){
			uint64_t tx_end = job->now_ms + ((job->frame.length * 1000) / job->byterate) + 1;
			EtherFrame* eframe = new_eframe(job->frame, job->now_ms, tx_end, target);
			corrupt_bytearray(eframe->frame.raw, eframe->frame.length, job->corrupt_rate);
			double loss_chance = get_loss_chance(job->now_ms, job->loss_rate0, job->spin_rate_rpm, job->silent_section, job->spin_on);
			int lost = roll_chance(loss_chance);
			if(lost){
				eframe_list_append(job->lostFrames, eframe);
			} else{
				eframe_list_append(job->inEther, eframe);
			}
			peer->radio_state = STATE_TX;
			peer->tx_end = tx_end;
		}
	}


	if(peer->radio_state == STATE_TX){
		assert(peer->tx_end >= job->now_ms);
	}
	if((peer->radio_state == STATE_TX) && (peer->tx_end == job->now_ms)){
		peer->radio_state = STATE_RX;
	}
}





static void print_job_status(TXRXJob* job){
	PRINTFF(0, "=====================================\n");
	PRINTFF(0,"Frames created: %d\n", job->inEther->n + job->missedFrames->n + job->lostFrames->n + job->receivedFrames->n);
	PRINTFF(0,"Frames lost: %d\n", job->lostFrames->n);
	PRINTFF(0,"Frames in ether: %d\n", job->inEther->n);
	PRINTFF(0,"Frames received: %d\n", job->receivedFrames->n);
	PRINTFF(0,"Frames missed: %d\n", job->missedFrames->n);
	PRINTFF(0,"Payloads created 1: %d\n", job->payloadList1->n);
	PRINTFF(0,"Payloads created 2: %d\n", job->payloadList2->n);
	int unacked_of_1 = payload_list_count_unreceived(job->payloadList1);
	int unacked_of_2 = payload_list_count_unreceived(job->payloadList2);
	PRINTFF(0,"Payloads acked 1: %d\n", job->payloadList1->n - unacked_of_1 );
	PRINTFF(0,"Payloads acked 2: %d\n", job->payloadList2->n - unacked_of_2 );
	PRINTFF(0,"Payloads not acked 1: %d\n", unacked_of_1);
	PRINTFF(0,"Payloads not acked 2: %d\n", unacked_of_2);
	PRINTFF(0, "=====================================\n");
}






