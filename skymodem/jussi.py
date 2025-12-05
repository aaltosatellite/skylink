import multiprocessing
import struct
import numpy as np
import threading
import socket
import multiprocessing as mpr
from multiprocessing import shared_memory
import time
import os
import uhd
from dsp_library.kuokka.lib_tools import make_samples2, radionoise, DebugPrinter
from queue import Queue, Empty, Full
from numba import njit
from dsp_library.kuokka.lib_resampler import create_staged_resampler, staged_resampler_execute_stream, minimal_disc_halflen_for_staged_resampler, minimal_frac_halflen_for_staged_resampler
from dsp_library.kuokka.lib_resampler import create_upsample_taps, upsample



MAX_CLI_COUNT 		= 6
SHM_NAME_PREFIX 	= "JUSSI_SHM_734"
IDDLEN				= 6

@njit(cache=True)  # very fast with or without njit (~3200MS/s, ~5.0µs/call with njit). Somewhat faster with njit apparently
def ring_buffer_extraction(buffer_table, toggle_table, length_table, local_head_table, n_sources):
    leng = 0
    n_stack = 0
    ringlen = buffer_table.shape[1]
    batch_maxlen = buffer_table.shape[2]
    batch = np.zeros(batch_maxlen, dtype=np.complex64)
    for i_src in range(n_sources):
        local_head = local_head_table[i_src]
        if toggle_table[i_src, local_head]:   # todo: we could loop through several (~2) head positions per source index if the first is empty (toggle = 0)
            l_ = length_table[i_src, local_head]
            batch[0:l_] = batch[0:l_] + buffer_table[i_src, local_head, 0:l_]
            toggle_table[i_src, local_head] = 0
            n_stack += 1
            leng = max(leng, l_)
            local_head_table[i_src] = (local_head+1) % ringlen  # this can be in "if" clause, or out of it...
    if n_stack > 1:
        batch[:] = batch[:] * (1.0 / np.max(np.abs(batch)))
    return batch[0:leng]


def ring_buffer_insertion(samples, idx, ring_head, buffer_table, length_table, toggle_table, insertion_trig_event):
    n_samples = len(samples)
    ringlen = buffer_table.shape[1]
    batch_maxlen = buffer_table.shape[2]
    write_cursor = 0
    while write_cursor < n_samples:
        nslp = 0
        while toggle_table[idx, ring_head]:
            time.sleep(0.0005)
            nslp += 1
            if nslp > 1000:
                return -1
        n_write = min(batch_maxlen, n_samples - write_cursor)
        buffer_table[idx, ring_head, 0:n_write] = samples[write_cursor:write_cursor + n_write]
        length_table[idx,ring_head] = n_write
        toggle_table[idx,ring_head] = 1
        insertion_trig_event.set()
        write_cursor += n_write
        ring_head = (ring_head + 1) % ringlen
    return ring_head


def shm_array_from_name(name, shape, dtype):
    shm = shared_memory.SharedMemory(name=name)
    arr = np.ndarray(shape=shape, dtype=dtype, buffer=shm.buf)
    return arr, shm


def create_shm_array(name, shape, dtype):
    shm = shared_memory.SharedMemory(name=name, create=True, size=np.zeros(shape, dtype=dtype).nbytes)
    arr = np.ndarray(shape=shape, dtype=dtype, buffer=shm.buf)
    """
	shm = shared_memory.SharedMemory(name=name_, create=True, size=np.zeros(arrlen, dtype=np.complex128).nbytes)
	arr = np.ndarray(shape=arrlen, dtype=np.complex128, buffer=shm.buf)
	arr[:] = 0.0
	"""
    return arr, shm





