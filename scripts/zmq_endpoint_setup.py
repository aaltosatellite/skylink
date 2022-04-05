from setuptools import setup, Extension
from Cython.Build import cythonize
import os

sourcefiles = [
	"zmq_endpoint.pyx",

	"../src/diag.c",
	"../src/elementbuffer.c",
	"../src/fec.c",
	"../src/frame.c",
	"../src/hmac.c",
	"../src/mac.c",
	"../src/reliable_vc.c",
	"../src/sequence_ring.c",
	"../src/skylink_rx.c",
	"../src/skylink_tx.c",
	"../src/utilities.c",

	"../src/ext/cifra/blockwise.c",
	"../src/ext/cifra/hmac.c",
	"../src/ext/cifra/sha256.c",
	"../src/ext/gr-satellites/golay24.c",
	"../src/ext/libfec/ccsds_tab.c",
	"../src/ext/libfec/decode_rs_8.c",
	"../src/ext/libfec/encode_rs_8.c",

	"../tests/zmq_endpoint.c",
	"../tests/tst_utilities.c",
	"../tests/tools/tools.c",
]

extensions = [Extension("zmq_endpoint", sourcefiles, extra_compile_args=["-O3"], extra_link_args=["/lib/x86_64-linux-gnu/libzmq.so"])]


setup(
	name = "fast endpoint",
	ext_modules = cythonize(extensions, compiler_directives={"language_level":"3"}),
	zip_safe = False
)




