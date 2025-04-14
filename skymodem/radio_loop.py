import time
import uhd
import numpy as np
import threading
from queue import Queue, Empty
import SoapySDR
from SoapySDR import SOAPY_SDR_RX, SOAPY_SDR_TX, SOAPY_SDR_CF32
from datetime import datetime as dtime


DEBUG_PRINT_ON = True
def DBGPRINT(*args, **kwargs):
	ts = "[{}]".format( dtime.now().isoformat()[-15:] )
	ts += " "*(17-len(ts)) + "[RadioLoop]" + " "
	first, args = args[0], args[1:]
	if DEBUG_PRINT_ON:
		print(ts+str(first), *args, **kwargs)




class RadioConfig:
	def __init__(self, mode, rx_sr, rx_f_tune, rx_f_center, tx_sr, tx_f_tune, tx_f_center):
		assert mode in ("usrp", "soapy")
		self.mode 			= mode
		self.rx_sr0 		= rx_sr
		self.rx_f_tune 		= rx_f_tune
		self.rx_f_center 	= rx_f_center
		self.tx_sr0 		= tx_sr
		self.tx_f_tune 		= tx_f_tune
		self.tx_f_center 	= tx_f_center





class RadioLoop:
	def __init__(self, radio_config:RadioConfig, que_tx_samples_in:Queue, que_rx_samples_out:Queue):
		self.radio_config 			= radio_config
		self.que_tx_samples_in 		= que_tx_samples_in # samples of packets
		self.que_rx_samples_out 	= que_rx_samples_out
		self.on 				= True
		self.rx_thread 			= threading.Thread(target=None, args=tuple())
		self.tx_thread 			= threading.Thread(target=None, args=tuple())
		self.self_mute 			= False


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


	def _soapy_start(self):
		"""
		This start method will be called when used on the ground station machine.
		The Soapy-code is incomplete, and will certainly not work yet. You will need to attach to a 'leecher' device created by the soapy-shared library.
		"""
		DBGPRINT("SoapySDR start")
		args = dict(device="uhd")
		sdr = SoapySDR.Device(args) #args
		if type(sdr) == tuple:
			sdr = sdr[0]
		print(sdr)
		DBGPRINT("SoapySDR driver key: ", sdr.getDriverKey())
		DBGPRINT("SoapySDR driver key: ", sdr.getHardwareKey())
		DBGPRINT("Assuming we are on a SoapyShared leecher device.")
		DBGPRINT("Radio parameters can not be changed, instead we config to what we believe they are.")
		DBGPRINT("Assuming:  f-tune = {} MHz".format(self.radio_config.rx_f_tune))
		DBGPRINT("Assuming:  	 sr > {} MS/s".format(self.radio_config.rx_sr0))
		sdr.setSampleRate(SOAPY_SDR_RX, 0, self.radio_config.rx_sr0)
		sdr.setSampleRate(SOAPY_SDR_TX, 0, self.radio_config.tx_sr0)
		sdr.setFrequency(SOAPY_SDR_RX, 0, self.radio_config.rx_f_tune)
		sdr.setFrequency(SOAPY_SDR_TX, 0, self.radio_config.tx_f_tune)
		sdr.setGain(SOAPY_SDR_RX, 0, 56)
		sdr.setGain(SOAPY_SDR_TX, 0, 56)
		DBGPRINT("RX gain range:      {}".format( sdr.getGainRange(SOAPY_SDR_RX, 0) ))
		DBGPRINT("TX gain range:      {}".format( sdr.getGainRange(SOAPY_SDR_TX, 0) ))
		DBGPRINT("RX gain:            {}".format( sdr.getGain(SOAPY_SDR_RX, 0) ))
		DBGPRINT("TX gain:            {}".format( sdr.getGain(SOAPY_SDR_TX, 0) ))
		self.rx_thread			= threading.Thread(target=self._soapy_rx_loop,   args=(sdr, 1024*4), daemon=True)
		self.tx_thread 			= threading.Thread(target=self._soapy_tx_loop,   args=(sdr, 1024*4), daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.tx_thread.start()


	def _usrp_start(self):
		# This noise injection enforces the jit-compilation of much of the signal processing pipeline before the loop starts.
		DBGPRINT("USRP start")
		gain = 56 # dB
		usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
		usrp.set_rx_rate(self.radio_config.rx_sr0, 0)
		usrp.set_tx_rate(self.radio_config.tx_sr0, 0)
		usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(self.radio_config.rx_f_tune), 0)
		usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(self.radio_config.tx_f_tune), 0)
		usrp.set_rx_gain(gain, 0)
		usrp.set_tx_gain(gain, 0)
		DBGPRINT("RX gain range:      {}".format( str(usrp.get_rx_gain_range(0))[:-1] ))
		DBGPRINT("TX gain range:      {}".format( str(usrp.get_tx_gain_range(0))[:-1] ))
		DBGPRINT("usrp RX gain:       {}".format( usrp.get_rx_gain(0) ))
		DBGPRINT("usrp TX gain:       {}".format( usrp.get_tx_gain(0) ))
		DBGPRINT("usrp RX samplerate: {} ksps".format( round(usrp.get_rx_rate(0)*1e-3, 3) ))
		DBGPRINT("usrp TX samplerate: {} ksps".format( round(usrp.get_tx_rate(0)*1e-3, 3) ))
		DBGPRINT("usrp RX tune-f:     {} MHz".format( round(usrp.get_rx_freq(0)*1e-6, 3) ))
		DBGPRINT("usrp TX tune-f:     {} MHz".format( round(usrp.get_tx_freq(0)*1e-6, 3) ))
		self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    args=(usrp, 1024*2), daemon=True) #TODO bufferlen as setting?
		self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    args=(usrp, 1024*2), daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.tx_thread.start()


	def recording_start(self, fpath=None, sr0=None, fcenter0=None):
		DBGPRINT("Recording start")
		import pickle
		if fpath is None:
			fpath, fcenter0, sr0 = ("/home/elmore/datasetit/radiotallenteet/uhf-965_437.0MHz-1000ksps.pickled",-124.0e3, 1e6)
		f = open(fpath, "rb")
		rd = f.read()
		f.close()
		samples = pickle.loads(rd)
		samples = samples * np.exp(2j*np.pi * np.arange(len(samples)) * (1/sr0) * (fcenter0+25e3))
		assert len(samples.shape) == 1
		assert type(samples) == np.ndarray
		samples = np.complex64(samples)
		self.radio_config.rx_sr0 	= sr0
		self.radio_config.tx_sr0 	= sr0
		self.radio_config.rx_f_tune = 437e6
		self.radio_config.tx_f_tune = 437e6
		self.radio_config.rx_f_center = self.radio_config.rx_f_tune + 25e3
		self.radio_config.tx_f_center = self.radio_config.tx_f_tune + 25e3
		self.rx_thread 			= threading.Thread(target=self._recording_rx_loop,  args=(samples, sr0), daemon=True) #TODO bufferlen as setting?
		self.tx_thread 			= threading.Thread(target=self._recording_tx_loop,  args=tuple(),        daemon=True) #TODO bufferlen as setting?
		self.on = True
		self.rx_thread.start()
		self.tx_thread.start()




	# === RECORDING ==========================================================================================================================================================================
	# === RECORDING ==========================================================================================================================================================================
	def _recording_rx_loop(self, samples):
		time.sleep(2)
		nsamples = len(samples)
		cursor = 0
		n_received = 0
		t0 = time.perf_counter()
		t_sleep = 0.0
		default_batchlen = 1024*2
		while self.on:
			time.sleep(t_sleep)
			batchlen = min(default_batchlen, nsamples-cursor )
			batch = samples[cursor:cursor+batchlen]
			assert len(batch) == batchlen
			cursor += batchlen
			if cursor >= nsamples:
				cursor = 0
				DBGPRINT("Recordning cursor zeroed.")
			if not self.que_rx_samples_out.full():
				self.que_rx_samples_out.put_nowait(batch)
			else:
				DBGPRINT("WARNING! radio-to-process queue overflow!  {}".format( 1e-6 * n_received / (time.perf_counter() - t0) ))
			n_received += batchlen
			t_next = t0 + ((n_received + batchlen) / self.radio_config.rx_sr0)
			t_sleep = max(0, t_next - time.perf_counter())


	def _recording_tx_loop(self):
		while self.on:
			try:
				_ = self.que_tx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
	# === RECORDING ==========================================================================================================================================================================
	# === RECORDING ==========================================================================================================================================================================




	# === USRP ===============================================================================================================================================================================
	# === USRP ===============================================================================================================================================================================
	def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP, rx_buffer_len):
		# Set up the stream and receive buffer
		st_args = uhd.usrp.StreamArgs("fc32", "sc16")
		st_args.channels = [0]
		rx_streamer = usrp.get_rx_stream(st_args)
		# Start Stream
		stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
		stream_cmd.stream_now = True
		recv_buffer = np.zeros((1, rx_buffer_len), dtype=np.complex64)
		metadata = uhd.types.RXMetadata()
		n_rx_loops = 0
		rx_streamer.issue_stream_cmd(stream_cmd)
		while self.on:
			if (n_rx_loops % 1000) == 0:
				DBGPRINT("(rx-#{})".format(n_rx_loops))
			rx_ret = rx_streamer.recv(recv_buffer, metadata) #blocking until rx_buffer_len samples acquired
			if rx_ret != rx_buffer_len:
				DBGPRINT("RECV RETURNED NON-FULL BUFFER WITH RET VALUE "+str(rx_ret))
				#assert rx_ret == rx_buffer_len
			#if self.self_mute:
			#	continue
			if not self.que_rx_samples_out.full():
				self.que_rx_samples_out.put_nowait(recv_buffer[0, :rx_ret].copy())
			else:
				DBGPRINT("WARNING: radio-to-process queue overflow!")
				raise Exception("radio-loop: radio-to-process queue overflow.")
			n_rx_loops += 1


	def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP, tx_batch_len):
		tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
		# stream_args.args = "spp=200" # Note this setting is not valid for all USRPs
		tx_stream_args.channels = [0]
		tx_streamer = usrp.get_tx_stream(tx_stream_args)
		tx_metadata = uhd.types.TXMetadata()
		while self.on:
			try:
				samplearr = self.que_tx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
			N = samplearr.shape[1]
			dtt = N / self.radio_config.tx_sr0
			idx = 0
			t_end = time.perf_counter() + dtt
			self.self_mute = True  # the 5ms initial silence in composed samples also ensures this will have effect.
			while idx < N:
				if (N - idx) <= tx_batch_len:
					tx_metadata.end_of_burst = True
				tx_streamer.send(samplearr[0,idx:idx+tx_batch_len], tx_metadata)
				idx += tx_batch_len
			tx_metadata.end_of_burst = False
			t_to_end = max(0, t_end - time.perf_counter())
			time.sleep(t_to_end + 0.0e-3)
			self.self_mute = False
			DBGPRINT("tx end. sleep of {}/{} ms.".format( round(t_to_end*1e3, 2), round(dtt*1e3, 2) ))
	# === USRP ===============================================================================================================================================================================
	# === USRP ===============================================================================================================================================================================




	# === Soapy ==============================================================================================================================================================================
	# === Soapy ==============================================================================================================================================================================
	def _soapy_rx_loop(self, sdr:SoapySDR.Device, bufferlen):
		rxStream = sdr.setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32)
		timeout = int(1e6 * bufferlen * 0.8 / self.radio_config.rx_sr0)
		n_rx_loops = 0
		buff = np.zeros(bufferlen*1024, np.complex64)
		absolute_bufflen = len(buff)
		n_rx_total = 0
		avg_sr = 0.0
		t00 = time.perf_counter()
		sdr.activateStream(rxStream)
		while self.on:
			if (n_rx_loops % 1000) == 0:
				DBGPRINT("(rx-#{} (sr~{} MS/s)".format(n_rx_loops, round(1e-6*avg_sr, 2) ))
			ret = sdr.readStream(rxStream, [buff], absolute_bufflen, timeoutUs=timeout)
			rx_ret = ret.ret
			n_rx_total += rx_ret
			avg_sr = n_rx_total / (time.perf_counter() - t00)
			#print(ret.ret) #num samples or error code
			#print(ret.flags) #flags set by receive operation
			#print(ret.timeNs) #timestamp for receive buffer
			#if rx_ret != bufferlen:
				#DBGPRINT("RECV RETURNED NON-FULL BUFFER WITH RET VALUE "+str(rx_ret))
				#assert rx_ret == rx_buffer_len
			#if self.self_mute:
			#	continue
			if not self.que_rx_samples_out.full():
				self.que_rx_samples_out.put_nowait(buff[:rx_ret].copy())
			else:
				DBGPRINT("WARNING: radio-to-process queue overflow!")
				raise Exception("radio-loop: radio-to-process queue overflow.")
			n_rx_loops += 1
		sdr.deactivateStream(rxStream) #stop streaming
		sdr.closeStream(rxStream)


	def _soapy_tx_loop(self, sdr:SoapySDR.Device, batchlen):
		txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
		sdr.activateStream(txStream)
		while self.on:
			try:
				samplearr = self.que_tx_samples_in.get(timeout=0.20)
			except Empty:
				continue
			except Exception as e:
				DBGPRINT("Queue.get() exception (tx-thread):", e)
				self.on = False
				break
			N = samplearr.shape[1]
			samplearr = samplearr[0]
			assert len(samplearr) == N
			dtt = N / self.radio_config.tx_sr0
			idx = 0
			t_end = time.perf_counter() + dtt
			self.self_mute = True  # the 5ms initial silence in composed samples also ensures this will have effect.

			while idx < N:
				blen = min(batchlen, len(samplearr) - idx)
				#t0 = time.perf_counter()
				if idx < (len(samplearr)-batchlen):
					sdr.writeStream(txStream, [samplearr[idx:idx+blen]], blen, timeoutUs=1000000)
				else:
					sdr.writeStream(txStream, [samplearr[idx:idx+blen]], blen, timeoutUs=1000000, flags=SoapySDR.SOAPY_SDR_END_BURST)
				#print("written in ", round( 1e6*(time.perf_counter() - t0), 2), "µs")
				idx += batchlen
			#sdr.deactivateStream(txStream)
			#sdr.closeStream(txStream)
			#sdr.closeStream()
			t_to_end = max(0, t_end - time.perf_counter())
			time.sleep(t_to_end + 0.0e-3)
			self.self_mute = False
			DBGPRINT("tx end. sleep of {}/{} ms.".format( round(t_to_end*1e3, 2), round(dtt*1e3, 2) ))
	# === Soapy ==============================================================================================================================================================================
	# === Soapy ==============================================================================================================================================================================



