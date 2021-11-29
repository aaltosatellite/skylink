//
// Created by elmore on 28.11.2021.
//

#include "sky_rx_test.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/utilities.h"
#include "../src/skylink/fec.h"
#include "tst_utilities.h"
#include "tools/tools.h"
#include <assert.h>

void sky_rx_test_cycle();

void sky_rx_test(int load){
	PRINTFF(0, "[sky_tx test: randomized state]\n");
	for (int i = 0; i < load*1000 +1; ++i) {
		sky_rx_test_cycle();
		if(i%1000 == 0){
			PRINTFF(0,"\ti=%d\n",i);
		}
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void sky_rx_test_cycle(){
	SkyConfig* conf = new_vanilla_config();
	SkyConfig* conf2 = new_vanilla_config();
	SkyRadioFrame* frame = new_frame();


	int golay_on = randint_i32(0,1) == 1;
	int pl_in = randint_i32(1,10) <= 4;
	int len_pl = randint_i32(0,SKY_MAX_PAYLOAD_LEN);
	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);


	SkyHandle self = new_handle(conf);
	SkyHandle self2 = new_handle(conf2);







	sky_rx(self, frame, golay_on);


	destroy_frame(frame);
	destroy_handle(self);
	destroy_handle(self2);
	destroy_config(conf);
	destroy_config(conf2);
}





