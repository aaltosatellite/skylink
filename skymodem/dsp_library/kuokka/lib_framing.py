import numpy as np
from numba import njit
from .lib_reedsolomon import RS_encode, RS_decode, RS_MIN_ENCODED_LEN, RS_MAX_PL_LEN, RS_MAX_ENCODED_LEN
from .lib_tools import ccsds_tm_whitening_bytes




## GOLAY =====================================================================================================================================================================================
## GOLAY =====================================================================================================================================================================================
@njit(cache=True)
def popcount(x, leng):
	sm = 0
	for i in range(leng):
		sm += (x >> i) & 1
	return sm

@njit(cache=True)
def bit_parity_8(x):
	ParityTable = np.array([ #[256]
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	], dtype=np.int64)
	return ParityTable[x]

@njit(cache=True)
def bit_parity_16(x):
	x ^= (x >> 8)
	return bit_parity_8(x & 0xff)

@njit(cache=True)
def bit_parity_32(x):
	x ^= (x >> 16)
	x ^= (x >> 8)
	return bit_parity_8(x & 0xff)

@njit(cache=True)
def bit_parity_64(x):
	x ^= (x >> 32)
	x ^= (x >> 16)
	x ^= (x >> 8)
	return bit_parity_8(x & 0xff)


@njit(cache=True)
def encode_golay24(data_12b):
	H = np.array([ 0x8008ed, 0x4001db, 0x2003b5, 0x100769, 0x80ed1, 0x40da3,
						   0x20b47,  0x1068f,  0x8d1d,   0x4a3b,   0x2477,  0x1ffe], dtype=np.uint32)
	r = data_12b & 0xfff
	N = 12
	s = 0
	for i in range(N):
		s <<= 1
		s |= bit_parity_32(H[i] & r)
	ret = ((0xFFF & s) << N) | r
	return ret


@njit(cache=True)
def decode_golay24(r_32):
	r_32 = r_32 & 0xffffff
	H = np.array([ 0x8008ed, 0x4001db, 0x2003b5, 0x100769, 0x80ed1, 0x40da3,
						   0x20b47,  0x1068f,  0x8d1d,   0x4a3b,   0x2477,  0x1ffe], dtype=np.uint32)
	N = 12
	s = 0
	for i in range(N):
		s <<= 1
		s |= bit_parity_32(H[i] & r_32) # must be 32
	s = s & 0xffff
	# Step 2. if w(s) <= 3, then e = (s, 0) and go to step 8
	count = popcount(s, 16)
	if count <= 3:
		e = s
		e <<= N
		return step8(r=r_32, e=e)
	# Step 3. if w(s + B[i]) <= 2, then e = (s + B[i], e_{i+1}) and go to step 8
	for i in range(N):
		count = popcount(s ^ (H[i] & 0xfff), 16)
		if count <= 2:
			e = s ^ (H[i] & 0xfff)
			e <<= N
			e |= 1 << (N - i - 1)
			return step8(r=r_32, e=e)
	# Step 4. compute q = B*s
	q = 0
	for i in range(N):
		q <<= 1
		q |= bit_parity_16((H[i] & 0xfff) & s)
	q = q & 0xffff
	# Step 5. If w(q) <= 3, then e = (0, q) and go to step 8
	count = popcount(q, 32)
	if count <= 3:
		e = q
		return step8(r=r_32, e=e)
	# Step 6. If w(q + B[i]) <= 2, then e = (e_{i+1}, q + B[i]) and got to step 8
	for i in range(N):
		count = popcount(q ^ (H[i] & 0xfff), 16)
		if count <= 2:
			e = 1 << (2 * N - i - 1)
			e |= q ^ (H[i] & 0xfff)
			return step8(r=r_32, e=e)
	# Step 7. r is uncorrectable
	return -1, -1


@njit(cache=True)
def step8(r, e):
	# Step 8. c = r + e
	res = (r ^ e) & 0xfff
	count = popcount(e, 32)
	return res, count
## GOLAY =====================================================================================================================================================================================
## GOLAY =====================================================================================================================================================================================