class JussiL0:
    def __init__(self, rx_samplerate, tx_samplerate, rx_tune_frequency, tx_tune_frequency, rx_gain, tx_gain, tx_ringlen, tx_batch_maxlen, jussi_idx):
        assert type(jussi_idx) == int
        assert 1 <= jussi_idx < 16
        self.dbgprinter 			= DebugPrinter(log_title="Jussi", stdprint=True, zmqprint_host_port=("localhost", 11001))
        self.DBGPRINT 				= self.dbgprinter.DBGPRINT
        self.rx_samplerate_tgt 			= rx_samplerate
        self.tx_samplerate_tgt 			= tx_samplerate
        self.rx_tune_frequency 				= rx_tune_frequency
        self.tx_tune_frequency 				= tx_tune_frequency
        self.rx_gain 				= rx_gain
        self.tx_gain 				= tx_gain
        self.tx_ringlen				= tx_ringlen
        self.tx_batch_maxlen		= tx_batch_maxlen
        self.rx_samplerate_actual 			= None
        self.tx_samplerate_actual 			= None
        self.rx_print_ival_init 	= 1.0
        self.rx_print_ival_max 		= 30.0
        self.rx_print_ival_incr 	= 2.0
        self.maxreset_interval 		= 30.0
        self.rx_ringlen				= 128
        self.rx_ring_arraylen		= 1024 * 256
        self.rx_ring_head			= 0
        for midfix in ["-rbr-","-btb-","-ttb-","-ltb-"]:
            name = SHM_NAME_PREFIX+midfix+str(jussi_idx)
            try:
                shm = shared_memory.SharedMemory(name=name)
                shm.unlink()
            except:
                pass
        self.rx_buff_ring,shm0		= create_shm_array(name=SHM_NAME_PREFIX+"-rbr-"+str(jussi_idx), shape=(self.rx_ringlen, self.rx_ring_arraylen), dtype=np.complex64) #todo ensure the arrays here and in L0 are the same length.
        self.tx_buffer_table,shm1 	= create_shm_array(name=SHM_NAME_PREFIX + "-btb-"+str(jussi_idx), shape=(MAX_CLI_COUNT, self.tx_ringlen, self.tx_batch_maxlen), dtype=np.complex64)
        self.tx_toggle_table,shm2 	= create_shm_array(name=SHM_NAME_PREFIX + "-ttb-"+str(jussi_idx), shape=(MAX_CLI_COUNT, self.tx_ringlen), dtype=np.int8)
        self.tx_length_table,shm3 	= create_shm_array(name=SHM_NAME_PREFIX + "-ltb-"+str(jussi_idx), shape=(MAX_CLI_COUNT, self.tx_ringlen), dtype=np.int64)
        self.rx_upd_ques			= list()
        self.q_rx_samples_out 		= Queue(512)
        self.q_tx_samples_in 		= Queue(2)
        self.rx_memring_thread 		= threading.Thread(target=None,    args=tuple())
        self.rx_thread 				= threading.Thread(target=None,    args=tuple())
        self.tx_thread 				= threading.Thread(target=None,    args=tuple())
        self.local_head_table 		= np.array(MAX_CLI_COUNT, dtype=np.int64)
        self.tx_trigger_event		= mpr.Event()
        self._shared_memories		= [shm0,shm1,shm2,shm3] # these must be kept in context. GC collecting them sometimes breaks the shared memory access.
        self.on = True


    def usrp_start(self):
        self.DBGPRINT("USRP INIT")
        usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
        usrp.set_rx_rate(self.rx_samplerate_tgt, 0)
        usrp.set_tx_rate(self.tx_samplerate_tgt, 0)
        usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(self.rx_tune_frequency), 0)
        usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(self.tx_tune_frequency), 0)
        usrp.set_rx_gain(self.rx_gain, 0)
        usrp.set_tx_gain(self.tx_gain, 0)
        usrp.set_gpio_attr("FP0", "ATR_TX", 0x0100, 0x0100)
        usrp.set_gpio_attr("FP0", "ATR_XX", 0x0100, 0x0100)
        time.sleep(0.1)
        self.rx_samplerate_actual = float(usrp.get_rx_freq(0))
        self.tx_samplerate_actual = float(usrp.get_tx_freq(0))
        if not np.isclose(self.rx_samplerate_tgt, self.rx_samplerate_actual):
            self.DBGPRINT("WARNING: Actual RX-samplerate differs from specified: {} MS/s specified -vs- {} MS/s obtained.".format(round(1e-6*self.rx_samplerate_tgt), round(1e-6*self.rx_samplerate_actual)))
        if not np.isclose(self.tx_samplerate_tgt, self.tx_samplerate_actual):
            self.DBGPRINT("WARNING: Actual TX-samplerate differs from specified: {} MS/s specified -vs- {} MS/s obtained.".format(round(1e-6*self.tx_samplerate_tgt,3), round(1e-6*self.tx_samplerate_actual)))

        #print("bank 0:", usrp.get_gpio_banks(0)) #['FP0', 'RXA', 'TXA']  (No further banks in B210)
        #print("FP0 CTRL", usrp.get_gpio_attr("FP0", "CTRL"))
        #print("FP0 DDR", usrp.get_gpio_attr("FP0", "DDR"))
        #print("FP0 OUT", usrp.get_gpio_attr("FP0", "OUT"))
        #print("FP0 ATR_0X", usrp.get_gpio_attr("FP0", "ATR_0X"))
        #print("FP0 ATR_RX", usrp.get_gpio_attr("FP0", "ATR_RX"))
        #print("FP0 ATR_TX", usrp.get_gpio_attr("FP0", "ATR_TX"))
        #print("FP0 ATR_XX", usrp.get_gpio_attr("FP0", "ATR_XX"))
        #print(usrp.set_gpio_src("FP0", "RX"))
        #DBGPRINT("-------------------------------------------------")
        #DBGPRINT("RX num channels", usrp.get_rx_num_channels())
        #DBGPRINT("TX num channels", usrp.get_tx_num_channels())
        self.DBGPRINT("RX antennas(0):         {}".format(usrp.get_rx_antennas(0)))
        self.DBGPRINT("TX antennas(0):         {}".format(usrp.get_tx_antennas(0)))
        self.DBGPRINT("RX antenna in use(0):   {}".format(usrp.get_rx_antenna(0)))
        self.DBGPRINT("TX antenna in use(0):   {}".format( usrp.get_tx_antenna(0)))
        #DBGPRINT("RX antennas(1)", usrp.get_rx_antennas(1))
        #DBGPRINT("TX antennas(1)", usrp.get_tx_antennas(1))
        #DBGPRINT("-------------------------------------------------")
        self.DBGPRINT("RX gain range:        {}".format( str(usrp.get_rx_gain_range(0))[:-1] ))
        self.DBGPRINT("TX gain range:        {}".format( str(usrp.get_tx_gain_range(0))[:-1] ))
        self.DBGPRINT("usrp RX gain:         {}".format( usrp.get_rx_gain(0) ))
        self.DBGPRINT("usrp TX gain:         {}".format( usrp.get_tx_gain(0) ))
        self.DBGPRINT("usrp RX samplerate:   {} ksps".format( round(usrp.get_rx_rate(0)*1e-3, 6) ))
        self.DBGPRINT("usrp TX samplerate:   {} ksps".format( round(usrp.get_tx_rate(0)*1e-3, 6) ))
        self.DBGPRINT("usrp RX tune-f:       {} MHz".format( round(usrp.get_rx_freq(0)*1e-6, 3) ))
        self.DBGPRINT("usrp TX tune-f:       {} MHz".format( round(usrp.get_tx_freq(0)*1e-6, 3) ))

        self.rx_memring_thread 	= threading.Thread(target=self._rx_q_to_memring_loop, 	args=tuple(), daemon=True)
        self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    		args=(usrp,), daemon=True)
        self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    		args=(usrp,), daemon=True)
        self.rx_memring_thread.start()
        self.rx_thread.start()
        self.tx_thread.start()
        self.DBGPRINT("USRP loop started.")


    def _rx_q_to_memring_loop(self):
        while self.on:
            try:
                samples = self.q_rx_samples_out.get(timeout=0.25)
            except Empty:
                continue
            self.rx_buff_ring[self.rx_ring_head, 0:len(samples)] = samples
            for q in self.rx_upd_ques:
                if not q.full():
                    q.put_nowait( (self.rx_ring_head, len(samples)) )
            self.rx_ring_head = (self.rx_ring_head+1) % self.rx_ringlen


    # === USRP ===============================================================================================================================================================================
    def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP):
        rx_bufferlen 			= int(self.rx_samplerate_actual * 2e-3) # todo batch time parameter
        recv_buffer 			= np.zeros((1, rx_bufferlen), dtype=np.complex64)
        n_rx_loops 				= 0
        n_rx_total 				= 0
        dt_amplitude_measure 	= 0.0
        sample_maxamps 			= np.zeros(3, dtype=np.float64)
        rx_print_interval		= self.rx_print_ival_init
        t_next_print    		= time.monotonic() + rx_print_interval
        t_next_maxreset 		= time.monotonic() + self.maxreset_interval
        st_args = uhd.usrp.StreamArgs("fc32", "sc16")
        st_args.channels = [0]
        rx_streamer = usrp.get_rx_stream(st_args)
        stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
        stream_cmd.stream_now = True
        metadata = uhd.types.RXMetadata()
        t00 = time.perf_counter()
        rx_streamer.issue_stream_cmd(stream_cmd)
        t00 = (t00+time.perf_counter())/2.0
        while self.on:
            # -----------------------------------------------------------------------------------------------------------------------------------
            ts_mono = time.monotonic()
            ts_s0_mono = ts_mono
            ts_s0_unix = time.time()
            # receive samples -------------------------------------------------------------------------------------------------------------------
            rx_ret = rx_streamer.recv(recv_buffer, metadata) #blocking until rx_bufferlen samples acquired
            n_rx_total += rx_ret
            if rx_ret != rx_bufferlen:
                self.DBGPRINT("WARNING: RECV RETURNED NON-FULL BUFFER WITH RET VALUE "+str(rx_ret))
            buff_ = recv_buffer[0, :rx_ret].copy()
            # -----------------------------------------------------------------------------------------------------------------------------------
            # output samples --------------------------------------------------------------------------------------------------------------------
            if not self.q_rx_samples_out.full():
                self.q_rx_samples_out.put_nowait((buff_, ts_s0_mono, ts_s0_unix))
            else:
                self.DBGPRINT("ERROR: Level-0 radio-to-process queue overflow.") # todo sample overflow can be an acceptable state?
                self.on = False
                break
            # -----------------------------------------------------------------------------------------------------------------------------------
            # statistics ------------------------------------------------------------------------------------------------------------------------
            avg_sr = n_rx_total / (time.perf_counter() - t00)
            t00_ = time.perf_counter()
            sample_maxamps[0] = np.max( (sample_maxamps[0], np.max(np.abs(buff_.real))) )
            sample_maxamps[1] = sample_maxamps[1] +  (np.average( np.abs(buff_[0:32]) ) - sample_maxamps[1]) * 0.1
            dt_amplitude_measure += (time.perf_counter() - t00_)

            if ts_mono >= t_next_print:
                ampmax_int_time = round(ts_mono - (t_next_print - self.maxreset_interval), 1)
                report_str = "(sr~{} MS/s measured vs {} MS/s specced). component-max:{}, avg-amplitude:{} ({}s)".format( round(1e-6*avg_sr, 5), round(1e-6*self.rx_samplerate_tgt, 5), sample_maxamps[0], sample_maxamps[1], ampmax_int_time)
                self.DBGPRINT(report_str)
                t_next_print = ts_mono + rx_print_interval
                rx_print_interval = min(self.rx_print_ival_max, rx_print_interval+self.rx_print_ival_incr)
            if ts_mono >= t_next_maxreset:
                sample_maxamps[0] = 0.0
                t_next_maxreset = ts_mono + self.maxreset_interval
            n_rx_loops += 1
            # -----------------------------------------------------------------------------------------------------------------------------------


    def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP):
        tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
        # stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
        tx_stream_args.channels = [0]
        tx_streamer = usrp.get_tx_stream(tx_stream_args)
        tx_metadata = uhd.types.TXMetadata()
        tx_batchlen = int(self.tx_samplerate_actual * 1e-3) # todo batch time parameter
        while self.on:
            trig = self.tx_trigger_event.wait(timeout=0.25)
            if not trig:
                continue
            self.tx_trigger_event.clear()
            samplearr = ring_buffer_extraction(buffer_table=self.tx_buffer_table, toggle_table=self.tx_toggle_table, length_table=self.tx_length_table, local_head_table=self.local_head_table, n_sources=MAX_CLI_COUNT)
            if len(samplearr) == 0:
                continue
            t_end = time.perf_counter()
            tx_metadata.start_of_burst = True
            tx_metadata.end_of_burst = False
            while True:
                nsamples = len(samplearr)
                idx = 0
                t0_arr = time.perf_counter()
                while idx < nsamples:
                    tx_streamer.send(samplearr[idx:idx+tx_batchlen], tx_metadata)
                    tx_metadata.start_of_burst = False
                    idx = min(idx+tx_batchlen, nsamples)
                    t_end = t0_arr + idx / self.tx_samplerate_actual
                    time.sleep(max(0, t_end-time.perf_counter()-2.0e-3)) 	# We sleep until 2 ms before the pushed batch run out. The size is not critical.
                                                                                                                                    # The function of this waittime is to limit the speed at which the queue is emptied to the SDR buffer,
                                                                                                                                    # in case the SDR buffer is very large.
                time.sleep(max(0, t_end-time.perf_counter()-1.0e-3))		# We sleep until 1 ms before the given samples run out
                samplearr = ring_buffer_extraction(buffer_table=self.tx_buffer_table, toggle_table=self.tx_toggle_table, length_table=self.tx_length_table, local_head_table=self.local_head_table, n_sources=MAX_CLI_COUNT)
                if len(samplearr) > 0:
                    continue												# If we obtain a new array of samples, loop back to transmitting.
                tx_metadata.end_of_burst = True
                tx_streamer.send(np.zeros(32, dtype=np.complex64), tx_metadata)
                break
    # === USRP ===============================================================================================================================================================================


