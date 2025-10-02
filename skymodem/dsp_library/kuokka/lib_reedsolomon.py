import numpy as np
from numba import njit
"""
Foresail1, Foresail1p, and Suomi100 use this.
const ReedSolomonConfig CCSDS_RS_255_223 = {  
	.symbol_size = 8,
	.primitive_polynomial = 0x187,  // x^8 + x^7 + x^2 + x + 1
	.first_consecutive_root = 112,
	.generator_root_gap = 11,
	.coded_bytes = 223,
	.num_roots = 32,
	.pad = 0
};

const ReedSolomonConfig CCSDS_RS_255_239 = {
	.symbol_size = 8,
	.primitive_polynomial = 0x187,  // x^8 + x^7 + x^2 + x + 1
	.first_consecutive_root = 120,
	.generator_root_gap = 11,
	.coded_bytes = 239,
	.num_roots = 16,
	.pad = 0
};

"""

RS_MAX_PL_LEN 			= 223
RS_MAX_ENCODED_LEN 		= 223+32
RS_MIN_ENCODED_LEN 		= 0+32

def get_default_rs():  # Suomi100 also uses this
	rs_mx, rs_cfg = ReedSolomonInit(symbol_size=8, primitive_polynomial=0x187, first_consecutive_root=112, generator_root_gap=11, coded_bytes=223, num_roots=32, pad=0)
	return rs_mx, rs_cfg


def ReedSolomonInit(symbol_size, primitive_polynomial, first_consecutive_root, generator_root_gap, coded_bytes, num_roots, pad):
	for param in (symbol_size, primitive_polynomial, generator_root_gap, num_roots, first_consecutive_root, pad):
		assert type(param) in (int, np.int64)
	#/* Check parameter ranges */
	if (symbol_size == 0) or (symbol_size > (8 * 1)):  # 1 standin for sizeof(DataType)
		raise AssertionError("ReedSolomon: Invalid Reed Solomon symbol size")
	if first_consecutive_root >= (1 << symbol_size):
		raise AssertionError("ReedSolomon: First consecutive root i")
	if (generator_root_gap == 0) or (generator_root_gap >= (1 << symbol_size)):
		raise AssertionError("ReedSolomon: Primitive polynom term count doesn't match with symbol size!")
	if num_roots >= (1 << symbol_size):
		raise AssertionError("ReedSolomon: Can't have more roots than symbol values!")
	if pad >= ((1 << symbol_size) - 1 - num_roots):
		raise AssertionError("ReedSolomon: Too much padding")


	alpha_to = np.zeros(1024, dtype=np.int32)  	#todo datatype?
	index_of = np.zeros(1024, dtype=np.int32)  	#todo datatype?
	poly = np.zeros(1024, dtype=np.int32)  		#todo datatype?

	# Symbol lookup table sizes
	symbol_count = (1 << symbol_size) - 1
	alpha_to = np.resize(alpha_to, symbol_count +1)
	index_of = np.resize(index_of, symbol_count +1)

	# Generate Galois field lookup tables
	index_of[0] = symbol_count  # log(zero) = -inf
	alpha_to[symbol_count] = 0  # alpha**-inf = 0
	sr = 1
	for i in range(symbol_count):
		index_of[sr] = i
		alpha_to[i] = sr
		sr <<= 1
		if sr & (1 << symbol_size):
			sr ^= primitive_polynomial
		sr &= symbol_count

	if sr != 1:
		raise AssertionError("ReedSolomon: Field generator polynomial is not primitive!")

	# Form RS code generator polynomial from its roots
	poly = np.resize(poly, num_roots+1)

	# Find prim-th root of 1, used in decoding
	iprim = 1
	while (iprim % generator_root_gap) != 0:
		iprim += symbol_count
	assert iprim % generator_root_gap == 0
	iprim = int(iprim / generator_root_gap)

	for i in range(len(poly)):
		poly[i] = 0

	poly[0] = 1
	root = first_consecutive_root * generator_root_gap
	for i in range(num_roots):
		poly[i + 1] = 1

		# Multiply poly[] by  @**(root + x)
		for j in range(i, 0 , -1):
			if poly[j] != 0:
				poly[j] = poly[j - 1] ^ alpha_to[(index_of[poly[j]] + root) % symbol_count]
			else:
				poly[j] = poly[j - 1]

		# poly[0] can never be zero
		poly[0] = alpha_to[(index_of[poly[0]] + root) % symbol_count]
		root += generator_root_gap

	# convert poly[] to index form for quicker encoding
	for i in range(num_roots):
		poly[i] = index_of[poly[i]]

	rs_cfg = np.array( (symbol_size, primitive_polynomial, first_consecutive_root, generator_root_gap, coded_bytes, num_roots, pad), dtype=np.int64)
	rs_mx 			= np.zeros( (4, 512), dtype=np.int64)
	rs_mx[0,0] 	= iprim
	rs_mx[0,1] 	= symbol_count
	rs_mx[1,0] 	= len(alpha_to)
	rs_mx[1,1:1+len(alpha_to)] = alpha_to
	rs_mx[2,0] 	= len(index_of)
	rs_mx[2,1:1+len(index_of)] = index_of
	rs_mx[3,0] 	= len(poly)
	rs_mx[3,1:1+len(poly)] = poly
	return rs_mx, rs_cfg






