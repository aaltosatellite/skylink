"""
This script creates a new Doppler configuration file for TLE-based Doppler correction.

Can be used if skymodem is run in a new ground station or for a new satellite for example.
"""
import os
import json


def get_input_with_default(prompt, default):
    """Get user input with a default value."""
    user_input = input(f"{prompt} [{default}]: ").strip()
    return user_input if user_input else default


if __name__ == "__main__":
    print("=== TLE Doppler Configuration Generator ===\n")
    
    # Get ground station parameters
    print("Ground Station Parameters:")
    gs_latitude = float(get_input_with_default("Input ground station latitude (decimal degrees). If none will use OH2AGS latitude", "60.1871545"))
    gs_longitude = float(get_input_with_default("Input ground station longitude (decimal degrees). If none will use OH2AGS longitude", "24.8181605"))
    gs_elevation = float(get_input_with_default("Input ground station elevation (meters). If none will use OH2AGS elevation", "30"))
    
    print("\nSatellite Parameters:")
    satellite_name = get_input_with_default("Input satellite name. If none will use Foresail-1p", "Foresail-1p")
    norad_id = int(get_input_with_default("Input NORAD ID. If none will use Foresail-1p NORAD-ID", "66778"))
    
    print("\nOutput File:")
    filename = get_input_with_default("Give a filename (without path). If none will use custom_TLE_doppler_config.json", "custom_TLE_doppler_config.json")
    
    # Create configuration dictionary
    config = {
        "gs_latitude": gs_latitude,
        "gs_longitude": gs_longitude,
        "gs_elevation": gs_elevation,
        "satellite_name": satellite_name,
        "norad_id": norad_id
    }
    
    # Write to file
    filepath = os.path.join(os.path.dirname(__file__), filename)
    with open(filepath, 'w') as f:
        json.dump(config, f, indent=4)
    
    print(f"\n✓ Configuration saved to: {filepath}")
    print(f"\nConfiguration:")
    print(json.dumps(config, indent=4))