JUSSIMSG_TAG	= b"J+"
MTKEY_NEW_CMDSTRM		= 90
MTKEY_NEW_RXSTRM		= 91
MTKEY_NEW_TXSTRM		= 92
MTKEY_SET_F_RX			= 2
MTKEY_GET_F_RX			= 3
MTKEY_SET_F_TX			= 4
MTKEY_GET_F_TX			= 5
MTKEY_SET_SR_RX			= 6
MTKEY_GET_SR_RX			= 7
MTKEY_SET_SR_TX			= 8
MTKEY_GET_SR_TX			= 9
MTKEY_SET_GAIN_RX		= 10
MTKEY_GET_GAIN_RX		= 11
MTKEY_SET_GAIN_TX		= 12
MTKEY_GET_GAIN_TX		= 13
MTKEY_START_RX			= 50
MTKEY_STOP_RX			= 51

MTKEY_ACK				= 100
MTKEY_NACK				= 101
MTKEY_IDD				= 102
#struct.unpack("5s2B", b"abc88a\xff")
JUSSI_TYPEKEYS = {
        MTKEY_NEW_CMDSTRM : (6 + 2 * 4, "6s2I"),
        MTKEY_NEW_RXSTRM : 	(4, "4s"),
        MTKEY_NEW_TXSTRM : 	(4, "4s"),
        MTKEY_IDD : 	    (4, "4s"),
}


