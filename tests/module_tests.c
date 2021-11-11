//
// Created by elmore on 8.10.2021.
//

#include "elebuffer_tests.h"
#include "ring_test.h"
#include "packet_encode_test.h"
#include "hmac_tests.h"
#include "fec_test.h"
#include "tx_rx_cycle_test.h"
#include "tools/tools.h"

void foobar(){
	for (int i = 0; i < 100; ++i) {
		int32_t A = 0;
		int32_t B = 0;
		fillrand(&A, 4);
		fillrand(&B, 4);
		int32_t D = ( (A-B) & 0xfffffff );
		PRINTFF(0, "%d  \n",D);
	}
}


int main() {
	reseed_random();


	foobar();
	quick_exit(1);


	elebuffer_tests();
	ring_tests();
	packet_tests();
	fec_test();
	hmac_tests();
	txrx_tests();
}


