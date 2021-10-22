//
// Created by elmore on 16.10.2021.
//


#include <string.h>
#include "skylink/skylink.h"
#include "skylink/endian.h"
#include "skylink/hmac.h"

#define SKY_PLAIN_FRAME_MIN_LENGTH		15
#define SKYLINK_START_BYTE				'S'
#define SKYLINK_VERSION_BYTE			'1'
#define EXTENSION_ARQ_SEQ				1
#define EXTENSION_ARQ_RESEND_REQ		2
#define EXTENSION_ARQ_SETUP				3
#define EXTENSION_MAC_PARAMETERS		4


#define I_PK_IDENTITY					(2)
#define I_PK_VC							(2+5)
#define I_PK_HMAC						(2+5+1)
#define I_PK_MAC_LENGTH					(2+5+1+2)
#define I_PK_MAC_LEFT					(2+5+1+2+2)
#define I_PK_EXTENSION_COUNT			(2+5+1+2+2+2)
#define I_PK_EXTENSIONS					(2+5+1+2+2+2+1)



static int decode_skylink_extension(SkyRadioFrame* frame, int cursor);
static int encode_skylink_extension(SkyHandle_t self, SkyRadioFrame* frame, int i_extension, int cursor);

int decode_skylink_packet(SkyRadioFrame* frame){
	if(frame->length < SKY_PLAIN_FRAME_MIN_LENGTH){
		return SKY_RET_INVALID_PACKET;
	}
	if(frame->raw[0] != SKYLINK_START_BYTE){
		return SKY_RET_INVALID_PACKET;
	}
	if(frame->raw[1] != SKYLINK_VERSION_BYTE){
		return SKY_RET_INVALID_PACKET;
	}
	frame->flags = 0;

	memcpy(frame->identity, frame->raw + I_PK_IDENTITY, SKY_IDENTITY_LEN);

	frame->vc = frame->raw[I_PK_VC] & 0x3;
	if(frame->vc >= SKY_NUM_VIRTUAL_CHANNELS){
		return SKY_RET_INVALID_PACKET;
	}

	frame->hmac_sequence = *(uint16_t*)(frame->raw + I_PK_HMAC);
	frame->hmac_sequence = sky_hton16(frame->hmac_sequence);

	frame->mac_length = *(uint16_t*)(frame->raw + I_PK_MAC_LENGTH);
	frame->mac_length = sky_hton16(frame->mac_length);

	frame->mac_left = *(uint16_t*)(frame->raw + I_PK_MAC_LEFT);
	frame->mac_left = sky_hton16(frame->mac_left);

	int n_extensions  = frame->raw[I_PK_EXTENSION_COUNT];
	if(n_extensions > SKY_MAX_EXTENSION_COUNT){
		return SKY_RET_INVALID_PACKET;
	}

	int cursor = I_PK_EXTENSIONS;
	frame->arq_sequence = 0;
	frame->n_extensions = 0;
	while (n_extensions){
		int r = decode_skylink_extension(frame, cursor);
		if(r < 0){
			return SKY_RET_INVALID_PACKET;
		}
		cursor += r;
		n_extensions--;
	}

	frame->payload = frame->raw + cursor;
	frame->payload_length = frame->length - cursor;
	return 0;
}



