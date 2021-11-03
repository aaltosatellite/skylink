//
// Created by elmore on 29.10.2021.
//

#ifndef SKYLINK_CMAKE_SKYLINK_TX_H
#define SKYLINK_CMAKE_SKYLINK_TX_H

#include "skylink.h"
#include "skypacket.h"
#include "mac_2.h"
#include "hmac.h"
#include "arq_ring.h"
#include "fec.h"
#include "utilities.h"


int sky_tx(SkyHandle self, SendFrame* frame, uint8_t vc, int insert_golay);


#endif //SKYLINK_CMAKE_SKYLINK_TX_H
