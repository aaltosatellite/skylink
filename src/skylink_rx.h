//
// Created by elmore on 29.10.2021.
//

#ifndef SKYLINK_CMAKE_SKYLINK_RX_H
#define SKYLINK_CMAKE_SKYLINK_RX_H

#include "skylink/skylink.h"
#include "skylink/skypacket.h"
#include "skylink/mac_2.h"
#include "skylink/hmac.h"
#include "skylink/arq_ring.h"
#include "skylink/fec.h"
#include "skylink/utilities.h"


int sky_rx_0(SkyHandle self, SkyRadioFrame* frame);


#endif //SKYLINK_CMAKE_SKYLINK_RX_H
