import queue
import numpy as np
from .lib_receiver import Receiver, RXDSPConfig, precompile_receiver
from .lib_framing import frame_packet
from .lib_tools import doppler_correction, doppler_correction_tle, calculate_assumed_carrier_frequency, ints_to_bits, FS1P_SYNCHWORD_LEN, FS1P_SYNCHWORD, make_samples2, snr_dB, DebugPrinter
from .lib_reedsolomon import get_default_rs
import threading
from queue import Queue, Empty
import time
import multiprocessing as mpr
from multiprocessing import shared_memory
from copy import copy

SHM_MEM_BASENAME = "skymodem-dsp-multimode-shm-"

_dbgprinter = DebugPrinter(log_title="DSPLoop", stdprint=True, zmqprint_host_port=("localhost", 11001))
DBGPRINT = _dbgprinter.DBGPRINT_toggled


class TXDSPConfig:
    """
    Configurations used by the DSP Loop for transmission processing.

    RXDSPConfig used for reception can be found in lib_receiver.py
    """
    def __init__(self, tx_samplerate, tx_tune_frequency, tx_center_frequency, baudrate):
        # radio device -------------------------------------
        self.tx_samplerate				= tx_samplerate
        self.tx_tune_frequency			= tx_tune_frequency
        # --------------------------------------------------
        # signal properties --------------------------------
        self.tx_center_frequency 		= tx_center_frequency
        self.baudrate			= baudrate		# Baudrate of the transmission. Has a definite effect on performance. More so if resampling rate is not adjusted.
        # --------------------------------------------------
        # --------------------------------------------------
        self.tx_BT				= 0.5
        self.tx_mod_index		= 0.5
        # --------------------------------------------------
        self.tx_f_adjustment_halfband = 12e3
        self.synchword			= FS1P_SYNCHWORD
        self.synchword_len		= FS1P_SYNCHWORD_LEN

    def check_validity(self):
        """
        Make sure that configuration parameters are within reasonable limits.
        """
        assert 1e3 < self.tx_samplerate < 32e6
        assert self.tx_tune_frequency > 1.0e3
        assert self.tx_center_frequency > 1.0e3
        assert 0 < self.baudrate < (self.tx_samplerate/2)
        assert (abs(self.tx_tune_frequency - self.tx_center_frequency) + self.tx_f_adjustment_halfband + self.baudrate * 0.6) < (0.5 * self.tx_samplerate), "Radio tuned to this frequency with this samplerate cannot see the entire band."
        assert (self.tx_BT >= 0.4) or (self.tx_BT == -1)
        assert 0.5 <= self.tx_mod_index < 10.0
        if not (self.tx_mod_index in (0.5, 0.75)):
            raise Warning("Non standard transmission modulation index of", self.tx_mod_index)







