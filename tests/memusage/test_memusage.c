
#include "tst_utilities.h"
#include "tools/tools.h"
#include "skylink/utilities.h"


size_t allocated_bytes = 0;
int allocations = 0;

void* instrumented_malloc(size_t size)
{
	allocated_bytes += size;
	allocations++;
	printf("  (allocating %ld)\n", size);
	fflush(stdout);
	return malloc(size);
}


void instrumented_free(void *ptr)
{
	printf("  (free %x)\n", (unsigned int)ptr);
	fflush(stdout);
}


int main() {
	
	reseed_random();

	PRINTFF(0,"-- x --\n");
	SkyConfig* config = new_vanilla_config();
	PRINTFF(0,"-- x --\n");
	new_handle(config);
	PRINTFF(0,"-- x --\n");

	printf("=====================\n");
	printf("%ld bytes allocated.\n", allocated_bytes);
	printf("%d allocations.\n", allocations);
	printf("=====================\n");

	return 0;
}


