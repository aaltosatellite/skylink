"""
Make a WAV file of a SkyLink repeater frame.

Configurable sample rate, Generates a 16-bit PCM WAV file.
"""
import argparse
import sys

from repeater_frame_bytes import construct_frame_bytes
from scipy.io import wavfile
from dsp_library.kuokka.lib_tools import make_samples, make_samples_alternative
import numpy as np
import os


def _prompt_if_none(value, prompt, default=None):
    if value is not None:
        return value
    text = input(prompt)
    if default is not None and text.strip() == "":
        return default
    return text

def bytes_to_bits_msb_first(data):
    """Convert bytes to bits, MSB first."""
    bits = np.zeros(len(data)*8, dtype=np.int64)
    for i in range(len(data)):
        for j in range(8):
            bits[i*8+j] = (data[i] >> (7-j)) & 1  # MSB first
    return bits

def construct_samples_for_wav(frame_bytes, srate, baudrate=9600):
    # Convert frame bytes to bitstring using MSB-first ordering (matching frame_packet)
    bitstring = bytes_to_bits_msb_first(frame_bytes) * 2 - 1  # Convert to -1, +1 (NRZ)
    sps = srate / baudrate

    # Alternative sample generator uses the same samples per symbol for the NRZ modulator
    iq_samples, nrz_modulator = make_samples_alternative(
        samples_per_symbol=sps,
        bitstring=bitstring,
        frequency_offset=0.0,
        power=1.0,
        modulation_index=0.5,
        shaper_BT_prod=0.5,
        shaper_n_taps=301,
    )

    return iq_samples, nrz_modulator


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate WAV (mono NRZ or stereo IQ) for a SkyLink repeater frame")

    # Frame args (passed through)
    parser.add_argument("--message", type=str, default=None, help="Message (max 111 bytes). If omitted, prompt.")
    parser.add_argument("--dest", dest="dest_addr", type=str, default=None, help="Destination callsign. If omitted, prompt.")
    parser.add_argument("--src", dest="src_addr", type=str, default=None, help="Source callsign. If omitted, prompt.")
    parser.add_argument("--digi", dest="digis", action="append", default=None, help="Digipeater callsign, to add multiple use --digi multiple times")
    parser.add_argument("--no-digis", action="store_true", help="Do not include digipeaters and do not prompt for them.")

    sync = parser.add_mutually_exclusive_group()
    sync.add_argument("--syncword", dest="add_syncword", action="store_true")
    sync.add_argument("--no-syncword", dest="add_syncword", action="store_false")
    parser.set_defaults(add_syncword=None)

    pre = parser.add_mutually_exclusive_group()
    pre.add_argument("--preamble", dest="add_preamble", action="store_true")
    pre.add_argument("--no-preamble", dest="add_preamble", action="store_false")
    parser.set_defaults(add_preamble=None)

    # WAV generation args
    parser.add_argument("--wav", dest="wav_filename", type=str, default=None, help="Output WAV filename")
    parser.add_argument("--srate", type=int, default=None, help="Sample rate Hz")

    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--mono", dest="use_mono", action="store_true", help="Mono NRZ output")
    mode.add_argument("--iq", dest="use_mono", action="store_false", help="Stereo IQ output")
    parser.set_defaults(use_mono=None)

    return parser.parse_args(argv)




if __name__ == "__main__":
    args = _parse_args(sys.argv[1:])

    frame_bytes = construct_frame_bytes(
        message=args.message,
        dest_addr=args.dest_addr,
        src_addr=args.src_addr,
        digis=args.digis,
        no_digis=bool(args.no_digis),
        add_syncword=args.add_syncword,
        add_preamble=args.add_preamble,
    )

    wav_filename = args.wav_filename
    if wav_filename is None:
        wav_filename = _prompt_if_none(None, "Input output WAV filename (default repeater_frame.wav): ", default="repeater_frame.wav")

    srate = args.srate
    if srate is None:
        srate_str = _prompt_if_none(None, "Input sample rate in Hz (default 48000): ", default="48000")
        srate = int(srate_str)

    iq_samples, nrz_modulator = construct_samples_for_wav(frame_bytes, srate=srate, baudrate=9600)

    use_mono = args.use_mono
    if use_mono is None:
        mono_file = _prompt_if_none(None, "Generate mono file? (y/n, default y): ", default="y")
        use_mono = mono_file.strip().lower() != "n"

    guard_time = 0.2

    # Store adjacent to current script
    wav_filepath = os.path.abspath(__file__)
    wav_dir = os.path.dirname(wav_filepath)
    wav_filename = os.path.join(wav_dir, wav_filename)
    

    # Mono file requires the NRZ modulator since the radio hardware will do the FSK modulation
    if use_mono:
        # Add silence guard time at start and end
        num_guard_samples = int(guard_time * srate)
        silence = np.zeros(num_guard_samples, dtype=nrz_modulator.dtype)
        nrz_with_guard = np.concatenate((silence, nrz_modulator, silence))
        
        # Normalize to int16 range
        mono_samples = nrz_with_guard.astype(np.float64)
        mono_samples /= np.max(np.abs(mono_samples))
        mono_samples = (mono_samples * 32767).astype(np.int16)

        wavfile.write(wav_filename, srate, mono_samples)
        print(f"WAV file '{wav_filename}' written successfully (Mono NRZ)")
    else:
        # Note: When testing the stereo file decoding with gr_satellites, I found that I had to use --disable_dc_block and/or --use_agc (And of course --iq).

        # Add silence guard time at start and end
        num_guard_samples = int(guard_time * srate)
        silence = np.zeros(num_guard_samples, dtype=iq_samples.dtype)
        iq_samples = np.concatenate((silence, iq_samples, silence))

        # Store IQ samples as stereo.
        iq_samples_stereo = np.zeros((len(iq_samples), 2), dtype=np.float64)
        iq_samples_stereo[:, 0] = iq_samples.real
        iq_samples_stereo[:, 1] = iq_samples.imag

        # Normalize to int16 range
        iq_samples_stereo /= np.max(np.abs(iq_samples_stereo))
        iq_samples_stereo = (iq_samples_stereo * 32767).astype(np.int16)
        
        wavfile.write(wav_filename, srate, iq_samples_stereo)
        print(f"WAV file '{wav_filename}' written successfully (Stereo IQ)")