@njit(cache=True)
def RS_encode(msg, rs_mx, rs_cfg):
	_, _, _, _, cfg_coded_bytes, cfg_num_roots, _ = rs_cfg
	iprim, symbol_count = rs_mx[0,0:2]
	alpha_to 	= rs_mx[1,1:1+rs_mx[1,0]]
	index_of 	= rs_mx[2,1:1+rs_mx[2,0]]
	poly 		= rs_mx[3,1:1+rs_mx[3,0]]
	assert len(msg) <= cfg_coded_bytes
	#if len(msg) > cfg_coded_bytes:
	#	raise AssertionError("Too long message to be coded with Reed Solomon")
	parity = np.zeros(cfg_num_roots, dtype=np.int64) #Todo datatype!??


	pad = cfg_coded_bytes - len(msg)
	m = symbol_count - cfg_num_roots - pad
	for i in range(m):
		feedback = index_of[msg[i] ^ parity[0]]
		if feedback != symbol_count: # feedback term is non-zero
			for j in range(1, cfg_num_roots):
				parity[j] ^= alpha_to[(feedback + poly[cfg_num_roots - j]) % symbol_count]

		# Shift
		#memmove(&parity[0], &parity[1], sizeof(uint8_t) * (cfg_num_roots - 1)) #dst, src, n
		parity[0:cfg_num_roots-1] = parity[1:1+cfg_num_roots-1]
		if feedback != symbol_count:
			parity[cfg_num_roots - 1] = alpha_to[(feedback + poly[0]) % symbol_count]
		else:
			parity[cfg_num_roots - 1] = 0
	encoded = np.concatenate( (msg,parity) )
	return encoded
	#msg.insert(msg.end(), parity.begin(), parity.end())









