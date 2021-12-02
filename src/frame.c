//
// Created by elmore on 16.10.2021.
//

#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/utilities.h"
#include "skylink/platform.h"



SkyRadioFrame* new_frame(){
	SkyRadioFrame* frame = SKY_MALLOC(sizeof(SkyRadioFrame));
	memset(frame, 0, sizeof(SkyRadioFrame));
	return frame;
}

void destroy_frame(SkyRadioFrame* frame) {
	SKY_FREE(frame);
}

void sky_frame_clear(SkyRadioFrame* frame) {
	memset(frame, 0, sizeof(SkyRadioFrame));
}



//=== ENCODING =========================================================================================================
//======================================================================================================================
int sky_packet_add_extension_arq_sequence(SkyRadioFrame* frame, arq_seq_t sequence) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_SEQUENCE;
	extension->length = sizeof(ExtARQSeq) +1;
	extension->ARQSeq.sequence = sky_hton16(sequence);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_arq_request(SkyRadioFrame* frame, arq_seq_t sequence, uint16_t mask) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_REQUEST;
	extension->length = sizeof(ExtARQReq) +1;
	extension->ARQReq.sequence = sky_hton16(sequence);
	extension->ARQReq.mask = sky_hton16(mask);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_arq_ctrl(SkyRadioFrame* frame, arq_seq_t tx_head_sequence, arq_seq_t rx_head_sequence){
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_CTRL;
	extension->length = sizeof(ExtARQCtrl) +1;
	extension->ARQCtrl.tx_sequence = sky_hton16(tx_head_sequence);
	extension->ARQCtrl.rx_sequence = sky_hton16(rx_head_sequence);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_arq_handshake(SkyRadioFrame* frame, uint8_t state_flag, uint32_t identifier){
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_HANDSHAKE;
	extension->length = sizeof(ExtARQHandshake) +1;
	extension->ARQHandshake.peer_state = state_flag;
	extension->ARQHandshake.identifier = sky_ntoh32(identifier);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_mac_tdd_control(SkyRadioFrame* frame, uint16_t window, uint16_t remaining) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_MAC_TDD_CONTROL;
	extension->length = sizeof(ExtTDDControl) +1;
	extension->TDDControl.window = sky_hton16(window);
	extension->TDDControl.remaining = sky_hton16(remaining);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_packet_add_extension_hmac_sequence_reset(SkyRadioFrame* frame, uint16_t sequence) {
	SkyPacketExtension* extension = (SkyPacketExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_HMAC_SEQUENCE_RESET;
	extension->length = sizeof(ExtHMACSequenceReset) +1;
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



SkyPacketExtension* sky_rx_get_extension(const SkyRadioFrame* frame, uint8_t this_type){
	if((int)(frame->ext_length + EXTENSION_START_IDX) > (int)frame->length) {
		return NULL; //todo error: too short packet.
	}
	if(frame->ext_length <= 1){
		return NULL; //no extensions.
	}

	unsigned int cursor = 0;
	while (cursor < frame->ext_length) {
		SkyPacketExtension* ext = (SkyPacketExtension*)&frame->raw[EXTENSION_START_IDX + cursor]; //Magic happens here.
		if (cursor + ext->length >= frame->length)
			return NULL;
		if(ext->length == 0){
			return NULL;
		}
		cursor += ext->length;

		if(this_type == ext->type){
			return ext;
		}

	}
	return NULL;
}
