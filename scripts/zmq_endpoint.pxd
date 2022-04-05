from libc cimport stdint

ctypedef stdint.uint64_t uint64_t
ctypedef stdint.uint32_t uint32_t
ctypedef stdint.uint16_t uint16_t
ctypedef stdint.uint8_t uint8_t
ctypedef stdint.int64_t int64_t
ctypedef stdint.int32_t int32_t
ctypedef stdint.int16_t int16_t
ctypedef stdint.int8_t int8_t

"""
cdef extern from "../src/skylink/conf.h":
	ctypedef struct SkyPHYConfig:
		uint8_t enable_scrambler;
		uint8_t enable_rs;

	ctypedef struct SkyMACConfig:
		int32_t maximum_window_length_ticks;
		int32_t minimum_window_length_ticks;
		int32_t gap_constant_ticks;
		int32_t tail_constant_ticks;
		uint8_t unauthenticated_mac_updates;
		int32_t shift_threshold_ticks;
		int32_t idle_timeout_ticks;
		int16_t window_adjust_increment_ticks;
		int16_t carrier_sense_ticks;
		int8_t window_adjustment_period;
		uint8_t idle_frames_per_window;

	ctypedef struct SkyVCConfig:
		int element_size;
		int rcv_ring_len;
		int horizon_width;
		int send_ring_len;
		uint8_t require_authentication;

	ctypedef struct HMACConfig:
		int32_t key_length;
		uint8_t key[16];
		int32_t maximum_jump;

	ctypedef struct SkyConfig:
		SkyPHYConfig phy;
		SkyMACConfig mac;
		HMACConfig	hmac;
		SkyVCConfig vc[4];
		uint8_t identity[6];
		int32_t arq_timeout_ticks;
		int8_t arq_idle_frames_per_window;
"""

cdef extern from "../tests/zmq_endpoint.h":

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


	SkylinkPeer* ep_init_peer(int32_t ID, double relspeed, int32_t baudrate, uint8_t pipe_up, uint8_t pipe_down, uint8_t* cfg, int32_t cfg_len);

	void ep_close(SkylinkPeer* peer);

	int ep_vc_push_pl(SkylinkPeer* peer, int32_t vc, uint8_t* pl, int32_t pl_len) nogil;

	int ep_vc_read_pl(SkylinkPeer* peer, int32_t vc, uint8_t* pl, int32_t maxlen) nogil;

	int ep_vc_init_arq(SkylinkPeer* peer, int32_t vc) nogil;

	int ep_get_skylink_status(SkylinkPeer* peer, SkylinkStatus* status) nogil;