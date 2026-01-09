from libc cimport stdint #, string
#from libcpp cimport bool

ctypedef stdint.uint64_t uint64_t
ctypedef stdint.uint32_t uint32_t
ctypedef stdint.uint32_t in_addr_t
ctypedef stdint.int32_t int32_t
ctypedef stdint.uint16_t uint16_t
ctypedef stdint.int16_t int16_t
ctypedef stdint.int16_t q15_t
ctypedef stdint.uint8_t uint8_t
ctypedef stdint.int8_t int8_t

cdef bint boolean_variable = True



cdef extern from "skylink.h":
	cdef const int SKY_NUM_VIRTUAL_CHANNELS		# 4



cdef extern from "blake3.h":
	cdef const int BLAKE3_KEY_LEN				# 32



cdef extern from "sky_platform.h":
	ctypedef int32_t sky_tick_t



cdef extern from "frame.h":
	cdef const int SKY_MAX_IDENTITY_LEN			# 7

	ctypedef struct sky_radio_frame:
		sky_tick_t rx_time_ticks;
		unsigned int length;
		uint8_t raw[3 + 255]; #// Reserve space for the Golay code + RS message + RS paritys



cdef extern from "hmac.h":
	cdef const int SKY_MAX_HMAC_KEY_COUNT			# 4

	ctypedef struct SkyHMACKey:
		uint8_t key[BLAKE3_KEY_LEN];
		unsigned int len;

	ctypedef struct SkyHMACVChannel:
		uint8_t send_sequence_reset;
		int32_t sequence_tx;
		int32_t sequence_rx;

	ctypedef struct SkyHMAC:
		SkyHMACKey keys[SKY_MAX_HMAC_KEY_COUNT];
		unsigned int num_keys;
		SkyHMACVChannel vc[SKY_NUM_VIRTUAL_CHANNELS];
		uint32_t nonce_seed;
		void *ctx;



cdef extern from "conf.h":
	cdef const int SKY_CONFIG_FLAG_AUTHENTICATE_TX			# 0b0001
	cdef const int SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION	# 0b0010
	cdef const int SKY_CONFIG_FLAG_REQUIRE_SEQUENCE			# 0b0100
	cdef const int SKY_CONFIG_FLAG_USE_CRC32				# 0b1000

	ctypedef struct SkyMACConfig:
		int32_t maximum_window_length_ticks;
		int32_t minimum_window_length_ticks;
		int32_t gap_constant_ticks;
		int32_t tail_constant_ticks; #// TODO: Rename switch_delay_ticks.
		int32_t idle_timeout_ticks;
		int16_t window_adjust_increment_ticks;
		int16_t carrier_sense_ticks;
		uint8_t unauthenticated_mac_updates;
		int8_t window_adjustment_threshold;
		uint8_t idle_frames_per_window;

	ctypedef struct SkyHMACConfig:
		int32_t maximum_jump;

	ctypedef struct SkyARQConfig:
		int32_t timeout_ticks;
		int32_t idle_frame_threshold;
		int8_t idle_frames_per_window;

	ctypedef struct SkyVCConfig:
		int usable_element_size;
		int rcv_ring_len;
		int horizon_width;
		int send_ring_len;
		uint8_t require_authentication;
		uint8_t tx_key, rx_key;

	ctypedef struct SkyConfig:
		SkyMACConfig    mac; #// MAC/TDD Configuration
		SkyHMACConfig	hmac; #// HMAC Configuration
		SkyVCConfig     vc[SKY_NUM_VIRTUAL_CHANNELS]; #// Virtual channel configurations  SKY_NUM_VIRTUAL_CHANNELS
		SkyARQConfig    arq; #// Automatic repeat request configurations
		uint8_t         identity[SKY_MAX_IDENTITY_LEN]; #// Identity MAX_IDENTITY_LEN
		unsigned int    identity_len; #// Length of identity in bytes



cdef extern from "element_buffer.h":
	ctypedef struct SkyElementBuffer:
		pass



cdef extern from "sequence_ring.h":
	ctypedef struct SkySendRing:
		pass

	ctypedef struct SkyRcvRing:
		pass



cdef extern from "reliable_vc.h":
	cdef const int ARQ_STATE_OFF			# 0
	cdef const int ARQ_STATE_IN_INIT		# 1
	cdef const int ARQ_STATE_ON				# 2

	ctypedef struct SkyVirtualChannel:
		SkyElementBuffer* elementBuffer;    #// Storage structure shared by the sendRing and rcvRing.
		SkySendRing* sendRing;              #// Sequence ring tracking sent payloads and their sequence numbering.
		SkyRcvRing* rcvRing;                #// Sequence ring tracking received payloads and their sequence numbering.
		uint8_t arq_state; #// TODO: enum

		#// A flag to indicate a need to send a response ARQ handshake extension on next transmission window.
		uint8_t handshake_send; #// TODO: Rename send_handshake

		#// A flag set to indicate a need to send rend recall extension.
		#// In the case where a received control extension reveals that the latest received payload is not the latest
		#// the peer has sent, we need to recall this packet, despite our horizon being empty.
		uint8_t need_recall;

		#// A unique identifier of the current ARQ session, if ARQ is on.
		uint32_t arq_session_identifier;

		sky_tick_t last_tx_tick;            #// Tick of last time peer confirmed new payloads received, or being in sync with us.
		sky_tick_t last_rx_tick;            #// Tick of last time a new continuous payloads was received, or we confirmed sync with peer.
		sky_tick_t last_ctrl_send_tick;     #// Tick of last time a control extension was transmitted.

		#// Helper counter for the number of received packets which we have not yet acknownledged.
		int16_t unconfirmed_payloads;



