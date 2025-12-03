import time
import numpy as np
from .lib_demodulation import create_demod_statemx, create_DD_statemx, demodulate, decide_decode
from .lib_symsynching import create_classic_JPL_statemx
from .lib_fft_finder import create_fft_f_centerer_csense_statemx, fft_f_centerer_csense
from .lib_framing import create_deframer, RS_MAX_ENCODED_LEN, frame_packet, RS_MAX_PL_LEN
from .lib_tools import FS1P_SYNCHWORD, FS1P_SYNCHWORD_LEN, radionoise, make_samples2, ints_to_bits, choose_fftlen, freq_shift_phased_precomp, create_freq_shifter_precomp
from .lib_reedsolomon import get_default_rs
from .lib_resampler import staged_resampler_execute_stream, create_staged_resampler, minimal_disc_halflen_for_staged_resampler, minimal_frac_halflen_for_staged_resampler
from .lib_tools import get_frequency_search_map
from .lib_symsynching import classic_JPL_synch_strm



class RXDSPConfig:
    def __init__(self, rx_sr0, rx_f_tune, rx_f_center, baudrate, bufferlen, batch_maxlen):
        self.bufferlen			= bufferlen
        self.batch_maxlen		= batch_maxlen
        # radio device -------------------------------------
        self.rx_sr0 			= rx_sr0		# Raw samplerate of the radio. Will be downsampled with a rate of baudrate*sps/sr0
        self.rx_f_tune 			= rx_f_tune		# Tuned frequency of the radio in absolute Hz (for example 350.0e6)
        # --------------------------------------------------
        # signal properties --------------------------------
        self.rx_f_center 		= rx_f_center	# The (absolute) frequency of the transmissions in absolute Hz (for example 350.12e6)
        self.baudrate			= baudrate		# Baudrate of the transmission. Has a definite effect on performance. More so if resampling rate is not adjusted.
        # --------------------------------------------------
        # resampling ---------------------------------------
        self.sps 				= 12 			# ! sps (samples-per-symbol) for the signal processing pipeline. Determines resampling rate. Has a _minor_ effect on performance. (See tests_resamples.py)
        self.n_banks 			= 64			# - Number of resampling banks. Almost no effect on performance, and 64 seems good for all purposes.
        self.rs_f_cutoff_coeff 	= 0.499			# - Lowpass associated with the resampling. In interval (0:0.5). 0.499 still enables some aliasing at edges.
        # --------------------------------------------------
        # fft detection ------------------------------------
        self.fftlen_mpr 		= 52			# ! Length of the fft window in multiples of sps in center frequency detector. Larger number increases frequency resolution, but also induces decoding delay.
        self.mod_index 			= 0.5			# S Modulation index. A core FM-modulation parameter. Determines the frequency deviation from center.
        self.BT_rx_match 		= 0.425			# S Bandwidth-Time product of an optional gaussian filter on modulating squarewave. set to -1 for no gaussian filtering. TODO: best match for 0.5 at UHF-firmware is 0.425 here
        self.centering_delay_mpr= 2.0 			# ! Center frequency estimate is collected for (centering_delay_mpr*fftlen) samples ahead of demodulation. TODO should be in symbols?
        self.centerf_halflife	= 12.0 			# ! Exponential decay factor of the center frequency correlation sum. c_centerf = 0.5**(1/centerf_halflife)
        self.search_halfband	= 20.0e3 		# - Determines the frequency band above and below the center frequency where the demodulator looks for signals. (For 437MHz at orbital speeds, maximum doppler ~11kHz)
        # --------------------------------------------------
        # JPL synchronizer ---------------------------------
        self.JPL_halflife 		= 38 			# ! How quickly JPL-synchronizer's accumulator exponentially decays. The values are updated as: acc = (acc + measurement) * (0.5**(1/JPL_n_halflife))
        # --------------------------------------------------
        # demodulation -------------------------------------
        self.lp_ntaps			= 161			# ! number of taps in the low-pass filter in demodulation
        self.lp_cutoff_coeff	= 0.570			# ! cutoff frequency of the low-pass filter, as multiples of baudrate (0.570 seems best both for mod_idx=0.5 and mod_idx=0.75)
        self.synch_delay_mpr	= 30 			# ! demodulator decides symbols synch_delay_mpr symboltimes behind the synchronizer. This allows a synch to be found before symbols are decoded.
        # --------------------------------------------------
        # carrier sense ------------------------------------
        self.carrier_sense		= True
        # --------------------------------------------------
        # framing ------------------------------------------
        self.synchword			= FS1P_SYNCHWORD
        self.synchword_len		= FS1P_SYNCHWORD_LEN
        self.use_scrambler 		= True
        self.use_rs 			= True
        self.synch_threshold 	= 3
        self.data_maxlen 		= RS_MAX_ENCODED_LEN
        # --------------------------------------------------

    def check_validity(self):
        assert 50e3 <= self.bufferlen < 100e6
        assert type(self.bufferlen) == int
        assert 100 < self.batch_maxlen < (0.05*self.bufferlen)
        assert type(self.batch_maxlen) == int
        assert 1e3 < self.rx_sr0 < 32e6
        assert self.rx_f_tune > 1.0
        assert self.rx_f_center > 1.0
        assert 0 < self.baudrate < (self.rx_sr0/2)
        assert (abs(self.rx_f_tune - self.rx_f_center) + self.search_halfband + self.baudrate * 0.6) < (0.5 * self.rx_sr0), "Radio tuned to this frequency with this samplerate cannot see the entire band."
        assert ((self.search_halfband + self.baudrate*0.6) / (self.baudrate * self.sps)) < 0.5, "Resampling down to this sps at this baudrate cannot see the entire search band."
        sign1 = np.sign(self.rx_f_center+(self.search_halfband+self.baudrate*0.6) - self.rx_f_tune )
        sign2 = np.sign(self.rx_f_center-(self.search_halfband+self.baudrate*0.6) - self.rx_f_tune )
        assert sign1 == sign2, "The search band stretches across tuning frequency. DC-spike will potentially interfere with reception. With very high bandwidths this in inevitable, and this assertion should be commented out."
        assert 2 < self.sps <= 100
        assert type(self.sps) == int
        #assert 4 < self.d_halflen < 42
        #assert (self.d_halflen*2) > int(self.rx_sr0 / (self.baudrate * self.sps)),  (self.d_halflen*2, int(self.rx_sr0 / (self.baudrate * self.sps)))
        #assert type(self.d_halflen) == int
        assert 24 < self.n_banks < 240
        assert type(self.n_banks) == int
        assert 0 < self.rs_f_cutoff_coeff < 0.5
        assert (self.rs_f_cutoff_coeff * (self.sps * self.baudrate / self.rx_sr0)) > ((self.search_halfband + self.baudrate*0.6) / self.rx_sr0)
        assert type(self.fftlen_mpr) == int
        assert 3 < self.fftlen_mpr < 150
        assert 0.5 <= self.mod_index < 10.0
        assert (self.BT_rx_match >= 0.4) or (self.BT_rx_match == -1)  # Canonically BT should never be under 0.5. However, using 0.425 when generating samples produces best match to recordings from UHF with CC1125 chip.
        assert 1 <= self.centering_delay_mpr < 20
        assert 1.0 <= self.centerf_halflife < 50.0
        assert 1.0 <= self.JPL_halflife < 100.0
        assert 30.0 <= self.lp_ntaps < 300.0
        assert type(self.lp_ntaps) == int
        assert (self.lp_ntaps % 2) == 1
        assert 0 < self.lp_cutoff_coeff < (0.5*self.sps)
        assert 0 <= self.synch_delay_mpr <= 64.0
        assert type(self.synchword) == int
        assert self.synchword > 0
        assert type(self.synchword_len) == int
        assert self.synchword_len >= 16
        assert self.synchword_len <= 64
        assert self.synchword < (2**self.synchword_len)
        assert type(self.use_scrambler) == bool
        assert type(self.use_rs) == bool
        assert type(self.synch_threshold) == int
        assert 0 <= self.synch_threshold <= 4
        assert type(self.data_maxlen) == int
        assert self.data_maxlen > 10
        if self.use_rs:
            assert self.data_maxlen == RS_MAX_ENCODED_LEN

    def get_r_rate(self):
        r_rate = self.sps * self.baudrate / self.rx_sr0
        return r_rate

    def get_f_center_search_map(self, fftlen, is_precentered:bool):
        if is_precentered:
            f_center_min_nrm = -self.search_halfband / (self.baudrate * self.sps)
            f_center_max_nrm = self.search_halfband / (self.baudrate * self.sps)
        else:
            f_center_min_nrm = (self.rx_f_center - self.search_halfband - self.rx_f_tune) / (self.baudrate * self.sps)
            f_center_max_nrm = (self.rx_f_center + self.search_halfband - self.rx_f_tune) / (self.baudrate * self.sps)
        f_center_search_map = get_frequency_search_map(fftlen=fftlen, f_min_nrm=f_center_min_nrm, f_max_nrm=f_center_max_nrm, assert_in_window=True)
        return f_center_search_map

    def estimate_reception_delay(self):
        #xarr41_A = (parameter_arrays["fftlen_mpr"]*parameter_arrays["centering_delay_mpr"]) / 9600		# fftlen * centering_delay
        #xarr41_B = (parameter_arrays["synch_delay_mpr"]) / 9600
        #xarr41_C = (parameter_arrays["lp_ntaps"]*0.5) / (parameter_arrays["sps"]*9600)
        A = self.fftlen_mpr * self.centering_delay_mpr / self.baudrate
        B = self.synch_delay_mpr / self.baudrate
        C = self.lp_ntaps*0.5 / (self.sps * self.baudrate)
        return A + B + C



