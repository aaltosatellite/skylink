//
// Created by elmore on 28.10.2021.
//

#ifndef SKYLINK_CMAKE_TST_UTILITIES_H
#define SKYLINK_CMAKE_TST_UTILITIES_H

#include "../skylink/skylink.h"
#include "../skylink/conf.h"
#include "../skylink/mac_2.h"
#include "../skylink/hmac.h"
#include "tools/tools.h"


SkyConfig* new_vanilla_config();

SkyHandle new_handle(SkyConfig* config);

void destroy_config(SkyConfig* config);

void destroy_handle(SkyHandle self);

#endif //SKYLINK_CMAKE_TST_UTILITIES_H
