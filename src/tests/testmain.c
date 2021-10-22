//
// Created by elmore on 8.10.2021.
//

#include "tests/elebuffer_tests.h"
#include "tests/tools/tools.h"

int main() {
	PRINTFF(0,"A\n");
	reseed_random();
	test1();
	test2();

	test_ratios();
}


