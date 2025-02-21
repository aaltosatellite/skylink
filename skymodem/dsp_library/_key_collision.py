import numpy as np
from matplotlib import pyplot as plt
from numba import njit
import time
import os

def experiment_key_trigger_levels():
	keylens = (13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36)
	ratios = list()
	for keylen in keylens:
		key = np.random.randint(0,2, keylen)*2 - 1
		N_found = 0
		for _ in range(100000):
			z = np.max(np.correlate(key, np.random.randint(0,2,800)*2-1))
			if z > (int(keylen*0.8)):
				N_found += 1
		print("For keylen={}".format(keylen))
		print("\tN-3 = {}, ({})%".format(N_found, round(100*N_found/100000,3)))
		ratios.append(N_found/100000)

	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(211)
	ax2 = fig.add_subplot(212)

	ax1.plot(keylens, ratios, marker="x")
	ax1.grid()

	ax2.plot(keylens, ratios, marker="x")
	ax2.grid()
	ax2.semilogy()

	fig.set_layout_engine("tight")
	plt.show()





def synchword_to_bits(synchword, nbits):
	return np.array( [int(x) for x in ("0"*(nbits+1)+bin(synchword)[2:])[-nbits:]], dtype=np.int64)

def bitarr_to_int(bitarr):
	return int("".join( [str(x) for x in bitarr]), 2)


@njit(cache=True)
def get_self_correlation(bitarr):
	assert np.all(np.isclose(bitarr, 0) + np.isclose(bitarr, 1))
	bitarr = bitarr*2 -1
	n = len(bitarr)
	xx = np.arange(n)
	yy = np.zeros(n, dtype=np.float64)
	for i in range(n):
		yy[i] = np.sum( bitarr * np.roll(bitarr, i) )
	return xx, yy


@njit(cache=True)
def measure_bitarr(bitarr):
	selfcorr = get_self_correlation(bitarr)[1]
	max_selfcorrabs = np.max( np.abs(selfcorr[1:]) )
	max_selfcorr = np.max( selfcorr[1:] )
	abssum = np.sum( np.abs(selfcorr[1:]) )
	return max_selfcorrabs, max_selfcorr, abssum


@njit(cache=True)
def draw_synchwords(nbits, Ndraw, do_print):
	best = None
	best_selfcorrmax = nbits +1
	best_selfcorrabsmax = nbits +1000
	best_abssum = 1000000.0
	for ii in range(Ndraw):
		bitarr = np.random.randint(0,2, nbits)
		max_selfcorrabs, max_selfcorr, abssum = measure_bitarr(bitarr)

		A = max_selfcorrabs < best_selfcorrabsmax
		B = (max_selfcorrabs == best_selfcorrabsmax) and (max_selfcorr < best_selfcorrmax)
		C = (max_selfcorr == best_selfcorrmax) and (max_selfcorrabs == best_selfcorrabsmax) and (abssum < best_abssum)
		if A or B or C:

		#A = max_selfcorr < best_selfcorrmax
		#B = (max_selfcorr == best_selfcorrmax) and (abssum < best_abssum)
		#if A or B:
			best = bitarr
			best_selfcorrmax = max_selfcorr
			best_selfcorrabsmax = max_selfcorrabs
			best_abssum = abssum
			if do_print:
				print("\t", ii, "New best: ",max_selfcorrabs, max_selfcorr, abssum)
	return best, best_selfcorrmax


def synchword_draw_loop(nbits, Ndraw, Nloops):
	best, best_selfcorr = draw_synchwords(nbits, min(1000, max(Ndraw, 20)), True)
	for _ in range(Nloops):
		bitarr, selfcorr = draw_synchwords(nbits, Ndraw, True)
		if selfcorr < best_selfcorr:
			best, best_selfcorr = bitarr, selfcorr
	return best, best_selfcorr







def compare_synchwords(synchwords):
	fig = plt.figure(figsize=(14,11))
	ax1 = fig.add_subplot(111)

	for wordint,nbits in synchwords:
		bitarr = synchword_to_bits(wordint, nbits)
		xx, yy = get_self_correlation(bitarr)
		ax1.plot(xx, yy)

	ax1.grid()

	fig.set_layout_engine("tight")
	plt.show()
















