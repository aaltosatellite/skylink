
#include "skylink/skylink.h"
#include "skylink/diag.h"

unsigned int sky_diag_mask;

SkyDiagnostics* new_diagnostics(){
	SkyDiagnostics* diag = SKY_MALLOC(sizeof(SkyDiagnostics));
	memset(diag, 0, sizeof(SkyDiagnostics));
	return diag;
}

void destroy_diagnostics(SkyDiagnostics* diag){
	SKY_FREE(diag);
}