def receive_up_to(sock:socket.socket, nbytes:int, timeout):
    rcvbuff = bytearray()
    nleft = nbytes
    t_end = time.monotonic() + timeout
    while True:
        sock.settimeout(max(0.001, t_end - time.monotonic()))
        try:
            rcv = sock.recv(nleft)
        except socket.timeout:
            if time.monotonic() > t_end:
                return bytes(rcvbuff)
            continue
        if not rcv:
            return b""
        rcvbuff.extend(rcv)
        if len(rcvbuff) >= nbytes:
            return bytes(rcvbuff)
        if time.monotonic() > t_end:
            return bytes(rcvbuff)
        nleft = nbytes - len(rcvbuff)


def receive_jussi_frame(sock:socket.socket, timeout):
    rcvbuff = bytearray()
    nleft = len(JUSSIMSG_TAG)
    t_end = time.monotonic() + timeout
    state = 0
    unpack_code = ""
    unpack_len = 0
    mtype = 0
    while True:
        sock.settimeout(max(0.001, t_end - time.monotonic()))
        try:
            rcv = sock.recv(nleft)
        except socket.timeout:
            if time.monotonic() > t_end:
                return -1, None
            continue
        if not rcv:
            return -2, None
        rcvbuff.extend(rcv)

        if state == 0:
            if len(rcvbuff) >= len(JUSSIMSG_TAG):
                i_start = rcvbuff.find(JUSSIMSG_TAG)
                if i_start >= 0:
                    rcvbuff = rcvbuff[i_start+len(JUSSIMSG_TAG):]
                    state += 1
                    nleft = 1
        if state == 1:
            if len(rcvbuff) >= 1:
                mtype = rcvbuff[0]
                if not mtype in JUSSI_TYPEKEYS:
                    return -3, None
                unpack_len, unpack_code = JUSSI_TYPEKEYS[mtype]
                nleft = unpack_len
                rcvbuff = rcvbuff[1:]
                state += 1
        if state == 2:
            if len(rcvbuff) >= unpack_len:
                try:
                    pl_elements = struct.unpack(unpack_code, rcvbuff[0:unpack_len])
                except:
                    return -4, None
                return 0, (mtype, pl_elements)


