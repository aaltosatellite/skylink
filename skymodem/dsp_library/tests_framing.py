import time
import numpy as np
from matplotlib import pyplot as plt
from kuokka.lib_tools import CC1125_symbolrate_for_M_E, CC1125_M_E_for_symbolrate
from kuokka.lib_tools import CC1125_peak_deviation_for_M_E, CC1125_DEV_M_E_for_peak_deviation
from kuokka.lib_tools import CCSDS_TM_whitener_sequence, DEFAULT_SYNCHWORD
from kuokka.lib_reedsolomon import RS_decode, RS_encode, get_default_rs, RS_MAX_PL_LEN
from kuokka.lib_framing import deframe, frame_packet, create_deframer, encode_golay24, decode_golay24, deframe_synchword

# TOOLS =======================================================================================================================================================
# TOOLS =======================================================================================================================================================
def corrupt_n_bits_of_bytearr(bytearr, n_corrupt):
	corrupted = bytearr.copy()
	assert np.all(corrupted < 256)
	assert np.all(corrupted >= 0)
	assert corrupted.dtype in (np.int64, np.int32, np.int16, np.int8)
	icib_set = set()
	ichar = -1
	ibit = -1
	icib_set.add( (-1,-1) )
	n_roll = 0
	for _ in range(n_corrupt):
		while (ichar, ibit) in icib_set:
			ichar = np.random.randint(0,len(corrupted))
			ibit = np.random.randint(0,8)
			n_roll += 1
		corrupted[ichar] = corrupted[ichar] ^ (1 << ibit)
		icib_set.add( (ichar,ibit) )
	assert n_roll >= n_corrupt
	assert len(icib_set) == (n_corrupt +1)
	return corrupted

def corrupt_n_bits_of_bitarr(bitarr, n_corrupt):
	corrupted = bitarr.copy()
	assert np.all(corrupted <= 1)
	assert np.all(corrupted >= 0)
	assert corrupted.dtype in (np.int64, np.int32, np.int16, np.int8)
	nbits = len(corrupted)
	ibit_set = {-1}
	ibit = -1
	for _ in range(n_corrupt):
		while ibit in ibit_set:
			ibit = np.random.randint(0,nbits)
		corrupted[ibit] = corrupted[ibit] ^ 1
		ibit_set.add( ibit )
	assert len(ibit_set) == (n_corrupt +1)
	return corrupted
# TOOLS =======================================================================================================================================================
# TOOLS =======================================================================================================================================================





def utest_CCSDS_whitening_pseudorandom(do_print):
	bitref = "111111110100100000001110110000001001101"  #Ref: CCSDS 131.0-B-4, 10.4.1
	byteref = np.array([0xff, 0x48, 0x0e, 0xc0, 0x9a, 0x0d], dtype=np.int32)
	Ba, ba = CCSDS_TM_whitener_sequence(8 * 6)
	bitstring = "".join( [str(x) for x in ba] )
	check1 = bitstring.startswith(bitref)
	check2 = np.all(Ba == byteref)
	print("")
	print("## CCSDS whitening pseudorandom ================================")
	print("\tbit-array matches reference:  {}".format( check1 ))
	print("\tbyte-array matches reference: {}".format( check2 ))
	assert check1
	assert check2
	if do_print:
		print( [hex(x) for x in Ba])
		Ba, ba = CCSDS_TM_whitener_sequence(8 * 1024)
		assert len(Ba) == 1024
		print("[")
		for ii in range(len(Ba)):
			print("{}, ".format( hex(Ba[ii]) ) + " "*(Ba[ii] < 16) , end="")
			if (ii%24) == 23:
				print("")
		print("]")
	print("\t[Passes.]")
	print("## CCSDS whitening pseudorandom ================================")
	print("")



