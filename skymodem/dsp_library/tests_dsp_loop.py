import time
import numpy as np
from kuokka.dsp_loop import DSPLoop, TXDSPConfig, RXDSPConfig
from kuokka.lib_tools import make_samples2, radionoise, bytes_to_bits, dt_array_report
from queue import Queue
import os







def t1_single_rx():
	f_center = 437.000e6
	base_sr = 2e6
	rx_config = RXDSPConfig(rx_sr0=base_sr, rx_f_tune=436.700e6, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=16*1024)
	tx_config = TXDSPConfig(tx_sr0=base_sr, tx_f_tune=436.700e6, tx_f_center=f_center, baudrate=9600)
	que_rx_samples_in1 		= Queue(100)
	que_rx_payloads_out1 	= Queue(256)
	que_tx_payloads_in1 	= Queue(100)
	que_tx_samples_out1 	= Queue(100)
	que_signaldata_out1 	= Queue(100)

	que_rx_samples_in2 		= Queue(100)
	que_rx_payloads_out2 	= Queue(100)
	que_tx_payloads_in2 	= Queue(100)
	que_tx_samples_out2 	= Queue(100)
	que_signaldata_out2 	= Queue(100)

	dsploop1 = DSPLoop(rx_dsp_config=rx_config, tx_dsp_config=tx_config, que_rx_samples_in=que_rx_samples_in1, que_rx_payloads_out=que_rx_payloads_out1, que_tx_payloads_in=que_tx_payloads_in1,
			que_tx_samples_out=que_tx_samples_out1, que_signaldata_out=que_signaldata_out1) #precompiles in init
	dsploop_tx = DSPLoop(rx_dsp_config=rx_config, tx_dsp_config=tx_config, que_rx_samples_in=que_rx_samples_in2, que_rx_payloads_out=que_rx_payloads_out2, que_tx_payloads_in=que_tx_payloads_in2,
			que_tx_samples_out=que_tx_samples_out2, que_signaldata_out=que_signaldata_out2) #precompiles in init

	P_noise = 0.040 / 9600
	baudrates = [4800, 9600, 9600*2, 9600*4]
	ideal_snrs = [(1/br) / P_noise for br in baudrates]
	payloads = list()

	#dsploop1.start()
	dsploop1.start_multimode(baudrates=baudrates, mem_index=0)
	dsploop_tx.start()

	N_TOTAL = 300
	batchlen = 1024*4
	t_mono0 = 10.0123
	t_mono = t_mono0 * 1.0
	t_unix = t_mono + 194645646.3452
	pl_rx_list = list()
	br_snr_lists = dict()
	for br in baudrates:
		br_snr_lists[br] = list()
	i_next_pl = int((t_mono0+10.0)*base_sr)
	t_end = 1e16
	n_samples_given = 0
	t_slept = 0
	samplebuffer = np.complex64( radionoise(n=1024*2, sr=base_sr, W_per_Hz=P_noise) )
	N_midsleep = N_TOTAL//2
	midsleep_taken = False

	t00 = time.perf_counter()
	while t_mono < t_end:
		if (not midsleep_taken) and (len(payloads) >= N_midsleep):
			print("MIDSLEEP")
			time.sleep(9.0)
			t_slept += 5
			midsleep_taken = True

		if len(samplebuffer) < batchlen:
			samplebuffer = np.concatenate( (samplebuffer, radionoise(n=batchlen*6+10, sr=base_sr, W_per_Hz=P_noise)) )

		batch = samplebuffer[0:batchlen]
		samplebuffer = samplebuffer[batchlen:]
		dsploop1.que_rx_samples_in.put( (batch, t_mono, t_unix) )
		if (t_end - t_mono) < 0.020:
			while not dsploop1.que_rx_samples_in.empty():
				time.sleep(0.001)
				t_slept += 0.001
		t_mono = t_mono + len(batch) / base_sr
		t_unix = t_unix + len(batch) / base_sr
		n_samples_given += len(batch)

		if (len(payloads) < N_TOTAL) and (n_samples_given >= (i_next_pl - (batchlen+10))):
			baudrate = baudrates[np.random.randint(0,len(baudrates))]
			print("PL #{},  br {}".format(len(payloads), baudrate))
			dsploop_tx.set_tx_baudrate(baudrate=baudrate)
			pl = os.urandom( np.random.randint(4, 220) )
			dsploop_tx.que_tx_payloads_in.put_nowait((pl, t_mono-100e-3))
			samples = dsploop_tx.que_tx_samples_out.get(timeout=1.0)[0,:]
			payloads.append( (len(payloads), pl, baudrate) )
			n_overlap = max(0, n_samples_given+len(samplebuffer) - i_next_pl)
			if n_overlap > 0:
				samplebuffer[-n_overlap:] = samplebuffer[-n_overlap:] + samples[:n_overlap]
				samples = samples[n_overlap:]
			assert len(samples) > 0
			samples = samples + radionoise(n=len(samples), sr=base_sr, W_per_Hz=P_noise)
			samplebuffer = np.concatenate( (samplebuffer, samples) )
			dt_gap = 64e-3 * (np.random.random()**2) + 0.0e-3
			n_gap = int(dt_gap * base_sr)
			i_next_pl = n_samples_given + len(samplebuffer) + n_gap
			if len(payloads) == N_TOTAL:
				t_end = t_mono + 4.0

		while not dsploop1.que_rx_payloads_out.empty():
			pltup = dsploop1.que_rx_payloads_out.get_nowait()
			if pltup[0] == "pl":
				signaldata = dsploop1.que_signaldata_out.get(timeout=0.010)  #(ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl)
				assert signaldata[-1] == pltup[1]  # signaldata of the same payload...
				power_tuple = signaldata[2]  #P_pl, P_bg, bandlen
				P_pl, P_bg = power_tuple[0:2]
				snr = (P_pl - P_bg) / P_bg
				print("Snr:", snr)
				pl_rx_list.append(pltup[1])
				br_snr_lists[signaldata[-2]].append(snr)

	comptime = time.perf_counter() - t00

	simtime = n_samples_given / base_sr
	hasterate = simtime / (comptime - t_slept)
	print("="*42)
	print("Simulated time:    {} s".format(round( simtime,3 )))
	print("Computation time:  {} s".format(round( comptime,3 )))
	print("(t_slept):         {} s".format(round( t_slept,3 )))
	print("Haste rate:        {}".format(round( hasterate,2 )))
	print("="*42)


	all_pls = [tup[1] for tup in payloads]
	n_success_all = sum([pl in pl_rx_list for pl in all_pls])
	br_pl_lists = dict()
	br_n_success = dict()
	br_ratios = dict()
	for br in baudrates:
		pls_ = [tup[1] for tup in payloads if tup[2] == br]
		n_success_ = sum([pl in pl_rx_list for pl in pls_])
		br_pl_lists[br] = pls_
		br_n_success[br] = n_success_
		br_ratios[br] = n_success_ / len(pls_)

	print("{}/{} ({}%) of total payloads received.".format( n_success_all, len(all_pls), round(100*n_success_all/len(all_pls),2) ))
	for br in baudrates:
		print("{}/{} ({}%)  of br={} payloads received.".format( br_n_success[br], len(br_pl_lists[br]), round(100*br_ratios[br] ,2), br ))
	print("")

	print("SNRs for baudrates:")
	for br in baudrates:
		avgsnr = round(np.average(br_snr_lists[br]), 3)
		idealsnr = round(ideal_snrs[baudrates.index(br)],3)
		print("Avg: {} vs {} ideal. Ratio: {}".format(avgsnr, idealsnr, round(avgsnr/idealsnr, 3)  ))
	print("")

	for i,pl,br in payloads:
		if not pl in pl_rx_list:
			print("    #{},  br: {}".format(i, br))

	dsploop1.close()
	time.sleep(2.0)
	for idd in sorted(list(dsploop1.dsp_perf_stats_mpr.keys())):
		dt_array, n_processed = dsploop1.dsp_perf_stats_mpr[idd]
		S = dt_array_report(dt_array=dt_array, nsamples=n_processed, sr0=dsploop1.rx_dsp_config.rx_sr0, dt_array_names=dsploop1.rx.dt_array_names)
		print("mpr-",idd)
		print(S)
		print("")
	time.sleep(10)









t1_single_rx()







