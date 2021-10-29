//
// Created by elmore on 16.10.2021.
//

#include "skylink/skypacket.h"
#include "skylink/hmac.h"
#include "skylink/fec.h"



static int decode_skylink_extension(SkyRadioFrame* frame, int n, int cursor);
static int encode_skylink_extension(SkyRadioFrame* frame, int i_extension, int cursor);

SkyRadioFrame* new_frame(){
	SkyRadioFrame* frame = SKY_MALLOC(sizeof(SkyRadioFrame));
	memset(frame, 0, sizeof(SkyRadioFrame));
	return frame;
}

void destroy_frame(SkyRadioFrame* frame){
	free(frame);
}







//=== ENCODING =========================================================================================================
//======================================================================================================================
int sky_packet_assign_hmac_sequence(SkyHandle self, SkyRadioFrame* frame){
	int32_t s = sky_hmac_get_next_hmac_tx_sequence_and_advance(self, frame->vc);
	frame->hmac_sequence = (uint16_t) s;
	return 0;
}


int sky_packet_add_extension_mac_params(SkyRadioFrame* frame, int default_window_size, int gap_size){
	//todo: check for invalid window and gap sizes?
	SkyPacketExtension* extension = &frame->extensions[frame->n_extensions];
	extension->type = EXTENSION_MAC_PARAMETERS;
	extension->ext_union.MACSpec.default_window_size = default_window_size;
	extension->ext_union.MACSpec.gap_size = gap_size;
	frame->n_extensions++;
	return 0;
}


int sky_packet_add_extension_arq_setup(SkyRadioFrame* frame, int new_sequence, uint8_t toggle){
	//todo: check for invalid sequence?
	SkyPacketExtension* extension = &frame->extensions[frame->n_extensions];
	extension->type = EXTENSION_ARQ_SETUP;
	extension->ext_union.ArqSetup.enforced_sequence = new_sequence;
	extension->ext_union.ArqSetup.toggle = toggle;
	frame->n_extensions++;
	return 0;
}


int sky_packet_add_extension_arq_resend_request(SkyRadioFrame* frame, int sequence, uint8_t mask1, uint8_t mask2){
	//todo: check for invalid sequence?
	SkyPacketExtension* extension = &frame->extensions[frame->n_extensions];
	extension->type = EXTENSION_ARQ_RESEND_REQ;
	extension->ext_union.ArqReq.sequence = sequence;
	extension->ext_union.ArqReq.mask1 = mask1;
	extension->ext_union.ArqReq.mask2 = mask2;
	frame->n_extensions++;
	return 0;
}


int encode_skylink_packet(SkyRadioFrame* frame){
	frame->raw[0] = SKYLINK_START_BYTE;
	frame->raw[1] = SKYLINK_VERSION_BYTE;

	memcpy(frame->raw + I_PK_IDENTITY, frame->identity, SKY_IDENTITY_LEN);

	uint8_t flag = 0;
	if(frame->hmac_on){
		flag |= SKY_FLAG_FRAME_AUTHENTICATED;
	}
	if(frame->arq_on){
		flag |= SKY_FLAG_ARQ_ON;
	}
	frame->raw[I_PK_FLAG] = flag;

	uint8_t vc_n_ext = ((frame->vc << 4) | (frame->n_extensions & 0x0F));
	frame->raw[I_PK_VC_N_EXT] = vc_n_ext;

	uint16_t hmac_tx = sky_hton16(frame->hmac_sequence);
	memcpy(frame->raw + I_PK_HMAC, &hmac_tx, sizeof(uint16_t));

	uint16_t mac_window_tx = sky_hton16(frame->mac_length);
	uint16_t mac_remaining_tx = sky_hton16(frame->mac_remaining); //todo: needs to be evaluated as late as possible
	memcpy(frame->raw + I_PK_MAC_LENGTH, &mac_window_tx, sizeof(uint16_t));
	memcpy(frame->raw + I_PK_MAC_LEFT, &mac_remaining_tx, sizeof(uint16_t));

	frame->raw[I_PK_ARQ_SEQUENCE] = 0;
	if(frame->arq_on){
		frame->raw[I_PK_ARQ_SEQUENCE] = frame->arq_sequence;
	}

	int cursor = I_PK_EXTENSIONS;
	for (int i = 0; i < frame->n_extensions; ++i) {
		int r = encode_skylink_extension(frame, i, cursor);
		if(r < 0){
			return SKY_RET_INVALID_EXTENSION;
		}
		cursor += r;
	}

	frame->encoding_cursor = cursor;
	frame->length = cursor;
	return 0;
}


int sky_packet_available_payload_space(SkyRadioFrame* frame){
	return RS_MSGLEN - (frame->encoding_cursor + SKY_HMAC_LENGTH);
}


int sky_packet_extend_with_payload(SkyRadioFrame* frame, void* payload, int length){
	if(sky_packet_available_payload_space(frame) < length){
		return -1;
	}
	memcpy(&frame->raw[frame->encoding_cursor], payload, length);
	frame->length += length;
	return 0;
}