def send_jussi_frame(sock:socket.socket, pl:bytes):
    sock.settimeout(0.25)
    try:
        sock.send(JUSSIMSG_TAG + pl)
        return 0
    except:
        return -1




def handshake_new_cmd_connection(sock:socket.socket, dd_idd_sock_ts, common_lock):
    with common_lock:
        idd = os.urandom(4)
        while idd in dd_idd_sock_ts:
            idd = os.urandom(4)
        dd_idd_sock_ts[idd] = {"cmd":sock, "ts":time.time(), "rx":None, "tx":None}
        dd_idd_sock_ts[idd] = [sock, time.time(), "CMD"]
    send_jussi_frame(sock, pl=bytes([MTKEY_IDD]) + idd)

def handshake_new_rx_connection(sock:socket.socket, dd_idd_sock_ts, common_lock):
    ok, frame = receive_jussi_frame(sock=sock, timeout=2.0)
    if not ok:
        return
    key, pl_elements = frame
    if key != MTKEY_IDD:
        return
    idd = pl_elements[0]
    with common_lock:
        if not idd in dd_idd_sock_ts:
            return
        dd_idd_sock_ts[idd] = [sock, time.time(), "RX"]
    send_jussi_frame(sock, pl=bytes([MTKEY_IDD]) + idd)

def handshake_new_tx_connection(sock:socket.socket, dd_idd_sock_ts, common_lock):
    with common_lock:
        idd = os.urandom(4)
        while idd in dd_idd_sock_ts:
            idd = os.urandom(4)
        dd_idd_sock_ts[idd] = [sock, time.time(), "TX"]
    send_jussi_frame(sock, pl=bytes([MTKEY_IDD]) + idd)







class JussiSrvr:
    def __init__(self, port, jussil0:JussiL0):
        super().__init__()
        self.on = True
        self.port = port
        self.jussil0 = jussil0
        self.dd_idd_sock_ts	= dict() # dict()
        self.common_lock 	= threading.RLock() # threading.RLock()

    def cmd_acceptor(self):
        print("[J][CMD Acceptor started]")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", self.port))
        sock.listen(5)
        sock.settimeout(1.0)
        while self.on:
            try:
                cli_sock, addr = sock.accept()
                print("[J][Connection accepted]")
                t = threading.Thread(target=handshake_new_cmd_connection, args=(cli_sock, self.dd_idd_sock_ts, self.common_lock))
                t.start()
            except socket.timeout:
                pass
            except:
                self.on = False
                #todo report exception.
                break
        #todo report exit.


    def rx_acceptor(self):
        print("[J][RX Acceptor started]")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", self.port+1))
        sock.listen(5)
        sock.settimeout(1.0)
        while self.on:
            try:
                cli_sock, addr = sock.accept()
                print("[J][Connection accepted]")
                t = threading.Thread(target=handshake_new_rx_connection, args=(cli_sock, self.dd_idd_sock_ts, self.common_lock))
                t.start()
            except socket.timeout:
                pass
            except:
                self.on = False
                #todo report exception.
                break
        #todo report exit.


    def tx_acceptor(self):
        print("[J][Acceptor started]")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", self.port+2))
        sock.listen(5)
        sock.settimeout(1.0)
        while self.on:
            try:
                cli_sock, addr = sock.accept()
                print("[J][Connection accepted]")
                t = threading.Thread(target=handshake_new_tx_connection, args=(cli_sock, self.dd_idd_sock_ts, self.common_lock))
                t.start()
            except socket.timeout:
                pass
            except:
                self.on = False
                #todo report exception.
                break
        #todo report exit.














