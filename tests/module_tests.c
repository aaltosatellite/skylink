//
// Created by elmore on 8.10.2021.
//

#include "elebuffer_tests.h"
#include "ring_test.h"
#include "packet_encode_test.h"
#include "hmac_tests.h"
#include "fec_test.h"
#include "tools/tools.h"
#include "arq_tests.h"
#include "arq_tests2.h"
#include "sky_tx_test.h"

void mac_test(int load);

int main() {
	reseed_random();

	elebuffer_tests();
	packet_tests();
	fec_test();
	hmac_tests();
	mac_test(10);
	ring_tests(15);
	arq_system_test1(21);
	arq_system_test2(21);
	arq_system_test3(21);
	sky_tx_test(12);
}