def utest_chip_symbolrate_config_math(do_plot):
	print("")
	print("## Chip symbolrate config math ================================")
	max_E = 0x0a
	symrate_cap = CC1125_symbolrate_for_M_E(SRATE_M=2 ** 20 - 1, SRATE_E=max_E)
	n_rounds = 100000
	for _ in range(n_rounds):
		symrate = (np.random.random()**2) * symrate_cap
		M,E = CC1125_M_E_for_symbolrate(symbolrate=symrate)
		max_deviation = CC1125_symbolrate_for_M_E(SRATE_M=1, SRATE_E=max_E) - CC1125_symbolrate_for_M_E(SRATE_M=0, SRATE_E=E)
		symrate_computed = CC1125_symbolrate_for_M_E(SRATE_M=M, SRATE_E=E)
		assert abs(symrate_computed - symrate) < (max_deviation*1.001)
	print("\tAll {} test rounds passed.".format(n_rounds))
	print("\tFor 9600: {}".format(CC1125_symbolrate_for_M_E(*CC1125_M_E_for_symbolrate(9600))))
	print("\tFor 800:  {}".format(CC1125_symbolrate_for_M_E(*CC1125_M_E_for_symbolrate(800))))
	print("\tFor 120:  {}".format(CC1125_symbolrate_for_M_E(*CC1125_M_E_for_symbolrate(800))))
	print("\t[Passes.]")
	print("## =============================================================")
	print("")
	if do_plot:
		all_rates = list()
		for E in range(0, 16):
			for M in range(0, 2**20):
				all_rates.append(CC1125_symbolrate_for_M_E(SRATE_M=M, SRATE_E=E))
		xx = np.arange(len(all_rates))

		fig = plt.figure(figsize=(14,11))
		ax1 = fig.add_subplot(211)
		ax2 = fig.add_subplot(212)

		ax1.plot(xx[::10], all_rates[::10], marker="x")
		ax1.semilogy()
		ax1.grid()

		ax2.plot(xx[1::5], 1 /  np.array(all_rates[1::5]), marker="x")
		ax2.semilogy()
		ax2.grid()

		fig.set_layout_engine("tight")
		plt.show()



def utest_chip_peak_deviation_config_math():
	print("## Chip peak deviation config math ======================================")
	print("\tAlgorithm for obtaining E & M gives the best or second best combination.")
	max_diff = (40e6 / (2**24)) * (2**7)
	n_rep = 4000
	for i_tst in range(n_rep):
		f_dev_wish = (np.random.random()**3) * 155000  # f_dev_max is just under 156kHz
		M,E = CC1125_DEV_M_E_for_peak_deviation(f_dev=f_dev_wish)
		f_dev_got = CC1125_peak_deviation_for_M_E(DEV_M=M, DEV_E=E)
		diff = abs(f_dev_wish  - f_dev_got)
		assert diff < max_diff
		n_better_params = 0
		for DEV_E in range(2**3):
			for DEV_M in range(2**8):
				f_dev_x = CC1125_peak_deviation_for_M_E(DEV_M=M, DEV_E=E)
				diff_x = abs(f_dev_x - f_dev_wish)
				if diff_x < diff:
					n_better_params += 1
		#if i_tst < 5:
		#	print("\t{} better params.".format(n_better_params))
		assert n_better_params <= 1
	print("\tIn all {} attempts, this held true.".format(n_rep))
	print("\t[Passes.]")
	print("## Peak deviation config math ============================================")
	print("")



def plot_peak_deviations():
	fdev_arr1 = list()
	fdev_arr2 = list()
	for DEV_E in range(1,2**3):
		for DEV_M in range(2**8):
			f_dev = CC1125_peak_deviation_for_M_E(DEV_M=DEV_M, DEV_E=DEV_E)
			fdev_arr2.append(f_dev)

	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(111)

	ax1.plot(np.arange(len(fdev_arr1)), fdev_arr1, marker="x")
	ax1.plot(np.arange(len(fdev_arr2)), fdev_arr2, marker="x")
	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()








