import struct

cimport zmq_endpoint
import cython

class VCStat():
	arq_state = 0
	arq_identity  = 0
	n_to_tx = 0
	n_to_resend = 0
	tx_full = 0
	n_to_read = 0
	hmac_tx_seq = 0
	hmac_rx_seq = 0

	def get_str(self, _prefix=""):
		members = ["arq_state","arq_identity","n_to_tx","n_to_resend","tx_full",
				   "n_to_read","hmac_tx_seq","hmac_rx_seq"]
		S = ""
		for mem in members:
			S += _prefix+"{}: {}\n".format(mem, str(getattr(self,mem)))
		return S

class SkylinkStat:
	my_window = 0
	peer_window = 0
	own_remaining = 0
	peer_remaining = 0
	last_mac_update = 0
	tick = 0
	radio_state = 0
	vcs = None
	def __init__(self):
		self.vcs = list()

	def __str__(self):
		return self.get_str()

	def get_str(self):
		members = ["my_window", "peer_window", "own_remaining", "last_mac_update", "tick", "radio_state"]
		S = ""
		S += 30*"="+"\n"
		for mem in members:
			S += "{}: {}\n".format(mem, str(getattr(self,mem)))
		for vc in self.vcs:
			S += vc.get_str("\t")
			S += "\t"+30*"-"+"\n"
		S += 30*"="+"\n"
		return S


SIZE_VCSTATUS = 4*8

cdef class SkyLink:
	cdef zmq_endpoint.SkylinkPeer* Peer
	def __init__(self, ID, relspeed, baudrate, pipe_up, pipe_down, cfg, cfg_len, localtime0):
		pass

	def __cinit__(self, int32_t ID, double relspeed, int32_t baudrate, uint8_t pipe_up, uint8_t pipe_down, uint8_t* cfg, int32_t cfg_len, int32_t localtime0):
		self.Peer = zmq_endpoint.ep_init_peer(ID, relspeed, baudrate, pipe_up, pipe_down, cfg, cfg_len, localtime0)
		if self.Peer is NULL:
			raise MemoryError()

	def __dealloc__(self):
		zmq_endpoint.ep_close(self.Peer)
		self.Peer = NULL

	def push_pl(self, vc, pl:bytes):
		if len(pl) > 177:
			raise AssertionError("Pl too long {}".format(len(pl)))
		return self._vc_push_pl(vc, pl, len(pl))

	cdef _vc_push_pl(self, int32_t vc, uint8_t* pl, int32_t pl_len):
		#cdef int32_t r = 0
		with nogil:
			r = zmq_endpoint.ep_vc_push_pl(self.Peer, vc, pl, pl_len)
		return r

	def read_pl(self, vc):
		return self._vc_read_pl(vc)

	cdef _vc_read_pl(self, int32_t vc):
		cdef int32_t r;
		cdef uint8_t[1000] buff;
		cdef uint8_t* tgt = cython.cast(cython.pointer(uint8_t), &buff[0])
		with nogil:
			r = zmq_endpoint.ep_vc_read_pl(self.Peer, vc, tgt, 500)
		if r < 0:
			return None
		return buff[:r]

	def init_arq(self, vc):
		return self._vc_init_arq(vc)

	cdef _vc_init_arq(self, int32_t vc):
		with nogil:
			zmq_endpoint.ep_vc_init_arq(self.Peer, vc)

	def get_status(self):
		b = b""
		r = bytes(self._get_skylink_status())
		status = SkylinkStat()
		x = struct.unpack("7i", r[0:4*7])
		status.my_window, status.peer_window, status.own_remaining, status.peer_remaining, status.last_mac_update, status.tick, status.radio_state = x
		for i in range(4):
			c = (4*7) + i * SIZE_VCSTATUS
			x = struct.unpack("iI6i", r[c:c+SIZE_VCSTATUS])
			vc = VCStat()
			vc.arq_state, vc.arq_identity, vc.n_to_tx, vc.n_to_resend, vc.tx_full, vc.n_to_read, vc.hmac_tx_seq, vc.hmac_rx_seq = x
			status.vcs.append(vc)
		return status

	cdef _get_skylink_status(self):
		cdef zmq_endpoint.SkylinkStatus status;
		cdef int r;
		cdef uint8_t* tgt;
		with nogil:
			r = zmq_endpoint.ep_get_skylink_status(self.Peer, &status)
		tgt = cython.cast(cython.pointer(uint8_t), &status)
		return tgt[:r]












