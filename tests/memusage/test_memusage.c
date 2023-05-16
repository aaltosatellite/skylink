
#include "tst_utilities.h"
#include "tools.h"
#include "skylink/utilities.h"
#include "sky_platform.h"


size_t allocated_bytes = 0;
int allocations = 0;

void *instrumented_malloc(const char *where, size_t size)
{
	allocated_bytes += size;
	allocations++;
	printf("   %s allocating %ld bytes\n", where, size);
	fflush(stdout);
	return malloc(size);
}

void instrumented_free(const char *where, void *ptr)
{
	printf("  %s freeing 0x%x\n", where, (unsigned int)(uintptr_t)ptr);
	fflush(stdout);
	free(ptr);
}


int main() {
	
	reseed_random();

	printf("\n-- Creating vanilla config --\n");
	SkyConfig* config = new_vanilla_config();
	
	printf("\n-- Initializing sky_handle --\n");
	SkyHandle handle = sky_create(config);


	printf("=======================\n");
	printf("  %ld bytes allocated\n", allocated_bytes);
	printf("  %d allocations\n", allocations);
	printf("=======================\n");

	sky_destroy(handle);
	free(config);
	return 0;
}


