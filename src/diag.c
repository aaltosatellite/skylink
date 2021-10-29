//
// Created by elmore on 28.10.2021.
//
#include "diag.h"
#include "skylink/skylink.h"



SkyDiagnostics* new_diagnostics(){
	SkyDiagnostics* diag = SKY_MALLOC(sizeof(SkyDiagnostics));
	memset(diag, 0, sizeof(SkyDiagnostics));
	return diag;
}

void destroy_diagnostics(SkyDiagnostics* diag){
	free(diag);
}


