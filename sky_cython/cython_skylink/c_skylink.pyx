cimport c_skylink
from libc.stdlib cimport malloc, free
from libc.string cimport memcpy
#from libc cimport stdint


_STUFF = "Hi"
num_virtual_channels 	= c_skylink.SKY_NUM_VIRTUAL_CHANNELS
max_identity_len 		= c_skylink.SKY_MAX_IDENTITY_LEN
mod_time_ticks 			= c_skylink.MOD_TIME_TICKS

auth_flag_auth_tx 		= c_skylink.SKY_CONFIG_FLAG_AUTHENTICATE_TX
auth_flag_require_auth 	= c_skylink.SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION
auth_flag_require_seq 	= c_skylink.SKY_CONFIG_FLAG_REQUIRE_SEQUENCE
auth_flag_use_crc32 	= c_skylink.SKY_CONFIG_FLAG_USE_CRC32


class ArqConfig:
	timeout_ticks		 	= 26000
	idle_frame_threshold 	= 6500
	idle_frames_per_window 	= 1

class HMACConfig:
	maximum_jump 			= 24

class MACConfig:
	gap_constant_ticks              = 600
	tail_constant_ticks             = 80
	minimum_window_length_ticks     = 250
	maximum_window_length_ticks     = 1000
	window_adjust_increment_ticks   = 250
	window_adjustment_threshold     = 2
	unauthenticated_mac_updates     = 0
	idle_frames_per_window          = 0
	idle_timeout_ticks              = 30000
	carrier_sense_ticks             = 200

class VCConfig:
	require_authentication      = auth_flag_auth_tx | auth_flag_use_crc32
	rcv_ring_len                = 28
	horizon_width               = 16
	send_ring_len               = 24
	usable_element_size         = 175
	tx_key                      = 0
	rx_key                      = 0

class SkyConfiguration:
	def __init__(self):
		self.vc = [VCConfig(), VCConfig(), VCConfig(), VCConfig()]
		self.mac = MACConfig()
		self.hmac = HMACConfig()
		self.arq = ArqConfig()
		self.identity = b"tstID"


