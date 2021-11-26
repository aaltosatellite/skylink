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

int main() {
	reseed_random();

	elebuffer_tests();
	packet_tests();
	fec_test();
	hmac_tests();
	ring_tests(15);
	arq_tests(21);
	arq_tests2(21);
}


