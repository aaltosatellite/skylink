
#include "tst_utilities.h"
#include "tools/tools.h"
#include "skylink/utilities.h"
#include "sky_platform.h"


size_t allocated_bytes = 0;
int allocations = 0;

void *instrumented_malloc(const char *where, size_t size)
{
	allocated_bytes += size;
	allocations++;
	printf("   %s allocating %ld\n", where, size);
	fflush(stdout);
	return malloc(size);
}

void instrumented_free(const char *where, void *ptr)
{
	printf("  (free %x)\n", (unsigned int)(uintptr_t)ptr);
	fflush(stdout);
}


int main() {
	
	reseed_random();

	PRINTFF(0,"\n-- Creating vanilla config --\n");
	SkyConfig* config = new_vanilla_config();
	
	PRINTFF(0,"\n-- Initializing sky_handle --\n");
	new_handle(config);
	
	//PRINTFF(0,"\n--  --\n");

	printf("=======================\n");
	printf("  %ld bytes allocated\n", allocated_bytes);
	printf("  %d allocations\n", allocations);
	printf("=======================\n");

	return 0;
}


