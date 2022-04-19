//
// Created by elmore on 14.4.2022.
//

#ifndef SKYLINK_AMATEUR_RADIO_H
#define SKYLINK_AMATEUR_RADIO_H

#include "../src/skylink/skylink.h"
#include "../src/skylink/frame.h"
#include "../src/skylink/fec.h"
#include "../src/skylink/mac.h"
#include "../src/skylink/hmac.h"
#include "../src/skylink/utilities.h"


int skylink_encode_amateur_pl(uint8_t* identity, uint8_t* pl, int32_t pl_len, uint8_t* tgt, int insert_golay);


#endif //SKYLINK_AMATEUR_RADIO_H
