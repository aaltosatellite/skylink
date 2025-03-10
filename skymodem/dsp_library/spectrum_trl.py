import numpy as np
from PIL import Image
from matplotlib import pyplot as plt
import uhd
from mtools.tools_dsp import waterfall_mx
import time





def get_jpg_mx(fpath, do_plot=False):
	f = open(fpath, "rb")
	im = Image.open(f)
	mx = np.array(im)
	mx = np.array(mx, dtype=np.float64)
	mx = np.sum(mx, 2)
	mx = mx / np.max(mx)
	mx = 1 - mx
	print(np.min(mx), np.max(mx))
	if do_plot:
		fig = plt.figure(figsize=(6,6))
		ax = fig.add_subplot(111)
		ax.imshow(mx, origin="lower", aspect=1.0, cmap="Greys")
		fig.set_layout_engine("tight")
		plt.show()
	return mx


def rollsmooth(arr, n):
	arr2 = arr.copy()
	for _ in range(n):
		arr2 = (np.roll(arr2, 1)+np.roll(arr2, -1)+arr2) * (1/3)
	return arr2


def pixmap_to_samples(mx, nrep, do_plot=False):
	assert len(mx.shape) == 2
	mx = mx**2
	mx = np.repeat( np.repeat(mx, nrep, 0), nrep, 1)

	samples = np.zeros(0, dtype=np.complex128)
	nrows = mx.shape[0]
	for irow in range(nrows):
		smpls = np.fft.ifft(np.fft.fftshift(np.complex128(rollsmooth(mx[irow],0))))
		samples = np.concatenate( (smpls, samples) )
	samples = samples / np.max(np.abs(samples))
	samples = samples * 2

	print("amp", np.average(np.abs(samples)))
	if do_plot:
		waterfall_mx(samples, fftlen=1024, fft_jump=512+43,  srate=1*1e6, plot_and_show=True)
	print("produced {}k samples".format(len(samples)*1e-3))
	return samples






def paint_in_spectrum(f_tune, sr, gain, samples):
	usrp = uhd.usrp.MultiUSRP("num_recv_frames=1000")
	usrp.set_rx_rate(sr)
	usrp.set_tx_rate(sr)
	usrp.set_rx_freq(uhd.libpyuhd.types.tune_request(f_tune), 0)
	usrp.set_tx_freq(uhd.libpyuhd.types.tune_request(f_tune), 0)

	usrp.set_rx_gain(gain, 0)
	usrp.set_tx_gain(gain, 0)
	print("rx gain range",usrp.get_rx_gain_range())

	tx_buffer = np.complex64(samples)
	tx_stream_args = uhd.usrp.StreamArgs("fc32", "sc16")
	tx_stream_args.channels = [0]
	tx_streamer = usrp.get_tx_stream(tx_stream_args)
	tx_metadata = uhd.types.TXMetadata()
	print("---------------------------")
	[print(x) for x in tx_streamer.__dir__()]
	print(tx_streamer.get_num_channels())
	print(tx_streamer.get_max_num_samps())
	print(tx_streamer.recv_async_msg())
	print("---------------------------")
	print("Sending loop")
	while True:
		t0 = time.perf_counter()
		cc = 0
		while cc < len(tx_buffer):
			tx_streamer.send(tx_buffer[cc:cc+1024*8], tx_metadata)
			cc += 1024*8
		print("tx-dt: ", round(time.perf_counter() - t0, 3))
		time.sleep(1.0)


if __name__ == '__main__':
	fpath0 = "/home/elmore/Desktop/jaan1.jpg"
	fpath1 = "/home/elmore/Desktop/pedro-pedro-400.png"
	mx = get_jpg_mx(fpath0)
	samples = pixmap_to_samples(mx, 8, False)
	paint_in_spectrum(f_tune=437.1e6, sr=1e5, gain=75, samples=samples)




