import time
import numpy as np
from kuokka.dsp_loop import DSPLoop, TXDSPConfig, RXDSPConfig
from kuokka.lib_tools import make_samples2, radionoise, bytes_to_bits, dt_array_report
from queue import Queue
import os
from matplotlib import pyplot as plt








def t1_multimode_reception_rx(rx_baudrates):
    for br in rx_baudrates:
        assert br in (4800, 9600, 9600*2, 9600*4)
    rx_baudrates = sorted(list(set(rx_baudrates))) # remove duplicates and sort.
    assert len(rx_baudrates) >= 1
    assert len(rx_baudrates) <= 4
    print("-------------------------------------------------")
    print("-------------------------------------------------")
    if len(rx_baudrates) == 1:
        print("DSPLoop test 1: Monomode reception at {}.".format(rx_baudrates[0]))
    else:
        print("DSPLoop test 1: Multimode reception.")
    f_center 	= 437.000e6
    base_sr 	= 2e6
    rx_config = RXDSPConfig(rx_samplerate=base_sr, rx_tune_frequency=436.700e6, rx_f_center=f_center, baudrate=rx_baudrates[0], bufferlen=800000, batch_maxlen=16*1024)
    tx_config = TXDSPConfig(tx_samplerate=base_sr, tx_tune_frequency=436.700e6, tx_f_center=f_center, baudrate=rx_baudrates[0])
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

    dsploop1.dbgprint_mask   = DSPLoop.DBGP_INITSTOP | DSPLoop.DBGP_ERRORS
    dsploop_tx.dbgprint_mask = DSPLoop.DBGP_INITSTOP | DSPLoop.DBGP_ERRORS

    P_noise 		= 0.10 / (9600*4)
    tx_baudrates 	= [4800, 9600, 9600*2, 9600*4]
    ideal_snrs 		= [(1/br) / P_noise for br in tx_baudrates]


    if len(rx_baudrates) > 1:
        dsploop1.start_multimode(baudrates=rx_baudrates, mem_index=0)
    else:
        assert len(rx_baudrates) == 1
        dsploop1.start()
    dsploop_tx.start()

    N_TOTAL 		= 300
    batchlen 		= 1024*4
    t_mono0 		= 10.0123
    t_mono 			= t_mono0 * 1.0
    t_unix 			= t_mono0 + 194645646.3452
    pl_rx_list 		= list()
    br_snr_lists 	= dict()
    for br in tx_baudrates:
        br_snr_lists[br] = list()
    i_next_pl 		= int((t_mono0+10.0)*base_sr)
    t_end 			= 1e16
    n_samples_given = 0
    t_slept 		= 0
    samplebuffer 	= np.complex64( radionoise(n=1024*2, sr=base_sr, W_per_Hz=P_noise) )
    N_midsleep 		= N_TOTAL//2
    max_geplen_s	= 64e-3
    midsleep_taken 	= False

    tx_payloads 		= list()
    print("\t")
    print("\tWill generate {} payloads.".format(N_TOTAL))
    print("\tInterpayload gap < {} ms.".format( round(1e3*max_geplen_s, 1) ))
    print("\tStarting reception loop.")
    t00 = time.perf_counter()
    while t_mono < t_end:
        if (not midsleep_taken) and (len(tx_payloads) >= N_midsleep):
            print("\tMIDSLEEP 12s ... ",end="")
            time.sleep(12.0)
            print("done.")
            t_slept += 12
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

        if (len(tx_payloads) < N_TOTAL) and (n_samples_given >= (i_next_pl - (batchlen+10))):
            baudrate = tx_baudrates[np.random.randint(0,len(tx_baudrates))]
            #print("PL #{},  br {}".format(len(payloads), baudrate))
            dsploop_tx.set_tx_baudrate(baudrate=baudrate)
            pl = os.urandom( np.random.randint(4, 220) )
            dsploop_tx.que_tx_payloads_in.put_nowait((pl, t_mono-100e-3))
            samples = dsploop_tx.que_tx_samples_out.get(timeout=1.0)[0,:]
            tx_payloads.append( (len(tx_payloads), pl, baudrate) )
            n_overlap = max(0, n_samples_given+len(samplebuffer) - i_next_pl)
            if n_overlap > 0:
                samplebuffer[-n_overlap:] = samplebuffer[-n_overlap:] + samples[:n_overlap]
                samples = samples[n_overlap:]
            assert len(samples) > 0
            samples = samples + radionoise(n=len(samples), sr=base_sr, W_per_Hz=P_noise)
            samplebuffer = np.concatenate( (samplebuffer, samples) )
            dt_gap = max_geplen_s * (np.random.random()**2) + 0.0e-3
            n_gap = int(dt_gap * base_sr)
            i_next_pl = n_samples_given + len(samplebuffer) + n_gap
            if len(tx_payloads) == N_TOTAL:
                t_end = t_mono + 4.0

        while not dsploop1.que_rx_payloads_out.empty():
            pltup = dsploop1.que_rx_payloads_out.get_nowait()
            if pltup[0] == "pl":
                signaldata = dsploop1.que_signaldata_out.get(timeout=0.010)  # (ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl)
                assert signaldata[-1] == pltup[1]  # signaldata of the same payload...
                power_tuple = signaldata[2]  # P_pl, P_bg, bandlen
                P_pl, P_bg = power_tuple[0:2]
                snr = (P_pl - P_bg) / P_bg
                #print("Snr:", snr)
                pl_rx_list.append(pltup[1])
                br_snr_lists[signaldata[-2]].append(snr)
    comptime = time.perf_counter() - t00
    print("\tloop done.")

    # Performance (computation speed) statistics
    simtime = n_samples_given / base_sr
    hasterate = simtime / (comptime - t_slept)
    print("")
    print("\tPerformance:")
    print("\t\tSimulated time:     {} s".format(round( simtime,3 )))
    print("\t\tComputation time:   {} s".format(round( comptime,3 )))
    print("\t\t(t_slept):          {} s".format(round( t_slept,3 )))
    print("\t\tHaste rate:         {}".format(round( hasterate,2 )))
    print("")

    # Reception rates at different baudrates
    all_pls = [tup[1] for tup in tx_payloads]
    n_success_all = sum([pl in pl_rx_list for pl in all_pls])
    br_pl_lists = dict()
    br_n_success = dict()
    br_ratios = dict()
    for br in tx_baudrates:
        pls_ = [tup[1] for tup in tx_payloads if tup[2] == br]
        n_success_ = sum([pl in pl_rx_list for pl in pls_])
        br_pl_lists[br] = pls_
        br_n_success[br] = n_success_
        br_ratios[br] = n_success_ / len(pls_)
    print("\tReception rates:")
    print("\t\t{}/{} ({}%) of total payloads received.".format( n_success_all, len(all_pls), round(100*n_success_all/len(all_pls),2) ))
    for br in tx_baudrates:
        print("\t\t{}/{} ({}%)  of br={} payloads received.".format( br_n_success[br], len(br_pl_lists[br]), round(100*br_ratios[br] ,2), br ))
    print("")

    # Average SNRs for different baudrates
    print("\tSNRs for baudrates:")
    for br in tx_baudrates:
        if len(br_snr_lists[br]) == 0:
            print("\t\t{}: no receptions.".format(br))
            continue
        avgsnr = round(np.average(br_snr_lists[br]), 3)
        idealsnr = round(ideal_snrs[tx_baudrates.index(br)],3)
        print("\t\t{}: {} vs {} ideal. Ratio: {}".format(br, avgsnr, idealsnr, round(avgsnr/idealsnr, 3)  ))
    print("")

    # Missing payloads that should have been received.
    print("\tMissing payloads:")
    n_missing = 0
    for i,pl,br in tx_payloads:
        missing = not (pl in pl_rx_list)
        should_have_been_received = br in rx_baudrates
        if missing and should_have_been_received:
            print("\t\t{},  br: {}".format(i, br))
            n_missing += 1
    if n_missing == 0:
        print("\t\t(none)")
    print("")

    # Performance statistics of individual processes, or the single Receiver in monomode
    """dsploop1.close()
	if len(rx_baudrates) > 1:
		time.sleep(2.0)
		for idd in sorted(list(dsploop1.dsp_perf_stats_mpr.keys())):
			dt_array, n_processed = dsploop1.dsp_perf_stats_mpr[idd]
			S = dt_array_report(dt_array=dt_array, nsamples=n_processed, sr0=dsploop1.rx_dsp_config.rx_samplerate, dt_array_names=dsploop1.rx.dt_array_names)
			print("\tmpr-process-{}:".format(idd))
			S = "\t" + S
			S = S.replace("\n", "\n\t")
			S = S.replace("=", "-")
			print(S)
			print("")
	else:
		S = dt_array_report(dt_array=dsploop1.rx.dt_array, nsamples=dsploop1.rx.n_processed, sr0=dsploop1.rx_dsp_config.rx_samplerate, dt_array_names=dsploop1.rx.dt_array_names)
		print("\tReceiver:")
		S = "\t" + S
		S = S.replace("\n", "\n\t")
		S = S.replace("=", "-")
		print(S)
		print("")"""

    print("\t{} payloads missing.".format(n_missing))
    assert n_missing == 0, "At this noise level, no payloads (of rx configured baudrates) should be missing."
    print("OK")
    print("--------------------------------------------------")
    print("--------------------------------------------------")
    print("")













