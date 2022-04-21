//
// Created by elmore on 8.10.2021.
//

#include "elebuffer_test.h"
#include "ring_test.h"
#include "packet_encode_test.h"
#include "hmac_test.h"
#include "fec_test.h"
#include "tools/tools.h"
#include "arq_test1.h"
#include "arq_test2.h"
#include "sky_tx_test.h"

void mac_test(int load);

#define MODULO_NUMBER 4096

int __modulo(int i){
	return ((i % MODULO_NUMBER) + MODULO_NUMBER) % MODULO_NUMBER;
}

int posmod(int i){
	return i & 0xFFF;
}


void print_struct_alignments(){
	printf("PHY: %ld\n", sizeof(SkyPHYConfig));
	printf("MAC: %ld\n", sizeof(SkyMACConfig ));
	printf("VC: %ld\n", sizeof(SkyVCConfig ));
	printf("HMAC: %ld\n", sizeof(HMACConfig));
	printf("SKY: %ld\n", sizeof(SkyConfig));
	SkyMACConfig mcc;
	memset(&mcc, 0, sizeof(SkyMACConfig));
	mcc.window_adjustment_period = 1;
	mcc.maximum_window_length_ticks = 1;
	mcc.minimum_window_length_ticks = 1;
	mcc.unauthenticated_mac_updates = 1;
	mcc.gap_constant_ticks = 1;
	mcc.tail_constant_ticks = 1;
	mcc.carrier_sense_ticks = 1;
	mcc.idle_frames_per_window = 1;
	mcc.idle_timeout_ticks = 1;
	mcc.shift_threshold_ticks = 1;
	mcc.window_adjust_increment_ticks = 1;
	uint8_t* p = (uint8_t*)(&mcc);
	for (int i = 0; i < (int)sizeof(SkyMACConfig); ++i) {
		uint8_t x = *(p+i);
		printf("%x ", x);
	}
	printf("\n");

	SkyConfig* conf = new_vanilla_config();
	memset(conf, 0, sizeof(SkyConfig));
	*(uint8_t*)(&conf->phy) = 0xa;
	*(uint8_t*)(&conf->mac) = 0xb;
	*(uint8_t*)(&conf->hmac) = 0xc;
	*(uint8_t*)(&conf->vc[0]) = 0x1;
	*(uint8_t*)(&conf->vc[1]) = 0x2;
	*(uint8_t*)(&conf->vc[2]) = 0x3;
	*(uint8_t*)(&conf->vc[3]) = 0x4;
	*(uint8_t*)(&conf->identity) = 0xf;
	*(uint8_t*)(&conf->arq_timeout_ticks) = 0xaa;
	*(uint8_t*)(&conf->arq_idle_frame_threshold) = 0xbb;
	*(uint8_t*)(&conf->arq_idle_frames_per_window) = 0xcc;

	for (int i = 0; i < (int)sizeof(SkyConfig); ++i) {
		uint8_t* cp = (uint8_t*)conf;
		uint8_t c = *(uint8_t*)(cp+i);
		if(c != 0){
			printf("\n%d: ", i);
		}
		printf("%x", c);
	}
	printf("\n%ld", sizeof(SkyConfig));

}


int main() {
	reseed_random();
	//print_struct_alignments();

	uint64_t t0 = real_microseconds();

	elebuffer_tests();
	packet_tests();
	fec_test();
	hmac_tests(15);
	mac_test(15);
	ring_tests(15);
	//arq_system_test1(21);
	//arq_system_test2(21);
	//arq_system_test3(21);
	sky_tx_test(21);

	uint64_t t1 = real_microseconds();

	PRINTFF(0,"Time: %ld ms.\n", (t1-t0)/ 1000);
}