static int encode_skylink_extension(SkyRadioFrame* frame, int i_extension, int cursor){
	SkyPacketExtension* extension = &frame->extensions[i_extension];

	if(extension->type == EXTENSION_ARQ_SETUP){
		ExtArqSetup arqSetup = extension->ext_union.ArqSetup;
		frame->raw[cursor]  = (extension->type << 4) & 0xf0;
		frame->raw[cursor] |= 2 & 0x0f;
		frame->raw[cursor+1] = arqSetup.toggle;
		frame->raw[cursor+2] = arqSetup.enforced_sequence;
		return 1+2;
	}

	if(extension->type == EXTENSION_ARQ_RESEND_REQ){
		ExtArqReq arqReq = extension->ext_union.ArqReq;
		frame->raw[cursor]  = (extension->type << 4) & 0xf0;
		frame->raw[cursor] |= 3 & 0x0f;
		frame->raw[cursor+1] = arqReq.sequence;
		frame->raw[cursor+2] = arqReq.mask1;
		frame->raw[cursor+3] = arqReq.mask2;
		return 1+3;
	}

	if(extension->type == EXTENSION_MAC_PARAMETERS){
		ExtMACSpec macSpec = extension->ext_union.MACSpec;
		frame->raw[cursor]  = (extension->type << 4) & 0xf0;
		frame->raw[cursor] |= 4 & 0x0f;
		uint16_t window_size_tx = sky_hton16(macSpec.default_window_size);
		uint16_t gap_size_tx = sky_hton16(macSpec.gap_size);
		memcpy(frame->raw + cursor + 1, &window_size_tx, 2);
		memcpy(frame->raw + cursor + 3, &gap_size_tx, 2);
		return 1+4;
	}

	return -1;
}
//=== ENCODING =========================================================================================================
//======================================================================================================================







//=== DECODING =========================================================================================================
//======================================================================================================================
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

	memcpy(frame->identity, frame->raw + I_PK_IDENTITY, SKY_IDENTITY_LEN);


	uint8_t flag = frame->raw[I_PK_FLAG];
	frame->hmac_on = (flag & SKY_FLAG_FRAME_AUTHENTICATED) != 0;
	frame->arq_on = (flag & SKY_FLAG_ARQ_ON) != 0;

	uint8_t vc_n_ext = frame->raw[I_PK_VC_N_EXT];
	frame->vc = (vc_n_ext & 0xF0) >> 4;
	frame->n_extensions = (vc_n_ext & 0x0F);

	if(frame->vc >= SKY_NUM_VIRTUAL_CHANNELS){
		return SKY_RET_INVALID_PACKET;
	}

	if(frame->n_extensions > SKY_MAX_EXTENSION_COUNT){
		return SKY_RET_INVALID_PACKET;
	}

	frame->hmac_sequence = *(uint16_t*)(frame->raw + I_PK_HMAC);
	frame->hmac_sequence = sky_hton16(frame->hmac_sequence);

	frame->mac_length = *(uint16_t*)(frame->raw + I_PK_MAC_LENGTH);
	frame->mac_length = sky_hton16(frame->mac_length);

	frame->mac_remaining = *(uint16_t*)(frame->raw + I_PK_MAC_LEFT);
	frame->mac_remaining = sky_hton16(frame->mac_remaining);

	frame->arq_sequence = 0;
	if(frame->arq_on){
		frame->arq_sequence = frame->raw[I_PK_ARQ_SEQUENCE];
	}

	int cursor = I_PK_EXTENSIONS;
	int n_ext = 0;
	while (n_ext < frame->n_extensions){
		int r = decode_skylink_extension(frame, n_ext, cursor);
		if(r < 0){
			return SKY_RET_INVALID_PACKET;
		}
		cursor += r;
		n_ext++;
	}

	frame->payload_read_length = frame->length - cursor;
	frame->payload_read_start = frame->raw + cursor;
	return 0;
}



static int decode_skylink_extension(SkyRadioFrame* frame, int n, int cursor){
	if(frame->length < (cursor+1)){
		return -1;
	}
	uint8_t extension_type = (frame->raw[cursor] & 0xf0) >> 4;
	uint8_t extension_len = frame->raw[cursor] & 0x0f;
	if(frame->length < (cursor + 2 + extension_len)){
		return -1;
	}

	SkyPacketExtension* extension = &frame->extensions[n];

	if(extension_type == EXTENSION_ARQ_RESEND_REQ){
		if(extension_len != 3){
			return -1;
		}
		extension->type = EXTENSION_ARQ_RESEND_REQ;
		extension->ext_union.ArqReq.sequence = frame->raw[cursor + 1];
		extension->ext_union.ArqReq.mask1 = frame->raw[cursor + 2];
		extension->ext_union.ArqReq.mask2 = frame->raw[cursor + 3];
		return extension_len+1;
	}

	if(extension_type == EXTENSION_ARQ_SETUP){
		if(extension_len != 2){
			return -1;
		}
		extension->type = EXTENSION_ARQ_SETUP;
		extension->ext_union.ArqSetup.toggle = frame->raw[cursor + 1];
		extension->ext_union.ArqSetup.enforced_sequence = frame->raw[cursor + 2];
		return extension_len+1;
	}

	if(extension_type == EXTENSION_MAC_PARAMETERS){
		if(extension_len != 4){
			return -1;
		}
		extension->type = EXTENSION_MAC_PARAMETERS;
		uint16_t default_window_size = *(uint16_t*)(&frame->raw[cursor+1]);
		extension->ext_union.MACSpec.default_window_size = sky_hton16(default_window_size);
		uint16_t gap_size = *(uint16_t*)(&frame->raw[cursor+3]);
		extension->ext_union.MACSpec.gap_size = sky_hton16(gap_size);
		return extension_len+1;
	}
	return -1;
}
//=== DECODING =========================================================================================================
//======================================================================================================================