cdef class SkyLink:
	cdef c_skylink.SkyHandle handle #sky_all
	cdef c_skylink.SkyConfig conf
	#cdef public :
	#	object conf_override

	def __init__(self, conf_override): #runs after __cinit__
		self._init0(conf_override)

	cdef _init0(self, conf_override):
		self.conf.arq.timeout_ticks = conf_override.arq.timeout_ticks
		self.conf.arq.idle_frame_threshold = conf_override.arq.idle_frame_threshold
		self.conf.arq.idle_frames_per_window = conf_override.arq.idle_frames_per_window

		self.conf.hmac.maximum_jump = conf_override.hmac.maximum_jump

		self.conf.mac.carrier_sense_ticks = conf_override.mac.carrier_sense_ticks
		self.conf.mac.gap_constant_ticks = conf_override.mac.gap_constant_ticks
		self.conf.mac.idle_frames_per_window = conf_override.mac.idle_frames_per_window
		self.conf.mac.idle_timeout_ticks = conf_override.mac.idle_timeout_ticks
		self.conf.mac.maximum_window_length_ticks = conf_override.mac.maximum_window_length_ticks
		self.conf.mac.minimum_window_length_ticks = conf_override.mac.minimum_window_length_ticks
		self.conf.mac.tail_constant_ticks = conf_override.mac.tail_constant_ticks
		self.conf.mac.unauthenticated_mac_updates = conf_override.mac.unauthenticated_mac_updates
		self.conf.mac.window_adjust_increment_ticks = conf_override.mac.window_adjust_increment_ticks
		self.conf.mac.window_adjustment_threshold = conf_override.mac.window_adjustment_threshold

		for i in range(c_skylink.SKY_NUM_VIRTUAL_CHANNELS):
			self.conf.vc[i].horizon_width = conf_override.vc[i].horizon_width
			self.conf.vc[i].rcv_ring_len = conf_override.vc[i].rcv_ring_len
			self.conf.vc[i].require_authentication = conf_override.vc[i].require_authentication
			self.conf.vc[i].send_ring_len = conf_override.vc[i].send_ring_len
			self.conf.vc[i].tx_key = conf_override.vc[i].tx_key
			self.conf.vc[i].rx_key = conf_override.vc[i].rx_key
			self.conf.vc[i].usable_element_size = conf_override.vc[i].usable_element_size
			self.conf.vc[i].require_authentication = conf_override.vc[i].require_authentication

		cdef int id_len = 0;
		id_len = min( len(conf_override.identity), c_skylink.SKY_MAX_IDENTITY_LEN )
		memcpy(self.conf.identity, <uint8_t*> conf_override.identity, id_len)
		#self.conf.identity[0:min(c_skylink.SKY_MAX_IDENTITY_LEN, len(conf_override.identity))] = conf_override.identity
		self.conf.identity_len = id_len
		self.handle = c_skylink.sky_create(&self.conf)
		pass

	def __cinit__(self, conf_override): #Run before __init__
		# cdef c_skylink.SkyConfig conf
		# conf.arq.timeout_ticks = conf_override.arq.timeout_ticks
		# print("A")
		pass

	def __dealloc__(self):
		self._destruct()

	cdef _destruct(self):
		c_skylink.sky_destroy(self.handle)



	# === HMAC-KEYS ========================================================================================================================
	cdef _set_hmac_keys(self, uint8_t* keydata, int keycount):
		# assert keycount > 1, <= nmax
		cdef c_skylink.SkyHMACKey* keys;
		keys = <c_skylink.SkyHMACKey*> malloc(sizeof(c_skylink.SkyHMACKey)*4)
		for i in range(keycount):
			keys[i].len = c_skylink.BLAKE3_KEY_LEN
			memcpy(keys[i].key, <uint8_t*> &keydata[i*c_skylink.BLAKE3_KEY_LEN] , c_skylink.BLAKE3_KEY_LEN)
		c_skylink.sky_hmac_set_keys(self.handle, keys, keycount)
		free(keys)

	def set_hmac_keys(self, key_list):
		assert 0 < len(key_list) <= c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		for key in key_list:
			assert type(key) == bytes
			assert len(key) == c_skylink.BLAKE3_KEY_LEN
		keydata = b""
		for key in key_list:
			keydata += key
		self._set_hmac_keys(<uint8_t*> keydata, <int> len(key_list))

	cdef _get_hmac_key(self, int ichannel):
		cdef uint8_t* key;
		key = self.handle.hmac.keys[ichannel].key
		k = bytes( key[0:c_skylink.BLAKE3_KEY_LEN] )
		return k

	def get_hmac_key(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return bytes(self._get_hmac_key( <int> ichannel))
	# === HMAC-KEYS ========================================================================================================================



	# === RX/TX ============================================================================================================================
	cdef _sky_rx(self, uint8_t* data, int leng):
		cdef c_skylink.SkyRadioFrame frame;
		cdef int iret = 0;
		memcpy(frame.raw, data, leng)
		frame.length = leng
		iret = c_skylink.sky_rx(self.handle, &frame)
		return iret

	def sky_rx(self, raw_frame_bytes):
		return self._sky_rx(<uint8_t*> raw_frame_bytes, <int> len(raw_frame_bytes))


	cdef _sky_tx(self):
		cdef c_skylink.SkyRadioFrame frame;
		frame.length = 0
		cdef int iret = 0;
		#tgt = <uint8_t*> malloc( sizeof(c_skylink.SkyRadioFrame) )
		iret = c_skylink.sky_tx(self.handle, &frame)
		frame_bytes = bytes( frame.raw[:frame.length] )
		return iret, frame_bytes

	def sky_tx(self):
		return self._sky_tx()
	# === RX/TX ============================================================================================================================




	# === SEND =============================================================================================================================
	cdef _sky_vc_push_packet_to_send(self, int ichannel, uint8_t* data, int datalen):
		cdef int ret = 0;
		ret = c_skylink.sky_vc_push_packet_to_send(self.handle.virtual_channels[ichannel], data, datalen)
		return ret

	def sky_vc_push_packet_to_send(self, ichannel, data):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_push_packet_to_send(<int> ichannel, <uint8_t*> data, <int> len(data))


	cdef _sky_vc_count_packets_to_tx(self, int ichannel, int include_resend):
		cdef int ret = 0;
		ret = c_skylink.sky_vc_count_packets_to_tx(self.handle.virtual_channels[ichannel], include_resend)
		return ret

	def sky_vc_count_packets_to_tx(self, ichannel, include_resend):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_count_packets_to_tx(<int> ichannel, <int> int(include_resend))


	def sky_vc_send_buffer_is_full(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		cdef int ret = 0;
		ret = c_skylink.sky_vc_send_buffer_is_full(self.handle.virtual_channels[ichannel])
		return ret
	# === SEND =============================================================================================================================





	# === RECEIVE ==========================================================================================================================
	cdef _sky_vc_count_readable_rcv_packets(self, int ichannel):
		cdef int ret = 0;
		ret = c_skylink.sky_vc_count_readable_rcv_packets(self.handle.virtual_channels[ichannel])
		return ret

	def sky_vc_count_readable_rcv_packets(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_count_readable_rcv_packets(<int> ichannel)


	cdef _sky_vc_read_next_received(self, int ichannel):
		cdef int ret = 0;
		cdef uint8_t* tgt;
		tgt = <uint8_t*> malloc(1024)
		ret = c_skylink.sky_vc_read_next_received(self.handle.virtual_channels[ichannel], tgt, 1024)
		if ret < 0:
			free(tgt)
			return ret, b""
		ret_b = bytes( tgt[0:ret] )
		free(tgt)
		return ret, ret_b

	def sky_vc_read_next_received(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_read_next_received(<int> ichannel)
	# === RECEIVE ==========================================================================================================================




	# === CONTROL ==========================================================================================================================
	cdef _sky_get_state(self):
		cdef SkyState state;
		c_skylink.sky_get_state(self.handle, &state)
		return state

	def sky_get_state(self):
		cdef SkyState state;
		state = self._sky_get_state()
		vc_list = list()
		for i in range(c_skylink.SKY_NUM_VIRTUAL_CHANNELS):
			dd = dict()
			dd["state"] = state.vc[i].state
			dd["free_tx_slots"] = state.vc[i].free_tx_slots
			dd["tx_frames"] = state.vc[i].tx_frames
			dd["rx_frames"] = state.vc[i].rx_frames
			dd["session_identifier"] = state.vc[i].session_identifier
			vc_list.append(dd)
		return vc_list


	def sky_get_stats(self):
		cdef SkyDiagnostics* stats;
		stats = self.handle.diag
		dd = dict()
		dd["rx_frames"] = stats.rx_frames
		dd["rx_bytes"] = stats.rx_bytes
		dd["rx_fec_ok"] = stats.rx_fec_ok
		dd["rx_fec_fail"] = stats.rx_fec_fail
		dd["rx_fec_errs"] = stats.rx_fec_errs
		dd["rx_arq_resets"] = stats.rx_arq_resets
		dd["rx_hmac_fail"] = stats.rx_hmac_fail
		dd["vc"] = list()
		for i in range(c_skylink.SKY_NUM_VIRTUAL_CHANNELS):
			dd["vc"].append( dict() )
			dd["vc"][i]["arq_retransmits"] = stats.vc_stats[i].arq_retransmits
			dd["vc"][i]["total_tx_frames"] = stats.vc_stats[i].tx_frames
			dd["vc"][i]["total_rx_frames"] = stats.vc_stats[i].rx_frames
		return dd


	def sky_diag_clear(self):
		c_skylink.sky_diag_clear(self.handle.diag)


	cdef _sky_vc_arq_connect(self, int ichannel):
		cdef int ret = 0;
		ret = sky_vc_arq_connect(self.handle.virtual_channels[ichannel])
		return ret

	def sky_vc_arq_connect(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_arq_connect(<int> ichannel)


	cdef _sky_vc_arq_disconnect(self, int ichannel):
		cdef int ret = 0;
		ret = sky_vc_arq_disconnect(self.handle.virtual_channels[ichannel])
		return ret

	def sky_vc_arq_disconnect(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_arq_disconnect(<int> ichannel)


	cdef _sky_vc_get_arq_state(self, int ichannel):
		cdef uint8_t x;
		x = self.handle.virtual_channels[ichannel].arq_state
		return int(x)

	def sky_vc_get_arq_state(self, ichannel):
		assert 0 <= ichannel < c_skylink.SKY_NUM_VIRTUAL_CHANNELS
		return self._sky_vc_get_arq_state(ichannel)


	cdef _can_send(self):
		cdef sky_tick_t now;
		cdef bint b;
		now = c_skylink.sky_get_tick_time()
		b = c_skylink.mac_can_send(self.handle.mac, now)
		return b

	def can_send(self):
		return bool(self._can_send())


	def carrier_sensed(self):
		cdef sky_tick_t now;
		now = self.get_tick_time()
		c_skylink.sky_mac_carrier_sensed(self.handle.mac, now)


	def get_tick_time(self):
		return int( c_skylink.sky_get_tick_time() )

	def sky_tick(self, ticks):
		cdef sky_tick_t tick_t_tick;
		cdef int ret;
		tick_t_tick = ticks
		ret = c_skylink.sky_tick(tick_t_tick)
		return ret
	# === CONTROL ==========================================================================================================================

