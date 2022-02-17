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
