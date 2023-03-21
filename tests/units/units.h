#ifndef __UNITS_H__
#define __UNITS_H__

#include "tst_utilities.h"
#include "tools.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>



// arq_test1.c
void arq_system_test1(int load);
void arq_system_test2(int load);

// arq_test2.c
void arq_system_test3(int load);

// elebuffer_test.c
void elebuffer_tests();

// hmac_test.c
void hmac_tests(int load);

// mac_test.c
void mac_test(int load);

// packet_encode_test.c
void packet_tests();

// fec_test.c
void fec_test();

// ring_test.c
void ring_tests(int load);

// rx_test.c
void sky_rx_test(int load);

// tx_test.c
void sky_tx_test(int load);


#endif /* __UNITS_H__ */