## FRAMING ===================================================================================================================================================================================
## FRAMING ===================================================================================================================================================================================
@njit(cache=True)
def frame_packet(pl, synchword_int, synchword_len, use_scrambler, use_rs, rs_mx, rs_cfg, nrz_shift):
	if use_rs:
		assert len(pl) <= RS_MAX_PL_LEN
	if use_rs:
		data = RS_encode(msg=pl, rs_mx=rs_mx, rs_cfg=rs_cfg)
	else:
		data = pl
	n_chars = len(data)

	nbits = synchword_len + 24 + 8*n_chars
	bits = np.zeros(nbits, dtype=np.int64) #todo this typing is idiotic

	ibit = 0
	for i in range(synchword_len-1, -1, -1):
		bits[ibit] = (synchword_int >> i) & 1
		ibit += 1

	golay = encode_golay24(n_chars)
	for i in range(24-1, -1, -1):
		bits[ibit] = (golay >> i) & 1
		ibit += 1

	for ic, c in enumerate(data):
		if use_scrambler:
			c = c ^ ccsds_tm_whitening_bytes[ic]
		for ib in range(8-1,-1,-1):
			bits[ibit] = (c >> ib) & 1
			ibit += 1

	if nrz_shift:
		bits = bits*2 - 1

	return bits


@njit(cache=True)
def deframe_synchword(bit, latest_bits, synchword, synch_length_mask, synchword_len, synch_threshold): # return: [ok, latest_bits, bit_idx, n_errors]
	latest_bits = ((latest_bits<<1) | bit) & synch_length_mask
	errmap = (latest_bits & synch_length_mask) ^ synchword
	n_errs = popcount(x=errmap, leng=synchword_len)
	if n_errs <= synch_threshold:
		return np.array((1, 0, 0, n_errs), dtype=np.int64)							# synchword found. Proceed to header deframing.
	return np.array((0, latest_bits, 0, n_errs), dtype=np.int64) 					# synchword not found. Continue.


@njit(cache=True)
def deframe_header(bit, latest_bits, bit_idx, use_rs, data_maxlen): # return: [ok, latest_bits, bit_idx, data_len]
	#print(type(latest_bits), type(bit))
	latest_bits = ((latest_bits << 1) | bit) & 0xffffffff
	bit_idx += 1
	if bit_idx < 24:
		return np.array((0, latest_bits, bit_idx, -1), dtype=np.int64)		# not enough bits to decode. Continue.
	data_len, errcount = decode_golay24(r_32=latest_bits)
	if errcount < 0:
		return np.array((-1, 0, 0, -1), dtype=np.int64)  					# Golay decoding failed. Reset.
	if use_rs and ((data_len < RS_MIN_ENCODED_LEN) or (data_len > RS_MAX_ENCODED_LEN)):
		#print("ld::: ",data_len)
		return np.array((-2, 0, 0, -1), dtype=np.int64)						# Golay decoding produced an impossible length. Reset.
	if (not use_rs) and (data_len > data_maxlen):
		return np.array((-3, 0, 0, -1), dtype=np.int64)						# Golay decoding produced an impossible length. Reset.
	return np.array((1, 0, 0, data_len), dtype=np.int64)						# Golay decoding succeeded. Proceed to payload deframing.


@njit(cache=True)
def deframe_payload(bit, latest_bits, bit_idx, chars, char_idx, use_scrambler, data_len, use_rs, rs_mx, rs_cfg):  # return: [ok, latest_bits, bit_idx, char_idx, pl_len]
	latest_bits = ((latest_bits << 1) | bit) #& 0xff
	bit_idx += 1
	if bit_idx < 8:
		return np.array((0, latest_bits, bit_idx, char_idx, -1), dtype=np.int64)		# Not enough bits to decode. Continue.
	bit_idx = 0
	char = latest_bits
	latest_bits = 0
	if use_scrambler:
		char = char ^ ccsds_tm_whitening_bytes[char_idx]
	chars[char_idx] = char
	char_idx += 1
	if char_idx < data_len:
		return np.array((0, latest_bits, bit_idx, char_idx, -1), dtype=np.int64)		# Not enough bits to decode. Continue.
	if use_rs:
		decoded_pl, errcount = RS_decode(msg=chars[0:char_idx], rs_mx=rs_mx, rs_cfg=rs_cfg)
		if errcount < 0:
			return np.array((-1, 0, 0, 0, -1), dtype=np.int64)						# Enough bits, but Reed Solomon decoding failed. Reset.
		chars[0:len(decoded_pl)] = decoded_pl
		pl_len = len(decoded_pl)
		return np.array((1, 0, 0, 0, pl_len), dtype=np.int64)						# Enough bits and Reed Solomon decoding succeeded. Reset and take <len> bytes.
	pl_len = char_idx
	return np.array((1, 0, 0, 0, pl_len), dtype=np.int64)							# Enough bits and no FEC employed. Reset and take <len> bytes.