class DSPLoop:
    DBGP_ERRORS 	= 1<<0
    DBGP_INITSTOP 	= 1<<1
    DBGP_RX 		= 1<<2
    DBGP_TX 		= 1<<3
    def __init__(self, rx_dsp_config:RXDSPConfig, tx_dsp_config:TXDSPConfig, que_rx_samples_in:Queue, que_rx_payloads_out:queue.Queue, que_tx_payloads_in:Queue, que_tx_samples_out:Queue, que_signaldata_out:Queue):
        """
        Create a DSP Loop for handling modulation and demodulation.
        Includes separate threads for transmission and reception.
        There are two different modes for reception: single-mode and multi-mode.
        Single-mode processes one baudrate at a time, while multi-mode can process multiple baudrates in parallel.
        Needs to be started by calling start() or start_multimode().

        Also includes precompilation of the DSP chain to improve performance.
        """
        DBGPRINT(1, "Precompile DSP")
        precompile_receiver(rx_dsp_config, do_print=False)
        self.rx_dsp_config 			= rx_dsp_config
        self.tx_dsp_config			= tx_dsp_config
        self.do_frequency_following = True
        self.do_baudrate_following 	= False
        self.do_doppler_correction  = False
        self.do_tle_doppler_correction = False
        self.preamble_bits 			= ints_to_bits( (0xaa,)*8, bits_per_int=8) * 2 -1
        rs_mx, rs_cfg 				= get_default_rs()
        self.rs_mx 					= rs_mx
        self.rs_cfg 				= rs_cfg
        self.rlock 					= threading.RLock()
        self.que_rx_samples_in		= que_rx_samples_in
        self.que_rx_payloads_out	= que_rx_payloads_out
        self.que_tx_payloads_in		= que_tx_payloads_in
        self.que_tx_samples_out		= que_tx_samples_out
        self.que_signaldata_out 	= que_signaldata_out
        self.rx 					= Receiver(config=rx_dsp_config)
        self.rx_process_thread 		= threading.Thread(target=None, args=tuple())
        self.rx_process_thread.start()
        self.tx_process_thread 		= threading.Thread(target=None, args=tuple())
        self.tx_process_thread.start()
        self.last_verified_freq 	= (0, 0.0)  # (absolute_frequency, monotonic_timestamp)
        self.last_verified_baudrate = None
        self.own_recently_sent 		= dict()
        self.on 					= True
        self.tx_schedule			= list()
        self.t_now_mono				= 0.0
        self.dsp_perf_stats			= None
        self.dsp_perf_stats_mpr		= dict()
        self.dbgprint_mask			= self.DBGP_ERRORS | self.DBGP_INITSTOP | self.DBGP_RX | self.DBGP_TX


    def is_ok(self):
        """
        Checks if DSPLoop hasn't had any fatal errors and makes sure all threads are still alive.

        Used together with is_ok functions for other loops to determine if the whole modem is still functioning properly.
        """
        if not self.on:
            return False
        if not self.rx_process_thread.is_alive():
            return False
        if not self.tx_process_thread.is_alive():
            return False
        return True


    def close(self):
        """
        Terminate the DSP Loop and its threads.
        """
        self.on = False
        self.rx_process_thread.join(timeout=1.0)
        self.tx_process_thread.join(timeout=1.0)


    def start(self):
        """
        Start the DSP loop in normal mode

        This mode processes only a single baudrate for reception.
        """
        self.rx_process_thread = threading.Thread(target=self._rx_loop, args=tuple(), daemon=True)
        self.rx_process_thread.start()
        self.tx_process_thread = threading.Thread(target=self._tx_loop, args=tuple(), daemon=True)
        self.tx_process_thread.start()


    def start_multimode(self, baudrates, mem_index):
        """
        Start the DSP loop in multi-mode.

        This means that multiple baudrates are processed in parallel for reception.
        """
        rx_dsp_config_list = []
        ring_length = 42
        for baudrate in baudrates:
            config = copy(self.rx_dsp_config)
            config.baudrate = baudrate
            rx_dsp_config_list.append(config)
        self.rx_process_thread = threading.Thread(target=self._rx_loop_mpr, args=(rx_dsp_config_list, ring_length, mem_index), daemon=True)
        self.rx_process_thread.start()
        time.sleep(1.33)
        self.tx_process_thread = threading.Thread(target=self._tx_loop, args=tuple(), daemon=True)
        self.tx_process_thread.start()


    def set_tx_baudrate(self, baudrate):
        """
        Set currently used baudrate for transmission only.
        """
        with self.rlock:
            self.tx_dsp_config.baudrate = baudrate


    def set_baudrate(self, baudrate):
        """
        Set currently used baudrate for both transmission and reception.
        """
        with self.rlock:
            self.tx_dsp_config.baudrate = baudrate
            self.rx_dsp_config.baudrate = baudrate
            self.rx.switch_baudrate(baudrate, sps=self.rx.config.sps)


    def set_doppler_correction(self, toggle:bool):
        """
        Enable or disable Doppler correction based on received packets.
        """
        with self.rlock:
            self.do_doppler_correction = bool(toggle)

    def set_tle_doppler_correction(self, toggle:bool):
        """
        Enable or disable TLE based Doppler correction for transmission.
        """
        with self.rlock:
            self.do_tle_doppler_correction = bool(toggle)



    # == private functions ===================================================================================================================================================================
    # ========================================================================================================================================================================================
    def _tx_on(self, t_mono):
        i = 0
        n = len(self.tx_schedule)
        while i < n:
            tx_tup = self.tx_schedule[i]
            if tx_tup[1] < t_mono:
                self.tx_schedule.pop(i)
                n -= 1
                continue
            elif tx_tup[0] > t_mono:
                return False
            elif tx_tup[0] <= t_mono <= tx_tup[1]:
                return True
        return False


    def _schedule_tx(self, t_start_mono, t_end_mono):
        for i in range(len(self.tx_schedule)):
            if self.tx_schedule[i][0] > t_start_mono:
                self.tx_schedule.insert(i, (t_start_mono, t_end_mono))
                return
        self.tx_schedule.append((t_start_mono, t_end_mono))


    def _clean_own_sent(self, ts_now_mono):
        """
        Clean up recently sent packets that are too old to be sensed by self.
        """
        for key in list(self.own_recently_sent):
            if (ts_now_mono - self.own_recently_sent[key]) > 3.0:  # real time to parametric todo: 3.0 should be a parameter
                del self.own_recently_sent[key]


    def _get_transmit_frequency(self, as_offset:bool, ts_now_mono):
        """
        Get frequency to use for transmission.

        Can be either the nominal configured frequency, a frequency set to match last rx frequency, a doppler correction based on last received frequency, or a TLE based doppler correction.
        
        Returns either absolute frequency or frequency offset from tx_tune_frequency.
        """
        if self.do_frequency_following and ((ts_now_mono - self.last_verified_freq[1]) < 60.0) and (self.last_verified_freq[1] > 0): # real time to parametric todo: 60.0 should be a parameter
            f_recv_abs = self.last_verified_freq[0]
            if self.do_doppler_correction:
                f_use_abs = doppler_correction(f_rx_received=f_recv_abs, f_rx_original=self.rx_dsp_config.rx_center_frequency, f_tx_at_target=self.tx_dsp_config.tx_center_frequency)
            if self.do_tle_doppler_correction:
                f_use_abs = self.tx_dsp_config.tx_center_frequency + doppler_correction_tle(uncorrected_tx_frequency=self.tx_dsp_config.tx_center_frequency)
            else:
                f_use_abs = f_recv_abs
        else:
            f_use_abs = self.tx_dsp_config.tx_center_frequency
        if as_offset:
            return f_use_abs - self.tx_dsp_config.tx_tune_frequency
        return f_use_abs


    def _get_transmit_baudrate(self):
        """
        Get baudrate to use for transmission based on last reception.

        If nothing has been received recently, use configured baudrate.
        """
        if (not self.do_baudrate_following) or (not self.last_verified_baudrate):
            return self.tx_dsp_config.baudrate
        return self.last_verified_baudrate


    def _compose_samples(self, payload, ts_now_mono, usrp_reshape, as_c64):
        """
        Compose samples that will be sent to RadioLoop for transmission.
        """
        baudrate = self._get_transmit_baudrate()
        f_use_offset = self._get_transmit_frequency(as_offset=True, ts_now_mono=ts_now_mono)
        f_offset_nrm = f_use_offset / self.tx_dsp_config.tx_samplerate
        pl_char_ints = np.array(bytearray(payload), dtype=np.int64)
        bits = frame_packet(pl=pl_char_ints, synchword_int=self.tx_dsp_config.synchword, synchword_len=self.tx_dsp_config.synchword_len, use_scrambler=True, use_rs=True, rs_mx=self.rs_mx, rs_cfg=self.rs_cfg, nrz_shift=True)
        bits = np.concatenate( (self.preamble_bits, bits) )
        sps = self.tx_dsp_config.tx_samplerate / baudrate
        n_silence_start = int(self.tx_dsp_config.tx_samplerate * 10.0e-3) # Accounts for PA ramp up. TODO: this should be a setting?
        samples, _ = make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_nrm, power=1.0, mod_index=self.tx_dsp_config.tx_mod_index, shaper_BT_prod=self.tx_dsp_config.tx_BT, n_silence_start=n_silence_start, n_silence_end=0)
        #samples = np.exp(2j*np.pi*np.arange(len(samples)) * 0.005 )
        if usrp_reshape:
            samples = np.reshape(samples, (1, len(samples)))
        if as_c64:
            samples = np.array(samples, dtype=np.complex64)
        return samples, f_use_offset+self.tx_dsp_config.tx_tune_frequency



    # -- loops -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    def _rx_loop(self):
        """
        Main demodulation loop for single-mode reception.

        Receives samples from RadioLoop, processes them, and outputs received payloads to SkyLinkLoop.
        """
        default_batchlen = self.rx_dsp_config.batch_maxlen // 2
        T_sample = 1.0 / self.rx_dsp_config.rx_samplerate
        ts_last_cs = 0.0
        while self.on:
            try:
                samples, ts_s0_mono, ts_s0_unix = self.que_rx_samples_in.get(timeout=0.20)
            except Empty:
                continue
            except Exception as e:
                DBGPRINT(self.dbgprint_mask&self.DBGP_ERRORS, "Queue.get() exception in _rx_loop(): ", e)
                self.on = False
                return
            with self.rlock:
                c = 0
                while c < len(samples):
                    batch = samples[c:c+default_batchlen]
                    rx_pls, carrier_sensed = self.rx.process_batch(batch=batch, give_bits=False)
                    ts_mono = ts_s0_mono + c * T_sample
                    ts_unix = ts_s0_unix + c * T_sample
                    self.t_now_mono = ts_mono
                    tx_interference = self._tx_on(t_mono=ts_mono)
                    if carrier_sensed and (not tx_interference) and ((ts_mono - ts_last_cs) > 20e-3):
                        self.que_rx_payloads_out.put(("cs", None, ts_mono), timeout=1.0)
                        ts_last_cs = ts_mono
                    for rx_pl, rx_f_absolute, power_tuple in rx_pls:
                        self._clean_own_sent(ts_now_mono=self.t_now_mono)
                        snr = snr_dB(pl_power=power_tuple[0], noise_power=power_tuple[1])

                        # Discard self receptions, also ignore if SNR is too high (indicates likely self reception)
                        if rx_pl in self.own_recently_sent:
                            DBGPRINT(self.dbgprint_mask&self.DBGP_RX, "Discarded self reception.")
                            continue

                        # This is a quick patch for problem of correcting doppler to own transmission. Need to improve later.
                        # Doppler only on GS so this allows lab usage with high SNR.
                        if snr > 50.0 and (self.do_doppler_correction or self.do_tle_doppler_correction):
                            DBGPRINT(self.dbgprint_mask&self.DBGP_RX, f"High SNR but not recognized as self reception: {snr} dB. Discarded.")
                            continue

                        DBGPRINT(self.dbgprint_mask&self.DBGP_RX, f"Received a Payload: {len(rx_pl)} bytes\n\t Absolute Frequency: {round(rx_f_absolute*1e-6, 3)} MHz, SNR: {round(snr_dB(pl_power=power_tuple[0], noise_power=power_tuple[1]), 2)}")
                        # Calculate assumed carrier frequency based on doppler correction
                        # Seems to drift so just log for now
                        if self.do_tle_doppler_correction:
                            self.tx_dsp_config.tx_center_frequency = calculate_assumed_carrier_frequency(absolute_rx_frequency=rx_f_absolute, uncorrected_tx_frequency=self.tx_dsp_config.tx_center_frequency)


                        self.last_verified_freq = (rx_f_absolute, ts_mono)
                        self.last_verified_baudrate = self.rx_dsp_config.baudrate
                        self.que_rx_payloads_out.put(("pl", rx_pl, ts_mono), timeout=1.0)
                        if not self.que_signaldata_out.full():
                            self.que_signaldata_out.put((ts_unix, rx_f_absolute, power_tuple, self.rx_dsp_config.baudrate, rx_pl), timeout=1.0)
                    c += default_batchlen


    def _tx_loop(self):
        """
        Main modulation loop for transmission.

        Receives payloads from SkyLinkLoop, processes them into samples, and outputs samples to RadioLoop.
        """
        while self.on:
            if not self.que_tx_samples_out.empty():
                time.sleep(0.002)
                continue
            try:
                payload, t_start_mono = self.que_tx_payloads_in.get(timeout=0.20)
            except Empty:
                continue
            except Exception as e:
                DBGPRINT(self.dbgprint_mask&self.DBGP_ERRORS, "Queue.get() exception in _tx_loop():", e)
                self.on = False
                return
            with self.rlock:
                assert type(payload) in (bytes, bytearray)
                self.own_recently_sent[payload] = t_start_mono
                samplearr, f_use_abs = self._compose_samples(payload=payload, ts_now_mono=self.t_now_mono, usrp_reshape=True, as_c64=True)
                DBGPRINT(self.dbgprint_mask&self.DBGP_TX, f"TX Start at {f_use_abs * 1e-6} MHz")
                t_end_mono = t_start_mono + (samplearr.shape[1] / self.tx_dsp_config.tx_samplerate)
                self._schedule_tx(t_start_mono=t_start_mono -5e-3, t_end_mono=t_end_mono +5e-3)
            self.que_tx_samples_out.put(samplearr, timeout=4.0)


    def _rx_loop_mpr(self, dsp_config_list, ring_len, mem_idx):
        """
        Demodulation loop for multi-mode reception (Multiple baudrates in parallel).

        Receives samples from RadioLoop, distributes them to multiple processes for parallel processing, and outputs received payloads to SkyLinkLoop.
        """
        base_name = SHM_MEM_BASENAME + str(mem_idx) + "-"
        _mpr_memfree_idxed(base_name, [str(i) for i in range(ring_len+10)]+["A"])
        idle_timeout 			= 10.0 # todo: a parameter?
        que_mpr_processes_out 	= mpr.Queue(300)
        que_rdy_rprt 			= mpr.Queue(64)
        arrlen 					= int(1024 * 4)
        buffer_ring = list()
        buffer_shm_list = list()
        for ii in range(ring_len):
            name_ = base_name + str(ii)
            shm = shared_memory.SharedMemory(name=name_, create=True, size=np.zeros(arrlen, dtype=np.complex128).nbytes)
            arr = np.ndarray(shape=arrlen, dtype=np.complex128, buffer=shm.buf)
            arr[:] = 0.0
            buffer_ring.append(arr)
            buffer_shm_list.append(shm)
        name_ = base_name + "A"
        flag_ring_shm = shared_memory.SharedMemory(name=name_, create=True, size=np.zeros((ring_len,4), dtype=np.int64).nbytes)
        ring_flag_arr = np.ndarray(shape=(ring_len, 4), dtype=np.int64, buffer=flag_ring_shm.buf) # (batch_counter, nsamples, ts_s0_mono_ns, ts_s0_unix_ns)
        ring_flag_arr[:] = -1
        processes = list()
        process_events = list()
        for i in range(len(dsp_config_list)):
            mpr_ev = mpr.Event()
            mpr_ev.clear()
            process_events.append(mpr_ev)
            process_idd = len(processes)
            p = mpr.Process(target=_rx_mpr_process, args=(dsp_config_list[i], process_idd, mpr_ev, [(shm.name,arrlen,np.complex128) for shm in buffer_shm_list], flag_ring_shm.name, que_mpr_processes_out, que_rdy_rprt, idle_timeout, self.dbgprint_mask), daemon=True) #dsp_config:RXDSPConfig, idd, trig_ev, shm_buffer_ring_shm_names, flag_ring_shm_name, que_out
            processes.append(p)
            p.start()

        rdy_batch_indexes = [0,] * len(dsp_config_list)
        min_rdy_index = min(rdy_batch_indexes)
        batch_index = 0
        ring_head = 0
        ts_last_cs = 0.0
        wait_sleep_streak = 0
        while self.on:
            # Set events. Communicates main loop is alive.
            for mpr_ev in process_events:
                mpr_ev.set()
            # Deplete queue of payloads and carrier-sense reports.
            with self.rlock:
                while not que_mpr_processes_out.empty():
                    rcode, p_idd, tup = que_mpr_processes_out.get_nowait()
                    if (rcode == "cs") and (not self._tx_on(t_mono=tup[0])) and ((tup[0] - ts_last_cs) > 20e-3):
                        ts_mono, ts_unix = tup[:2]
                        self.que_rx_payloads_out.put(("cs", None, ts_mono), timeout=1.0)
                        ts_last_cs = ts_mono
                    elif rcode == "pl":   #ts_mono, ts_unix, rx_pl, rx_f_absolute, power_tuple, rx_dsp_config.baudrate
                        (ts_mono, ts_unix, rx_pl, rx_f_absolute, power_tuple, baudrate) = tup
                        self._clean_own_sent(ts_now_mono=self.t_now_mono)
                        if rx_pl in self.own_recently_sent:
                            DBGPRINT(self.dbgprint_mask&self.DBGP_RX, "Discarded self reception.")
                            continue
                        DBGPRINT(self.dbgprint_mask&self.DBGP_RX, f"Received a Payload: {len(rx_pl)} bytes\n\t Absolute Frequency: {round(rx_f_absolute*1e-6, 3)} MHz, Baudrate: {baudrate}, SNR: {round(snr_dB(pl_power=power_tuple[0], noise_power=power_tuple[1]), 2)}")
                        self.last_verified_baudrate = baudrate
                        self.last_verified_freq = (rx_f_absolute, ts_mono)
                        self.que_rx_payloads_out.put(("pl", rx_pl, ts_mono), timeout=1.0)
                        if not self.que_signaldata_out.full():
                            self.que_signaldata_out.put((ts_unix, rx_f_absolute, power_tuple, baudrate, rx_pl), timeout=1.0)
                    elif rcode == "-1":
                        dt_array, n_processed = tup
                        self.dsp_perf_stats_mpr[p_idd] = (dt_array, n_processed)
            # Read processes reporting advance in batch processing.
            while not que_rdy_rprt.empty():
                (idd, rdy_batch_index) = que_rdy_rprt.get_nowait()
                rdy_batch_indexes[idd] = rdy_batch_index
                min_rdy_index = min(rdy_batch_indexes)
            # If any process is more than (ring_len-8) behind, wait up to 1000 cycles of 2.5ms, and then perform error exit.
            a_process_is_behind = (batch_index - min_rdy_index) > (ring_len-8)
            if a_process_is_behind:
                wait_sleep_streak += 1
                if wait_sleep_streak > 1000:
                    DBGPRINT(self.dbgprint_mask&self.DBGP_ERRORS, f"ERROR: Processing thread {rdy_batch_indexes.index(min_rdy_index)} stalled in _rx_loop_mpr().")
                    self.on = False
                    return
                time.sleep(0.0025)
            # If no process is behind, zero the counter, and receive more samples to process.
            if not a_process_is_behind:
                wait_sleep_streak = 0
                try:
                    samples, ts_s0_mono, ts_s0_unix = self.que_rx_samples_in.get(timeout=0.20)
                    nsamples = len(samples)
                except Empty:
                    samples, ts_s0_mono, ts_s0_unix = None,None,None
                    nsamples = 0
                except Exception as e:
                    DBGPRINT(self.dbgprint_mask&self.DBGP_ERRORS, "Queue.get() exception in _rx_loop_mpr(): ", e)
                    self.on = False
                    break
                with self.rlock:
                    if not (samples is None):
                        buffer_ring[ring_head][:nsamples] = samples
                        ring_flag_arr[ring_head] = (batch_index, nsamples, int(ts_s0_mono*1e9), int(ts_s0_unix*1e9))
                        ring_head = (ring_head+1) % ring_len
                        batch_index += 1
                        self.t_now_mono = ts_s0_mono
                        for mpr_ev in process_events:
                            mpr_ev.set()
        ring_flag_arr[:] = -2
        _mpr_memfree_idxed(base_name, [str(i) for i in range(ring_len)]+["A"])
    # -- loops -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------