class JussiRXStreamSrvr:
    def __init__(self, cli_idx, sdr_rx_samplerate, cli_rx_samplerate, rx_que, rx_ringlen, output_samplering_shm_name, output_lenring_shm_name, output_trigger_socket, jussi_idx):
        assert 0 <= int(cli_idx) < 8
        assert cli_rx_samplerate / sdr_rx_samplerate <= 1.0
        assert type(jussi_idx) == int
        assert 1 <= jussi_idx < 16
        self.on = True
        self.rx_on = False
        self.idx 					= int(cli_idx)
        self.rx_resampling_ratio 	= cli_rx_samplerate / sdr_rx_samplerate
        self.do_rx_resampling 		= False
        self.rx_ringlen 			= rx_ringlen
        self.j0_rx_ring, shm0		= shm_array_from_name(name=SHM_NAME_PREFIX+"-rbr-"+str(jussi_idx), shape=(self.rx_ringlen, 1024*256), dtype=np.complex64)  #todo ensure the arrays here and in L0 are the same length.
        self.output_samplering, shm1= shm_array_from_name(name=output_samplering_shm_name, shape=(self.rx_ringlen, 1024*256), dtype=np.complex64)  #todo ensure the array is the correct size
        self.output_lenring, shm2	= shm_array_from_name(name=output_lenring_shm_name, shape=(self.rx_ringlen, ), dtype=np.int64)  #todo ensure the array is the correct size
        self.output_ring_head		= 0
        self.output_trigger_socket	= output_trigger_socket
        self.rx_que 				= rx_que
        self.rx_rs_mx1 = None
        self.rx_rs_mx2 = None
        self.shm_list = [shm0, shm1, shm2]
        if self.rx_resampling_ratio < 1.0:
            self.do_rx_resampling = True
            f_cutoff = 0.499*self.rx_resampling_ratio
            min_f_undisturbed = self.rx_resampling_ratio * 0.4  # todo parameter (can't be much bigger)
            halflen_disc = minimal_disc_halflen_for_staged_resampler(r_rate=self.rx_resampling_ratio, f_cutoff=f_cutoff, min_f_undisturbed=min_f_undisturbed, minimum_value=16, require_total_sampling=True)
            halflen_frac = minimal_frac_halflen_for_staged_resampler(halflen_div=halflen_disc, r_rate=self.rx_resampling_ratio, f_cutoff=f_cutoff, minimum_value=8)
            self.rx_rs_mx1, self.rx_rs_mx2 = create_staged_resampler(halflen_div=halflen_disc, halflen_f=halflen_frac, r_rate=self.rx_resampling_ratio, n_banks=64, f_cutoff=f_cutoff, allow_aliasing=False)
        self.loop_thread = threading.Thread(target=self._rx_loop, args=tuple(), daemon=True)

    def _rx_loop(self):
        out_arr = np.zeros(1024*512, dtype=np.complex64)
        while self.rx_on:
            try:
                ring_idx, n_samples = self.rx_que.get(timeout=0.50) # todo check the ring_idx is +1 from last one.
            except Empty:
                ring_idx, n_samples = -1, 0
            if ring_idx >= 0:
                #io2 = staged_resampler_execute_stream(in_arr=self.j0_rx_ring[ring_idx], ii0=0, nsamples=n_samples, out_arr=self.output_sampleing[self.output_ring_head], io0=0, mx1=self.rx_rs_mx1, mx2=self.rx_rs_mx2)
                #self.output_lenring[self.output_ring_head] = io2
                #self.output_trigger_socket.send(struct.pack("B", self.output_ring_head))
                #self.output_ring_head = (self.output_ring_head + 1) % self.rx_ringlen
                samples0 = self.j0_rx_ring[ring_idx, 0:n_samples] # todo .copy()?  could change during resampler execution...
                if self.do_rx_resampling:
                    io2 = staged_resampler_execute_stream(in_arr=samples0, ii0=0, nsamples=len(samples0), out_arr=out_arr, io0=0, mx1=self.rx_rs_mx1, mx2=self.rx_rs_mx2) # todo resample DIRECTLY into tgt_rx_ring!
                    samples = out_arr[0:io2]
                else:
                    samples = samples0
            else:
                samples = np.zeros(0, dtype=np.complex64)
            self.output_samplering[self.output_ring_head, 0:len(samples)] = samples
            self.output_lenring[self.output_ring_head] = len(samples)
            self.output_trigger_socket.send(struct.pack("B", self.output_ring_head))  # faster than bytes([self.output_ring_head,])
            self.output_ring_head = (self.output_ring_head + 1) % self.rx_ringlen
        for shm in self.shm_list:
            try:
                shm.close()
            except:
                pass









