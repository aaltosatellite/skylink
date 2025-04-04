import numpy as np
import os
from mtools.tools_dsp import waterfall_mx
import pickle

#fpath = "/home/elmore/fs1p/sshtransfer/samples000.pkl"
fpath = "/home/elmore/fs1p/sshtransfer/samples-3"
assert os.path.isfile(fpath)

f = open(fpath, "rb")
rd = f.read()
f.close()

samples = pickle.loads(rd)
print("Samples: {}  {}".format(samples.dtype, len(samples) ))

samples = samples[1000000*4:1000000*8]

waterfall_mx(samples=samples, fftlen=2048, fft_jump=1024, srate=4e6, plot_and_show=True)