SAVE_DPATH = ""
#SAVE_DPATH = "/home/elmore/datasetit/"
#SAVE_DPATH = "/home/aalto/samplesets/"


class Receiver:
    def __init__(self, config:RXDSPConfig):
        config.check_validity()
        self.config 			= config
        self.bufferlen 			= int(config.bufferlen)
        self.rs_array 			= np.zeros(self.bufferlen, dtype=np.complex128)
        self.center_f_array 	= np.zeros(self.bufferlen, dtype=np.float64)
        self.dmd_array 			= np.zeros(self.bufferlen, dtype=np.float64)
        self.synch_array 		= np.zeros((self.bufferlen, 3), dtype=np.int64)
        self.power_array 		= np.zeros((self.bufferlen, 3), dtype=np.float64)    # for energy sense
        self.fft_instr_array 	= np.zeros((self.bufferlen, 5), dtype=np.float64)
        self.bufferhalf			= int(self.bufferlen / 2)
        self.buffer_roll_limit 	= int(self.bufferlen*3/4)
        self.rs_head 			= 0
        self.center_f_head 		= 0
        self.dmd_head 			= 0
        self.synch_head 		= 0
        self.opt_tap_idx_f 		= 0.0
        self.freq_shifter_arr 	= np.zeros(2, dtype=np.complex128) #TODO dtype?
        self.rsmpl_mx1 			= np.zeros((2,2), dtype=np.float64)
        self.rsmpl_mx2 			= np.zeros((2,2), dtype=np.float64)
        self.FFTstatemx 		= np.zeros((2,2), dtype=np.float64)
        self.JPLstatemx 		= np.zeros((2,2), dtype=np.float64)
        self.demodmx 			= np.zeros((2,2), dtype=np.float64)
        self.sddmx 				= np.zeros((2,2), dtype=np.float64)
        self.deframermx 		= np.zeros((2,2), dtype=np.float64)
        self.white_noise		= np.zeros(config.batch_maxlen*3+1024, dtype=np.complex64)
        self.add_white_noise	= False
        rs_mx, rs_cfg 			= get_default_rs()
        self.rs_mx 				= rs_mx
        self.rs_cfg 			= rs_cfg
        self.fftlen 			= 0
        self.centering_fdelta_nrm = 0.0
        self.centering_phase 	= 0.0
        self.centering_phase_idx= 0
        self.freq_shifter_mod   = 1
        self.n_processed		= 0
        self.dt_array			= np.zeros(7, dtype=np.float64)
        self.dt_array_names		= ("f-shift", "resample", "fft-center", "demodulate", "synch", "decide-decode", "buffer-roll")
        self.save_fp 			= None
        self.save_len 			= 0
        self.save_print_ts 		= 0
        self.save_fname 		= ""
        if SAVE_DPATH:
            import os
            assert os.path.isdir(SAVE_DPATH)
            self.save_fname = "receiver_samplerec_{}.bytes".format( str(int(time.time())) )
            self.save_fp = open(SAVE_DPATH + "/" + self.save_fname, "wb" )
            print("saving to ", SAVE_DPATH + "/" + self.save_fname)
        self._setup()
        # This series of baudrate switches pre-generates correlation masks to memory.
        #_br = self.config.baudrate
        #_sps = self.config.sps
        #self.switch_baudrate(baudrate=9600, sps=_sps)
        #self.switch_baudrate(baudrate=9600*2, sps=_sps)
        #self.switch_baudrate(baudrate=9600*2*2, sps=_sps)
        #self.switch_baudrate(baudrate=_br, sps=_sps)


    def _setup(self):
        c_stat_update = 1/200
        carrier_sense_threshold = 5.0
        self.config.check_validity()
        config = self.config
        self.fftlen, _ = choose_fftlen(config.fftlen_mpr*config.sps, window_halfwid=int(0.06*config.fftlen_mpr*config.sps))
        f_cutoff = config.get_r_rate() * config.rs_f_cutoff_coeff
        f_center_search_map = config.get_f_center_search_map(fftlen=self.fftlen, is_precentered=True)
        min_f_undisturbed = (config.search_halfband + config.baudrate*0.6) / config.rx_sr0
        min_f_undisturbed = min_f_undisturbed + 0.2*(f_cutoff - min_f_undisturbed)
        #print("f_cutoff: ",f_cutoff)
        #print("min_f_undisturbed: ",min_f_undisturbed)
        halflen_disc = minimal_disc_halflen_for_staged_resampler(r_rate=config.get_r_rate(), f_cutoff=f_cutoff, min_f_undisturbed=min_f_undisturbed, minimum_value=16, require_total_sampling=True)
        halflen_frac = minimal_frac_halflen_for_staged_resampler(halflen_div=halflen_disc, r_rate=config.get_r_rate(), f_cutoff=f_cutoff, minimum_value=8)
        #print("[derived discrete halflen of   {}]".format(halflen_disc))
        #print("[derived fractional halflen of {}]".format(halflen_frac))
        self.centering_fdelta_nrm = -(config.rx_f_center - config.rx_f_tune) / config.rx_sr0
        shifter, m, fdelta_actual = create_freq_shifter_precomp(sr=config.rx_sr0, fdelta=-(config.rx_f_center - config.rx_f_tune), max_batchlen=config.batch_maxlen, fdelta_threshold=config.rx_sr0*1e-6)
        #print("fdelta - fdelta_actual: {} Hz (m:{}, fd_a:{})".format( abs(fdelta_actual - -(config.rx_f_center - config.rx_f_tune)), m, fdelta_actual ))
        self.freq_shifter_arr = shifter
        self.freq_shifter_mod = m
        rsmpl_mx1, rsmpl_mx2 = create_staged_resampler(halflen_div=halflen_disc, halflen_f=halflen_frac, r_rate=config.get_r_rate(), n_banks=config.n_banks, f_cutoff=f_cutoff, allow_aliasing=False)
        self.rsmpl_mx1 = rsmpl_mx1
        self.rsmpl_mx2 = rsmpl_mx2
        self.FFTstatemx	= create_fft_f_centerer_csense_statemx(fftlen=self.fftlen, sps=config.sps, baudrate=config.baudrate, f_center_search_map=f_center_search_map, mod_index=config.mod_index, BT_rx_match=config.BT_rx_match, centering_delay_mpr=config.centering_delay_mpr, centerf_halflife=config.centerf_halflife, c_stat_update=c_stat_update, carrier_sense_threshold=carrier_sense_threshold)
        self.JPLstatemx = create_classic_JPL_statemx(N_eps=config.sps, n_halflife=config.JPL_halflife)
        self.demodmx 	= create_demod_statemx(lp_ntaps=config.lp_ntaps, lp_cutoff_coeff=config.lp_cutoff_coeff, sps_f=config.sps)
        self.sddmx 		= create_DD_statemx(synch_delay_mpr_f=config.synch_delay_mpr, sps_f=config.sps, Neps=int(config.sps))
        self.deframermx = create_deframer(use_scrambler=config.use_scrambler, use_rs=config.use_rs, data_maxlen=config.data_maxlen, synchword=config.synchword, synchword_len=config.synchword_len, synch_threshold=config.synch_threshold)
        a = int(config.batch_maxlen * config.get_r_rate() * 2)
        b = self.fftlen * 3
        self.buffer_roll_limit 	= self.bufferlen - (a + b)
        assert self.buffer_roll_limit > (self.bufferlen * 0.9), self.buffer_roll_limit/self.bufferlen
        self.rs_head 			= 0
        self.center_f_head 		= 0
        self.dmd_head 			= 0
        self.synch_head 		= 0
        self.opt_tap_idx_f 		= 0.0
        self.centering_phase 	= 0.0
        self.centering_phase_idx = 0
        self.n_processed		= 0
        self.dt_array *= 0.0


    def get_fftlen(self):
        fftlen1  = int(self.FFTstatemx[0,0])
        fftlen2  = int(self.FFTstatemx.shape[1])
        assert fftlen1 == fftlen2
        assert fftlen1 >= 20
        return fftlen1


    def switch_baudrate(self, baudrate, sps):
        assert baudrate > 0
        assert type(sps) == int
        assert sps > 1
        self.config.baudrate = baudrate
        self.config.sps = sps
        self.config.check_validity()
        self._setup()


    def set_additive_noise_amplitude(self, W_per_Hz):
        if W_per_Hz <= 0:
            self.add_white_noise = False
        else:
            self.add_white_noise = True
            self.white_noise = radionoise(n=self.config.batch_maxlen*3+1024, sr=self.config.baudrate*self.config.sps, W_per_Hz=W_per_Hz)


    def process_samples(self, samples, give_bits=False):
        if len(samples) <= self.config.batch_maxlen:
            return self.process_batch(batch=samples, give_bits=give_bits)
        elif not give_bits:
            c = 0
            pl_list = []
            carrier_sensed = False
            while c < len(samples):
                pl_list_, carrier_sensed_ = self.process_batch(batch=samples[c:c+self.config.batch_maxlen], give_bits=False)
                pl_list += pl_list_
                c += self.config.batch_maxlen
                carrier_sensed = carrier_sensed or carrier_sensed_
            return pl_list, carrier_sensed
        else:
            c = 0
            bits = np.zeros(0, dtype=np.int64)
            carrier_sensed = False
            while c < len(samples):
                bits_, carrier_sensed_ = self.process_batch(batch=samples[c:c+self.config.batch_maxlen], give_bits=False)
                bits = np.concatenate( (bits, bits_) )
                carrier_sensed = carrier_sensed or carrier_sensed_
            return bits, carrier_sensed


    def process_batch(self, batch, give_bits):
        assert len(batch) <= self.config.batch_maxlen
        self.n_processed += len(batch)

        ## Coarse frequency shift: Expected center frequency shifted to 0
        t0 = time.perf_counter()
        #batch2, self.centering_phase = freq_shift_phased(batch, sr=1.0, fdelta=self.centering_fdelta_nrm, phase0=self.centering_phase)
        batch2, self.centering_phase_idx = freq_shift_phased_precomp(batch=batch, shifter_arr=self.freq_shifter_arr, phase_idx0=self.centering_phase_idx, phase_mod=self.freq_shifter_mod)
        self.dt_array[0] += (time.perf_counter() - t0)

        ## Resample down to sr = sps*baudrate
        t0 = time.perf_counter()
        rs_head_new = staged_resampler_execute_stream(in_arr=batch2, ii0=0, nsamples=len(batch2), out_arr=self.rs_array, io0=self.rs_head, mx1=self.rsmpl_mx1, mx2=self.rsmpl_mx2)
        #if self.add_white_noise:
        #	i = np.random.randint(0, 1024)
        #	self.rs_array[self.rs_head:rs_head_new] += self.white_noise[i:i+(rs_head_new - self.rs_head)]
        self.dt_array[1] += (time.perf_counter() - t0)

        ## Write samples to a file if one is open
        if self.save_fp:
            k = np.complex64(self.rs_array[self.rs_head:rs_head_new]).tobytes()
            self.save_len += len(k)
            self.save_fp.write(k)
            self.save_fp.flush()
            if (time.perf_counter() - self.save_print_ts) > 3.0:
                print("Saved: {} Mb of samples to {}".format( round(self.save_len/1e6, 1) , self.save_fname))
                self.save_print_ts = time.perf_counter()

        ## Center frequency determination, carrier sense, and snr measurement
        t0 = time.perf_counter()
        center_f_head_new, dmd_rs_up_to, carrier_sensed = fft_f_centerer_csense(sample_arr=self.rs_array, isample0=self.rs_head, nsamples=rs_head_new - self.rs_head, center_f_arr=self.center_f_array, power_arr=self.power_array, instr_arr=self.fft_instr_array, center_f_head0=self.center_f_head, statemx=self.FFTstatemx)
        assert center_f_head_new == rs_head_new
        #self.center_f_array[self.center_f_head:center_f_head_new] = -0.003
        self.dt_array[2] += (time.perf_counter() - t0)

        ## FM Demodulation
        t0 = time.perf_counter()
        dmd_head_new = demodulate(sample_arr=self.rs_array, center_f_arr=self.center_f_array, sample_i0=self.dmd_head, demod_n_samples=dmd_rs_up_to-self.dmd_head, dmd_arr=self.dmd_array, demodmx=self.demodmx)
        assert dmd_head_new == dmd_rs_up_to
        self.dt_array[3] += (time.perf_counter() - t0)

        ## Symbol synch
        t0 = time.perf_counter()
        synch_head_new = classic_JPL_synch_strm(sample_arr=self.dmd_array, i_sample0=self.dmd_head, nsamples=dmd_head_new-self.dmd_head, synch_arr=self.synch_array, synch_head0=self.synch_head, statemx=self.JPLstatemx)
        assert synch_head_new == dmd_head_new
        self.dt_array[4] += (time.perf_counter() - t0)

        ## Symbol decision, payload deframing
        t0 = time.perf_counter()
        dd_ret_ = decide_decode(dmd_arr=self.dmd_array, center_f_arr=self.center_f_array, power_arr=self.power_array, synch_arr=self.synch_array, dmdsynch_head=dmd_head_new, opt_tap_idx_f0=self.opt_tap_idx_f, sddmx=self.sddmx, deframermx=self.deframermx, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg)
        payloads, payload_delimits, payload_frequencies, payload_powertuples, fault_counts, bits, opt_tap_idx_f_new = dd_ret_
        #assert opt_tap_idx_f_new > (dmd_head_new - int(self.sddmx[0,2]) - self.config.sps*2)
        #assert opt_tap_idx_f_new <= max(dmd_head_new - int(self.sddmx[0,2]) + self.config.sps*2, 0), opt_tap_idx_f_new
        ret = self._split_payloads(payloads=payloads, delimits=payload_delimits, nrm_offset_frequencies=payload_frequencies, payload_powertuples=payload_powertuples)
        self.dt_array[5] += (time.perf_counter() - t0)

        ## Roll buffers
        t0 = time.perf_counter()
        self.rs_head = rs_head_new
        self.center_f_head = center_f_head_new
        self.dmd_head = dmd_head_new
        self.synch_head = synch_head_new
        self.opt_tap_idx_f = opt_tap_idx_f_new
        assert self.rs_head == self.center_f_head
        assert self.rs_head > self.dmd_head
        assert self.dmd_head == self.synch_head
        if self.rs_head >= self.buffer_roll_limit:
            self._buffer_roll_3()
            assert self.rs_head > 0
            assert self.center_f_head > 0
            assert self.dmd_head > 0
            assert self.synch_head > 0
            assert self.opt_tap_idx_f > 0
        self.dt_array[6] += (time.perf_counter() - t0)

        if give_bits:
            return bits, carrier_sensed
        return ret, carrier_sensed


    def _split_payloads(self, payloads, delimits, nrm_offset_frequencies, payload_powertuples):
        pl_list = list()
        for i_pl, (i0,i1) in enumerate(delimits):
            #f_offset_nrm = nrm_offset_frequencies[i_pl] + self.centering_fdelta_nrm
            f_absolute = (nrm_offset_frequencies[i_pl] * self.config.sps * self.config.baudrate) + self.config.rx_f_center
            pl_power, noise_power, power_summation_len = payload_powertuples[i_pl]
            power_bw = (power_summation_len / self.fftlen) * self.config.sps * self.config.baudrate
            pl_list.append((bytes(payloads[i0:i1]), f_absolute, (pl_power, noise_power, power_bw)))
            assert len(pl_list[-1][0]) == (i1-i0)
        return pl_list


    def _buffer_roll_3(self):
        # B --- B
        self.rs_array[0:self.rs_head-self.bufferhalf] 				= self.rs_array[self.bufferhalf:self.rs_head]
        self.center_f_array[0:self.center_f_head-self.bufferhalf] 	= self.center_f_array[self.bufferhalf:self.center_f_head]
        self.fft_instr_array[0:self.center_f_head-self.bufferhalf] 	= self.fft_instr_array[self.bufferhalf:self.center_f_head]
        self.power_array[0:self.center_f_head-self.bufferhalf] 		= self.power_array[self.bufferhalf:self.center_f_head]			# for energy sense
        self.dmd_array[0:self.dmd_head-self.bufferhalf] 			= self.dmd_array[self.bufferhalf:self.dmd_head]
        self.synch_array[0:self.synch_head - self.bufferhalf] 		= self.synch_array[self.bufferhalf:self.synch_head]
        self.rs_head 			= self.rs_head - self.bufferhalf
        self.center_f_head 		= self.center_f_head - self.bufferhalf
        self.dmd_head 			= self.dmd_head - self.bufferhalf
        self.synch_head 		= self.synch_head - self.bufferhalf
        self.opt_tap_idx_f 		= self.opt_tap_idx_f - self.bufferhalf
        #print("BUFFER ROLLED", flush=True)
        # B --- B