class JussiRXStream:
    def __init__(self, output_trigger_socket:socket.socket, rx_ringlen:int, rx_ringwidth:int, output_samplering_shm_name:str, output_lenring_shm_name:str):
        assert 1 < rx_ringlen < 256
        assert 8*1024 <= rx_ringwidth <= 8*1024*1024
        self.on = True
        self.rx_on = False
        self.do_rx_resampling 		= False
        self.rx_ringlen 			= rx_ringlen
        self.rx_ringwidth 			= rx_ringwidth
        self.output_samplering, shm0= shm_array_from_name(name=output_samplering_shm_name, shape=(self.rx_ringlen, self.rx_ringwidth), dtype=np.complex64)  #todo ensure the array is the correct size
        self.output_lenring, shm1	= shm_array_from_name(name=output_lenring_shm_name, shape=(self.rx_ringlen, ), dtype=np.int64)  #todo ensure the array is the correct size
        self.output_ring_head		= 0
        self.output_trigger_socket	= output_trigger_socket
        self.rx_que 				= Queue(128)
        self.shm_list 				= [shm0, shm1]
        self._loop_thread 			= threading.Thread(target=self._rx_loop, args=tuple(), daemon=True)
        self._loop_thread.start()


    def _rx_loop(self):
        self.output_trigger_socket.settimeout(0.25)
        previous_head = -1
        OF_reported = False
        last_rx = time.monotonic()
        while self.rx_on:
            try:
                ring_head = self.output_trigger_socket.recv(1)
            except socket.timeout:
                if (time.monotonic() - last_rx) > 3.0:
                    print("ERROR: RX-stream socket silent.")
                    self.rx_on = False
                    break
                continue
            last_rx = time.monotonic()
            if ring_head == b"": # todo throw error?
                print("ERROR: RX-stream socket closed.")
                self.rx_on = False
                break
            ring_head = ring_head[0]
            if ring_head >= self.rx_ringlen: # todo: throw error?
                print("ERROR: RX-stream received ring_head invalid.")
                self.rx_on = False
                break
            if (ring_head != ((previous_head+1)%self.rx_ringlen)) and (previous_head != -1):
                print("WARNING: RX-stream fell out of synch. Resynching.")
            l_batch = self.output_lenring[ring_head]
            if (l_batch > self.rx_ringwidth) or (l_batch < 0): #todo throw error?
                print("ERROR: RX-stream received batch length ({}) invalid.".format(l_batch))
                self.rx_on = False
                break
            batch = self.output_samplering[0:l_batch]
            if l_batch > 0:
                if not self.rx_que.full():
                    self.rx_que.put_nowait(batch)
                    OF_reported = False
                elif not OF_reported:
                    print("WARNING: RX-stream reception queue overflow.")
                    OF_reported = True
            previous_head = ring_head
        for shm in self.shm_list:
            try:
                shm.close()
            except:
                pass


    def receive(self, timeout):
        return self.rx_que.get(timeout=timeout)














class JussiTXStreamSrvr:
    def __init__(self, cli_idx, sdr_tx_samplerate, cli_tx_samplerate, tx_trig_event, tx_ringlen, tx_batch_maxlen, input_shm_name, input_ringlen, tx_trigger_socket, jussi_idx):
        assert 0 <= int(cli_idx) < 8
        assert sdr_tx_samplerate / cli_tx_samplerate >= 1.0
        assert type(jussi_idx) == int
        assert 1 <= jussi_idx < 16
        self.tx_on = False
        self.idx = int(cli_idx)
        self.tx_resampling_ratio 	= sdr_tx_samplerate / cli_tx_samplerate
        self.do_tx_resampling 		= False
        self.tx_ringlen 			= tx_ringlen
        self.tx_batch_maxlen		= tx_batch_maxlen
        self.input_ringlen			= input_ringlen
        self.trig_socket 			= tx_trigger_socket
        self.input_samplering, shm0	= shm_array_from_name(name=input_shm_name, shape=(self.input_ringlen, 1024 * 256), dtype=np.complex64)
        self.input_lenring, shm1	= shm_array_from_name(name=input_shm_name + "-la", shape=self.input_ringlen, dtype=np.int64)
        self.tx_buffer_table,shm2 	= shm_array_from_name(name=SHM_NAME_PREFIX+"-btb"+str(jussi_idx), shape=(MAX_CLI_COUNT, self.tx_ringlen, self.tx_batch_maxlen), dtype=np.complex64)
        self.tx_toggle_table,shm3 	= shm_array_from_name(name=SHM_NAME_PREFIX+"-ttb"+str(jussi_idx), shape=(MAX_CLI_COUNT, self.tx_ringlen), dtype=np.int8)
        self.tx_length_table,shm4 	= shm_array_from_name(name=SHM_NAME_PREFIX+"-ltb"+str(jussi_idx), shape=(MAX_CLI_COUNT, self.tx_ringlen), dtype=np.int64)
        self.tx_ring_head			= 0
        self.tx_trig_event			= tx_trig_event
        self.shm_list = [shm0,shm1,shm2,shm3,shm4]
        self.upsplr_scalars = None
        self.upsplr_taps = None
        self.upsplr_window = None
        if self.tx_resampling_ratio > 1.0:
            self.do_tx_resampling = True
            r_ = create_upsample_taps(r_rate=self.tx_resampling_ratio, dtype=np.complex64, m_halflen=9, L_limit=14, up_cutoff=0.499)
            scalar_arr, taps_up, window, delay_estimate = r_
            self.upsplr_scalars = scalar_arr # i
            self.upsplr_taps = taps_up # f/cf
            self.upsplr_window = window # f


    def tx_loop(self):
        input_head = -1
        self.trig_socket.settimeout(0.25)
        while self.tx_on:
            try:
                input_head_ = self.trig_socket.recv(1)
            except:
                continue
            if input_head_ == b"":
                self.tx_on = False # todo errorprint. Socket closed.
                break
            input_head_ = input_head_[0]
            if (input_head_ != input_head) and (input_head >= 0):
                self.tx_on = False # todo errorprint. Out of synch.
                break
            #if input_head_ >= self.input_ringlen:
            #	self.tx_on = False # todo errorprint. Invalid (too large) head index.
            #	break
            l = self.input_lenring[input_head_]
            tx_samples0 = self.input_samplering[input_head_][0:l]
            input_head = (input_head_ + 1) % self.input_ringlen

            if self.do_tx_resampling:
                r_ = upsample(samples=tx_samples0, scalar_arr=self.upsplr_scalars, taps_up=self.upsplr_taps, window=self.upsplr_window)
                tx_samples, self.txrs_v1, self.txrs_v2, self.txrs_v3, self.txrs_v4, _ = r_  # interp, window, window_head, x_minus1, y_minus1, 0.0
            else:
                tx_samples = tx_samples0
            head_ = ring_buffer_insertion(samples=tx_samples, idx=self.idx, ring_head=self.tx_ring_head, buffer_table=self.tx_buffer_table, length_table=self.tx_length_table, toggle_table=self.tx_toggle_table, insertion_trig_event=self.tx_trig_event)
            if head_ < 0: # wait limit exceeded. SDR sink inactive?
                self.tx_on = False # todo errorprint
                break
            self.tx_ring_head = head_
        for shm in self.shm_list:
            try:
                shm.close()
            except:
                pass












