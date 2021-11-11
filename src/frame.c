//
// Created by elmore on 16.10.2021.
//

#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/utilities.h"


SkyRadioFrame* new_send_frame(){
	SkyRadioFrame* frame = SKY_MALLOC(sizeof(SkyRadioFrame));
	memset(frame, 0, sizeof(SkyRadioFrame));
	return frame;
}

SkyRadioFrame* new_receive_frame(){
	SkyRadioFrame* frame = SKY_MALLOC(sizeof(SkyRadioFrame));
	memset(frame, 0, sizeof(SkyRadioFrame));
	return frame;
}

void destroy_receive_frame(SkyRadioFrame* frame){
	SKY_FREE(frame);
}
void destroy_send_frame(SkyRadioFrame* frame){
	SKY_FREE(frame);
}





//=== ENCODING =========================================================================================================
//======================================================================================================================
int sky_packet_add_extension_arq_sequence(SkyRadioFrame* frame, uint8_t sequence) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_SEQUENCE;
	extension->length = sizeof(ExtARQSeq);
	extension->ARQSeq.sequence = sequence;

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}

int sky_packet_add_extension_arq_request(SkyRadioFrame* frame, uint8_t sequence, uint16_t mask) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_REQUEST;
	extension->length = sizeof(ExtARQReq);
	extension->ARQReq.sequence = sequence;
	extension->ARQReq.mask = sky_hton16(mask);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_arq_reset(SkyRadioFrame* frame, uint8_t toggle, uint8_t sequence) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_RESET;
	extension->length = sizeof(ExtARQReset);
	extension->ARQReset.toggle = toggle;
	extension->ARQReset.enforced_sequence = sequence;

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_mac_params(SkyRadioFrame* frame, uint16_t gap_size, uint16_t window_size) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_MAC_PARAMETERS;
	extension->length = sizeof(ExtTDDParams);
	extension->TDDParams.gap_size = sky_hton16(gap_size);
	extension->TDDParams.window_size = sky_hton16(window_size);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_mac_tdd_control(SkyRadioFrame* frame, uint16_t window, uint16_t remaining) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_MAC_TDD_CONTROL;
	extension->length = sizeof(ExtTDDControl);
	extension->TDDControl.window = sky_hton16(window);
	extension->TDDControl.remaining = sky_hton16(remaining);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_hmac_sequence_reset(SkyRadioFrame* frame, uint16_t sequence) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_HMAC_SEQUENCE_RESET;
	extension->length = sizeof(ExtHMACSequenceReset);
	extension->HMACSequenceReset.sequence = sky_hton16(sequence);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}



int available_payload_space(SkyRadioFrame* radioFrame) {
	return RS_MSGLEN - (radioFrame->length + SKY_HMAC_LENGTH);
}


int sky_packet_extend_with_payload(SkyRadioFrame* frame, void* pl, int32_t length){
	if(available_payload_space(frame) < length){
		return SKY_RET_NO_SPACE_FOR_PAYLOAD;
	}
	memcpy(frame->raw + EXTENSION_START_IDX + frame->ext_length, pl, length);
	frame->length = EXTENSION_START_IDX + frame->ext_length + length;
	return 0;
}
//=== ENCODING =========================================================================================================
//======================================================================================================================