def run():
	SYNCHWORD_1 = 0x930B51DE		# 4, 4, 64
	SYNCHWORD_2 = 0x1ACFFC1D		# 8, 8, 60
	sw_32_1 = 0x700e5b35			# 4, 0, 28
	sw_32_2 = 0x0c3655ec			# 4, 0, 28
	sw_32_3 = 0x2f3d9075			# 4, 0, 28
	sw_64_1 = 0x6a244c694617d93c	# 8, 4, 148
	sw_64_2 = 0xd2eff1654e717933	# 8, 4, 132
	sw_64_3 = 0x0512d1fa9cd337e1	# 8, 4, 128



	sw_7_1  = 0x31					# 1, -1, 6		---

	sw_8_1  = 0x79					# 4, 0, 4
	sw_9_1  = 0x43					# 3, 1, 12
	sw_10_1 = 0x1c8					# 2, 2, 18

	sw_11_1 = 0x171					# 1, -1, 10		---

	sw_12_1 = 0x874					# 4, 0, 8
	sw_13_1 = 0xb3f					# 1, 1, 12 		???
	sw_14_1 = 0x363a				# 2, 2, 26

	sw_15_1 = 0x323d				# 1, -1, 14		---

	sw_16_1 = 0xf94c				# 4, 0, 12
	sw_17_1 = 0x189f5				# 3, 1, 28
	sw_18_1 = 0x213af				# 2, 2, 34

	sw_19_1 = 0x1ea19				# 1, -1, 18		---

	sw_20_1 = 0x3bad8				# 4, 0, 16
	sw_21_1 = 0x19d17b				# 3, 1, 28
	sw_22_1 = 0x20829d				# 2, 2, 42

	sw_23_1 = 0x5e0a66				# 1, -1, 22		---

	sw_24_1 = 0xd987b5				# 4, 0, 8
	sw_25_1 = 0x8c0bc9				# 3, 1, 36
	sw_26_1 = 0x1ea392				# 2, 2, 50
	sw_27_1 = 0x1d18165				# 3, 3, 38	+
	sw_28_1 = 0x183457b				# 4, 0, 24
	sw_29_1 = 0x234a7c1				# 3, 1, 44
	sw_30_1 = 0xe35b770				# 2, 2, 58

	sw_31_1 = 0x3a8f6e48			# 1, -1, 30		---				# 0x48751edc
	sw_31_2 = 0x33d2b883			# 1, -1, 30		---				# 0x48751edc
	sw_31_3 = 0x8367a57				# 1, -1, 30		---				# 0x48751edc
	sw_31_4 = 0x48751edc			# 1, -1, 30		---				# 0x48751edc

	sw_32_1 = 0x700e5b35			# 4, 0, 28
	sw_33_1 = 0x1b02928e5			# 3, 1, 52
	sw_34_1 = 0x34ed75f08			# 2, 2, 66
	sw_35_1 = 0x1edf895d6			# 3, 3, 58
	sw_36_1 = 0x4acdf40e3			# 4, 0, 36
	sw_37_1 = 0x4791dbeb9			# 3, 1, 60
	sw_38_1 = 0x72a32487f			# 2, 2, 74

	sw_39_1 = 0x49cd56fd87			# 3, 3, 62  ???

	sw_40_1 = 0x8da9f4c075			# 4, 0, 40
	sw_41_1 = 0x12ab07a4219			# 3, 1, 56

	sw_42_1 = 0x386e939a810			# 6, 2, 86  ???
	sw_43_1 = 0x50dc6049d2b			# 5, 3, 66  ???
	sw_44_1 = 0x892071c8ade 		# 4, 4, 56  ???
	sw_45_1 = 0xfa32b743cf7			# 5, 5, 84  ???

	sw_46_1 = 0x2651fd958285		# 6, 2, 106 ???
	sw_47_1 = 0x709abb3d08b4		# 5, 3, 94  ???
	sw_48_1 = 0x82fcf473a56c		# 4, 4, 64
	sw_49_1 = 0x635cbbfd0d17		# 5, 5, 108

	sw_50_1 = 0x226ebed3c2b70		# 6, 2, 122
	sw_51_1 = 0x6bb00e226a5a3		# 5, 3, 110
	sw_52_1 = 0x6f8dc09584452		# 4, 4, 108
	sw_53_1 = 0x10715acd9d96ff		# 5, 5, 136

	sw_54_1 = 0x31e0d537ac8648		# 6, 2, 130
	sw_55_1 = 0x19f728db4d4071		# 5, 3, 138
	sw_56_1 = 0x3faf65cd76035f		# 4, 4, 136
	sw_57_1 = 0x1518d3db068103b		# 7, 5, 132

	sw_63_1 = 0x6251776c878d7080	# 7, 7, 166

	print("SW_1: ", measure_bitarr(synchword_to_bits(synchword=SYNCHWORD_1, nbits=32)))
	print("SW_2: ", measure_bitarr(synchword_to_bits(synchword=SYNCHWORD_2, nbits=32)))
	print("sw_32_1: ", measure_bitarr(synchword_to_bits(synchword=sw_32_1, nbits=32)))
	print("sw_32_2: ", measure_bitarr(synchword_to_bits(synchword=sw_32_1, nbits=32)))
	print("sw_32_3: ", measure_bitarr(synchword_to_bits(synchword=sw_32_3, nbits=32)))
	print("sw_64_1: ", measure_bitarr(synchword_to_bits(synchword=sw_64_1, nbits=64)))
	print("")
	print("sw_31_1 as 32: ", measure_bitarr(synchword_to_bits(synchword=sw_31_1, nbits=32) ) )
	print("sw_31_2 as 32: ", measure_bitarr(synchword_to_bits(synchword=sw_31_2, nbits=32) ) )
	print("SW_1 as 31: ", measure_bitarr(synchword_to_bits(synchword=SYNCHWORD_1, nbits=31)))
	print("SW_2 as 31: ", measure_bitarr(synchword_to_bits(synchword=SYNCHWORD_2, nbits=31)))
	print("")
	for i_og, og_sw in enumerate((sw_31_1,sw_31_2,sw_31_3,sw_31_4)):
		og_bitarr = synchword_to_bits(synchword=og_sw, nbits=31)
		for i_roll in range(31):
			barr_added = np.concatenate( (np.roll(og_bitarr,i_roll), (1,)) )
			msrmnt = measure_bitarr(barr_added)
			if (msrmnt[0] <= 4) and (msrmnt[1] <= 4):
				print("sw_31_{}, roll({}): ".format(i_og+1, i_roll), msrmnt )
		print("")
	print("")
	print("")


	NBITS = 31
	for Ndraw in (400, 10000000):
		t0 = time.perf_counter()
		best, _ = draw_synchwords(nbits=NBITS, Ndraw=Ndraw, do_print=True)
		dt = time.perf_counter() - t0
		speed = Ndraw / dt
		print("Speed:  {} k/s".format( round( 1e-3 * speed, 2 ) ))

	best = bitarr_to_int(bitarr=best)
	print("best: ", hex(best))

	compare_synchwords(synchwords=[(SYNCHWORD_1,32), (SYNCHWORD_2,32), (best,NBITS)])

	#experiment_key_trigger_levels()




#run()

@njit(cache=True)
def tobits(data):
	bits = np.zeros(len(data)*8, dtype=np.int64)
	for i in range(len(data)):
		for j in range(8):
			bits[i*8+j] = (data[i]>>j) & 1
	return bits


def tst_tobits():
	pl = os.urandom(256)

	tobits(pl)
	tobits(pl)
	Nrep = 1000
	t0 = time.perf_counter()
	for _ in range(Nrep):
		bits = tobits(pl)
	T_call = (time.perf_counter() - t0) / Nrep
	speed_call_per_s = 1 / T_call
	speed_bytes_ps = len(pl) / T_call
	speed_bits_ps = len(pl)*8 / T_call


	print("="*40)
	print("T-call:        {} µs".format( round(1e6*T_call, 2) ))
	print("call speed:    {} kcall/s".format( round(1e-3*speed_call_per_s, 2) ))
	print("speed:         {} Mbytes/s".format( round(1e-6*speed_bytes_ps, 2) ))
	print("speed:         {} Mbits/s".format( round(1e-6*speed_bits_ps, 2) ))
	print("="*40)

print(tobits(b"ABCDE"))
tst_tobits()






