# PRECOMPILE RECEIVER ====================================================================================================
def get_a_precompiling_sampleset(dsp_config:RXDSPConfig, do_print=False):
    sr0 = dsp_config.rx_sr0
    rel_offset_raw = (dsp_config.rx_f_center-dsp_config.rx_f_tune) / sr0  #0.1 * (sps*baudrate/sr0)
    rs_mx, rs_cfg = get_default_rs()
    pl = np.random.randint(0,255, RS_MAX_PL_LEN-2)
    preamble_bits = ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
    frame_bits = frame_packet(pl=pl, synchword_int=dsp_config.synchword, synchword_len=dsp_config.synchword_len, use_scrambler=True, use_rs=True, rs_mx=rs_mx, rs_cfg=rs_cfg, nrz_shift=True)
    bitstring = np.concatenate( (preamble_bits, frame_bits) )
    transmission, _ = make_samples2(sps_f=sr0/dsp_config.baudrate, bitstring=bitstring, f_offset=rel_offset_raw, power=1.0, mod_index=dsp_config.mod_index, shaper_BT_prod=dsp_config.BT_rx_match, n_silence_start=0, n_silence_end=0)

    n_fft_calibration = int( (sr0/(dsp_config.baudrate*dsp_config.sps)) * 2*dsp_config.fftlen_mpr*dsp_config.sps )
    nsamples = int(n_fft_calibration + len(transmission) + 1.0*sr0)
    i0 = int(n_fft_calibration)
    if do_print:
        print("\tEquivalent times per sample segment:")
        print("\t\t{} s for fft-calibration".format( round( n_fft_calibration/sr0 , 3 ) ))
        print("\t\t{} s for transmission".format( round( len(transmission)/sr0, 3)))
        print("\t\t{} s for margins".format( round(2.0, 3) ))
    noiseless = np.zeros(nsamples, dtype=np.complex128)
    noiseless[i0:i0+len(transmission)] += transmission
    noisePpHz = 0.02/dsp_config.baudrate
    noise = radionoise(n=nsamples, sr=sr0, W_per_Hz=noisePpHz)
    return noiseless, noise, noisePpHz


