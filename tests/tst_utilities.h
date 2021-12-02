//
// Created by elmore on 28.10.2021.
//

#ifndef SKYLINK_CMAKE_TST_UTILITIES_H
#define SKYLINK_CMAKE_TST_UTILITIES_H

#include "skylink/skylink.h"
#include "skylink/conf.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/frame.h"

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

uint16_t spin_to_seq(SkyVirtualChannel* sring, SkyVirtualChannel* rring, int target_sequence, int32_t now_ms);

void populate_horizon(SkyVirtualChannel* sring, SkyVirtualChannel* rring, int final_tx_head_seq, int final_rx_head_seq, uint16_t target_mask, int32_t now_ms, String** payloads);

SkyPacketExtension* get_extension(SkyRadioFrame* frame, unsigned int extension_type);

int roll_chance(double const chance);

uint8_t get_other_byte(uint8_t c);

void corrupt_bytearray(uint8_t* arr, int length, double ratio);

void tst_randoms(double chance1, double chance2, int NN);

#endif //SKYLINK_CMAKE_TST_UTILITIES_H
