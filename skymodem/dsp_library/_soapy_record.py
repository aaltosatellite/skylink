import numpy as np
import SoapySDR
import os, time, pickle
from SoapySDR import SOAPY_SDR_CF32, SOAPY_SDR_TX, SOAPY_SDR_RX, SOAPY_SDR_ABI_VERSION, SOAPY_SDR_API_VERSION





def transmit():
	sdr = SoapySDR.Device()
	print("Got device: ", sdr.getDriverKey())
	print("")

	sr0 = 4e6
	f_center = 436.0e6

	sdr.setSampleRate(SOAPY_SDR_TX, 0, sr0)
	sdr.setFrequency(SOAPY_SDR_TX, 0, f_center)
	print("Gain Range: {}".format( sdr.getGainRange(SOAPY_SDR_TX,0 ) ))
	#print("Gain:", sdr.setGain( SOAPY_SDR_RX, 0, 50.0))
	print("Gain:", sdr.getGain( SOAPY_SDR_TX, 0))
	print("F:", sdr.getFrequency( SOAPY_SDR_TX, 0))
	print("sr:", sdr.getSampleRate( SOAPY_SDR_TX, 0))
	#sdr.setGain(SOAPY_SDR_RX, 40)
	#txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
	#sdr.writeStream()

	buff = np.zeros(0, dtype=np.complex64)
	for _ in range(3*3):
		fshift_normed = (np.random.random()-0.5) * 0.2
		buff_ = np.array( np.exp(2j*np.pi * np.arange(int(sr0*0.333)) * fshift_normed ), dtype=np.complex64 )
		buff = np.concatenate( (buff, buff_) )
	bufferlen = len(buff)
	t_tx_expected = bufferlen / sr0

	txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
	sdr.activateStream(txStream) #start streaming
	#create a re-usable buffer for rx samples
	t0 = time.perf_counter()
	c = 0
	while c < bufferlen:
		batchlen = min(2048, bufferlen - c)
		ret = sdr.writeStream(txStream, [buff[c:c+batchlen]], batchlen)
		c += batchlen
		#print(ret.ret) #num samples or error code
		#print(ret.flags) #flags set by receive operation
		#print(ret.timeNs) #timestamp for receive buffer
		#break
	t_tx = (time.perf_counter() - t0)
	sdr.deactivateStream(txStream) #stop streaming
	sdr.closeStream(txStream)
	print("Transmitted in {} s  (should be {})".format( round(t_tx, 4), round(t_tx_expected, 4) ))
	#lett = "".join( [chr(x) for x in np.random.randint(ord("A"), ord("Z"), 3)] )















def record():
	sdr = SoapySDR.Device()
	print("Got device: ", sdr.getDriverKey())
	print("")

	sr0 = 8e6
	f_center = 436.0e6

	sdr.setSampleRate(SOAPY_SDR_RX, 0, sr0)
	sdr.setFrequency(SOAPY_SDR_RX, 0, f_center)
	print("Gain Range: {}".format( sdr.getGainRange(SOAPY_SDR_RX,0 ) ))
	print("Gain:", sdr.setGain( SOAPY_SDR_RX, 0, 50.0))
	print("Gain:", sdr.getGain( SOAPY_SDR_RX, 0))
	#sdr.setGain(SOAPY_SDR_RX, 40)
	#txStream = sdr.setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32)
	#sdr.writeStream()

	bufferlen = int(sr0*12)
	buff = np.zeros(bufferlen, np.complex64)

	rxStream = sdr.setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32)
	sdr.activateStream(rxStream) #start streaming
	#create a re-usable buffer for rx samples
	t0 = time.perf_counter()
	c = 0
	while c < (bufferlen - 2048):
		ret = sdr.readStream(rxStream, [buff[c:c+2048]], 2048)
		c += 2048
		#print(ret.ret) #num samples or error code
		#print(ret.flags) #flags set by receive operation
		#print(ret.timeNs) #timestamp for receive buffer
		#break
	sdr.deactivateStream(rxStream) #stop streaming
	sdr.closeStream(rxStream)
	t_rec = (time.perf_counter() - t0)
	print("Got samples in {} s".format( round(t_rec, 4) ))
	#lett = "".join( [chr(x) for x in np.random.randint(ord("A"), ord("Z"), 3)] )

	ii = 0
	fname = "./samples-{}".format(ii)
	while os.path.isfile(fname):
		ii += 1
		fname = "./samples-{}".format(ii)
	f = open(fname, "wb")
	f.write(pickle.dumps(buff))
	f.close()
	print("Samples written into: ",fname)



record()



