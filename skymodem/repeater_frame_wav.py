"""
Make a WAV file of a SkyLink repeater frame.

Configurable sample rate, Generates a 16-bit PCM WAV file.
"""
from repeater_frame_bytes import construct_frame_bytes
from scipy.io import wavfile
from dsp_library.kuokka.lib_tools import make_samples
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
    samples, _ = make_samples(samples_per_symbol=sps, bitstring=bitstring, frequency_offset=0.0, power=1.0, modulation_index=0.5, shaper_BT_prod=0.5)

    return samples

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
    samples = construct_samples_for_wav(frame_bytes, srate=srate, baudrate=9600)

    # Frequency shift by srate/4
    freq_shift = srate / 4
    time_vector = np.arange(len(samples))
    shifted_signal = samples * np.exp(1j * 2 * np.pi * (freq_shift / srate) * time_vector)

    # Take real part
    mono_samples = shifted_signal.real

    # Normalize to int16 range
    mono_samples /= np.max(np.abs(mono_samples))
    mono_samples = (mono_samples * 32767).astype(np.int16)
    
    # Store adjacent to current script
    wav_filepath = os.path.abspath(__file__)
    wav_dir = os.path.dirname(wav_filepath)
    wav_filename = os.path.join(wav_dir, wav_filename)
    wavfile.write(wav_filename, srate, mono_samples)
    print(f"WAV file '{wav_filename}' written successfully")