def t2_speedbench_multimode_rx():
    print("-"*52)
    print("DSPLoop test 2: Speedbench multimode reception.")
    f_center 		= 437.000e6
    base_sr 		= 3e6
    P_noise 		= 0.020 / 9600
    baudrates 		= [4800, 9600, 9600*2, 9600*4]
    batchlen 		= 1024*4
    t_mono0 		= 10.0123
    t_mono 			= t_mono0 * 1.0
    t_unix 			= t_mono + 194645646.3452
    t_slept 		= 0

    rx_config = RXDSPConfig(rx_samplerate=base_sr, rx_tune_frequency=436.700e6, rx_f_center=f_center, baudrate=9600, bufferlen=800000, batch_maxlen=16*1024)
    tx_config = TXDSPConfig(tx_samplerate=base_sr, tx_tune_frequency=436.700e6, tx_f_center=f_center, baudrate=9600)
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
    dsploop1 = DSPLoop(rx_dsp_config=rx_config, tx_dsp_config=tx_config, que_rx_samples_in=que_rx_samples_in1,
                                         que_rx_payloads_out=que_rx_payloads_out1, que_tx_payloads_in=que_tx_payloads_in1,
                                             que_tx_samples_out=que_tx_samples_out1, que_signaldata_out=que_signaldata_out1) #precompiles in init
    dsploop_tx = DSPLoop(rx_dsp_config=rx_config, tx_dsp_config=tx_config, que_rx_samples_in=que_rx_samples_in2,
                                             que_rx_payloads_out=que_rx_payloads_out2, que_tx_payloads_in=que_tx_payloads_in2,
                                             que_tx_samples_out=que_tx_samples_out2, que_signaldata_out=que_signaldata_out2) #precompiles in init
    dsploop1.dbgprint_mask = DSPLoop.DBGP_ERRORS | DSPLoop.DBGP_INITSTOP
    dsploop_tx.dbgprint_mask = DSPLoop.DBGP_ERRORS | DSPLoop.DBGP_INITSTOP

    print("\tStarting dsploops.")
    dsploop1.start_multimode(baudrates=baudrates, mem_index=0)
    dsploop_tx.start()
    time.sleep(0.5)

    print("\tComposing samplebuffer.")
    n_total 			= int(base_sr * 26.0)
    simtime 			= n_total / base_sr
    assert (n_total*2*8) < 3.0e9
    samplebuffer 		= np.complex64( radionoise(n=n_total, sr=base_sr, W_per_Hz=P_noise) )
    head_pl_addition 	= int(4.0*base_sr)
    pl_tx_list 			= list()
    pl_tx_dd 			= dict()
    print("\tComposing payloads.")
    while True:
        baudrate = baudrates[np.random.randint(0,len(baudrates))]
        dsploop_tx.set_tx_baudrate(baudrate=baudrate)
        pl = os.urandom( np.random.randint(4, 220) )
        dsploop_tx.que_tx_payloads_in.put_nowait((pl, t_mono-100e-3))
        pl_samples = dsploop_tx.que_tx_samples_out.get(timeout=1.0)[0,:]
        t_mono_end_ = t_mono0 + (head_pl_addition+len(pl_samples)) / base_sr
        pl_tx_list.append(pl)
        assert not (pl in pl_tx_dd)
        pl_tx_dd[pl] =  (len(pl_tx_list), baudrate, head_pl_addition, t_mono_end_)
        samplebuffer[head_pl_addition:head_pl_addition+len(pl_samples)] += pl_samples
        head_pl_addition += len(pl_samples) + int(250e-3 * np.random.random()**2 * base_sr)
        if ((n_total - head_pl_addition) / base_sr) < 2.0:
            break
        if (len(pl_tx_list) % 10) == 0:
            time.sleep(0.02)
    print("\tSamplebuffer contains {} payloads.".format(len(pl_tx_list)))


    pl_rx_list 			= list()
    pl_rx_dd 			= dict()
    br_snr_lists 		= dict()
    for br in baudrates:
        br_snr_lists[br] = list()
    feed_head = 0
    t00 = time.perf_counter()
    while feed_head < len(samplebuffer):
        batch = samplebuffer[feed_head:feed_head+batchlen]
        feed_head = feed_head + len(batch)
        dsploop1.que_rx_samples_in.put( (batch, t_mono, t_unix), timeout=5.50)
        t_mono = t_mono + len(batch) / base_sr
        t_unix = t_unix + len(batch) / base_sr
        if (n_total - feed_head) < (20e-3 * base_sr):
            while not dsploop1.que_rx_samples_in.empty():
                time.sleep(0.001)
                t_slept += 0.001

        while not dsploop1.que_rx_payloads_out.empty():
            pltup = dsploop1.que_rx_payloads_out.get_nowait() # ("pl", rx_pl, ts_mono)  /  ("cs", None, ts_mono)
            if pltup[0] == "pl":
                _, pl_rx_, ts_mono_rx_ = pltup
                signaldata = dsploop1.que_signaldata_out.get(timeout=0.010)  # (ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl)
                assert signaldata[-1] == pl_rx_  # signaldata of the same payload...
                power_tuple = signaldata[2]  # (P_pl, P_bg, bandlen)
                P_pl, P_bg = power_tuple[0:2]
                snr = (P_pl - P_bg) / P_bg
                pl_rx_list.append(pl_rx_)
                pl_rx_dd[pl_rx_] = (ts_mono_rx_, snr)
                br_snr_lists[signaldata[-2]].append(snr)
    comptime = time.perf_counter() - t00

    hasterate = simtime / (comptime - t_slept)
    print("")
    print("\tPerformance:")
    print("\t\tSimulated time:    {} s".format(round( simtime,3 )))
    print("\t\tComputation time:  {} s".format(round( comptime,3 )))
    print("\t\t(t_slept):         {} s".format(round( t_slept,3 )))
    print("\t\tHaste rate:        {}".format(round( hasterate,2 )))
    print("")

    estimated_delays = dict()
    for br in baudrates:
        rx_config.baudrate = br
        estimated_delays[br] = rx_config.estimate_reception_delay()

    nr_delay_lists = dict()
    for br in baudrates:
        nr_delay_lists[br] = list()
    print("\tPayloads:")
    n_received = 0
    for i,pl in enumerate(pl_tx_list):
        _, br_, head_add_, t_mono_tx_end_ = pl_tx_dd[pl]  #(len(pl_tx_list), baudrate, head_pl_addition, t_mono_end_)
        S1 = "  pl-{}:".format(i)
        S1 += " "*(10-len(S1))
        S2 = "br: {}".format(br_)
        S2 += " "*(10-len(S2))
        if not pl in pl_rx_dd:
            S3 = "NOT RECEIVED."
            print(S1+S2+S3)
            continue
        t_mono_rx_, snr = pl_rx_dd[pl] # pl_rx_dd[pl_rx_] = (ts_mono_rx_, snr)
        delay = t_mono_rx_ - t_mono_tx_end_
        n_received += 1
        S3 = "[ok] delay {} ms".format( round(1e3*delay, 1) )
        #print(S1+S2+S3)
        nr_delay_lists[br_].append( (i, delay) )
    print("\t\tReceived: {}/{}   ({} %)".format( n_received, len(pl_tx_list),  round(100*n_received/len(pl_tx_list), 1)  ))
    print("OK")
    print("-"*52)
    print("")

    colors = ["blue", "orange", "green", "red", "pink", "grey"]
    fig = plt.figure(figsize=(21,12))
    ax1 = fig.add_subplot(111)

    for ii,br in enumerate( sorted(list(nr_delay_lists.keys())) ):
        nr_arr = [t[0] for t in nr_delay_lists[br]]
        delay_arr = [t[1] for t in nr_delay_lists[br]]
        ax1.plot(nr_arr, delay_arr, marker="x", label="{}".format(br), color=colors[ii])
        ax1.plot([0, len(pl_tx_list)], np.ones(2)*estimated_delays[br], linestyle="--", label="{}".format(br), color=colors[ii])
    ax1.grid()
    ax1.legend()

    fig.set_layout_engine("tight")
    plt.show()









t1_multimode_reception_rx([9600,])
t1_multimode_reception_rx([4800,9600,9600*2,9600*4])
t2_speedbench_multimode_rx()