def _mpr_memfree_idxed(basename, suffixes):
    n0 = len(suffixes)
    n_ok = 0
    for suffix in suffixes:
        name = basename + suffix
        try:
            shm = shared_memory.SharedMemory(name=name)
            shm.unlink()
            n_ok += 1
        except:
            pass
    DBGPRINT(1, f"{n_ok}/{n0} shared memories unlinked by main-thread.")






def _rx_mpr_process(rx_dsp_config:RXDSPConfig, idd, trig_ev, shm_buffer_ring_shm_names, flag_ring_shm_name, que_out, que_rdy_rprt, idle_timeout, dbgprint_mask):
    """
    Process used for each baudrate in multi-mode reception.
    """
    default_batchlen = rx_dsp_config.batch_maxlen // 2
    T_sample = 1.0 / rx_dsp_config.rx_samplerate
    rx = Receiver(config=rx_dsp_config)
    buffer_ring = list()
    buffer_shm_list = list()
    for (name, shape, dtype) in shm_buffer_ring_shm_names:
        shm = shared_memory.SharedMemory(name=name)
        arr = np.ndarray(shape=shape, dtype=dtype, buffer=shm.buf)
        buffer_ring.append(arr)
        buffer_shm_list.append(shm)
    ring_len = len(buffer_ring)
    flag_ring_shm = shared_memory.SharedMemory(name=flag_ring_shm_name)
    ring_flag_arr = np.ndarray(shape=(ring_len, 4), dtype=np.int64, buffer=flag_ring_shm.buf) # (batch_counter, nsamples, ts_s0_mono_ns, ts_s0_unix_ns)
    ring_head = 0
    last_batch_index = -1
    t_timeout = time.monotonic() + idle_timeout
    t_next_stats = time.monotonic()
    DBGPRINT(dbgprint_mask&DSPLoop.DBGP_INITSTOP, f"mpr-thread-{idd} LOOP START")
    while True:
        trig = trig_ev.wait(timeout=0.25)
        if not trig:
            if ring_flag_arr[0,0] < -1:
                DBGPRINT(dbgprint_mask&DSPLoop.DBGP_INITSTOP,f"\tmpr-thread-{idd} exits: Flag ring < -1. Benign.")
                _mpr_memfree(idd, buffer_shm_list=buffer_shm_list, flag_ring_shm=flag_ring_shm)
                return
            if time.monotonic() > t_timeout:
                DBGPRINT(dbgprint_mask&(DSPLoop.DBGP_INITSTOP | DSPLoop.DBGP_ERRORS),f"\tmpr-thread-{idd} exits: Timeout. Benign or Malign.")
                _mpr_memfree(idd, buffer_shm_list=buffer_shm_list, flag_ring_shm=flag_ring_shm)
                return
            continue
        trig_ev.clear()
        while True:
            t_timeout = time.monotonic() + idle_timeout
            r0,r1,r2,r3 = ring_flag_arr[ring_head]  # (batch_counter, nsamples, ts_s0_mono_ns, ts_s0_unix_ns)
            if r0 < -1:
                DBGPRINT(dbgprint_mask&DSPLoop.DBGP_INITSTOP, f"\tmpr-thread-{idd} exits: Flag ring[i,0] < -1. Benign.")
                _mpr_memfree(idd, buffer_shm_list=buffer_shm_list, flag_ring_shm=flag_ring_shm)
                return
            if (r0 == (last_batch_index - ring_len + 1)) or (r0 == -1): # last entry from previous loop, or unused slot during the first loop.
                if time.monotonic() > t_next_stats:
                    t_next_stats = time.monotonic() + 2.0
                    que_out.put( ("-1", idd, (rx.dt_array, rx.n_processed)) )
                break
            if (last_batch_index != -1) and (r0 != (last_batch_index + 1)): # either the first batch, or batch index is next in order from the last one.
                DBGPRINT(dbgprint_mask&(DSPLoop.DBGP_INITSTOP | DSPLoop.DBGP_ERRORS), f"\tmpr-thread-{idd} exits: Out of synch ({last_batch_index} vs {r0}). Malign.")
                _mpr_memfree(idd, buffer_shm_list=buffer_shm_list, flag_ring_shm=flag_ring_shm)
                return
            last_batch_index = r0
            nsamples = int(r1)
            ts_s0_mono = r2 * 1.0e-9
            ts_s0_unix = r3 * 1.0e-9
            ts_last_cs = 0.0
            buffer_arr = buffer_ring[ring_head]
            c = 0
            while c < nsamples:
                rx_pls, carrier_sensed = rx.process_batch(batch=buffer_arr[c:min(c+default_batchlen,nsamples)] , give_bits=False)
                ts_mono = ts_s0_mono + c * T_sample
                ts_unix = ts_s0_unix + c * T_sample
                if carrier_sensed and ((ts_mono-ts_last_cs) > 20e-3):
                    que_out.put( ("cs",idd, (ts_mono, ts_unix)), timeout=1.0)
                    ts_last_cs = ts_mono
                for rx_pl, rx_f_absolute, power_tuple in rx_pls:
                    que_out.put( ("pl",idd, (ts_mono, ts_unix, rx_pl, rx_f_absolute, power_tuple, rx_dsp_config.baudrate)), timeout=1.0) #(ts_unix, rx_f_absolute, power_tuple, self.dsp_config.baudrate, rx_pl)
                c += default_batchlen
            ring_head = (ring_head+1) % ring_len
            if (last_batch_index%5) == 0:
                que_rdy_rprt.put((idd,last_batch_index))



def _mpr_memfree(idd, buffer_shm_list, flag_ring_shm):
    n0 = len(buffer_shm_list) + 1
    n_ok = 0
    for shm in buffer_shm_list:
        try:
            shm.unlink()
            n_ok += 1
        except:
            pass
    try:
        flag_ring_shm.unlink()
        n_ok += 1
    except:
        pass
    DBGPRINT(1, f"{n_ok}/{n0} of shared memories unlinked by mpr-thread-{idd}.")
