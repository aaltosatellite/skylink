import time
import uhd
import numpy as np
import threading
from queue import Queue, Empty
import SoapySDR
from SoapySDR import SOAPY_SDR_RX, SOAPY_SDR_TX, SOAPY_SDR_CF32
from .lib_tools import make_samples, radionoise, DebugPrinter


class RadioConfig:
    """
    Configuration class for RadioLoop.
    """

    def __init__(self, mode, rx_samplerate, rx_tune_frequency, tx_samplerate, tx_tune_frequency, rx_gain, tx_gain, device_serial=None):
        """
        Initializes RadioConfig instance with given parameters.

        Parameters:
            mode (str): Radio mode, either "usrp", "soapy"
            rx_samplerate (float): Base sample rate for receiving.
            rx_tune_frequency (float): Tuning frequency for receiving.
            tx_samplerate (float): Base sample rate for transmitting.
            tx_tune_frequency (float): Tuning frequency for transmitting.
            rx_gain (float): Gain setting for receiving.
            tx_gain (float): Gain setting for transmitting.
        """
        assert mode in ("usrp", "soapy")
        self.mode 			            = mode
        self.rx_base_samplerate 		= rx_samplerate
        self.rx_tune_frequency 		    = rx_tune_frequency
        self.tx_base_samplerate 		= tx_samplerate
        self.tx_tune_frequency 		    = tx_tune_frequency
        self.rx_gain		            = rx_gain
        self.tx_gain		            = tx_gain
        self.device_serial              = device_serial


