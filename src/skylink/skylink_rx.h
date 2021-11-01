//
// Created by elmore on 29.10.2021.
//

#ifndef SKYLINK_CMAKE_SKYLINK_RX_H
#define SKYLINK_CMAKE_SKYLINK_RX_H

#include "skylink.h"
#include "skypacket.h"
#include "mac_2.h"
#include "hmac.h"
#include "arq_ring.h"
#include "fec.h"
#include "utilities.h"


int sky_rx_0(SkyHandle self, SkyRadioFrame* frame);


#endif //SKYLINK_CMAKE_SKYLINK_RX_H