def create_deframer(use_scrambler, use_rs, data_maxlen, synchword, synchword_len, synch_threshold):
	assert synchword_len < 64
	assert synchword_len >= 16
	assert synch_threshold >= 0
	assert synch_threshold < synchword_len
	assert synchword < (2**synchword_len)
	assert synchword >= 0
	assert data_maxlen >= RS_MAX_ENCODED_LEN
	assert data_maxlen <= 1024
	if use_rs:
		data_maxlen = RS_MAX_ENCODED_LEN
	mx = np.zeros((3, data_maxlen + 1), dtype=np.int64)
	mx[0,0] = int(bool(use_scrambler))
	mx[0,1] = int(bool(use_rs))
	mx[0,2] = synchword
	mx[0,3] = (2**synchword_len) - 1
	mx[0,4] = synchword_len
	mx[0,5] = synch_threshold
	mx[0,6] = data_maxlen

	mx[1,0] = 0  	# state
	mx[1,1] = 0		# latest bits
	mx[1,2] = 0		# bit index
	mx[1,3] = 0		# char index
	mx[1,4] = 0		# encoded data length
	mx[1,5] = 0		# frequency sum
	mx[1,6] = 0		# frequency sum count
	mx[1,7] = 0		# power sum						# for power sense
	mx[1,8] = 0		# noise power avg sum			# for power sense
	mx[1,9] = 0		# power sum count				# for power sense
	mx[1,10] = 0	# power_scaler_om1				# for power sense
	mx[1,11] = 0	# power_scaler_om2				# for power sense
	mx[2,:] = 0		# chars
	return mx


