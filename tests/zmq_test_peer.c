//
// Created by elmore on 7.11.2021.
//

#include "tx_rx_zmq.h"
#include "tools/tools.h"

int main(int argc, char *argv[]) {
	reseed_random();
	tx_rx_zmq_test(argc, argv);
}