static int decode_skylink_extension(SkyRadioFrame* frame, int cursor){
	if(frame->length < (cursor+1)){
		return -1;
	}
	uint8_t extension_type = frame->raw[cursor] & 0x0f;
	uint8_t extension_len = (frame->raw[cursor] & 0xf0) >> 4;
	if(frame->length < (cursor + 2 + extension_len)){
		return -1;
	}

	SkyPacketExtension* extension = &frame->extensions[frame->n_extensions];

	if(extension_type == EXTENSION_ARQ_SEQ){
		if(extension_len != 1){
			return -1;
		}
		extension->type = EXTENSION_ARQ_SEQ;
		extension->extension.ArqNum.sequence = frame->raw[cursor+1];
		frame->arq_sequence = frame->raw[cursor+1];
		frame->n_extensions++;
		return extension_len+1;
	}

	if(extension_type == EXTENSION_ARQ_RESEND_REQ){
		if(extension_len != 3){
			return -1;
		}
		extension->type = EXTENSION_ARQ_RESEND_REQ;
		extension->extension.ArqReq.sequence = frame->raw[cursor+1];
		extension->extension.ArqReq.mask1 = frame->raw[cursor+2];
		extension->extension.ArqReq.mask2 = frame->raw[cursor+3];
		frame->n_extensions++;
		return extension_len+1;
	}

	if(extension_type == EXTENSION_ARQ_SETUP){
		if(extension_len != 2){
			return -1;
		}
		extension->type = EXTENSION_ARQ_SETUP;
		extension->extension.ArqSetup.toggle = frame->raw[cursor+1];
		extension->extension.ArqSetup.enforced_sequence = frame->raw[cursor+2];
		frame->n_extensions++;
		return extension_len+1;
	}

	if(extension_type == EXTENSION_MAC_PARAMETERS){
		if(extension_len != 4){
			return -1;
		}
		extension->type = EXTENSION_MAC_PARAMETERS;
		uint16_t default_window_size = *(uint16_t*)(&frame->raw[cursor+1]);
		extension->extension.MACSpec.default_window_size = sky_hton16(default_window_size);
		uint16_t gap_size = *(uint16_t*)(&frame->raw[cursor+3]);
		extension->extension.MACSpec.default_window_size = sky_hton16(gap_size);
		frame->n_extensions++;
		return extension_len+1;
	}

	return -1;
}





int encode_skylink_packet(SkyHandle_t self, SkyRadioFrame* frame){
	frame->raw[0] = SKYLINK_START_BYTE;
	frame->raw[1] = SKYLINK_VERSION_BYTE;

	memcpy(frame->raw + I_PK_IDENTITY, self->conf->identity, SKY_IDENTITY_LEN);

	frame->raw[I_PK_VC] = frame->vc;

	uint16_t hmac_tx = sky_hton16(frame->hmac_sequence);
	memcpy(frame->raw + I_PK_HMAC, &hmac_tx, sizeof(uint16_t));

	uint16_t mac_window_tx = sky_hton16(frame->mac_length);
	uint16_t mac_remaining_tx = sky_hton16(frame->mac_left); //todo: needs to be evaluated as late as possible
	memcpy(frame->raw + I_PK_MAC_LENGTH, &mac_window_tx, sizeof(uint16_t));
	memcpy(frame->raw + I_PK_MAC_LEFT, &mac_remaining_tx, sizeof(uint16_t));

	frame->raw[I_PK_EXTENSION_COUNT] = frame->n_extensions;
	int cursor = I_PK_EXTENSIONS;
	for (int i = 0; i < frame->n_extensions; ++i) {
		int r = encode_skylink_extension(self, frame, i, cursor);
		if(r < 0){
			return SKY_RET_INVALID_EXTENSION;
		}
		cursor += r;
		if ((cursor + frame->payload_length + SKY_HMAC_LENGTH) >= SKY_FRAME_MAX_LEN) {
			return SKY_RET_PACKET_TOO_LONG;
		}
	}
	//Observe: the payload is initially placed to the end of the packet.
	//If the total packet length is not beyond length limit, the payload will not be corrupted
	//during prior packet construction.
	for (int i = 0; i < frame->payload_length; ++i) {
		frame->raw[cursor+i] = frame->raw[SKY_FRAME_MAX_LEN - frame->payload_length + i];
	}
	frame->length = cursor + frame->payload_length;

	if(frame->hmac_sequence != 0){
		sky_hmac_authenticate(self, frame);
	}
	return 0;
}


static int encode_skylink_extension(SkyHandle_t self, SkyRadioFrame* frame, int i_extension, int cursor){
	SkyPacketExtension* extension = &frame->extensions[i_extension];

	if(extension->type == EXTENSION_ARQ_SEQ){

	}

	if(extension->type == EXTENSION_ARQ_SETUP){

	}

	if(extension->type == EXTENSION_ARQ_RESEND_REQ){

	}

	if(extension->type == EXTENSION_MAC_PARAMETERS){

	}

	return -1;
}