def utest_golay24():
	print("")
	print("## Golay24 ===================================================")
	print("\tno errors:")
	for x in range(2**12 -1):
		encoded = encode_golay24(x)
		decoded = decode_golay24(encoded)[0]
		assert x == decoded, (x, encoded, decoded)
	print("\t\tAll decoded successfully")

	nrep = 40000
	print("\t1 error:")
	for _ in range(nrep):
		x = np.random.randint(0, 2**12 -1)
		encoded = encode_golay24(x)
		enc_scrambled = encoded
		for _ in range(1):
			ibit = np.random.randint(0,24)
			enc_scrambled = enc_scrambled ^ (1 << ibit)
		decoded = decode_golay24(enc_scrambled)[0]
		assert x == decoded, (x, encoded, decoded)
	print("\t\tAll decoded successfully")

	print("\t2 errors:")
	for _ in range(nrep):
		x = np.random.randint(0, 2**12 -1)
		encoded = encode_golay24(x)
		enc_scrambled = encoded
		for _ in range(2):
			ibit = np.random.randint(0,24)
			enc_scrambled = enc_scrambled ^ (1 << ibit)
		decoded = decode_golay24(enc_scrambled)[0]
		assert x == decoded, (x, encoded, decoded)
	print("\t\tAll decoded successfully")

	print("\t3 errors:")
	for _ in range(nrep):
		x = np.random.randint(0, 2**12 -1)
		encoded = encode_golay24(x)
		enc_scrambled = encoded
		for _ in range(3):
			ibit = np.random.randint(0,24)
			enc_scrambled = enc_scrambled ^ (1 << ibit)
		decoded = decode_golay24(enc_scrambled)[0]
		assert x == decoded, (x, encoded, decoded)
	print("\t\tAll decoded successfully")

	print("\t4 errors:")
	for _ in range(nrep):
		x = np.random.randint(0, 2**12 -1)
		encoded = encode_golay24(x)
		enc_scrambled = encoded
		for _ in range(4):
			ibit = np.random.randint(0,24)
			enc_scrambled = enc_scrambled ^ (1 << ibit)
		decoded = decode_golay24(enc_scrambled)[0]
		assert (decoded == x) or (decoded == -1), (x, encoded, decoded)
	print("\t\tAll decoded successfully OR recognized as corrupt")
	print("\t[Passes.]")
	print("## Golay24 test ==============================================")

def speedbench_golay():
	x = np.random.randint(0, 2**12 -1)
	encode_golay24(x)
	encode_golay24(x)
	t0 = time.perf_counter()
	for _ in range(1000):
		encode_golay24(x)
		encode_golay24(x)
		encode_golay24(x)
	T_call = (time.perf_counter() - t0) / 3000
	print("")
	print("-- Golay24 encode ---------------------------------")
	print("\tT_encode:           {} µs".format( round( 1e6*T_call, 3) ))
	print("\tencoding speed:     {} M/s".format( round( 1e-6*1/T_call, 3) ))
	print("---------------------------------------------------")

	enc = encode_golay24( np.random.randint(0, 2**12 -1))
	decode_golay24(enc)
	decode_golay24(enc)
	t0 = time.perf_counter()
	for _ in range(1000):
		decode_golay24(enc)
		decode_golay24(enc)
		decode_golay24(enc)
	T_call = (time.perf_counter() - t0) / 3000
	print("-- Golay24 decode ---------------------------------")
	print("\tT_decode:           {} µs".format( round( 1e6*T_call, 3) ))
	print("\tdecoding speed:     {} M/s".format( round( 1e-6*1/T_call, 3) ))
	print("---------------------------------------------------")
	print("")










def utest_Reed_Solomon_1():
	rs_mx, rs_cfg = get_default_rs()
	print("")
	print("## Reed Solomon 1 =============================================")
	for msglen in range(0, RS_MAX_PL_LEN+1):
		for n_error in range(0, 16+1):
			for _ in range(8):
				msg = np.random.randint(0,256, msglen)
				msg_encoded = RS_encode(msg=msg, rs_mx=rs_mx, rs_cfg=rs_cfg)
				corrupted = corrupt_n_bits_of_bytearr(msg_encoded, n_corrupt=n_error)
				decoded, errcount = RS_decode(msg=corrupted, rs_mx=rs_mx, rs_cfg=rs_cfg)
				assert errcount >= 0
				assert len(decoded) == len(msg)
				assert np.allclose(decoded, msg)
	print("\tAll with less than 16 errors corrected successfully.")
	print("\t[Passes.]")
	print("## Reed Solomon ===============================================")
	print("")



