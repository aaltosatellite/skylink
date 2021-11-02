//
// Created by elmore on 16.10.2021.
//

#include "skylink/skypacket.h"


SendFrame* new_send_frame(){
	SendFrame* frame = SKY_MALLOC(sizeof(SendFrame));
	return frame;
}

RCVFrame* new_receive_frame(){
	RCVFrame* frame = SKY_MALLOC(sizeof(RCVFrame));
	return frame;
}

void destroy_receive_frame(RCVFrame* frame){
	free(frame);
}
void destroy_send_frame(SendFrame* frame){
	free(frame);
}





//=== ENCODING =========================================================================================================
//======================================================================================================================
int sky_packet_add_extension_arq_rr(SendFrame* frame, uint8_t sequence, uint8_t mask1, uint8_t mask2){
	void* ptr = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
	ExtArqReq* extension = ptr;
	extension->type = EXTENSION_ARQ_RESEND_REQ;
	extension->length = 4;
	extension->sequence = sequence;
	extension->mask1 = mask1;
	extension->mask2 = mask2;
	frame->radioFrame.ext_length += 4;
	frame->radioFrame.length = EXTENSION_START_IDX + frame->radioFrame.ext_length;
	return 1;
}


int sky_packet_add_extension_arq_enforce(SendFrame* frame, uint8_t toggle, uint8_t sequence){
	void* ptr = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
	ExtArqSeqReset* extension = ptr;
	extension->type = EXTENSION_ARQ_SEQ_RESET;
	extension->length = 3;
	extension->toggle = toggle;
	extension->enforced_sequence = sequence;
	frame->radioFrame.ext_length += 3;
	frame->radioFrame.length = EXTENSION_START_IDX + frame->radioFrame.ext_length;
	return 1;
}


int sky_packet_add_extension_hmac_enforce(SendFrame* frame, uint16_t sequence){
	void* ptr = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
	ExtHMACTxReset* extension = ptr;
	extension->type = EXTENSION_HMAC_ENFORCEMENT;
	extension->length = 3;
	extension->correct_tx_sequence = sequence; //+2 so that immediate sends don't ivalidate what we give here. Jump constant must be bigger.
	frame->radioFrame.ext_length += 3;
	frame->radioFrame.length = EXTENSION_START_IDX + frame->radioFrame.ext_length;
	return 1;
}


int sky_packet_add_extension_mac_params(SendFrame* frame, uint16_t gap_size, uint16_t window_size){
	void* ptr = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
	ExtMACSpec * extension = ptr;
	extension->type = EXTENSION_MAC_PARAMETERS;
	extension->length = 5;
	extension->gap_size = gap_size;
	extension->window_size = window_size;
	frame->radioFrame.ext_length += 5;
	frame->radioFrame.length = EXTENSION_START_IDX + frame->radioFrame.ext_length;
	return 1;
}


int available_payload_space(RadioFrame2* radioFrame){
	return RS_MSGLEN - (radioFrame->length + SKY_HMAC_LENGTH);
}


int sky_packet_extend_with_payload(SendFrame* frame, void* pl, int32_t length){
	if(available_payload_space(&frame->radioFrame) < length){
		return -1;
	}
	memcpy(frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length, pl, length);
	frame->radioFrame.length = EXTENSION_START_IDX + frame->radioFrame.ext_length + length;
	return 0;
}
//=== ENCODING =========================================================================================================
//======================================================================================================================




//=== DECODING =========================================================================================================
//======================================================================================================================
int interpret_extension(void* ptr, int max_length, SkyPacketExtension* extension){
	extension->type = -1;
	if(max_length < 2){
		return -1;
	}
	ExtensionTypemask* eMask = ptr;
	if((eMask->type == EXTENSION_ARQ_RESEND_REQ) && (max_length >= eMask->length) && (eMask->length == 4)){
		ExtArqReq* ext = ptr;
		extension->type = EXTENSION_ARQ_RESEND_REQ;
		extension->ext_union.ArqReq = *ext;
		return eMask->length;
	}
	if((eMask->type == EXTENSION_ARQ_SEQ_RESET) && (max_length >= eMask->length) && (eMask->length == 3)){
		ExtArqSeqReset* ext = ptr;
		extension->type = EXTENSION_ARQ_SEQ_RESET;
		extension->ext_union.ArqSeqReset = *ext;
		return eMask->length;
	}
	if((eMask->type == EXTENSION_MAC_PARAMETERS) && (max_length >= eMask->length) && (eMask->length == 5)){
		ExtMACSpec* ext = ptr;
		extension->type = EXTENSION_MAC_PARAMETERS;
		extension->ext_union.MACSpec = *ext;
		return eMask->length;
	}
	if((eMask->type == EXTENSION_HMAC_ENFORCEMENT) && (max_length >= eMask->length) && (eMask->length == 3)){
		ExtHMACTxReset* ext = ptr;
		extension->type = EXTENSION_HMAC_ENFORCEMENT;
		extension->ext_union.HMACTxReset = *ext;
		return eMask->length;
	}
	return -1;
}
//=== DECODING =========================================================================================================
//======================================================================================================================





