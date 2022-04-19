from libc cimport stdint

ctypedef stdint.uint64_t uint64_t
ctypedef stdint.uint32_t uint32_t
ctypedef stdint.uint16_t uint16_t
ctypedef stdint.uint8_t uint8_t
ctypedef stdint.int64_t int64_t
ctypedef stdint.int32_t int32_t
ctypedef stdint.int16_t int16_t
ctypedef stdint.int8_t int8_t


cdef extern from "../external_wrapping/amateur_radio.h":

	ctypedef struct SkylinkPeer:
		pass

	ctypedef struct VCStatus:
		int32_t arq_state;
		uint32_t arq_identity;
		int32_t n_to_tx;
		int32_t n_to_resend;
		int32_t tx_full;
		int32_t n_to_read;
		int32_t hmac_tx_seq;
		int32_t hmac_rx_seq;

	ctypedef struct SkylinkStatus:
		int32_t my_window;
		int32_t peer_window;
		int32_t last_mac_update;
		int32_t tick;
		int32_t radio_state;
		VCStatus vcs[4];


	int skylink_encode_amateur_pl(uint8_t* identity, uint8_t* pl, int32_t pl_len, uint8_t* tgt, int insert_golay);

