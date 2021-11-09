//
// Created by elmore on 28.10.2021.
//

#ifndef SKYLINK_CMAKE_TST_UTILITIES_H
#define SKYLINK_CMAKE_TST_UTILITIES_H

#include "../src/skylink/skylink.h"
#include "../src/skylink/conf.h"
#include "../src/skylink/mac.h"
#include "../src/skylink/hmac.h"
#include "../src/skylink/phy.h"
#include "tools/tools.h"



typedef struct test_job_vc {
	int ring_len;
	int hmac_on1;
	int hmac_on2;
	int arq_on1;
	int arq_on2;
	int horizon1;
	int horizon2;
	int recall1;
	int recall2;
	int auth_seq_1to2;
	int auth_seq_2to1;
	int auth1_tx_shift;
	int auth2_tx_shift;
	int arq_seq_1to2;
	int arq_seq_2to1;
	int tx_ahead1;
} TestJobVC;


typedef struct test_job {
	SkyHandle handle1;
	SkyHandle handle2;
	int max_jump;
	TestJobVC vcjobs[SKY_NUM_VIRTUAL_CHANNELS];
} TestJob;



SkyConfig* new_vanilla_config();

SkyHandle new_handle(SkyConfig* config);

void destroy_config(SkyConfig* config);

void destroy_handle(SkyHandle self);

uint16_t spin_to_seq(SkyArqRing* ring1, SkyArqRing* ring2, int target_sequence, int first_ahead);

#endif //SKYLINK_CMAKE_TST_UTILITIES_H