class RadioLoop:
    """
    RadioLoop class handles radio communication using USRP or SoapySDR devices.

    This class only manages the radio interface, while the DSP processing is handled in a separate DSPLoop class.
    Due to this there is no knowledge of packets or even symbols here, only samples to be sent or received.
    """
    def __init__(self, radio_config:RadioConfig, queue_tx_samples_from_dsp : Queue, queue_rx_samples_to_dsp : Queue):
        """
        Initializes RadioLoop instance.
        
        Parameters:
            radio_config (RadioConfig): Configuration for the radio.
            queue_tx_samples_from_dsp (Queue): Queue for samples to transmit with radio from DSP loop.
            queue_rx_samples_to_dsp (Queue): Queue for samples received from radio to DSP loop.
        """
        self.radio_config 			    = radio_config
        self.queue_tx_samples_from_dsp  = queue_tx_samples_from_dsp # samples of packets
        self.queue_rx_samples_to_dsp 	= queue_rx_samples_to_dsp
        self.on 				        = True
        self.tx_ready			        = threading.Event()
        self.rx_thread 			        = threading.Thread(target=None, args=tuple())
        self.tx_thread 			        = threading.Thread(target=None, args=tuple())

        # Amplitude statistics, [0] = real-component max, [1] = Moving average amplitude
        self.sample_amplitude_stats 	= np.zeros(2, np.float64)
        self.dbgprinter 		        = DebugPrinter(log_title="RadioLoop", stdprint=True, zmqprint_host_port=("localhost", 11001))
        self.DBGPRINT 			        = self.dbgprinter.DBGPRINT
        self.rx_print_interval	        = 20.0

    def is_ok(self):
        """
        Checks if RadioLoop hasn't had any fatal errors and makes sure all threads are still alive.

        Used together with is_ok functions for other loops to determine if the whole modem is still functioning properly.
        """
        if not self.on:
            return False
        for thrd in (self.tx_thread, self.rx_thread):
            if not thrd.is_alive():
                return False
        return True

    def close(self):
        """
        Closes RadioLoop instance, stops RX and TX threads.
        """
        self.on = False
        self.rx_thread.join(timeout=1.0)
        self.tx_thread.join(timeout=1.0)

    def start(self):
        """
        Starts RadioLoop in either USRP or Soapy mode based on RadioConfig.
        """
        assert self.radio_config.mode in ("usrp", "soapy")
        if self.radio_config.mode == "usrp":
            self._usrp_start()
        else:
            self._soapy_start()

    def sim_start(self, noiseSPD, ts_pl_list, rx_samplearr_que, tx_sample_que):
        """
        Starts RadioLoop in simulation mode.
        """
        self._sim_start(noiseSPD=noiseSPD, ts_pl_list=ts_pl_list, rx_samplearr_que=rx_samplearr_que, tx_sample_que=tx_sample_que)



    # === USRP ===============================================================================================================================================================================
    # === USRP ===============================================================================================================================================================================
    
    
    """
    USRP (Universal Software Radio Peripheral) Mode: Direct communication with USRP device.

    For this mode to be selected, RadioConfig.mode must be set to "usrp".
    """
    
    

    def _usrp_start(self):
        """
        Set necessary parameters for the USRP and start RX and TX threads.

        These threads handle communication of samples between the USRP device and DSP loops.
        """
        self.DBGPRINT("USRP start")

        # Set up USRP device
        usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")

        # Sample Rates
        usrp.set_rx_rate(self.radio_config.rx_base_samplerate, 0)
        usrp.set_tx_rate(self.radio_config.tx_base_samplerate, 0)

        # Frequencies that the USRP will be tuned to, this is different from what we will actually be receiving/transmitting at.
        usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(self.radio_config.rx_tune_frequency), 0)
        usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(self.radio_config.tx_tune_frequency), 0)

        # Gain Settings
        usrp.set_rx_gain(self.radio_config.rx_gain, 0)
        usrp.set_tx_gain(self.radio_config.tx_gain, 0)

        # GPIO Settings, Bank FP0 (Front Panel 0), ATR_TX is status of pins when transmitting, ATR_XX when in full-duplex mode.
        # Takes in mask and value. TODO: Document what is configured here.
        usrp.set_gpio_attr("FP0", "ATR_TX", (1 << 8), (1 << 8))
        usrp.set_gpio_attr("FP0", "ATR_XX", (1 << 8), (1 << 8))
        time.sleep(0.1)

        # Read actual sample rates
        self.rx_samplerate_actual = float(usrp.get_rx_freq(0))
        self.tx_samplerate_actual = float(usrp.get_tx_freq(0))

        # USRP Info/Settings printout
        self.DBGPRINT(f"RX antennas(0):         {usrp.get_rx_antennas(0)}")
        self.DBGPRINT(f"TX antennas(0):         {usrp.get_tx_antennas(0)}")
        self.DBGPRINT(f"RX antenna in use(0):   {usrp.get_rx_antenna(0)}")
        self.DBGPRINT(f"TX antenna in use(0):   {usrp.get_tx_antenna(0)}")
        self.DBGPRINT(f"RX gain range:        {str(usrp.get_rx_gain_range(0))[:-1]}")
        self.DBGPRINT(f"TX gain range:        {str(usrp.get_tx_gain_range(0))[:-1]}")
        self.DBGPRINT(f"usrp RX gain:         {usrp.get_rx_gain(0)}")
        self.DBGPRINT(f"usrp TX gain:         {usrp.get_tx_gain(0)}")
        self.DBGPRINT(f"usrp RX samplerate:   {round(usrp.get_rx_rate(0)*1e-3, 6)} ksps")
        self.DBGPRINT(f"usrp TX samplerate:   {round(usrp.get_tx_rate(0)*1e-3, 6)} ksps")
        self.DBGPRINT(f"usrp RX tune-f:       {round(usrp.get_rx_freq(0)*1e-6, 3)} MHz")
        self.DBGPRINT(f"usrp TX tune-f:       {round(usrp.get_tx_freq(0)*1e-6, 3)} MHz")

        # Start RX and TX threads
        self.rx_thread 			= threading.Thread(target=self._usrp_rx_loop,    args=(usrp, 1024*2), daemon=True) #TODO bufferlen as setting?
        self.tx_thread 			= threading.Thread(target=self._usrp_tx_loop,    args=(usrp, 1024*8), daemon=True) #TODO bufferlen as setting?
        self.on = True
        self.rx_thread.start()
        self.tx_thread.start()



    def _usrp_rx_loop(self, usrp:uhd.usrp.MultiUSRP, rx_buffer_length):
        """
        Receive loop for USRP mode.

        Receives samples from USRP and puts them into Queue for DSPLoop to process.

        Parameters:
            usrp (uhd.usrp.MultiUSRP): USRP device instance.
            rx_buffer_length (int): Length of the receive buffer.
        """
        # Set up the stream and receive buffer
        stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
        stream_args.channels = [0]
        rx_stream = usrp.get_rx_stream(stream_args)

        # Start Continuous Stream
        stream_cmd = uhd.types.StreamCMD(uhd.types.StreamMode.start_cont)
        stream_cmd.stream_now = True
        metadata = uhd.types.RXMetadata()

        # Receive buffer length given as argument to this function
        receive_buffer 			= np.zeros((1, rx_buffer_length), dtype=np.complex64)

        # Used for statistic calculations
        samples_received_total 	        = 0
        average_samplerate 		        = 0.0
        amplitude_max_reset_interval 	= 30.0

        next_print    		            = time.monotonic()
        next_max_amplitude_reset 		= time.monotonic() + amplitude_max_reset_interval

        # Start streaming
        rx_stream.issue_stream_cmd(stream_cmd)
        start_time = time.perf_counter()

        # Infinite loop unless an exception occurs
        while self.on:
            # Print out status every 20 seconds
            timestamp_monotonic = time.monotonic()
            if timestamp_monotonic >= next_print:
                ampmax_int_time = round(timestamp_monotonic - (next_print - amplitude_max_reset_interval), 1)
                self.DBGPRINT(f"(Samplerate: {round(1e-6 * average_samplerate, 5)} MS/s measured vs {round(1e-6 * self.radio_config.rx_base_samplerate, 5)} MS/s specified). component-max:{self.sample_amplitude_stats[0]}, avg-amplitude:{self.sample_amplitude_stats[1]} ({ampmax_int_time}s)")
                next_print = timestamp_monotonic + self.rx_print_interval

            
            # Reset maximum amplitude calculation for real component every 30 seconds
            if timestamp_monotonic >= next_max_amplitude_reset:
                self.sample_amplitude_stats[0] = 0.0
                next_max_amplitude_reset = timestamp_monotonic + amplitude_max_reset_interval

            # Timestamps that are sent along with the received samples
            sample_start_timestamp_monotonic = timestamp_monotonic
            sample_start_timestamp_unix = time.time()

            # Receive samples from USRP
            samples_received = rx_stream.recv(receive_buffer, metadata) # blocking until rx_buffer_length samples acquired
            average_samplerate = samples_received_total / (time.perf_counter() - start_time)
            samples_received_total += samples_received

            # Check for non-full receive buffer
            if samples_received != rx_buffer_length:
                self.DBGPRINT(f"WARNING: RECV RETURNED NON-FULL BUFFER WITH RET VALUE: {str(samples_received)}")

            # Copy only the received samples to a new buffer. Avoids overwriting before it is read from queue and bugs in case of non-full buffers.
            received_buffer = receive_buffer[0, :samples_received].copy()

            # Real amplitude maximum and moving average calculations (Exponentially weighted)
            self.sample_amplitude_stats[0] = np.max( (self.sample_amplitude_stats[0], np.max(np.abs(received_buffer.real))) )
            self.sample_amplitude_stats[1] = self.sample_amplitude_stats[1] +  (np.average( np.abs(received_buffer[0:32]) ) - self.sample_amplitude_stats[1]) * 0.1


            # Put received samples into queue that will be processed by DSP loop
            if not self.queue_rx_samples_to_dsp.full():
                self.queue_rx_samples_to_dsp.put_nowait((received_buffer, sample_start_timestamp_monotonic, sample_start_timestamp_unix))
            else:
                self.DBGPRINT("WARNING: radio-to-process queue overflow!")
                raise Exception("radio-loop: radio-to-process queue overflow.")
            



    def _usrp_tx_loop(self, usrp:uhd.usrp.MultiUSRP, tx_batch_len):
        """
        Transmission loop for USRP mode.

        Gets samples from DSPLoop and transmits them via USRP.

        Parameters:
            usrp (uhd.usrp.MultiUSRP): USRP device instance.
            tx_batch_len (int): Number of samples to send in each batch to USRP.
        """
        tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
        tx_stream_args.channels = [0]
        tx_stream = usrp.get_tx_stream(tx_stream_args)

        # Infinite loop unless an exception occurs
        while self.on:
            # Used by SkyLinkLoop so it does not feed packets to DSPLoop too fast.
            self.tx_ready.set()

            # Get samples to transmit from DSPLoop
            try:
                samplearr = self.queue_tx_samples_from_dsp.get(timeout=0.20)
            except Empty: # Nothing to transmit, continue loop
                continue
            except Exception as e:
                self.DBGPRINT("Queue.get() exception (tx-thread):", e)
                self.on = False
                break

            # Check sample types and correct them or just ignore bad samples and start over the loop.
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

            # Prevent SkyLinkLoop from sending more packets to DSPLoop
            self.tx_ready.clear()

            # Calculate end time for transmission so samples are not fed into streamer too fast.
            num_samples = samplearr.shape[1]
            total_transmission_time = num_samples / self.radio_config.tx_base_samplerate
            idx = 0
            t_end = time.perf_counter() + total_transmission_time

            tx_metadata = uhd.types.TXMetadata()
            tx_metadata.start_of_burst = True
            tx_metadata.end_of_burst = False

            # Transmit samples in batches
            while idx < num_samples:
                if (num_samples - idx) <= tx_batch_len:
                    tx_metadata.end_of_burst = True
                tx_stream.send(samplearr[0,idx:idx+tx_batch_len], tx_metadata)
                tx_metadata.start_of_burst = False
                idx += tx_batch_len
            
            # Sleep until transmission is done
            time_to_end = max(0, t_end - time.perf_counter() - 10e-3)
            time.sleep(time_to_end)
            self.DBGPRINT(f"tx end. sleep of {round(time_to_end * 1e3, 2)}/{round(total_transmission_time * 1e3, 2)} ms.")



    # === USRP ===============================================================================================================================================================================
    # === USRP ===============================================================================================================================================================================



    # === Soapy ==============================================================================================================================================================================
    # === Soapy ==============================================================================================================================================================================
    
    """
    Soapy Mode: Meant to be used on ground station machine with SoapyShared library.
    SoapyShared creates shared memory devices that can be connected to by multiple applications.

    In general this could be made to be used with any SoapySDR-supported device,
    however there is some hardcoding in place for SoapyShared leecher devices.
    """

    def _soapy_start(self):
        """
        Set necessary parameters for the SoapySDR device and start RX and TX threads.

        Currently meant for use with SoapyShared leecher devices.
        
        """
        self.DBGPRINT("SoapySDR start: list devices")
        # enumerate devices
        devices = SoapySDR.Device.enumerate()
        sdr = None

        # select device based on serial number if given
        serial = self.radio_config.device_serial
        if self.radio_config.device_serial is not None:
            for device in devices:
                self.DBGPRINT(device)
                if dict(device).get("serial") == serial or dict(device).get("seeder:serial") == serial or serial in dict(device).get("label"):
                    sdr = SoapySDR.Device(device)
        else:
            if len(devices) > 0:
                sdr = SoapySDR.Device(devices[0])

        SoapySDR.setLogLevel(SoapySDR.SOAPY_SDR_FATAL)

        # TODO: Maybe there should be a better way to configure devices than just hardcoding serial numbers here.
        if sdr == None:
            self.DBGPRINT("WARNING: Soapy device with correct serial number (label) was not found! Exiting...")
            exit()

        # Configure SDR
        sdr.setSampleRate(SOAPY_SDR_RX, 0, self.radio_config.rx_base_samplerate)
        sdr.setSampleRate(SOAPY_SDR_TX, 0, self.radio_config.tx_base_samplerate)
        sdr.setFrequency(SOAPY_SDR_RX, 0, self.radio_config.rx_tune_frequency)
        sdr.setFrequency(SOAPY_SDR_TX, 0, self.radio_config.tx_tune_frequency)
        sdr.setGain(SOAPY_SDR_RX, 0, self.radio_config.rx_gain)
        sdr.setGain(SOAPY_SDR_TX, 0, self.radio_config.tx_gain)

        # Print SDR info
        self.DBGPRINT(f"Selecting {sdr}")
        self.DBGPRINT(f"SoapySDR driver key: {sdr.getDriverKey()}")
        self.DBGPRINT(f"SoapySDR Hardware key: {sdr.getHardwareKey()}")
        self.DBGPRINT("Assuming we are on a SoapyShared leecher device.")
        self.DBGPRINT("Radio parameters can not be changed, instead we config to what we believe they are.")
        self.DBGPRINT(f"Assuming:  f-tune = {self.radio_config.rx_tune_frequency} MHz")
        self.DBGPRINT(f"Assuming:  	 sr > {self.radio_config.rx_base_samplerate} MS/s")
        self.DBGPRINT(f"RX gain range:      {sdr.getGainRange(SOAPY_SDR_RX, 0)}")
        self.DBGPRINT(f"TX gain range:      {sdr.getGainRange(SOAPY_SDR_TX, 0)}")
        self.DBGPRINT(f"RX gain:            {sdr.getGain(SOAPY_SDR_RX, 0)}")
        self.DBGPRINT(f"TX gain:            {sdr.getGain(SOAPY_SDR_TX, 0)}")

        # Start RX and TX threads
        self.rx_thread			= threading.Thread(target=self._soapy_rx_loop,   args=(sdr, 1024*4), daemon=True)
        self.tx_thread 			= threading.Thread(target=self._soapy_tx_loop,   args=(sdr, 1024*8), daemon=True) #TODO bufferlen as setting?
        self.on = True
        self.rx_thread.start()
        self.tx_thread.start()


    def _soapy_rx_loop(self, sdr:SoapySDR.Device, rx_buffer_length):
        """
        Receive loop for Soapy mode.

        Receives samples from SoapySDR and puts them into Queue for DSPLoop to process.

        Parameters:
            sdr (SoapySDR.Device): Current SoapySDR device.
            rx_buffer_length (int): Length of the receive buffer.
        """
        rx_stream                       = sdr.setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32)
        readStream_timeout              = int(1e6 * rx_buffer_length * 0.8 / self.radio_config.rx_base_samplerate)
        receive_buffer 			        = np.zeros(int(2**21), np.complex64)
        samples_received_total 			= 0
        average_samplerate 				= 0.0
        amplitude_max_reset_interval 	= 30.0
        next_print    		            = time.monotonic()
        t_next_maxreset 		        = time.monotonic() + amplitude_max_reset_interval
        absolute_bufflen 		        = len(receive_buffer)

        # Start stream
        sdr.activateStream(rx_stream)
        start_time = time.perf_counter()

        # Infinite loop unless an exception occurs
        while self.on:
            timestamp_monotonic = time.monotonic()

            # Print out status every 20 seconds
            if timestamp_monotonic >= next_print:
                ampmax_int_time = round(timestamp_monotonic - (t_next_maxreset - amplitude_max_reset_interval), 1)
                self.DBGPRINT(f"(Samplerate: {round(1e-6 * average_samplerate, 5)} MS/s measured vs {round(1e-6*self.radio_config.rx_base_samplerate, 5)} MS/s specced). component-max:{self.sample_amplitude_stats[0]}, avg-amplitude:{self.sample_amplitude_stats[1]} ({ampmax_int_time}s)")
                next_print = timestamp_monotonic + self.rx_print_interval

            # Reset maximum amplitude calculation for real component every 30 seconds
            if timestamp_monotonic >= t_next_maxreset:
                self.sample_amplitude_stats[0] = 0.0
                t_next_maxreset = timestamp_monotonic + amplitude_max_reset_interval

            # Timestamps that are sent along with the received samples
            sample_start_timestamp_monotonic = timestamp_monotonic
            sample_start_timestamp_unix = time.time()

            # Receive samples from SoapySDR
            ret = sdr.readStream(rx_stream, [receive_buffer], numElems=absolute_bufflen, timeoutUs=readStream_timeout)
            samples_received = ret.ret
            samples_received_total += samples_received
            average_samplerate = samples_received_total / (time.perf_counter() - start_time)

            # Copy only the received samples to a new buffer. Avoids overwriting before it is read from queue and bugs in case of non-full buffers.
            received_buffer = receive_buffer[:samples_received].copy()

            # Real amplitude maximum and moving average calculations (Exponentially weighted)
            self.sample_amplitude_stats[0] = np.max( (self.sample_amplitude_stats[0], np.max(np.abs(received_buffer.real))) )
            self.sample_amplitude_stats[1] = self.sample_amplitude_stats[1] + (np.average( np.abs(received_buffer[0:32]) ) - self.sample_amplitude_stats[1]) * 0.1

            # Put received samples into queue that will be processed by DSP loop
            if not self.queue_rx_samples_to_dsp.full():
                self.queue_rx_samples_to_dsp.put_nowait((received_buffer, sample_start_timestamp_monotonic, sample_start_timestamp_unix))
            else:
                self.DBGPRINT("WARNING: radio-to-process queue overflow!")
                raise Exception("radio-loop: radio-to-process queue overflow.")
        
        # Cleanup on exit
        sdr.deactivateStream(rx_stream) #stop streaming
        sdr.closeStream(rx_stream)


    def _soapy_tx_loop(self, sdr:SoapySDR.Device, batchlen):
        """
        Transmission loop for Soapy mode.
        
        Gets samples from DSPLoop and transmits them via SoapySDR.

        Parameters:
            sdr (SoapySDR.Device): Current SoapySDR device.
            batchlen (int): Length of each transmission batch.
        """

        # Set up TX stream and activate it
        txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
        sdr.activateStream(txStream)

        # Infinite loop unless an exception occurs
        while self.on:
            # Used by SkyLinkLoop so it does not feed packets to DSPLoop too fast.
            self.tx_ready.set()

            # Get samples to transmit from DSPLoop
            try:
                sample_array = self.queue_tx_samples_from_dsp.get(timeout=0.20)
            except Empty:
                continue
            except Exception as e:
                self.DBGPRINT("Queue.get() exception (tx-thread):", e)
                self.on = False
                break

            # Prevent SkyLinkLoop from sending more packets to DSPLoop
            self.tx_ready.clear()

            # Calculate end time for transmission so samples are not fed into streamer too fast.
            num_samples = sample_array.shape[1]
            sample_array = sample_array[0]
            assert len(sample_array) == num_samples
            assert num_samples > 1
            total_transmission_time = num_samples / self.radio_config.tx_base_samplerate
            idx = 0
            t_end = time.perf_counter() + total_transmission_time

            # Transmit samples in batches
            while idx < num_samples:
                current_batch_length = min(batchlen, num_samples - idx) # one batch or remaining samples
                if idx < (num_samples-batchlen):
                    sdr.writeStream(txStream, [sample_array[idx:idx + current_batch_length]], current_batch_length, timeoutUs=1000000)
                else:
                    sdr.writeStream(txStream, [sample_array[idx:idx + current_batch_length]], current_batch_length, timeoutUs=1000000, flags=SoapySDR.SOAPY_SDR_END_BURST)
                idx += batchlen

            # Sleep until transmission is done
            time_to_end = max(0, t_end -time.perf_counter() - 10e-3)
            time.sleep(time_to_end)
            self.DBGPRINT(f"tx end. sleep of {round(time_to_end * 1e3, 2)}/{round(total_transmission_time * 1e3, 2)} ms.")


    # === Soapy ==============================================================================================================================================================================
    # === Soapy ==============================================================================================================================================================================




    # TODO: These should maybe be cleaned up later or even moved to a separate file.
    # === SIM ================================================================================================================================================================================
    # === SIM ================================================================================================================================================================================
    def _sim_start(self, noiseSPD, ts_pl_list, rx_samplearr_que, tx_sample_que):
        self.DBGPRINT("Sim start")
        self.radio_config.rx_tune_frequency = 437e6
        self.radio_config.tx_tune_frequency = 437e6
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
            sample_start_timestamp_monotonic = time.monotonic()
            sample_start_timestamp_unix = time.time()
            batch = radionoise(batchlen, sr=self.radio_config.rx_base_samplerate, W_per_Hz=noiseSPD)

            while (pl_head < len(ts_pl_list)) and ((sample_start_timestamp_monotonic + batchlen/self.radio_config.rx_base_samplerate) > ts_pl_list[pl_head][0]):
                bits, baudrate, f_abs, power, mod_idx = ts_pl_list[pl_head][1]
                sps = self.radio_config.rx_base_samplerate / baudrate
                f_offset_rel = (f_abs - self.radio_config.rx_tune_frequency) / self.radio_config.rx_base_samplerate
                transmission = make_samples(samples_per_symbol=sps, bitstring=bits, frequency_offset=f_offset_rel, power=power, modulation_index=mod_idx, shaper_BT_prod=0.5, n_silence_start=0, n_silence_end=0)
                i_start = int((ts_pl_list[pl_head][0]-t0) * self.radio_config.rx_base_samplerate)
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

            if not self.queue_rx_samples_to_dsp.full():
                self.queue_rx_samples_to_dsp.put_nowait((batch, sample_start_timestamp_monotonic, sample_start_timestamp_unix))
            else:
                self.DBGPRINT("WARNING! radio-to-process queue overflow!  {}".format( 1e-6 * n_received / (time.perf_counter() - t0) ))
            n_received += batchlen
            t_next = t0 + (n_received / self.radio_config.rx_base_samplerate)
            t_sleep = max(0, t_next - time.perf_counter())


    def _sim_tx_loop(self, tx_samples_out_que):
        while self.on:
            try:
                samplearr = self.queue_tx_samples_from_dsp.get(timeout=0.20)
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
