"""
Make a WAV file of a SkyLink repeater frame.

Configurable sample rate, Generates a 16-bit PCM WAV file.
"""
from repeater_frame_bytes import construct_frame_bytes
from scipy.io import wavfile
from dsp_library.kuokka.lib_tools import make_samples, make_samples_alternative
import numpy as np
import os

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

if __name__ == "__main__":
    frame_bytes = construct_frame_bytes()
    wav_filename = input("Input output WAV filename (default repeater_frame.wav): ")
    if wav_filename.strip() == "":
        wav_filename = "repeater_frame.wav"

    srate = input("Input sample rate in Hz (default 48000): ")
    if srate.strip() == "":
        srate = 48000
    else:
        srate = int(srate)

    # Generate IQ samples
    iq_samples, nrz_modulator = construct_samples_for_wav(frame_bytes, srate=srate, baudrate=9600)

    use_mono = True
    mono_file = input("Generate mono file? (y/n, default y): ")
    if mono_file.strip().lower() == "n":
        use_mono = False

    # Silence seems to help gr_satellites decoder catch the clock. This is good to verify the file is valid.
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
