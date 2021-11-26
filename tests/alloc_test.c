//
// Created by elmore on 23.11.2021.
//


#include "tst_utilities.h"
#include "tools/tools.h"
#include "../src/skylink/utilities.h"


int main() {
	reseed_random();
	PRINTFF(0,"-- x --\n");
	SkyConfig* config = new_vanilla_config();
	PRINTFF(0,"-- x --\n");
	new_handle(config);
	PRINTFF(0,"-- x --\n");
	report_allocation();
}


