import time
import uhd
import numpy as np
import threading
from queue import Queue, Empty
import SoapySDR
from SoapySDR import SOAPY_SDR_RX, SOAPY_SDR_TX, SOAPY_SDR_CF32
#from kuokka.lib_tools import radionoise, make_samples2
from .lib_tools import make_samples2, radionoise, DebugPrinter

"""
try:
	from mtools.zmq_printout import ZMQPIn
	ZMQP_PRINTER = ZMQPIn(port=11001, hostname="localhost")
except:
	ZMQP_PRINTER = None

DEBUG_PRINT_ON = True
def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[RadioLoop]" + " "
	first, args = args[0], args[1:]
	if not DEBUG_PRINT_ON:
		return
	if not (ZMQP_PRINTER is None):
		ZMQP_PRINTER.print(ts+str(first))
	else:
		print(ts+str(first), *args, **kwargs)
"""




class RadioConfig:
	def __init__(self, mode, rx_sr, rx_f_tune, tx_sr, tx_f_tune, rx_gain, tx_gain):
		assert mode in ("usrp", "soapy")
		self.mode 			= mode
		self.rx_sr0 		= rx_sr
		self.rx_f_tune 		= rx_f_tune
		self.tx_sr0 		= tx_sr
		self.tx_f_tune 		= tx_f_tune
		self.rx_gain		= rx_gain
		self.tx_gain		= tx_gain