def precompile_receiver(dsp_config:RXDSPConfig, do_print=False):
    if do_print:
        print("[Precompiling]")
    t0 = time.perf_counter()
    if do_print:
        print("\t[Generating sampleset]")
    noiseless, noise, noisePpHz = get_a_precompiling_sampleset(dsp_config, do_print)
    samples = np.array(noiseless+noise, dtype=np.complex128)
    t1 = time.perf_counter()
    nsamples = len(samples)
    rx1 = Receiver(config=dsp_config)
    rx2 = Receiver(config=dsp_config)
    c = 0
    ret_pls1 = list()
    ret_pls2 = list()
    if do_print:
        print("\t[Processing]")
    while c < nsamples:
        c2 = min(c + dsp_config.batch_maxlen // 2, nsamples)
        batch = samples[c:c2]
        ret1, carrier_sensed1 = rx1.process_batch(batch=batch, give_bits=False)
        ret2, carrier_sensed2 = rx2.process_batch(batch=np.complex64(batch), give_bits=False)
        ret_pls1.extend(ret1)
        ret_pls2.extend(ret2)
        c = c2
    t2 = time.perf_counter()
    #if not ((len(ret_pls1) == 1) and (len(ret_pls2) == 1)):
    #	import pickle
    #	dd = {
    #		"noiseless":noiseless,
    #		"config":rx_config.__dict__,
    #		"noisePpHz":noisePpHz,
    #		"only_noise":noise
    #	}
    #	letters = "".join([chr(x) for x in np.random.randint(ord("A"), ord("Z")+1, 3)])
    #	f = open("precompile_fail_samples_and_config_{}.pkl".format(letters), "wb")
    #	f.write(pickle.dumps(dd))
    #	f.close()
    #	print("Repro data written for ",letters)

    #assert len(ret_pls1) == 1, len(ret_pls1)
    #assert len(ret_pls2) == 1, len(ret_pls2)

    if do_print:
        print("\t[Precompiled in {} s.  ({} s for sample generation)]".format( round(t2-t0, 3), round(t1-t0, 3)  ))
# PRECOMPILE RECEIVER ====================================================================================================
# @:374
