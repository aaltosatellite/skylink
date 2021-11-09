//
// Created by elmore on 7.11.2021.
//

#include "elebuffer_tests.h"
#include "ring_test.h"
#include "packet_encode_test.h"
#include "hmac_tests.h"
#include "fec_test.h"
#include "tx_rx_cycle_test.h"
#include "tx_rx_zmq.h"
#include "zmq_trial.h"
#include "tools/tools.h"

int main(int argc, char *argv[]) {
	reseed_random();
	tx_rx_zmq_test(argc, argv);
}