@njit(cache=True)
def RS_decode(msg, rs_mx, rs_cfg):
	_, _, cfg_first_consecutive_root, cfg_generator_root_gap, cfg_coded_bytes, cfg_num_roots, _ = rs_cfg
	iprim, symbol_count = rs_mx[0,0:2]
	alpha_to 	= rs_mx[1,1:1+rs_mx[1,0]]
	index_of 	= rs_mx[2,1:1+rs_mx[2,0]]
	assert len(msg) >= cfg_num_roots
	#if len(msg) < cfg_num_roots:
	#	raise AssertionError("Too short message")
	assert len(msg) <= (cfg_coded_bytes + cfg_num_roots)
	#if len(msg) > (cfg_coded_bytes + cfg_num_roots):
	#	raise AssertionError("Too long message: ",len(msg))

	A0 = symbol_count
	pad = cfg_coded_bytes - (len(msg) - cfg_num_roots)

	t = np.zeros(cfg_num_roots+1, dtype=np.int64)
	omega = np.zeros(cfg_num_roots+1, dtype=np.int64)
	root = np.zeros(cfg_num_roots, dtype=np.int64)
	reg = np.zeros(cfg_num_roots+1, dtype=np.int64)
	loc = np.zeros(cfg_num_roots, dtype=np.int64)

	# Form the syndromes; i.e., evaluate msg(x) at roots of g(x)
	s = np.zeros(cfg_num_roots, dtype=np.int64)
	for i in range(cfg_num_roots):
		s[i] = msg[0]

	for j in range(1, symbol_count -pad):
		for i in range(cfg_num_roots):
			if s[i] == 0:
				s[i] = msg[j]
			else:
				s[i] = msg[j] ^ alpha_to[(index_of[s[i]] + (cfg_first_consecutive_root + i) * cfg_generator_root_gap) % symbol_count]

	# Convert syndromes to index form, checking for non-zero condition
	syn_error = 0
	for i in range(cfg_num_roots):
		syn_error |= s[i]
		s[i] = index_of[s[i]]

	if syn_error == 0:
		# If syndrome is zero, msg[] is a codeword and there are no errors to correct.
		msg = msg[0:len(msg)-cfg_num_roots]
		return msg, 0

	lambd = np.zeros(cfg_num_roots+1, dtype=np.int32) #Todo dtype
	lambd[0] = 1

	b = np.zeros(cfg_num_roots + 1, dtype=np.int32)
	for i in range(cfg_num_roots+1):
		b[i] = index_of[lambd[i]]

	# Begin Berlekamp-Massey algorithm to determine error+erasure
	# locator polynomial
	r = 0 +1
	el = 0
	while r <= cfg_num_roots: 	# r is the step number
		# Compute discrepancy at the r-th step in poly-form
		discr_r = 0
		for i in range(r):
			if (lambd[i] != 0) and (s[r - i - 1] != A0):
				discr_r ^= alpha_to[(index_of[lambd[i]] + s[r - i - 1]) % symbol_count]

		discr_r = index_of[discr_r]
		if discr_r == A0:
			# 2 lines below: B(x) <-- x*B(x)
			b[1:1+cfg_num_roots] = b[0:cfg_num_roots]
			b[0] = A0

		else:
			# 7 lines below: T(x) <-- lambda(x) - discr_r*x*b(x)
			t[0] = lambd[0]
			for i in range(cfg_num_roots):
				if b[i] != A0:
					t[i + 1] = lambd[i + 1] ^ alpha_to[(discr_r + b[i]) % symbol_count]
				else:
					t[i + 1] = lambd[i + 1]

			if 2 * el <= r - 1:
				el = r - el
				# 2 lines below: B(x) <-- inv(discr_r) *  lambda(x)
				for i in range(cfg_num_roots+1):
					if lambd[i] == 0:
						b[i] = A0
					else:
						b[i] = (index_of[lambd[i]] - discr_r + symbol_count) % symbol_count

			else:
				# 2 lines below: B(x) <-- x*B(x)
				b[1:1+cfg_num_roots] = b[0:cfg_num_roots]
				b[0] = A0

			lambd[0:cfg_num_roots+1] = t[0:cfg_num_roots+1]
		r += 1

	# Convert lambda to index form and compute deg(lambda(x))
	deg_lambda = 0
	for i in range(cfg_num_roots+1):
		lambd[i] = index_of[lambd[i]]
		if lambd[i] != A0:
			deg_lambda = i

	# Find roots of the error+erasure locator polynomial by Chien search
	reg[1:1+cfg_num_roots] = lambd[1:1+cfg_num_roots]
	count = 0 # Number of roots of lambda(x)

	k = iprim - 1
	for i in range(1, symbol_count+1):
		q = 1 # lambda[0] is always 0
		for j in range(deg_lambda, 0, -1):
			if reg[j] != A0:
				reg[j] = (reg[j] + j) % symbol_count
				q ^= alpha_to[reg[j]]

		if q != 0:
			k = (k + iprim) % symbol_count
			continue # Not a root

		# store root (index-form) and error location number
		root[count] = i
		loc[count] = k

		# If we've already found max possible roots, abort the search to save time
		count += 1
		if count == deg_lambda:
			break
		k = (k + iprim) % symbol_count

	if deg_lambda != count:
		# deg(lambda) unequal to number of roots => uncorrectable
		# error detected
		return np.zeros(0, dtype=np.int64), -1

	# Compute err+eras evaluator poly omega(x) = s(x)*lambda(x) (modulo
	# x**cfg.num_roots). in index form. Also find deg(omega).
	deg_omega = deg_lambda - 1
	for i in range(deg_omega+1):
		tmp = 0
		for j in range(i, -1, -1):
			if (s[i - j] != A0) and (lambd[j] != A0):
				tmp ^= alpha_to[(s[i - j] + lambd[j]) % symbol_count]
		omega[i] = index_of[tmp]

	for j in range(count-1, -1, -1):
		num1 = 0
		for i in range(deg_omega, -1, -1):
			if omega[i] != A0:
				num1 ^= alpha_to[(omega[i] + i * root[j]) % symbol_count]

		num2 = alpha_to[(root[j] * (cfg_first_consecutive_root - 1) + symbol_count) % symbol_count]
		den = 0

		# lambda[i+1] for i even is the formal derivative lambda_pr of lambda[i]
		i_start = min(deg_lambda, cfg_num_roots-1) & ~1
		for i in range(i_start, -1, -2):
			if lambd[i + 1] != A0:
				den ^= alpha_to[(lambd[i + 1] + i * root[j]) % symbol_count]

		# Apply error to data
		if (num1 != 0) and (loc[j] >= pad):
			msg[loc[j] - pad] ^= alpha_to[(index_of[num1] + index_of[num2] + symbol_count - index_of[den]) % symbol_count]

	# Truncate the message to remove roots
	msg = msg[0:len(msg)-cfg_num_roots]

	return msg, count
















