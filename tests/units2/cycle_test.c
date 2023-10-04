/*
PORT OF ORIGINAL CYCLE TEST TO NARWHAL FRAMEWORK. WORK IN PROGRESS.
*/


#include <math.h>
#include "units.h"
#include "tools.h"
#include "tst_utilities.h"

#include "skylink/skylink.h"
#include "skylink/element_buffer.h"
#include "skylink/hmac.h"
#include "skylink/mac.h"
#include "skylink/reliable_vc.h"
#include "skylink/utilities.h"

#define HMAC_CYCLE_LENGTH	65000
#define STATE_RX		0
#define STATE_TX		1


extern sky_tick_t _global_ticks_now;
double corrupt_scaler = 10;
int spin_on_glob = 0;

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
	uint64_t now;
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

static void step_forward(int which, TXRXJob* job);

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
	//elist->vc[idx] = elist->vc[elist->n];
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
		if((!found) && (i > 0)){
			assert(payloadList->array[i-1]->ts_rcv > 0);
		}
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
			//PRINTFF(0, "\t%d Acked pl of len %d\n", target, msg_len);
		}
	}
	destroy_string(ref_string);
	/*
	if(found != 1){
		PRINTFF(0,"\n%d receiving.\n", target);
		PRINTFF(0, "found: %d. Msg_len:%d. byte0:%d byte1:%d time_now:%d\n",found, msg_len, ((uint8_t*)msg)[0], ((uint8_t*)msg)[1], ts_now);
		int unrec = payload_list_count_unreceived(payloadList);
		PRINTFF(0, "Peer unreceived:%d\n", unrec );
		for (int i = 0; i < unrec; ++i) {
			Payload* nth_unrec = payloadList->array[payloadList->n - (unrec - i)];
			PRINTFF(0, "\tLen of %dth unrec:%d\n", i, nth_unrec->msg->length );
		}
	}
	*/
	assert(found == 1);
}

#define DISABLE_TEST_DISCOVERY 1

TEST(loss_test){
	PRINTFF(0,"\n\t(testing loss function...");
	for (int i = 0; i < 1000; ++i) {
		uint64_t now_ms = 1;
        double rate0 = 0.1;
        double spin_rpm = randomd(0.5, 80.0);
        double silent_section = randomd(0.0, 0.99);
        int spin_on = 1;
        int N = (int) round( 1000000.0 / spin_rpm);
        double N_on = 0.0;
        double N_off = 0.0;
        for (int i = 0; i < N; ++i) {
            double r = get_loss_chance(now_ms, rate0, spin_rpm, silent_section, spin_on);
            if(r == 1.0){
                N_off += 1.0;
            }
            if(r == rate0){
                N_on += 1.0;
            }
            now_ms++;
        }
        double observed_silent_section = N_off / (N_on+N_off);
        double _rel = observed_silent_section - silent_section;
        ASSERT(fabs(_rel) < 0.02, "observed_silent_section: %f, silent_section: %f, _rel: %f, fabs(_rel): %f", observed_silent_section, silent_section, _rel, fabs(_rel));
    }
    printf("Loss test ok)\n");
}


TEST(test1_round){
	SkyConfig* config1 = SKY_MALLOC(sizeof(SkyConfig));
	SkyConfig* config2 = SKY_MALLOC(sizeof(SkyConfig));
	default_config(config1);
	default_config(config2);
	config1->identity[0] = 1;
	config2->identity[0] = 2;

	SkyHandle handle1 = sky_create(config1);
	SkyHandle handle2 = sky_create(config2);

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
	job.now = 9;
	job.byterate 		= 2*1200.0; //param
	job.lag_ms 			= 6; 		//param
	job.peer1.pl_rate 	= 3.0; 		//param
	job.peer2.pl_rate 	= 2.0; 		//param
	job.corrupt_rate	= 0.05; 	//param
	job.loss_rate0 		= 0.12; 	//param (0.12)
	job.spin_rate_rpm	= 6;		//param
	job.silent_section	= 0.24;		//param
	job.spin_on			= spin_on_glob;	//param
	job.payloadList1 	= new_payload_list();
	job.payloadList2 	= new_payload_list();
	job.inEther 		= new_eframe_list();
	job.lostFrames 		= new_eframe_list();
	job.receivedFrames 	= new_eframe_list();
	job.missedFrames 	= new_eframe_list();
	sky_tick(job.now);
	sky_vc_wipe_to_arq_init_state(handle1->virtual_channels[0]);
	uint64_t until =  15600; //Gets stuck when until > 15850
	for (uint64_t i = 0; i < until; ++i) {
		job.now++;
		sky_tick(job.now);
		step_forward(1, &job);
		step_forward(2, &job);
	}

	//sleepms(2000);

	destroy_payload_list(job.payloadList1);
	destroy_payload_list(job.payloadList2);
	destroy_eframe_list(job.inEther);
	destroy_eframe_list(job.lostFrames);
	destroy_eframe_list(job.receivedFrames);
	destroy_eframe_list(job.missedFrames);

	free(config1);
	free(config2);
	sky_destroy(handle1);
	sky_destroy(handle2);
}
#undef DISABLE_TEST_DISCOVERY

static void step_forward(int which, TXRXJob* job){
	//uint8_t tgt[1000];
	int target	  = (which == 1) ? 2 : 1;
	JobPeer* peer = (which == 1) ? &job->peer1 : &job->peer2;
	PayloadList* plList = (which == 1) ? job->payloadList1 : job->payloadList2;
	//PayloadList* peers_plList = (which == 1) ? job->payloadList2 : job->payloadList1;

	//generate new pl to send queue
	if((job->now > 10000) && roll_chance(peer->pl_rate / 1000.0) && !sky_vc_send_buffer_is_full(
			peer->handle->virtual_channels[0])){
		Payload* pl = new_random_payload(target, job->now);
		int push_ret = sky_vc_push_packet_to_send(peer->handle->virtual_channels[0], pl->msg->data, pl->msg->length);
		ASSERT(push_ret != SKY_RET_RING_RING_FULL, "push_ret: %d", push_ret);
		ASSERT(push_ret != SKY_RET_RING_BUFFER_FULL, "push_ret: %d", push_ret);
		ASSERT(push_ret >= 0, "push_ret: %d", push_ret);
		pl->assigned_sequence = push_ret;
		payload_list_append(plList, pl);
		/*
		PRINTFF(0, "%d Generated pl of len: %d. First bytes: ",which, pl->msg->length, pl->msg->data);
		for (int i = 0; i < 2; ++i) {
			if(i < pl->msg->length){
				PRINTFF(0,"%d ", pl->msg->data[i]);
			}
		}
		PRINTFF(0,"\n");
		*/
	}

	if(job->now > 15500){
		int state_on1 = job->peer1.handle->virtual_channels[0]->arq_state_flag == ARQ_STATE_ON;
		int state_on2 = job->peer2.handle->virtual_channels[0]->arq_state_flag == ARQ_STATE_ON;
		if(!state_on1 || !state_on2){
			FAIL("ARQ state not on. now:%ld   states_on:%d %d\n", job->now, state_on1, state_on2);
			//PRINTFF(0, "now:%ld   states_on:%d %d\n", job->now, state_on1, state_on2);
		}
		ASSERT(state_on1, "now:%ld   states_on:%d %d\n", job->now, state_on1, state_on2);
		ASSERT(state_on2, "now:%ld   states_on:%d %d\n", job->now, state_on1, state_on2);
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
}
