//
// Created by elmore on 7.11.2021.
//

#include "phy.h"


SkyPhysical* new_physical(){
	SkyPhysical* phy = SKY_MALLOC(sizeof(SkyPhysical));
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		phy->frames_sent_in_current_window_per_vc[i] = 0;
	}
	phy->radio_mode = MODE_RX;
	phy->total_frames_sent_in_current_window = 0;
	return phy;
}

void destroy_physical(SkyPhysical* phy){
	free(phy);
}


void turn_to_tx(SkyPhysical* phy){
	if(phy->radio_mode != MODE_TX){
		for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
			phy->frames_sent_in_current_window_per_vc[i] = 0;
		}
		phy->total_frames_sent_in_current_window = 0;
		phy->radio_mode = MODE_TX;
	}
}

void turn_to_rx(SkyPhysical* phy){
	if(phy->radio_mode != MODE_RX){
		phy->radio_mode = MODE_RX;
	}
}