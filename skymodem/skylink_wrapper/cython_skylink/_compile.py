from setuptools import setup, Extension
from Cython.Build import cythonize
import os

SKYLINK_PATH = "../../../"
SRC_PATH = SKYLINK_PATH + "/src/"
SKYLINK_INCL_PATH = SKYLINK_PATH + "/src/skylink/"
BLAKE3_PATH = SKYLINK_PATH+ "/src/ext/blake3/"
GR_SATELLITES_PATH = SKYLINK_PATH+ "/src/ext/gr-satellites/"
LIBFEC_PATH = SKYLINK_PATH+ "/src/ext/libfec/"
PLATFORMS_PATH = SKYLINK_PATH + "/platforms/linux/"

assert os.path.isdir(SRC_PATH)
assert os.path.isdir(SKYLINK_INCL_PATH)
assert os.path.isdir(BLAKE3_PATH)
assert os.path.isdir(GR_SATELLITES_PATH)
assert os.path.isdir(LIBFEC_PATH)
assert os.path.isdir(PLATFORMS_PATH)

include_paths = [
        SRC_PATH,
        SKYLINK_INCL_PATH,
        BLAKE3_PATH,
        GR_SATELLITES_PATH,
        LIBFEC_PATH,
        PLATFORMS_PATH,
]


source_paths = [
    SRC_PATH + "/crc.c",
    SRC_PATH + "/diag.c",
    SRC_PATH + "/element_buffer.c",
    SRC_PATH + "/fec.c",
    SRC_PATH + "/frame.c",
    SRC_PATH + "/hmac.c",
    SRC_PATH + "/mac.c",
    SRC_PATH + "/reliable_vc.c",
    SRC_PATH + "/sequence_ring.c",
    SRC_PATH + "/skylink_rx.c",
    SRC_PATH + "/skylink_tx.c",
    SRC_PATH + "/utilities.c",

        BLAKE3_PATH + "blake3.c",
        BLAKE3_PATH + "blake3_dispatch.c",
        BLAKE3_PATH + "blake3_portable.c",

        GR_SATELLITES_PATH + "golay24.c",

        LIBFEC_PATH + "ccsds_tab.c",
        LIBFEC_PATH + "decode_rs_8.c",
        LIBFEC_PATH + "encode_rs_8.c",
]



sourcefiles= ["c_skylink.pyx"]
sourcefiles.extend(source_paths)


compile_args = ["-O3", "-fPIC"] #"-Wall" ?
for icd in include_paths:
    compile_args.append( "-I"+icd )

extensions = [Extension("c_skylink", sourcefiles, extra_compile_args=compile_args)]

setup(
        name='SkyLink',
        ext_modules=cythonize(extensions, compiler_directives={"language_level":"3"}),
        zip_safe=False,
)
