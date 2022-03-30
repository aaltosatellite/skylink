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


int main() {
	reseed_random();


	/*
	for (int i = -90000000; i < 90000000; ++i) {
		assert(__modulo(i) == posmod(i));
		assert(__modulo(i) == positive_modulo(i, MODULO_NUMBER));
	}
	PRINTFF(0, "Jes! \n");
	*/
	uint64_t t0 = real_microseconds();

	elebuffer_tests();
	packet_tests();
	fec_test();
	hmac_tests();
	mac_test(15);
	ring_tests(15);
	arq_system_test1(21);
	arq_system_test2(21);
	arq_system_test3(21);
	sky_tx_test(20);

	uint64_t t1 = real_microseconds();

	PRINTFF(0,"Time: %ld ms.\n", (t1-t0)/ 1000);
}


