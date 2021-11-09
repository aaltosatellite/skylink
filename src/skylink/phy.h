//
// Created by elmore on 7.11.2021.
//

#ifndef SKYLINK_CMAKE_PHY_H
#define SKYLINK_CMAKE_PHY_H

#include "skylink.h"

SkyPhysical* new_physical();

void destroy_physical(SkyPhysical* phy);

void turn_to_tx(SkyPhysical* phy);

void turn_to_rx(SkyPhysical* phy);


#endif //SKYLINK_CMAKE_PHY_H