def utest_Reed_Solomon_2(do_plot):
	rs_mx, rs_cfg = get_default_rs()
	nrep = 320
	n_error_array = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,]
	ok_fraction_array_64 = list()
	avg_errorcount_array_64 = list()
	ok_fraction_array_223 = list()
	avg_errorcount_array_223 = list()
	print("")
	print("## Reed Solomon 2 =============================================")
	for msglen,okfrac_array,errorcount_arr in ( (64,ok_fraction_array_64,avg_errorcount_array_64), (223,ok_fraction_array_223,avg_errorcount_array_223) ):
		for n_error in n_error_array:
			okcount 			= 0
			misread_ok_count 	= 0
			errorcount_sum 		= 0
			for _ in range(nrep):
				#msglen = np.random.randint(1, 224)
				msg = np.random.randint(0,256, msglen)
				msg_encoded = RS_encode(msg=msg, rs_mx=rs_mx, rs_cfg=rs_cfg)
				corrupted = corrupt_n_bits_of_bytearr(msg_encoded, n_corrupt=n_error)
				decoded, errcount = RS_decode(msg=corrupted, rs_mx=rs_mx, rs_cfg=rs_cfg)
				if errcount < 0:
					continue
				if (len(decoded) != len(msg)) or (not np.allclose(decoded, msg)):
					misread_ok_count += 1
					continue
				okcount += 1
				errorcount_sum += errcount
			if n_error <= 16:
				assert okcount == nrep
			if (n_error > 16) and (nrep > 10):
				assert okcount < nrep
			okfrac_array.append( okcount / nrep )
			errorcount_arr.append( errorcount_sum / max(1,okcount) )
	print("\tAll with less than 16 errors corrected successfully.")
	print("\tDecoding probability drops off after 16 as expected.")
	print("\t[Passes.]")
	print("## Reed Solomon ===============================================")
	print("")

	if do_plot:
		fig = plt.figure(figsize=(14,11))
		ax1 = fig.add_subplot(211)
		ax2 = fig.add_subplot(212)

		ax1.plot(n_error_array, ok_fraction_array_64, marker="x", label="msglen=64")
		ax1.plot(n_error_array, ok_fraction_array_223, marker="x", label="msglen=223")
		ax1.grid()
		ax1.legend()
		ax1.set_ylabel("fraction of successful decode")
		ax1.set_xlabel("# bit errors")

		ax2.plot(n_error_array, avg_errorcount_array_64, marker="x", label="msglen=64")
		ax2.plot(n_error_array, avg_errorcount_array_223, marker="x", label="msglen=223")
		ax2.plot(n_error_array, n_error_array, label="x=y", linestyle="--", color="black")
		ax2.grid()
		ax2.legend()
		ax2.set_ylabel("estimated arrorcount")
		ax2.set_xlabel("# bit errors")

		fig.set_layout_engine("tight")
		plt.show()



