"""
Add a new modem config preset interactively.
"""

import os
import json

def get_input_with_default(prompt, default):
    """Get user input with a default value."""
    user_input = input(f"{prompt} [{default}]: ").strip()
    return user_input if user_input else default

if __name__ == "__main__":
    # Load existing presets
    presets_path = os.path.join(os.path.dirname(__file__), 'presets.json')
    with open(presets_path, 'r') as f:
        presets = json.load(f)
        print("=== Modem Config Preset Adder ===\n")

        # Get preset name
        preset_name = get_input_with_default("Enter a name for the new preset", "custom_preset")

        # Overwrite warning
        if preset_name in presets:
            print(f"Preset '{preset_name}' already exists. Overwriting. If you want to cancel, press Ctrl+C now.")

        # Get preset parameters: mode, vc_base, center_freq, rx_gain, tx_gain, auth, multimode, doppler, doppler_tle, no_follow, tle_doppler_config
        print("\nEnter preset parameters:")
        mode = ""
        while True:
            mode = get_input_with_default("Mode available values: usrp/soapy/soapy-buu (Soapy Backup-UHF)", "usrp")
            if mode in ["usrp", "soapy", "soapy_buu"]:
                break
            else:
                print("Invalid mode. Please enter 'usrp', 'soapy', or 'soapy-buu'.")
        
        vc_base = int(get_input_with_default("VC Base (integer)", 7100))
        center_freq = float(get_input_with_default("Center Frequency (Hz)", 437.125e6))
        rx_gain = float(get_input_with_default("RX Gain (dB)", 40.0))
        tx_gain = float(get_input_with_default("TX Gain (dB)", 80.0))
        while True:
            auth = get_input_with_default("Auth, available values: fm/spare/dev", "dev")
            if auth in ["fm", "spare", "dev"]:
                break
            else:
                print("Invalid auth. Please enter 'fm', 'spare', or 'dev'.")

        while True:
            multimode = get_input_with_default("Multimode, available values: true/false", "false")
            if multimode.lower() in ["true", "false"]:
                multimode = multimode.lower() == "true"
                break
            else:
                print("Invalid multimode. Please enter 'true' or 'false'.")

        while True:
            doppler = get_input_with_default("Doppler correction based on last packet, doppler TLE generally preferred (next parameter) available values: true/false", "false")
            if doppler.lower() in ["true", "false"]:
                doppler = doppler.lower() == "true"
                break
            else:
                print("Invalid doppler. Please enter 'true' or 'false'.")

        while True:
            doppler_tle = get_input_with_default("Doppler correction based on TLE, available values: true/false", "false")
            if doppler_tle.lower() in ["true", "false"]:
                doppler_tle = doppler_tle.lower() == "true"
                break
            else:
                print("Invalid doppler_tle. Please enter 'true' or 'false'.")

        while True:
            no_follow = get_input_with_default("No follow mode for frequency adjustments, available values: true/false", "false")
            if no_follow.lower() in ["true", "false"]:
                no_follow = no_follow.lower() == "true"
                break
            else:
                print("Invalid no_follow. Please enter 'true' or 'false'.")

        while True:
            tle_doppler_config = get_input_with_default("TLE Doppler config filename (must be in TLE_doppler_configs directory) or 'none' in case of no correction or for default file", "none")
            if tle_doppler_config.lower() == "none":
                tle_doppler_config = None
                break
            else:
                tle_path = os.path.join(os.path.dirname(__file__), '..', 'TLE_doppler_configs', tle_doppler_config)
                if os.path.isfile(tle_path):
                    tle_doppler_config = tle_path
                    break
                else:
                    print(f"File '{tle_doppler_config}' not found in TLE_doppler_configs. Please enter a valid filename or 'none'.")
        
        # Create preset dictionary
        preset = {
            "mode": mode,
            "vc_base": vc_base,
            "center_freq": center_freq,
            "rx_gain": rx_gain,
            "tx_gain": tx_gain,
            "auth": auth,
            "multimode": multimode,
            "doppler": doppler,
            "doppler_tle": doppler_tle,
            "no_follow": no_follow,
            "tle_doppler_config": tle_doppler_config
        }

        # Save preset
        presets[preset_name] = preset
        with open(presets_path, 'w') as f:
            json.dump(presets, f, indent=4)

        print(f"\n✓ Preset '{preset_name}' saved to: {presets_path}")

        