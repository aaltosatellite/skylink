import struct
cimport amateur_skypacket
from libc.stdlib cimport free
import cython






def compile_packet(identity:bytes, pl:bytes):
	if len(identity) != 6:
		raise AssertionError("Identity lenght must be 6")
	p = _compile_packet(identity, pl, len(pl))
	return len(p), p


cdef _compile_packet(uint8_t* identity, uint8_t* pl, int32_t pl_len):
	cdef int32_t r = 0
	cdef uint8_t[1000] buff;
	cdef uint8_t* tgt = cython.cast(cython.pointer(uint8_t), &buff[0])
	r = skylink_encode_amateur_pl(identity, pl, pl_len, tgt)
	if r < 0:
		return buff[:0]
	return buff[:r]