def speedbench_Reed_Solomon():
	rs_mx, rs_cfg = get_default_rs()
	nrep_speedbench = 2000

	msg_x = np.random.randint(0,256, 223)
	_ = RS_encode(msg=msg_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	_ = RS_encode(msg=msg_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	_ = RS_encode(msg=msg_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	t0 = time.perf_counter()
	for _ in range(nrep_speedbench):
		_ = RS_encode(msg=msg_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	T_call_encode 	 	= ((time.perf_counter() - t0) / nrep_speedbench) - 15e-9
	speed_encode 	 	= 1/T_call_encode
	speed_bps_encode 	= speed_encode * len(msg_x)
	print("")
	print("-- Reed Solomon encoding ---------------------------")
	print("\tT_encode:            {} µs".format( round(1e6 * T_call_encode, 3) ))
	print("\tencode speed:        {} kmsg/s".format( round(1e-3 * speed_encode, 3) ))
	print("\tencode speed:        {} Mbyte/s".format( round(1e-6 * speed_bps_encode, 3) ))
	print("\tencode speed:        {} Mbit/s".format( round(1e-6 * speed_bps_encode*8, 3) ))
	print("----------------------------------------------------")


	msg_encoded_x = RS_encode(msg=msg_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	corrupted_x = corrupt_n_bits_of_bytearr(msg_encoded_x, n_corrupt=8)
	_ = RS_decode(msg=corrupted_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	_ = RS_decode(msg=corrupted_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	_ = RS_decode(msg=corrupted_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	t0 = time.perf_counter()
	for _ in range(nrep_speedbench):
		_ = RS_decode(msg=corrupted_x, rs_mx=rs_mx, rs_cfg=rs_cfg)
	T_call_decode 		= ((time.perf_counter() - t0) / nrep_speedbench) - 15e-9
	speed_decode 		= 1/T_call_decode
	speed_bps_decode	= speed_decode * len(msg_x)
	print("-- Reed Solomon decoding ---------------------------")
	print("\tT_decode:            {} µs".format( round(1e6 * T_call_decode, 3) ))
	print("\tdecode speed:        {} kmsg/s".format( round(1e-3 * speed_decode, 3) ))
	print("\tdecode speed:        {} Mbyte/s".format( round(1e-6 * speed_bps_decode, 3) ))
	print("\tdecode speed:        {} Mbit/s".format( round(1e-6 * speed_bps_decode*8, 3) ))
	print("----------------------------------------------------")
	print("")



def utest_synchword_deframing():
	print("")
	print("## Synchword deframing =============================================")
	synchbits0 = np.zeros(32, dtype=np.int32)
	ib = 0
	for i in range(32-1, -1, -1):
		synchbits0[ib] = (DEFAULT_SYNCHWORD >> i) & 1
		ib += 1
	rint = np.random.randint
	synch_len_mask = (2**32)-1
	for n_corrupt in range(6):
		for threshold in range(4):
			for _ in range(36):
				synchbits = corrupt_n_bits_of_bitarr(bitarr=synchbits0, n_corrupt=n_corrupt)
				n_prefix = rint(0, 64)
				#n_suffix = rint(0, 64)
				prefix = rint(0,2,n_prefix)
				#suffix = rint(0,2,n_suffix)
				bits = np.concatenate( (prefix, synchbits) )
				latest_bits = 0
				ok = 0
				n_errors = -1
				for ib,b in enumerate(bits):
					ok, latest_bits, bit_idx, n_errors = deframe_synchword(bit=b, latest_bits=latest_bits, synchword=DEFAULT_SYNCHWORD, synch_length_mask=synch_len_mask, synchword_len=32, synch_threshold=threshold)
					if ib < (len(bits) -1):
						assert ok == 0, (ok, n_corrupt, threshold, "A")
				assert n_errors == n_corrupt
				if n_corrupt <= threshold:
					assert (ok == 1), (ok, n_corrupt, threshold, "B")
				else:
					assert ok == 0, (ok, n_corrupt, threshold, "C")
	print("\tpass.")
	print("\tTesting stochastic correlation.")
	for nn in [int(1e3), int(1e4), int(1e5), int(1e6), int(2e6)]:
		noise = rint(0,2, nn)
		min_error = 32
		latest_bits = 0
		threshold = 0
		for ib,b in enumerate(noise):
			ok, latest_bits, bit_idx, n_errors = deframe_synchword(bit=b, latest_bits=latest_bits, synchword=DEFAULT_SYNCHWORD, synch_length_mask=synch_len_mask, synchword_len=32, synch_threshold=threshold)
			min_error = min(min_error, n_errors)
		print("\tsmallest error in a noise of {}M length: {}".format( round(1e-6 * nn, 3), min_error))
	print("\t[Passes.]")
	print("## =================================================================")
	print("")



def utest_framing_basic_test_1():
	print("")
	print("## Framing basic test 1 ========================================")
	print("\tTest framing and deframing on packets over all lengths")
	print("\tand corrupted bit counts")
	rint = np.random.randint
	rs_mx, rs_cfg = get_default_rs()
	synchword = DEFAULT_SYNCHWORD
	noiseA = np.random.randint(0,2, 120)
	for pl_len in range(0, RS_MAX_PL_LEN+1):
		for n_corrupt in range(0, 16+1):
			for _ in range(24):
				pl_chars = np.random.randint(0,255, pl_len)
				bits = frame_packet(pl=pl_chars, synchword_int=synchword, synchword_len=32, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
				bits[:32] = corrupt_n_bits_of_bitarr(bitarr=bits[:32], n_corrupt=rint(0,4))
				bits[32:56] = corrupt_n_bits_of_bitarr(bitarr=bits[32:56], n_corrupt=rint(0,4))
				bits[56:] = corrupt_n_bits_of_bitarr(bitarr=bits[56:], n_corrupt=n_corrupt)
				bits = np.concatenate( (noiseA, bits) )
				deframermx = create_deframer(use_scrambler=True, use_rs=True, data_maxlen=255, synchword=synchword, synchword_len=32, synch_threshold=3)
				payloads, payload_delimits, _ = deframe(bits=bits, bit_frequencies=bits*0, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
				assert len(payload_delimits) == 1, (len(payload_delimits), pl_len, n_corrupt, deframermx[1,0])
				pl = payloads[payload_delimits[0,0]:payload_delimits[0,1]]
				assert np.all(pl == pl_chars)
	print("\t[Passes.]")
	print("## =============================================================")
	print("")



def utest_framing_basic_test_2():
	print("")
	print("## Framing basic test 2 ========================================")
	print("\tTest framing and deframing on two packets")
	rint = np.random.randint
	rs_mx, rs_cfg = get_default_rs()
	double_ret = 0
	for _ in range(8000):
		synchword = DEFAULT_SYNCHWORD
		pl_chars1 = np.random.randint(0,255, rint(0,223+1))
		pl_chars2 = np.random.randint(0,255, rint(0,223+1))
		bits1 = frame_packet(pl=pl_chars1, synchword_int=synchword, synchword_len=32, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
		bits2 = frame_packet(pl=pl_chars2, synchword_int=synchword, synchword_len=32, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)

		bits1[:32] = corrupt_n_bits_of_bitarr(bitarr=bits1[:32], n_corrupt=rint(0,4))
		bits2[:32] = corrupt_n_bits_of_bitarr(bitarr=bits2[:32], n_corrupt=rint(0,4))
		bits1[32:56] = corrupt_n_bits_of_bitarr(bitarr=bits1[32:56], n_corrupt=rint(0,4))
		bits2[32:56] = corrupt_n_bits_of_bitarr(bitarr=bits2[32:56], n_corrupt=rint(0,4))
		bits1[56:] = corrupt_n_bits_of_bitarr(bitarr=bits1[56:], n_corrupt=16)
		bits2[56:] = corrupt_n_bits_of_bitarr(bitarr=bits2[56:], n_corrupt=16)

		noiseA = np.random.randint(0,2, np.random.randint(1,320))
		noiseB = np.random.randint(0,2, np.random.randint(0,10))
		noiseC = np.random.randint(0,2, np.random.randint(1,320))
		bits = np.concatenate( (noiseA, bits1, noiseB, bits2, noiseC) )

		deframermx = create_deframer(use_scrambler=True, use_rs=True, data_maxlen=255, synchword=synchword, synchword_len=32, synch_threshold=3)
		bit_head = 0
		pl_list = list()
		while bit_head < len(bits):
			batchlen = rint(0,  400)
			batchlen = min(len(bits) - bit_head, batchlen)
			payload_bits, payload_delimits, _ = deframe(bits=bits[bit_head:bit_head+batchlen], bit_frequencies=bits*0, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
			if len(payload_delimits) > 0:
				for delims in payload_delimits:
					pl_list.append( payload_bits[delims[0]:delims[1]] )
				if len(payload_delimits) > 1:
					double_ret += 1
				#print("ping", payload_delimits)
			bit_head += batchlen
		assert len(pl_list) == 2, len(pl_list)
		assert np.all(pl_list[0] == pl_chars1)
		assert np.all(pl_list[1] == pl_chars2)
	assert double_ret > 0
	print("\t[Passes.]")
	print("## Framing basic test ==========================================")
	print("")



def speedbench_framing():
	#print("[Measuring framing and deframing speed]")
	nreps = 2000
	use_scrambler = True
	use_rs = True

	rs_mx, rs_cfg = get_default_rs()
	synchword = DEFAULT_SYNCHWORD
	pl_chars = np.random.randint(0,255, 122)
	bits = frame_packet(pl=pl_chars, synchword_int=synchword, synchword_len=32, use_scrambler=use_scrambler, use_rs=use_rs, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
	_ = frame_packet(pl=pl_chars, synchword_int=synchword, synchword_len=32, use_scrambler=use_scrambler, use_rs=use_rs, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
	_ = frame_packet(pl=pl_chars, synchword_int=synchword, synchword_len=32, use_scrambler=use_scrambler, use_rs=use_rs, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
	t0 = time.perf_counter()
	for _ in range(nreps):
		_ = frame_packet(pl=pl_chars, synchword_int=synchword, synchword_len=32, use_scrambler=use_scrambler, use_rs=use_rs, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=False)
	T_frame = (time.perf_counter() - t0) / nreps
	speed_packets = 1 / T_frame
	speed_bytes = len(pl_chars) / T_frame
	speed_bits = 8*len(pl_chars) / T_frame
	overmatch_9k6 = speed_bits / 9600
	print("")
	print("-- framing -----------------------------------------")
	print("\tT_frame:          {} µs".format( round(1e6*T_frame, 2) ))
	print("\tspeed:            {} kpacket/s".format( round(1e-3*speed_packets, 2) ))
	print("\tspeed:            {} Mbyte/s".format( round(1e-6*speed_bytes, 2) ))
	print("\tspeed:            {} Mbit/s".format( round(1e-6*speed_bits, 2) ))
	print("\tovermatch 9k6:    {} ".format( round(overmatch_9k6, 2) ))
	print("----------------------------------------------------")

	if use_rs:
		bits = corrupt_n_bits_of_bitarr(bitarr=bits, n_corrupt=6)
	bits = np.array(bits, dtype=np.int8)
	bit_fs = bits*0.0
	deframermx = create_deframer(use_scrambler=use_scrambler, use_rs=use_rs, data_maxlen=255, synchword=synchword, synchword_len=32, synch_threshold=3)
	payloads, payload_delimits, _ = deframe(bits=bits, bit_frequencies=bits*0, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
	assert len(payload_delimits) == 1
	assert np.all(payloads == pl_chars)
	_ = deframe(bits=bits, bit_frequencies=bit_fs, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
	_ = deframe(bits=bits, bit_frequencies=bit_fs, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
	_ = deframe(bits=bits, bit_frequencies=bit_fs, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
	t0 = time.perf_counter()
	for _ in range(nreps):
		_ = deframe(bits=bits, bit_frequencies=bit_fs, deframer_mx=deframermx, rs_mx=rs_mx, rs_cfg=rs_cfg)
	T_deframe = (time.perf_counter() - t0) / nreps
	speed_packets = 1 / T_deframe
	speed_bytes = len(pl_chars) / T_deframe
	speed_bits = len(pl_chars)*8 / T_deframe
	overmatch_9k6 = speed_bits / 9600
	print("-- deframing ---------------------------------------")
	print("\tT_deframe:        {} µs".format( round(1e6*T_deframe, 2) ))
	print("\tspeed:            {} kpacket/s".format( round(1e-3*speed_packets, 2) ))
	print("\tspeed:            {} Mbyte/s".format( round(1e-6*speed_bytes, 2) ))
	print("\tspeed:            {} Mbit/s".format( round(1e-6*speed_bits, 2) ))
	print("\tovermatch 9k6:    {} ".format( round(overmatch_9k6, 2) ))
	print("----------------------------------------------------")
	print("")







utest_CCSDS_whitening_pseudorandom(do_print=False)
utest_chip_symbolrate_config_math(do_plot=False)
utest_chip_peak_deviation_config_math()
#plot_peak_deviations()

utest_golay24()
utest_synchword_deframing()
utest_Reed_Solomon_1()
utest_Reed_Solomon_2(do_plot=True)
utest_framing_basic_test_1()
utest_framing_basic_test_2()

speedbench_golay()
speedbench_Reed_Solomon()
speedbench_framing()