@njit(cache=True)
def deframe(bits, bit_frequencies, bit_powers, deframer_mx, rs_mx, rs_cfg):  # "bit_frequencies" can be just an array of zeros. Has to be as long as "bits"
	use_scrambler 		= deframer_mx[0,0]
	use_rs 				= deframer_mx[0,1]
	synchword 			= deframer_mx[0,2]
	synch_length_mask 	= deframer_mx[0,3]
	synchword_len 		= deframer_mx[0,4]
	synch_threshold 	= deframer_mx[0,5]
	data_maxlen 		= deframer_mx[0,6]

	state 				= deframer_mx[1,0]
	latest_bits 		= deframer_mx[1,1]
	bit_idx 			= deframer_mx[1,2]
	char_idx 			= deframer_mx[1,3]
	data_len 			= deframer_mx[1,4]
	frequency_sum 		= deframer_mx[1,5]
	frequency_sum_count = deframer_mx[1,6]
	power_sum 			= deframer_mx[1,7]		# for power sense
	noise_avg_sum 		= deframer_mx[1,8]		# for power sense
	power_sum_count 	= deframer_mx[1,9]		# for power sense
	power_scaler_om1 	= deframer_mx[1,10]		# for power sense
	power_scaler_om2 	= deframer_mx[1,11]		# for power sense
	chars 				= deframer_mx[2]

	power_scaler1 = 10.0**power_scaler_om1
	power_scaler2 = 10.0**power_scaler_om2
	payloads = np.zeros( 0, dtype=np.uint8)
	payload_delimits = np.zeros( (0, 2), dtype=np.int64)
	payload_frequencies = np.zeros( 0, dtype=np.float64)
	payload_powertuples = np.zeros( (0,3), dtype=np.float64)			# for power sense
	pl_head = 0
	fault_counts = np.zeros(2, dtype=np.int64)

	ib = -1
	for bit in bits:
		ib += 1
		if state == 0:
			found, latest_bits, bit_idx, n_errs = deframe_synchword(bit=bit, latest_bits=latest_bits, synchword=synchword, synch_length_mask=synch_length_mask, synchword_len=synchword_len, synch_threshold=synch_threshold)
			if found:
				#print("\t(Deframer 0 > 1)", n_errs)
				state = 1
				frequency_sum = int(1e9 * bit_frequencies[ib])
				frequency_sum_count = 1
				power_scaler_om1 = int(np.log10(1e6 * (1/bit_powers[ib,0])))
				power_scaler_om2 = int(np.log10(1e6 * (1/bit_powers[ib,1])))
				power_scaler1 = 10.0**power_scaler_om1
				power_scaler2 = 10.0**power_scaler_om2
				power_sum = int(power_scaler1 * bit_powers[ib,0])		# for power sense
				noise_avg_sum = int(power_scaler2 * bit_powers[ib,1])	# for power sense
				power_sum_count = 1										# for power sense
			continue
		elif state == 1:
			ok, latest_bits, bit_idx, data_len = deframe_header(bit=bit, latest_bits=latest_bits, bit_idx=bit_idx, use_rs=use_rs, data_maxlen=data_maxlen)
			frequency_sum += int(1e9 * bit_frequencies[ib])
			frequency_sum_count += 1
			power_sum += int(power_scaler1 * bit_powers[ib,0])		# for power sense
			noise_avg_sum += int(power_scaler2 * bit_powers[ib,1])	# for power sense
			power_sum_count += 1									# for power sense
			if ok < 0:
				#print("\t(Deframer << 0! (Header deframe failed.))", ok)
				fault_counts[0] += 1
				state = 0
			if ok == 1:
				#print("\t(Deframer 1 > 2)")
				state = 2
				char_idx = 0
			continue
		elif state == 2:
			ok, latest_bits, bit_idx, char_idx, pl_leng = deframe_payload(bit=bit, latest_bits=latest_bits, bit_idx=bit_idx, chars=chars, char_idx=char_idx, use_scrambler=use_scrambler, data_len=data_len, use_rs=use_rs, rs_mx=rs_mx, rs_cfg=rs_cfg)
			frequency_sum += int(1e9 * bit_frequencies[ib])
			frequency_sum_count += 1
			power_sum += int(power_scaler1 * bit_powers[ib,0])		# for power sense
			noise_avg_sum += int(power_scaler2 * bit_powers[ib,1])	# for power sense
			power_sum_count += 1									# for power sense
			if ok < 0:
				#print("\t(Deframer << 0! (Decode failed.))")
				fault_counts[1] += 1
				state = 0
			if ok == 1:
				#print("\t(Deframer finished successfully!)")
				state = 0
				payload_delimits0 = np.zeros( (len(payload_delimits)+1, 2), dtype=np.int64 )
				payload_delimits0[:len(payload_delimits),:] = payload_delimits
				payload_delimits = payload_delimits0
				#payload_delimits = np.resize( payload_delimits, (len(payload_delimits)+1, 2) )
				payload_delimits[-1][0] = pl_head
				payload_delimits[-1][1] = pl_head+pl_leng
				payloads = np.concatenate( (payloads, np.zeros(pl_leng, dtype=np.uint8)) )
				#payloads = np.resize(payloads, len(payloads) + pl_leng)
				payloads[pl_head:pl_head+pl_leng] = chars[0:pl_leng]
				pl_head = pl_head + pl_leng
				freq = (1.0e-9*frequency_sum) / (1.0*frequency_sum_count)
				payload_frequencies0 = np.zeros(len(payload_frequencies)+1, dtype=np.float64)
				payload_frequencies0[:len(payload_frequencies)] = payload_frequencies
				payload_frequencies = payload_frequencies0
				#payload_frequencies = np.resize(payload_frequencies, len(payload_frequencies)+1)
				payload_frequencies[-1] = freq
				power = (power_sum/power_scaler1) / (1.0*power_sum_count)											# for power sense
				noise_avg = (noise_avg_sum/power_scaler2) / (1.0*power_sum_count)									# for power sense
				payload_powertuples0 = np.zeros((len(payload_powertuples)+1, 3), dtype=np.float64)	# for power sense
				payload_powertuples0[:len(payload_powertuples)] = payload_powertuples						# for power sense
				payload_powertuples = payload_powertuples0													# for power sense
				payload_powertuples[-1] = power, noise_avg, abs(bit_powers[ib,2])							# for power sense
			continue
	deframer_mx[1,0] = state
	deframer_mx[1,1] = latest_bits
	deframer_mx[1,2] = bit_idx
	deframer_mx[1,3] = char_idx
	deframer_mx[1,4] = data_len
	deframer_mx[1,5] = frequency_sum
	deframer_mx[1,6] = frequency_sum_count
	deframer_mx[1,7] = power_sum			# for power sense
	deframer_mx[1,8] = noise_avg_sum		# for power sense
	deframer_mx[1,9] = power_sum_count		# for power sense
	deframer_mx[1,10] = power_scaler_om1	# for power sense
	deframer_mx[1,11] = power_scaler_om2	# for power sense
	deframer_mx[2]   = chars
	return payloads, payload_delimits, payload_frequencies, payload_powertuples, fault_counts
## FRAMING ===================================================================================================================================================================================
## FRAMING ===================================================================================================================================================================================