cdef extern from "diag.h":
	ctypedef struct SkyVCDiagnostics:
		uint16_t tx_frames;
		uint16_t rx_frames;



cdef extern from "skylink.h":
	ctypedef struct SkyVCState:
		uint16_t state;
		uint16_t free_tx_slots;
		uint16_t tx_frames;
		uint16_t rx_frames;
		uint32_t session_identifier;

	ctypedef struct SkyState:
		SkyVCState vc[SKY_NUM_VIRTUAL_CHANNELS]

	ctypedef struct SkyDiagnostics:
		uint16_t rx_frames;
		uint16_t rx_bytes;
		uint16_t rx_fec_ok;
		uint16_t rx_fec_fail;
		uint16_t rx_fec_octs;
		uint16_t rx_fec_errs;
		uint16_t rx_hmac_fail;
		uint16_t arq_rx_timeouts;
		uint16_t arq_tx_timeouts;
		uint16_t arq_retransmits;
		uint16_t rx_arq_resets;
		uint16_t tx_frames;
		uint16_t tx_bytes;
		SkyVCDiagnostics vc_stats[SKY_NUM_VIRTUAL_CHANNELS];

	ctypedef struct SkyMAC:
		pass

	ctypedef struct sky_all:
		SkyConfig*          conf;                 # Configuration
		SkyDiagnostics*	    diag;                 # Diagnostics
		SkyVirtualChannel*  virtual_channels[SKY_NUM_VIRTUAL_CHANNELS]; # ARQ capable buffers
		SkyMAC*             mac;                  # MAC state
		SkyHMAC*            hmac;                 # HMAC authentication state


	ctypedef sky_all* SkyHandle;
	ctypedef sky_radio_frame SkyRadioFrame;







cdef extern from "diag.h":
	void sky_diag_clear(SkyDiagnostics *diag);



cdef extern from "utilities.h":
	cdef const int MOD_TIME_TICKS		# 16777216

	int sky_tick(sky_tick_t time_in_ticks);

	sky_tick_t sky_get_tick_time();



cdef extern from "skylink.h":
	SkyHandle sky_create(SkyConfig* config);

	void sky_destroy(SkyHandle self);

	int sky_tx(SkyHandle self, sky_radio_frame* frame);

	int sky_tx_with_golay(SkyHandle self, sky_radio_frame* frame);

	int sky_rx(SkyHandle self, const sky_radio_frame* frame);

	int sky_rx_with_golay(SkyHandle self, sky_radio_frame* frame);


cdef extern  from "mac.h":
	bint mac_can_send(SkyMAC* mac, sky_tick_t now);

	void sky_mac_carrier_sensed(SkyMAC* mac, sky_tick_t now);



cdef extern from "hmac.h":
	void sky_hmac_set_keys(SkyHandle self, const SkyHMACKey *keys, unsigned int count);



cdef extern from "reliable_vc.h":
	# Get skylink protocol state
	void sky_get_state(SkyHandle self, SkyState* state);

	int sky_vc_push_packet_to_send(SkyVirtualChannel *vchannel, const uint8_t *payload, unsigned int length);

	#// Returns boolean 1/0 whether the send ring is full.
	int sky_vc_send_buffer_is_full(SkyVirtualChannel* vchannel);

	#// Reads next message to be sent.
	#int sky_vc_read_packet_for_tx(SkyVirtualChannel *vchannel, uint8_t *tgt, sky_arq_sequence_t *sequence, int include_resend);

	#// Returns the number of messages in buffer.
	int sky_vc_count_packets_to_tx(SkyVirtualChannel* vchannel, int include_resend);

	#// Returns boolean 0/1 as to if there is content to be sent on this virtual channel.
	#int sky_vc_content_to_send(SkyVirtualChannel* vchannel, SkyConfig* config, sky_tick_t now, uint16_t frames_sent_in_this_vc_window);

	#// Read next message to tgt buffer. Return number of bytes written on success, or negative error code.
	int sky_vc_read_next_received(SkyVirtualChannel* vchannel, uint8_t *tgt, unsigned int max_length);

	#// How many messages there are in buffer as a continuous sequence, and thus readable by sky_vc_read_next_received()
	int sky_vc_count_readable_rcv_packets(SkyVirtualChannel* vchannel);

	#// This is called with the head-tx sequence provided by an arq-control-extension
	#void sky_vc_update_rx_sync(SkyVirtualChannel *vchannel, sky_arq_sequence_t peer_tx_head_sequence_by_ctrl, sky_tick_t now);

	# Start the ARQ process connecting procedure
	int sky_vc_arq_connect(SkyVirtualChannel *vchannel);

	# Disconnect/flush ARQ process
	int sky_vc_arq_disconnect(SkyVirtualChannel *vchannel);