def tsttool_fill_ring_buffers(idx, h0, nsamples, buffer_table, length_table, toggle_table):
    stuff = np.complex64(radionoise(n = nsamples, sr=1, W_per_Hz=1.0))
    write_cursor = 0
    head = h0
    ringlen = buffer_table.shape[1]
    batch_maxlen = buffer_table.shape[2]
    while write_cursor < nsamples:
        n_write = min(batch_maxlen, nsamples - write_cursor)
        buffer_table[idx,head,0:n_write] = stuff[write_cursor:write_cursor+n_write]
        length_table[idx,head] = n_write
        toggle_table[idx,head] = 1
        head = (head+1) % ringlen
        write_cursor += n_write

def zero_tables(buffer_table, length_table, toggle_table):
    buffer_table[:,:,:] = 0.0
    length_table[:,:] = 0
    toggle_table[:,:] = 0



def tst_ring_extraction_speed():
    ringlen 		= 16
    batch_maxlen 	= 1024 * 8
    n_active_sources= MAX_CLI_COUNT
    buffer_table  	= np.zeros((MAX_CLI_COUNT,ringlen,batch_maxlen), dtype=np.complex64)	# todo make shared memory
    toggle_table 	= np.zeros((MAX_CLI_COUNT,ringlen), dtype=np.int8) 							# todo make shared memory
    length_table  	= np.zeros((MAX_CLI_COUNT,ringlen), dtype=np.int64) 							# todo make shared memory
    local_head_table= np.zeros(MAX_CLI_COUNT, dtype=np.int64)

    print("Compiling.")
    for _ in range(3):
        tsttool_fill_ring_buffers(idx=0, h0=0, nsamples=int(batch_maxlen*9.5), buffer_table=buffer_table, length_table=length_table, toggle_table=toggle_table)
        tsttool_fill_ring_buffers(idx=1, h0=1, nsamples=int(batch_maxlen*1.5), buffer_table=buffer_table, length_table=length_table, toggle_table=toggle_table)
        txbuff = ring_buffer_extraction(buffer_table=buffer_table, toggle_table=toggle_table, length_table=length_table, local_head_table=local_head_table, n_sources=n_active_sources)
        txbuff = ring_buffer_extraction(buffer_table=buffer_table, toggle_table=toggle_table, length_table=length_table, local_head_table=local_head_table, n_sources=n_active_sources)
        txbuff = ring_buffer_extraction(buffer_table=buffer_table, toggle_table=toggle_table, length_table=length_table, local_head_table=local_head_table, n_sources=n_active_sources)
        zero_tables(buffer_table=buffer_table, length_table=length_table, toggle_table=toggle_table)

    print("Initializing.")
    tsttool_fill_ring_buffers(idx=0, h0=0, nsamples=int(batch_maxlen*9.5), buffer_table=buffer_table, length_table=length_table, toggle_table=toggle_table)
    tsttool_fill_ring_buffers(idx=1, h0=1, nsamples=int(batch_maxlen*2.5), buffer_table=buffer_table, length_table=length_table, toggle_table=toggle_table)

    print("Timing.")
    t0 = time.perf_counter()
    for _ in range(10):
        txbuff = ring_buffer_extraction(buffer_table=buffer_table, toggle_table=toggle_table, length_table=length_table, local_head_table=local_head_table, n_sources=n_active_sources)
    dt = (time.perf_counter() - t0) / 10
    call_speed = 1 / dt
    sample_speed = call_speed * batch_maxlen

    print("Done.")
    print("call time:		{} µs/calls".format( round(1e6*dt, 2) ))
    print("call speed:		{} kcalls/s".format( round(1e-3*call_speed, 2) ))
    print("sample speed:	{} MS/s".format( round(1e-6*sample_speed, 2) ))
    print("")









if __name__ == '__main__':
    tst_ring_extraction_speed()