class RadioLoop:
	def __init__(self, radio_config:RadioConfig, que_tx_samples_in:Queue, que_rx_samples_out:Queue):
		self.radio_config 			= radio_config
		self.que_tx_samples_in 		= que_tx_samples_in # samples of packets
		self.que_rx_samples_out 	= que_rx_samples_out
		self.on 				= True
		self.tx_ready			= threading.Event()
		self.rx_thread 			= threading.Thread(target=None, args=tuple())
		self.tx_thread 			= threading.Thread(target=None, args=tuple())
		self.sample_maxamps 	= np.zeros(3, np.float64)
		self.dbgprinter 		= DebugPrinter(log_title="RadioLoop", stdprint=True, zmqprint_host_port=("localhost", 11001))
		self.DBGPRINT 			= self.dbgprinter.DBGPRINT
		self.rx_print_interval	= 20.0

	def is_ok(self):
		if not self.on:
			return False
		for thrd in (self.tx_thread, self.rx_thread):
			if not thrd.is_alive():
				return False
		return True

	def close(self):
		self.on = False
		self.rx_thread.join(timeout=1.0)
		self.tx_thread.join(timeout=1.0)

	def start(self):
		assert self.radio_config.mode in ("usrp", "soapy")
		if self.radio_config.mode == "usrp":
			self._usrp_start()
		else:
			self._soapy_start()

	def sim_start(self, noiseSPD, ts_pl_list, rx_samplearr_que, tx_sample_que):
		self._sim_start(noiseSPD=noiseSPD, ts_pl_list=ts_pl_list, rx_samplearr_que=rx_samplearr_que, tx_sample_que=tx_sample_que)



	# === USRP ===============================================================================================================================================================================
	# === USRP ===============================================================================================================================================================================
	def _usrp_start(self):
		# This noise injection enforces the jit-compilation of much of the signal processing pipeline before the loop starts.
		self.DBGPRINT("USRP start")
		usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
		usrp.set_rx_rate(self.radio_config.rx_sr0, 0)
		usrp.set_tx_rate(self.radio_config.tx_sr0, 0)
		usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(self.radio_config.rx_f_tune), 0)
		usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(self.radio_config.tx_f_tune), 0)
		usrp.set_rx_gain(self.radio_config.rx_gain, 0)
		usrp.set_tx_gain(self.radio_config.tx_gain, 0)
		usrp.set_gpio_attr("FP0", "ATR_TX", 0x0100, 0x0100)
		usrp.set_gpio_attr("FP0", "ATR_XX", 0x0100, 0x0100)
		time.sleep(0.1)
		#print("bank 0:", usrp.get_gpio_banks(0)) #['FP0', 'RXA', 'TXA']  (No further banks in B210)
		#print("FP0 CTRL", usrp.get_gpio_attr("FP0", "CTRL"))
		#print("FP0 DDR", usrp.get_gpio_attr("FP0", "DDR"))
		#print("FP0 OUT", usrp.get_gpio_attr("FP0", "OUT"))
		#print("FP0 ATR_0X", usrp.get_gpio_attr("FP0", "ATR_0X"))
		#print("FP0 ATR_RX", usrp.get_gpio_attr("FP0", "ATR_RX"))
		#print("FP0 ATR_TX", usrp.get_gpio_attr("FP0", "ATR_TX"))
		#print("FP0 ATR_XX", usrp.get_gpio_attr("FP0", "ATR_XX"))
		#print(usrp.set_gpio_src("FP0", "RX"))
		self.rx_sr0_actual = float(usrp.get_rx_freq(0))
		self.tx_sr0_actual = float(usrp.get_tx_freq(0))
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
		self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    args=(usrp, 1024*2), daemon=True) #TODO bufferlen as setting?
		self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    args=(usrp, 1024*2), daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.tx_thread.start()


	def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP, rx_buffer_len):
		# Set up the stream and receive buffer
		st_args = uhd.usrp.StreamArgs("fc32", "sc16")
		st_args.channels = [0]
		#print("cpu format",st_args.cpu_format)
		#print("otw format",st_args.otw_format)
		#print("args",st_args.args)
		rx_streamer = usrp.get_rx_stream(st_args)
		# Start Stream
		stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
		stream_cmd.stream_now = True
		metadata = uhd.types.RXMetadata()
		recv_buffer 			= np.zeros((1, rx_buffer_len), dtype=np.complex64)
		n_rx_loops 				= 0
		n_rx_total 				= 0
		avg_sr 					= 0.0
		dt_amplitude_measure 	= 0.0
		maxreset_interval 		= 30.0
		t_next_print    		= time.monotonic()
		t_next_maxreset 		= time.monotonic() + maxreset_interval
		t00 = time.perf_counter()
		rx_streamer.issue_stream_cmd(stream_cmd)
		t00 = (t00+time.perf_counter())/2.0
		while self.on:
			ts_mono = time.monotonic()
			if ts_mono >= t_next_print:
				ampmax_int_time = round(ts_mono - (t_next_print - maxreset_interval), 1)
				self.DBGPRINT("(sr~{} MS/s measured vs {} MS/s specced). component-max:{}, avg-amplitude:{} ({}s)".format( round(1e-6*avg_sr, 5), round(1e-6*self.radio_config.rx_sr0, 5), self.sample_maxamps[0], self.sample_maxamps[1], ampmax_int_time) )
				#amp_measure_percentage = 100 * dt_amplitude_measure/(time.perf_counter()-t00)
				#DBGPRINT("t_ampl_measure: {} ms.  ({} % of total).".format( round(dt_amplitude_measure*1e3,3), round(amp_measure_percentage, 3) ))
				t_next_print = ts_mono + self.rx_print_interval
			if ts_mono >= t_next_maxreset:
				self.sample_maxamps[0] = 0.0
				t_next_maxreset = ts_mono + maxreset_interval
			ts_s0_mono = ts_mono
			ts_s0_unix = time.time()
			rx_ret = rx_streamer.recv(recv_buffer, metadata) #blocking until rx_buffer_len samples acquired
			avg_sr = n_rx_total / (time.perf_counter() - t00)
			n_rx_total += rx_ret
			if rx_ret != rx_buffer_len:
				self.DBGPRINT("WARNING: RECV RETURNED NON-FULL BUFFER WITH RET VALUE "+str(rx_ret))
				#assert rx_ret == rx_buffer_len
			buff_ = recv_buffer[0, :rx_ret].copy()
			t00_ = time.perf_counter()
			self.sample_maxamps[0] = np.max( (self.sample_maxamps[0], np.max(np.abs(buff_.real))) )
			self.sample_maxamps[1] = self.sample_maxamps[1] +  (np.average( np.abs(buff_[0:32]) ) - self.sample_maxamps[1]) * 0.1
			dt_amplitude_measure += (time.perf_counter() - t00_)
			if not self.que_rx_samples_out.full():
				self.que_rx_samples_out.put_nowait((buff_, ts_s0_mono, ts_s0_unix))
			else:
				self.DBGPRINT("WARNING: radio-to-process queue overflow!")
				raise Exception("radio-loop: radio-to-process queue overflow.")
			n_rx_loops += 1


	def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP, tx_batch_len):
		tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
		# stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
		tx_stream_args.channels = [0]
		tx_streamer = usrp.get_tx_stream(tx_stream_args)
		while self.on:
			self.tx_ready.set()
			try:
				samplearr = self.que_tx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				self.DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break

			if not (samplearr.dtype == np.complex64):
				if samplearr.dtype == np.complex128:
					self.DBGPRINT("TX SAMPLES OF WRONG DTYPE: {}. SHOULD BE np.complex64. Converting and transmitting.".format( str(samplearr.dtype) ))
					samplearr = np.astype(samplearr, np.complex64)
				else:
					self.DBGPRINT("TX SAMPLES OF WRONG DTYPE: {}. SHOULD BE np.complex64. No transmission.".format( str(samplearr.dtype) ))
					continue

			if not (samplearr.shape[0] == 1) and (len(samplearr.shape) == 2):
				self.DBGPRINT("TX SAMPLES IN WRONG SHAPE: {}. SHOULD BE (1,n). No transmission.".format( str(samplearr.shape) ) )
				continue
			self.tx_ready.clear()
			N = samplearr.shape[1]
			dtt = N / self.radio_config.tx_sr0
			idx = 0
			t_end = time.perf_counter() + dtt
			tx_metadata = uhd.types.TXMetadata()
			tx_metadata.start_of_burst = True
			tx_metadata.end_of_burst = False
			while idx < N:
				if (N - idx) <= tx_batch_len:
					tx_metadata.end_of_burst = True
				tx_streamer.send(samplearr[0,idx:idx+tx_batch_len], tx_metadata)
				tx_metadata.start_of_burst = False
				idx += tx_batch_len
			t_to_end = max(0, t_end -time.perf_counter() -10e-3)
			time.sleep(t_to_end)
			self.DBGPRINT("tx end. sleep of {}/{} ms.".format( round(t_to_end*1e3, 2), round(dtt*1e3, 2) ))
	# === USRP ===============================================================================================================================================================================
	# === USRP ===============================================================================================================================================================================



	# === Soapy ==============================================================================================================================================================================
	# === Soapy ==============================================================================================================================================================================
	def _soapy_start(self):
		"""
		This start method will be called when used on the ground station machine.
		The Soapy-code is incomplete, and will certainly not work yet. You will need to attach to a 'leecher' device created by the soapy-shared library.
		"""
		self.DBGPRINT("SoapySDR start")
		args = dict(device="uhd")
		sdr = SoapySDR.Device(args) #args
		SoapySDR.setLogLevel(SoapySDR.SOAPY_SDR_FATAL)
		if type(sdr) == tuple:
			sdr = sdr[0]
		print(sdr)
		self.DBGPRINT("SoapySDR driver key: ", sdr.getDriverKey())
		self.DBGPRINT("SoapySDR driver key: ", sdr.getHardwareKey())
		self.DBGPRINT("Assuming we are on a SoapyShared leecher device.")
		self.DBGPRINT("Radio parameters can not be changed, instead we config to what we believe they are.")
		self.DBGPRINT("Assuming:  f-tune = {} MHz".format(self.radio_config.rx_f_tune))
		self.DBGPRINT("Assuming:  	 sr > {} MS/s".format(self.radio_config.rx_sr0))
		sdr.setSampleRate(SOAPY_SDR_RX, 0, self.radio_config.rx_sr0)
		sdr.setSampleRate(SOAPY_SDR_TX, 0, self.radio_config.tx_sr0)
		sdr.setFrequency(SOAPY_SDR_RX, 0, self.radio_config.rx_f_tune)
		sdr.setFrequency(SOAPY_SDR_TX, 0, self.radio_config.tx_f_tune)
		sdr.setGain(SOAPY_SDR_RX, 0, self.radio_config.rx_gain)
		sdr.setGain(SOAPY_SDR_TX, 0, self.radio_config.tx_gain)
		self.DBGPRINT("RX gain range:      {}".format( sdr.getGainRange(SOAPY_SDR_RX, 0) ))
		self.DBGPRINT("TX gain range:      {}".format( sdr.getGainRange(SOAPY_SDR_TX, 0) ))
		self.DBGPRINT("RX gain:            {}".format( sdr.getGain(SOAPY_SDR_RX, 0) ))
		self.DBGPRINT("TX gain:            {}".format( sdr.getGain(SOAPY_SDR_TX, 0) ))
		self.rx_thread			= threading.Thread(target=self._soapy_rx_loop,   args=(sdr, 1024*4), daemon=True)
		self.tx_thread 			= threading.Thread(target=self._soapy_tx_loop,   args=(sdr, 1024*4), daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.tx_thread.start()


	def _soapy_rx_loop(self, sdr:SoapySDR.Device, bufferlen):
		rxStream = sdr.setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32)
		rx_call_timeout = int(1e6 * bufferlen * 0.8 / self.radio_config.rx_sr0)
		recv_buffer 			= np.zeros(int(2**21), np.complex64)
		n_rx_loops 				= 0
		n_rx_total 				= 0
		avg_sr 					= 0.0
		dt_amplitude_measure 	= 0.0
		maxreset_interval 		= 30.0
		t_next_print    		= time.monotonic()
		t_next_maxreset 		= time.monotonic() + maxreset_interval
		absolute_bufflen 		= len(recv_buffer)
		t00 = time.perf_counter()
		sdr.activateStream(rxStream)
		t00 = (t00+time.perf_counter())/2.0
		while self.on:
			ts_mono = time.monotonic()
			if ts_mono >= t_next_print:
				ampmax_int_time = round(ts_mono - (t_next_maxreset - maxreset_interval), 1)
				#self.DBGPRINT("(rx-#{}) (sr~{} MS/s) (vs {} MS/s)".format(n_rx_loops, round(1e-6*avg_sr, 4), round(1e-6*self.radio_config.rx_sr0, 5)))
				self.DBGPRINT("(sr~{} MS/s measured vs {} MS/s specced). component-max:{}, avg-amplitude:{} ({}s)".format( round(1e-6*avg_sr, 5), round(1e-6*self.radio_config.rx_sr0, 5), self.sample_maxamps[0], self.sample_maxamps[1], ampmax_int_time) )
				t_next_print = ts_mono + self.rx_print_interval
			if ts_mono >= t_next_maxreset:
				self.sample_maxamps[0] = 0.0
				t_next_maxreset = ts_mono + maxreset_interval
			ts_s0_mono = ts_mono
			ts_s0_unix = time.time()
			ret = sdr.readStream(rxStream, [recv_buffer], numElems=absolute_bufflen, timeoutUs=rx_call_timeout)
			rx_ret = ret.ret
			n_rx_total += rx_ret
			avg_sr = n_rx_total / (time.perf_counter() - t00)

			buff_ = recv_buffer[:rx_ret].copy()
			t00_ = time.perf_counter()
			self.sample_maxamps[0] = np.max( (self.sample_maxamps[0], np.max(np.abs(buff_.real))) )
			self.sample_maxamps[1] = self.sample_maxamps[1] +  (np.average( np.abs(buff_[0:32]) ) - self.sample_maxamps[1]) * 0.1
			dt_amplitude_measure += (time.perf_counter() - t00_)
			#print(ret.ret) #num samples or error code
			#print(ret.flags) #flags set by receive operation
			#print(ret.timeNs) #timestamp for receive buffer
			#if rx_ret != bufferlen:
				#DBGPRINT("RECV RETURNED NON-FULL BUFFER WITH RET VALUE "+str(rx_ret))
				#assert rx_ret == rx_buffer_len
			if not self.que_rx_samples_out.full():
				self.que_rx_samples_out.put_nowait((recv_buffer[:rx_ret].copy(), ts_s0_mono, ts_s0_unix))
			else:
				self.DBGPRINT("WARNING: radio-to-process queue overflow!")
				raise Exception("radio-loop: radio-to-process queue overflow.")
			n_rx_loops += 1
		sdr.deactivateStream(rxStream) #stop streaming
		sdr.closeStream(rxStream)


	def _soapy_tx_loop(self, sdr:SoapySDR.Device, batchlen):
		txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
		sdr.activateStream(txStream)
		while self.on:
			self.tx_ready.set()
			try:
				samplearr = self.que_tx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				self.DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
			self.tx_ready.clear()
			N = samplearr.shape[1]
			samplearr = samplearr[0]
			assert len(samplearr) == N
			assert N > 1
			dtt = N / self.radio_config.tx_sr0
			idx = 0
			t_end = time.perf_counter() + dtt
			while idx < N:
				blen = min(batchlen, N - idx)
				if idx < (N-batchlen):
					sdr.writeStream(txStream, [samplearr[idx:idx+blen]], blen, timeoutUs=1000000)
				else:
					sdr.writeStream(txStream, [samplearr[idx:idx+blen]], blen, timeoutUs=1000000, flags=SoapySDR.SOAPY_SDR_END_BURST)
				idx += batchlen
			#sdr.deactivateStream(txStream)
			#sdr.closeStream(txStream)
			#sdr.closeStream()
			t_to_end = max(0, t_end -time.perf_counter() -10e-3)
			time.sleep(t_to_end)
			self.DBGPRINT("tx end. sleep of {}/{} ms.".format( round(t_to_end*1e3, 2), round(dtt*1e3, 2) ))
	# === Soapy ==============================================================================================================================================================================
	# === Soapy ==============================================================================================================================================================================





	# === SIM ================================================================================================================================================================================
	# === SIM ================================================================================================================================================================================
	def _sim_start(self, noiseSPD, ts_pl_list, rx_samplearr_que, tx_sample_que):
		self.DBGPRINT("Sim start")
		#self.radio_config.rx_sr0 	= sr0
		#self.radio_config.tx_sr0 	= sr0
		self.radio_config.rx_f_tune = 437e6
		self.radio_config.tx_f_tune = 437e6
		self.rx_thread = threading.Thread(target=self._sim_rx_loop,  args=(noiseSPD, ts_pl_list, rx_samplearr_que),  daemon=True) #TODO bufferlen as setting?
		self.tx_thread = threading.Thread(target=self._sim_tx_loop,  args=(tx_sample_que,),  daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.tx_thread.start()


	def _sim_rx_loop(self, noiseSPD, ts_pl_list, samplearr_que):
		time.sleep(2)
		n_received = 0
		t0 = time.perf_counter()
		t_sleep = 0.0
		batchlen = 1024*2
		ts_pl_list = sorted(ts_pl_list, key=lambda x_: x_[0])
		ts_pl_list = [[x[0]+t0,x[1]] for x in ts_pl_list]
		pl_head = 0
		transmission_dict = dict()
		while self.on:
			time.sleep(t_sleep)
			ts_s0_mono = time.monotonic()
			ts_s0_unix = time.time()
			batch = radionoise(batchlen, sr=self.radio_config.rx_sr0, W_per_Hz=noiseSPD)

			while (pl_head < len(ts_pl_list)) and ((ts_s0_mono + batchlen/self.radio_config.rx_sr0) > ts_pl_list[pl_head][0]):
				bits, baudrate, f_abs, power, mod_idx = ts_pl_list[pl_head][1]
				sps = self.radio_config.rx_sr0 / baudrate
				f_offset_rel = (f_abs - self.radio_config.rx_f_tune) / self.radio_config.rx_sr0
				transmission = make_samples2(sps_f=sps, bitstring=bits, f_offset=f_offset_rel, power=power, mod_index=mod_idx, shaper_BT_prod=0.5, n_silence_start=0, n_silence_end=0)
				i_start = int((ts_pl_list[pl_head][0]-t0) * self.radio_config.rx_sr0)
				transmission_dict[pl_head] = transmission, i_start
				pl_head += 1

			while not samplearr_que.empty():
				transmission = samplearr_que.get_nowait()
				i_start = n_received + 10
				transmission_dict[np.random.randint(0,int(1e12))] = transmission, i_start

			for k in transmission_dict.keys():
				transmission, i_start = transmission_dict[k]
				i0_batch 	= np.clip(i_start - n_received, 0, batchlen)
				i0_tx 		= np.clip(n_received - i_start, 0, len(transmission))
				i_end_batch = np.clip(i0_batch + len(transmission)-i0_tx, 0, batchlen)
				i_end_tx 	= np.clip(i0_tx + batchlen-i0_batch, 0, len(transmission))
				if (i0_batch == i_end_batch) and (i0_batch == 0):
					del transmission_dict[k]
				else:
					batch[i0_batch:i_end_batch] += transmission[i0_tx,i_end_tx]

			if not self.que_rx_samples_out.full():
				self.que_rx_samples_out.put_nowait((batch, ts_s0_mono, ts_s0_unix))
			else:
				self.DBGPRINT("WARNING! radio-to-process queue overflow!  {}".format( 1e-6 * n_received / (time.perf_counter() - t0) ))
			n_received += batchlen
			t_next = t0 + (n_received / self.radio_config.rx_sr0)
			t_sleep = max(0, t_next - time.perf_counter())


	def _sim_tx_loop(self, tx_samples_out_que):
		while self.on:
			try:
				samplearr = self.que_tx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				self.DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
			samplearr = samplearr[0]
			assert len(samplearr) > 100
			assert len(samplearr.shape) == 2
			assert samplearr.shape[0] == 1
			assert samplearr.shape[1] > 100
			samplearr = samplearr[0]
			if tx_samples_out_que:
				tx_samples_out_que.put(samplearr, timeout=1.0)
	# === SIM ================================================================================================================================================================================
	# === SIM ================================================================================================================================================================================










