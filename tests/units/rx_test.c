#include "units.h"

#include "skylink/skylink.h"
#include "skylink/utilities.h"
#include "skylink/fec.h"


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
	int len_pl = randint_i32(0,SKY_MAX_PAYLOAD_LEN * pl_in);
	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);
	int auth_required[SKY_NUM_VIRTUAL_CHANNELS];
	int arq_on[SKY_NUM_VIRTUAL_CHANNELS];
	int rcv_horizon_len[SKY_NUM_VIRTUAL_CHANNELS];
	int rcv_head_s[SKY_NUM_VIRTUAL_CHANNELS];
	int stuff_in_horizon[SKY_NUM_VIRTUAL_CHANNELS];
	int horizon_mask[SKY_NUM_VIRTUAL_CHANNELS];
	int tx_head_s[SKY_NUM_VIRTUAL_CHANNELS];
	int tx_tx_head_s[SKY_NUM_VIRTUAL_CHANNELS];
	int tx_tail_s[SKY_NUM_VIRTUAL_CHANNELS];


	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		auth_required[i] = randint_i32(1,10) <= 5;


	}



	SkyHandle self = new_handle(conf);
	SkyHandle self2 = new_handle(conf2);




	sky_rx(self, frame);//, golay_on);


	destroy_frame(frame);
	destroy_handle(self);
	destroy_handle(self2);
	destroy_config(conf);
	destroy_config(conf2);
}





