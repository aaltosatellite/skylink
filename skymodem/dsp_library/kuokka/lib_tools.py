import numpy as np
from numba import njit, prange, objmode
import time
import sys
from skyfield.api import EarthSatellite, wgs84
import skyfield.api
import urllib.request
import os
import json
from datetime import datetime, timezone, timedelta


skyfield_timescale = skyfield.api.load.timescale()




# Speed of light for doppler calculations
c = 299792458.0

# Global satellite and groundstation objects (initialized by load_tle_by_norad_id)
satellite = None
groundstation = None


S100_CENTER_FREQUENCY = 437.7752e6
S100_SYNCHWORD 		= 0x930B51DE   	# Suomi100
S100_SYNCHWORD_LEN 	= 32
FS1P_SYNCHWORD 		= 0x1ACFFC1D	# Same for original FS1
FS1P_SYNCHWORD_LEN 	= 32



def load_satellite_and_gs_configs(config_path):
    """
    Load satellite and ground station configurations from a file into the globals `satellite` and `groundstation`.
    
    Args:
        config_path: Path to configuration file. (JSON)
    
    """

    print(f"Loading satellite and ground station configs from {config_path}...")
    global satellite
    global groundstation

    with open(config_path, 'r') as f:
        use_space_track = False
        config = json.load(f)
        norad_id = config['norad_id']
        satellite_name = config["satellite_name"]
        cache_file = os.path.join(os.path.dirname(__file__), '..', '..', 'tle_cache', f"{satellite_name.replace(' ', '_')}_tle.txt")
        using_cached = False

        # Check for TLE cache
        if os.path.exists(cache_file):
            # Don't spam space-track. Their rules say one TLE per hour and from experience they will ban the account if this rule is broken too much.
            # First line is the timestamp (ISO format) then two lines of TLE data.
            with open(cache_file, 'r') as f:
                lines = f.read().strip().split('\n')
                if len(lines) == 3:
                    tle_timestamp = datetime.fromisoformat(lines[0])
                    # Always load the TLE data. If something goes wrong with space-track, we can still use old TLE data. This is better than nothing.
                    tle_lines = lines[1:]
                    if (datetime.now(tz=timezone.utc) - tle_timestamp) < timedelta(hours=12):
                        print(f"Using cached TLE data for {satellite_name} from {tle_timestamp.isoformat()}")
                        print(f"Cached TLE data for {satellite_name}:\n{tle_lines[0]}\n{tle_lines[1]}")
                        satellite = EarthSatellite(tle_lines[0], tle_lines[1], satellite_name, skyfield_timescale)
                        using_cached = True
                    else:
                        print(f"Cached TLE data for {satellite_name} is older than 12 hours. Fetching new TLE data.")
                else:
                    print(f"TLE cache file for {satellite_name} is malformed. Fetching new TLE data.")
        tle_lines = None

        # Load TLE:
        if use_space_track and not using_cached:
            import requests
            URL = f"https://www.space-track.org/basicspacedata/query/class/gp/NORAD_CAT_ID/{norad_id}/orderby/EPOCH/format/tle"
            # In order to use space track, need to create credentials.json file in the same directory as this script
            # credentials.json should contain fields "identity" (username/email) and "password".
            # Also need to set use_space_track = True above. This feature is mostly for OH2AGS since Celestrak blocks access after too many requests.
            credentials = json.load(open(os.path.abspath(os.path.dirname(__file__)) + '/credentials.json'))
            with requests.Session() as session:
                # Login to Space Track
                resp = session.post('https://www.space-track.org/ajaxauth/login', data=credentials)
                if resp.status_code != 200:
                    raise Exception(f"Space Track login failed. Status code: {resp.status_code}")
                request = session.get(URL)
            response_lines = request.text.strip().split('\n')
            if len(response_lines) < 2:
                # Use cached TLE:
                print(f"Failed to fetch TLE data from Space Track for {satellite_name}. Status code: {request.status_code}. Using cached TLE data if available.")
                if len(tle_lines) == 2:
                    satellite = EarthSatellite(tle_lines[0], tle_lines[1], satellite_name, skyfield_timescale)
                    using_cached = True
                else:
                    raise Exception(f"No valid TLE data available for {satellite_name}.")
            # Add satellite name as first TLE line since Space Track does not provide it unless format 3le, which has slightly different line format.
            tle_lines = [satellite_name] + response_lines
            satellite = EarthSatellite(tle_lines[1], tle_lines[2], satellite_name, skyfield_timescale)
        elif not using_cached:
            try:
                request = urllib.request.urlopen(f"https://celestrak.com/NORAD/elements/gp.php?CATNR={norad_id}&FORMAT=tle")
                tle_lines = request.read().decode('utf-8').strip().split('\n')
                satellite = EarthSatellite(tle_lines[1], tle_lines[2], satellite_name, skyfield_timescale)
            except Exception as e:
                print(f"Error fetching TLE data from Celestrak for {satellite_name}: {e}")
                if len(tle_lines) == 2:
                    satellite = EarthSatellite(tle_lines[0], tle_lines[1], satellite_name, skyfield_timescale)
                    using_cached = True
                else:
                    raise Exception(f"No valid TLE data available for {satellite_name}.")

        # Write TLE to a file, Current path + satellite name_tle.txt:
        if not using_cached:
            with open(cache_file, 'w') as f:
                f.write(datetime.now(tz=timezone.utc).isoformat() + '\n')
                f.write('\n'.join(tle_lines[1:3]) + '\n')

        # Load Ground Station info
        gs_latitude = config["gs_latitude"]
        gs_longitude = config["gs_longitude"]
        gs_elevation = config["gs_elevation"]
        groundstation = wgs84.latlon(gs_latitude, gs_longitude, elevation_m=gs_elevation)

        print(f"Loaded TLE for {satellite_name} (NORAD ID: {norad_id})")
        print(f"Ground Station location: lat {gs_latitude} deg, lon {gs_longitude} deg, elev {gs_elevation} m")
        if len(tle_lines) == 3:
            print(f"Got TLE lines: \n{tle_lines[0]}\n{tle_lines[1]}\n{tle_lines[2]}")
        elif len(tle_lines) == 2:
            print(f"Got TLE lines: \n{tle_lines[0]}\n{tle_lines[1]}")


# = USRP B200/B210 VALID SAMPLERATES =========================================================================================================================================================
# = USRP B200/B210 VALID SAMPLERATES =========================================================================================================================================================
usrp_B200_valid_samplerates = np.array([
62500.0,    62992.1,    63492.1,    64000.0,    64516.1,    65040.7,    65573.8,    66115.7,
66666.7,    67226.9,    67796.6,    68376.1,    68965.5,    69565.2,    70175.4,    70796.5,
71428.6,    72072.1,    72727.3,    73394.5,    74074.1,    74766.4,    75471.7,    76190.5,
76923.1,    77669.9,    78431.4,    79207.9,    80000.0,    80808.1,    81632.7,    82474.2,
83333.3,    84210.5,    85106.4,    86021.5,    86956.5,    87912.1,    88888.9,    89887.6,
90909.1,    91954.0,    93023.3,    94117.6,    95238.1,    96385.5,    97561.0,    98765.4,
100000.0,   101266.0,   102564.0,   103896.0,   105263.0,   106667.0,   108108.0,   109589.0,
111111.0,   112676.0,   114286.0,   115942.0,   117647.0,   119403.0,   121212.0,   123077.0,
125000.0,   125984.0,   126984.0,   128000.0,   129032.0,   130081.0,   131148.0,   132231.0,
133333.0,   134454.0,   135593.0,   136752.0,   137931.0,   139130.0,   140351.0,   141593.0,
142857.0,   144144.0,   145455.0,   146789.0,   148148.0,   149533.0,   150943.0,   152381.0,
153846.0,   155340.0,   156863.0,   158416.0,   160000.0,   161616.0,   163265.0,   164948.0,
166667.0,   168421.0,   170213.0,   172043.0,   173913.0,   175824.0,   177778.0,   179775.0,
181818.0,   183908.0,   186047.0,   188235.0,   190476.0,   192771.0,   195122.0,   197531.0,
200000.0,   202532.0,   205128.0,   207792.0,   210526.0,   213333.0,   216216.0,   219178.0,
222222.0,   225352.0,   228571.0,   231884.0,   235294.0,   238806.0,   242424.0,   246154.0,
250000.0,   251969.0,   253968.0,   256000.0,   258065.0,   260163.0,   262295.0,   264463.0,
266667.0,   268908.0,   271186.0,   273504.0,   275862.0,   278261.0,   280702.0,   283186.0,
285714.0,   288288.0,   290909.0,   293578.0,   296296.0,   299065.0,   301887.0,   304762.0,
307692.0,   310680.0,   313725.0,   316832.0,   320000.0,   323232.0,   326531.0,   329897.0,
333333.0,   336842.0,   340426.0,   344086.0,   347826.0,   351648.0,   355556.0,   359551.0,
363636.0,   367816.0,   372093.0,   376471.0,   380952.0,   385542.0,   390244.0,   395062.0,
400000.0,   405063.0,   410256.0,   415584.0,   421053.0,   426667.0,   432432.0,   438356.0,
444444.0,   450704.0,   457143.0,   463768.0,   470588.0,   477612.0,   484848.0,   492308.0,
500000.0,   507937.0,   516129.0,   524590.0,   533333.0,   542373.0,   551724.0,   561404.0,
571429.0,   581818.0,   592593.0,   603774.0,   615385.0,   627451.0,   640000.0,   653061.0,
666667.0,   680851.0,   695652.0,   711111.0,   727273.0,   744186.0,   761905.0,   780488.0,
800000.0,   820513.0,   842105.0,   864865.0,   888889.0,   914286.0,   941176.0,   969697.0,
1000000.0,  1032260.0,  1066670.0,  1103450.0,  1142860.0,  1185190.0,  1230770.0,  1280000.0,
1333330.0,  1391300.0,  1454550.0,  1523810.0,  1600000.0,  1684210.0,  1777780.0,  1882350.0,
2000000.0,  2133330.0,  2285710.0,  2461540.0,  2666670.0,  2909090.0,  3200000.0,  3555560.0,
4000000.0,  4571430.0,  5333330.0,  6400000.0,  8000000.0,  10666700.0, 16000000.0, 32000000.0,],dtype=np.float64)
# = USRP B200/B210 VALID SAMPLERATES =========================================================================================================================================================
# = USRP B200/B210 VALID SAMPLERATES =========================================================================================================================================================



# = CCSDS TM RANDOMIZER ======================================================================================================================================================================
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================
# Suomi100 uses this
ccsds_tm_whitening_bytes = np.array([
0xff, 0x48, 0xe,  0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e,
0xa,  0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c,
0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20, 0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2,
0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7,
0xfa, 0x40, 0x76, 0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0,
0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5,
0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,  0xd8, 0x13, 0x41, 0xae, 0x17, 0x91,
0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f,
0xd2, 0x3,  0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf, 0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82,
0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d,
0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,  0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,  0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e,
0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e, 0xa,  0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe,
0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14,
0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20, 0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a,
0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76, 0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71,
0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4,
0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5, 0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1,
0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,  0xd8, 0x13, 0x41, 0xae, 0x17, 0x91, 0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52,
0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x3,  0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b,
0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf, 0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4,
0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,
0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,  0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97,
0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e, 0xa,  0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59,
0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20,
0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43,
0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76, 0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb,
0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9,
0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5, 0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,
0xd8, 0x13, 0x41, 0xae, 0x17, 0x91, 0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e,
0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x3,  0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf,
0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49,
0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,  0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,
0xc0, 0x9a, 0xd,  0x70, 0xbc, 0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce, 0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e, 0xa,  0x10, 0xf1,
0x88, 0x94, 0xcd, 0xea, 0xb1, 0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79, 0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c, 0xb5, 0x2e, 0xfb,
0x98, 0x65, 0x45, 0x7e, 0x7c, 0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63, 0xfd, 0x20, 0x3b, 0x2,  0x68, 0x35, 0xc2, 0xf2, 0x38, 0xb2, 0x4e,
0xb6, 0x9e, 0xdd, 0x1b, 0x39, 0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8, 0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7, 0xfa, 0x40, 0x76,
0x4,  0xd0, 0x6b, 0x85, 0xe4, 0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72, 0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0, 0x50, 0x87, 0x8c,
0x44, 0xa6, 0x6f, 0x55, 0x8f, 0xf4, 0x80, 0xec, 0x9,  0xa0, 0xd7, 0xb,  0xc8, 0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5, 0xa9, 0x77, 0xdc,
0xc3, 0x2a, 0x2b, 0xf3, 0xe0, 0xa1, 0xf,  0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f, 0xe9, 0x1,  0xd8, 0x13, 0x41, 0xae, 0x17, 0x91, 0xc5, 0x92, 0x75,
0xb4, 0xf6, 0xe8, 0xd9, 0xcb, 0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1, 0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f, 0xd2, 0x3,  0xb0,
0x26, 0x83, 0x5c, 0x2f, 0x23, 0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96, 0xa5, 0xdf, 0x73, 0xc,  0xa8, 0xaf, 0xcf, 0x82, 0x84, 0x3c, 0x62,
0x25, 0x33, 0x7a, 0xac, 0x7f, 0xa4, 0x7,  0x60, 0x4d, 0x6,  0xb8, 0x5e, 0x47, 0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d, 0x4b, 0xbe, 0xe6,
0x19, 0x51, 0x5f, 0x9f, 0x5,  0x8,  0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff, 0x48, 0xe,  0xc0, ],dtype=np.int32)

def CCSDS_TM_whitener_sequence(n):
    state = 0xff
    outp = 0
    bitarr = []
    bytearr = []
    for ii in range(n):
        outp = ((outp<<1) + (state & 1)) & 0xff
        if (ii%8) == 7:
            bytearr.append(outp)
        bitarr.append(state & 1)
        state = (state>>1) + ((((state >> 7) & 1) ^ ((state >> 5) & 1) ^ ((state >> 3) & 1) ^ (state & 1)) << 7)
    return np.array(bytearr, dtype=np.int32), np.array(bitarr, dtype=np.int8)
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================
# = CCSDS TM RANDOMIZER ======================================================================================================================================================================



## PN9 SEQUENCE ==============================================================================================================================================================================
## PN9 SEQUENCE ==============================================================================================================================================================================
PN9_bytes = np.array([255, 225, 29, 154, 237, 133, 51, 36, 234, 122, 210, 57, 112, 151, 87, 10, 84, 125, 45, 216, 109, 13, 186, 143, 103, 89, 199, 162, 191, 52,
                         202, 24, 48, 83, 147, 223, 146, 236, 167, 21, 138, 220, 244, 134, 85, 78, 24, 33, 64, 196, 196, 213, 198, 145, 138, 205, 231, 209, 78, 9, 50,
                         23, 223, 131, 255, 240, 14, 205, 246, 194, 25, 18, 117, 61, 233, 28, 184, 203, 43, 5, 170, 190, 22, 236, 182, 6, 221, 199, 179, 172, 99, 209,
                         95, 26, 101, 12, 152, 169, 201, 111, 73, 246, 211, 10, 69, 110, 122, 195, 42, 39, 140, 16, 32, 98, 226, 106, 227, 72, 197, 230, 243, 104, 167,
                         4, 153, 139, 239, 193, 127, 120, 135, 102, 123, 225, 12, 137, 186, 158, 116, 14, 220, 229, 149, 2, 85, 95, 11, 118, 91, 131, 238, 227, 89, 214,
                         177, 232, 47, 141, 50, 6, 204, 212, 228, 183, 36, 251, 105, 133, 34, 55, 189, 97, 149, 19, 70, 8, 16, 49, 113, 181, 113, 164, 98, 243, 121, 180,
                         83, 130, 204, 197, 247, 224, 63, 188, 67, 179, 189, 112, 134, 68, 93, 79, 58, 7, 238, 242, 74, 129, 170, 175, 5, 187, 173, 65, 247, 241, 44, 235,
                         88, 244, 151, 70, 25, 3, 102, 106, 242, 91, 146, 253, 180, 66, 145, 155, 222, 176, 202, 9, 35, 4, 136, 152, 184, 218, 56, 82, 177, 249, 60, 218,
                         41, 65, 230, 226, 123, 240, 31, 222, 161, 217, 94, 56, 67, 162, 174, 39, 157, 3, 119, 121, 165, 64, 213, 215, 130, 221, 214, 160, 251, 120, 150,
                         117, 44, 250, 75, 163, 140, 1, 51, 53, 249, 45, 201, 126, 90, 161, 200, 77, 111, 88, 229, 132, 17, 2, 68, 76, 92, 109, 28, 169, 216, 124, 30, 237,
                         148, 32, 115, 241, 61, 248, 15, 239, 208, 108, 47, 156, 33, 81, 215, 147, 206, 129, 187, 188, 82, 160, 234, 107, 193, 110, 107, 208, 125, 60, 203,
                         58, 22, 253, 165, 81, 198, 128, 153, 154, 252, 150, 100, 63, 173, 80, 228, 166, 55, 172, 114, 194, 8, 1, 34, 38, 174, 54, 142, 84, 108, 62, 143,
                         118, 74, 144, 185, 248, 30, 252, 135, 119, 104, 182, 23, 206, 144, 168, 235, 73, 231, 192, 93, 94, 41, 80, 245, 181, 96, 183, 53, 232, 62, 158, 101,
                         29, 139, 254, 210, 40, 99, 192, 76, 77, 126, 75, 178, 159, 86, 40, 114, 211, 27, 86, 57, 97, 132, 0, 17, 19, 87, 27, 71, 42, 54, 159, 71, 59, 37,
                         200, 92, 124, 15, 254, 195, 59, 52, 219, 11, 103, 72, 212, 245, 164, 115, 224, 46, 175, 20, 168, 250, 90, 176, 219, 26, 116, 31, 207, 178, 142, 69,
                         127, 105, 148, 49, 96, 166, 38, 191, 37, 217, 79, 43, 20, 185, 233, 13, 171, 156, 48, 66, 128, 136, 137, 171, 141, 35, 21, 155, 207, 163, 157, 18,
                         100, 46, 190, 7, 255, 225, 29, 154, 237, 133, 51, 36, 234, 122, 210, 57, 112, 151, 87, 10, 84, 125, 45, 216, 109, 13, 186, 143, 103, 89, 199, 162,
                         191, 52, 202, 24, 48, 83, 147, 223, 146, 236, 167, 21, 138, 220, 244, 134, 85, 78, 24, 33, 64, 196, 196, 213, 198, 145, 138, 205, 231, 209, 78, 9,
                         50, 23, 223, 131, 255, 240, 14, 205, 246, 194, 25, 18, 117, 61, 233, 28, 184, 203, 43, 5, 170, 190, 22, 236, 182, 6, 221, 199, 179, 172, 99, 209,
                         95, 26, 101, 12, 152, 169, 201, 111, 73, 246, 211, 10, 69, 110, 122, 195, 42, 39, 140, 16, 32, 98, 226, 106, 227, 72, 197, 230, 243, 104, 167, 4,
                         153, 139, 239, 193, 127, 120, 135, 102, 123, 225, 12, 137, 186, 158, 116, 14, 220, 229, 149, 2, 85, 95, 11, 118, 91, 131, 238, 227, 89, 214, 177,
                         232, 47, 141, 50, 6, 204, 212, 228, 183, 36, 251, 105, 133, 34, 55, 189, 97, 149, 19, 70, 8, 16, 49, 113, 181, 113, 164, 98, 243, 121, 180, 83, 130,
                         204, 197, 247, 224, 63, 188, 67, 179, 189, 112, 134, 68, 93, 79, 58, 7, 238, 242, 74, 129, 170, 175, 5, 187, 173, 65, 247, 241, 44, 235, 88, 244, 151,
                         70, 25, 3, 102, 106, 242, 91, 146, 253, 180, 66, 145, 155, 222, 176, 202, 9, 35, 4, 136, 152, 184, 218, 56, 82, 177, 249, 60, 218, 41, 65, 230, 226, 123,
                         240, 31, 222, 161, 217, 94, 56, 67, 162, 174, 39, 157, 3, 119, 121, 165, 64, 213, 215, 130, 221, 214, 160, 251, 120, 150, 117, 44, 250, 75, 163, 140, 1,
                         51, 53, 249, 45, 201, 126, 90, 161, 200, 77, 111, 88, 229, 132, 17, 2, 68, 76, 92, 109, 28, 169, 216, 124, 30, 237, 148, 32, 115, 241, 61, 248, 15, 239,
                         208, 108, 47, 156, 33, 81, 215, 147, 206, 129, 187, 188, 82, 160, 234, 107, 193, 110, 107, 208, 125, 60, 203, 58, 22, 253, 165, 81, 198, 128, 153, 154,
                         252, 150, 100, 63, 173, 80, 228, 166, 55, 172, 114, 194, 8, 1, 34, 38, 174, 54, 142, 84, 108, 62, 143, 118, 74, 144, 185, 248, 30, 252, 135, 119, 104,
                         182, 23, 206, 144, 168, 235, 73, 231, 192, 93, 94, 41, 80, 245, 181, 96, 183, 53, 232, 62, 158, 101, 29, 139, 254, 210, 40, 99, 192, 76, 77, 126, 75,
                         178, 159, 86, 40, 114, 211, 27, 86, 57, 97, 132, 0, 17, 19, 87, 27, 71, 42, 54, 159, 71, 59, 37, 200, 92, 124, 15, 254, 195, 59, 52, 219, 11, 103, 72,
                         212, 245, 164, 115, 224, 46, 175, 20, 168, 250, 90, 176, 219, 26, 116, 31, 207, 178, 142, 69, 127, 105, 148, 49, 96, 166, 38, 191, 37, 217, 79, 43, 20,
                         185, 233, 13, 171, 156, 48, 66, 128, 136, 137, 171, 141, 35, 21, 155, 207, 163, 157, 18, 100, 46, 190, 7, 255, 225, 29], dtype=np.uint8)


def PN9_whitener_byte_sequence(n):
    PN9 = 0xff
    bytearr = [PN9,]
    for ii in range(n):
        if (ii%8) == 7:
            bytearr.append(PN9 & 0xff)
        PN9 = (PN9>>1) + ((((PN9 >> 5) & 1) ^ (PN9 & 1)) << 8)
    return np.array(bytearr, dtype=np.int32)
## PN9 SEQUENCE ==============================================================================================================================================================================
## PN9 SEQUENCE ==============================================================================================================================================================================




## CC1125 SETTINGS ===========================================================================================================================================================================
## CC1125 SETTINGS ===========================================================================================================================================================================
SRATE_M_153k6 = 0x0f7510
SRATE_E_153k6 = 0x0a
SRATE_M_76k8  = 0x0f7510
SRATE_E_76k8  = 0x09
SRATE_M_38k4  = 0x0f7510
SRATE_E_38k4  = 0x08
SRATE_M_19k2  = 0x0f7510
SRATE_E_19k2  = 0x07
SRATE_M_9k6   = 0x0f7510
SRATE_E_9k6   = 0x06
SRATE_M_4k8   = 0x0f7510
SRATE_E_4k8   = 0x05
SRATE_M_2k4   = 0x0f7510
SRATE_E_2k4   = 0x04
SRATE_M_1k2   = 0x0f7510
SRATE_E_1k2   = 0x03
SRATE_M_0k6   = 0x0f7510
SRATE_E_0k6   = 0x02
SRATE_M_0k3   = 0x0f7510
SRATE_E_0k3   = 0x01
SRATE_M_0k150 = 0x0fba88
SRATE_E_0k150 = 0x00


def CC1125_symbolrate_for_M_E(SRATE_M, SRATE_E):  # maxdf = 2.384185791015625
    assert 0 <= SRATE_E < 16
    assert 0 <= SRATE_M < (2**20)
    if SRATE_E == 0:
        return 40e6 * SRATE_M / 2**38
    return 40e6 * (2**20 + SRATE_M) * (2**SRATE_E) / (2**39)

def CC1125_M_E_for_symbolrate(symbolrate):
    assert 0 < symbolrate <= 4999997.615814209
    if symbolrate <= CC1125_symbolrate_for_M_E(SRATE_M=0x0fffff, SRATE_E=0):
        return int(symbolrate * (2**38) / 40e6), 0
    E = int(np.log2( symbolrate * 2**39 / 40e6 ) - 20)
    M = int((symbolrate * 2**39 / (40e6 * 2**E)) - 2**20)
    return M, E

#    (40e6 / 2**24) * (256 + DEV_M) * 2**DEV_E     	|| where DEV_M is int8 and DEV_E is int3
def CC1125_peak_deviation_for_M_E(DEV_M, DEV_E):
    assert 0 <= DEV_E <= (2**3 -1)
    assert 0 <= DEV_M <= (2**8 -1)
    if DEV_E == 0:
        return (40e6/(2**23)) * DEV_M
    return (40e6/(2**24)) * (256+DEV_M) * 2**DEV_E

def CC1125_DEV_M_E_for_peak_deviation(f_dev):
    if f_dev < 256 * 40e6 / (2**23):
        E = 0
        M = int(f_dev * 2**23 / 40e6)
        assert M < 256
        return M, E
    E = int(np.log( f_dev * 2**24 /(40e6 * 256) ) / np.log(2))
    M = int((f_dev * 2**24 / ((2**E) * 40e6)) - 256)
    assert E < 8
    assert M < 256
    return M, E

def CC1125_DEV_M_E_config_for_peak_deviation(f_dev, BT_on):
    assert BT_on in (0,1)
    if f_dev < 256 * 40e6 / (2**23):
        E = 0
        M = int(f_dev * 2**23 / 40e6)
        assert M < 256
        if BT_on == 1:
            E = E | 0x8
        return M, E
    E = int(np.log( f_dev * 2**24 /(40e6 * 256) ) / np.log(2))
    M = int((f_dev * 2**24 / ((2**E) * 40e6)) - 256)
    assert E < 8
    assert M < 256
    if BT_on == 1:
        E = E | 0x8
    return M, E
## CC1125 SETTINGS ===========================================================================================================================================================================
## CC1125 SETTINGS ===========================================================================================================================================================================






# SAMPLE GENERATION ==========================================================================================================================================================================
# SAMPLE GENERATION ==========================================================================================================================================================================

# 1 / (2 * √ (2 ln(2))) 
GAUSS_STANDARD_DEVIATION_PER_HALFPOINTS = 1 / (2 * np.sqrt(2 * np.log(2)))

@njit(cache=True)
def gauss_curve(standard_deviation, sample_offsets):
    """
    Generate Gaussian curve values for given standard deviation and sample offsets.
    """
    a = 1/(standard_deviation * np.sqrt(2 * np.pi))
    return a * np.exp(-0.5 * ((sample_offsets / standard_deviation) ** 2))

@njit(cache=True)
def gauss_curve_sps(sps_f, BT, n_taps):
    standard_deviation = sps_f * GAUSS_STANDARD_DEVIATION_PER_HALFPOINTS / (2*BT)

    # Offsets centered around zero
    sample_offsets = np.linspace(-1.0, 1.0, n_taps) * (n_taps-1)

    # Generate Gaussian curve
    curve = gauss_curve(standard_deviation, sample_offsets)
    
    # Set area under curve to 1.0
    return curve / np.sum(curve)


@njit(cache=True)
def make_squarewave(binary_symbols, sps_f, i_sample_of_sym0_f, nsamples, npad):
    """

    """
    if nsamples < 0:
        nsamples = int(len(binary_symbols) * sps_f + i_sample_of_sym0_f)
    samples = np.zeros(nsamples)
    for i in range(nsamples):
        isym = int((i - i_sample_of_sym0_f) / sps_f)
        if isym < 0:
            continue
        if isym < len(binary_symbols):
            samples[i] = binary_symbols[isym]
    if npad > 0:
        pad = np.zeros(npad, dtype=np.float64)
        samples = np.concatenate( (pad, samples, pad) )
    return samples


@njit(cache=True, parallel=True)
def make_frequency_modulating_waveform_parallel(binary_symbols, sps_f, shaper_BT_prod, shaper_n_taps):
    """
    Make frequency modulating waveform utilizing Numba parallelization.

    Requires Python 3.11+
    """
    assert (shaper_BT_prod > 0) or (shaper_BT_prod == -1)

    # Create pulse shaping filter
    if shaper_BT_prod > 0:
        pulse = gauss_curve_sps(sps_f=sps_f, BT=shaper_BT_prod, n_taps=shaper_n_taps)
        assert len(pulse) == shaper_n_taps
    else: # No gaussian pulse shaping
        pulse = np.ones(1, dtype=np.float64)

    npulse = len(pulse)
    modulator0 = make_squarewave(binary_symbols=binary_symbols, sps_f=sps_f, i_sample_of_sym0_f=0.0, nsamples=-1, npad=npulse//2)

    if npulse > 1:
        modulator1 = np.zeros(len(modulator0)-npulse+1, dtype=np.float64)
        for i in prange(len(modulator1)):
            modulator1[i] = np.sum(pulse * modulator0[i:i+npulse])
    else:
        modulator1 = modulator0

    modulator1 = modulator1 / np.max(np.abs(modulator1))
    return modulator1

@njit(cache=True, parallel=False)
def make_frequency_modulating_waveform_simple(binary_symbols, sps_f, shaper_BT_prod, shaper_n_taps):
    """
    Make frequency modulating waveform without parallelization.

    This will be used on Python versions older than 3.11
    """
    assert (shaper_BT_prod > 0) or (shaper_BT_prod == -1)

    if shaper_BT_prod > 0:
        pulse = gauss_curve_sps(sps_f=sps_f, BT=shaper_BT_prod, n_taps=shaper_n_taps)
        assert len(pulse) == shaper_n_taps
    else:
        pulse = np.ones(1, dtype=np.float64)

    npulse = len(pulse)
    modulator0 = make_squarewave(binary_symbols=binary_symbols, sps_f=sps_f, i_sample_of_sym0_f=0.0, nsamples=-1, npad=npulse//2)
    if npulse > 1:
        modulator1 = np.correlate(modulator0, pulse)
    else:
        modulator1 = modulator0

    modulator1 = modulator1 / np.max(np.abs(modulator1))
    return modulator1


# Determine which version to use based on Python version.
if int(sys.version.split(" ")[0].split(".")[1]) >= 11:
    make_f_modulating_waveform = make_frequency_modulating_waveform_parallel
else:
    make_f_modulating_waveform = make_frequency_modulating_waveform_simple


@njit(cache=True)
def fm_mod(f_signal_offset, peak_deviation, modulator):
    """


    """
    assert np.min(modulator) >= -1.0
    assert np.max(modulator) <=  1.0
    assert abs(f_signal_offset) < 0.5
    nn = len(modulator)

    cs_mod = np.zeros(nn, dtype=np.float64)
    cs_mod[0] = modulator[0]
    for i in range(1,nn):
        cs_mod[i] = modulator[i] + cs_mod[i-1]

    signal = np.exp((2j*np.pi) * (f_signal_offset * np.arange(nn) + cs_mod * peak_deviation)) # TODO: make the frequency offset it's own exp-multiplication. Math would be cleaner.
    return signal


@njit(cache=True)
def fm_mod_expanding(f_signal_offset, peak_deviation, modulator, nsamples):
    """
    Expanding FM modulator. 

    params:
        f_signal_offset: Normalized frequency offset to apply to the signal.
        peak_deviation:  Peak frequency deviation
    """
    assert np.min(modulator) >= -1.0
    assert np.max(modulator) <=  1.0
    assert abs(f_signal_offset) < 0.5
    ratio = float(len(modulator)) / nsamples
    cs_mod = np.zeros(nsamples, dtype=np.float64)
    cs_mod[0] = modulator[0]
    for i in range(1,nsamples):
        im = int(ratio * i)
        cs_mod[i] = modulator[im] + cs_mod[i-1]
    signal = np.exp((2j*np.pi) * (f_signal_offset * np.arange(nsamples) + cs_mod * peak_deviation)) # TODO: make the frequency offset it's own exp-multiplication. Math would be cleaner.
    return signal



"""
# Apparently max deviation of CC1125 is about 155.9 kHz.          (40e6 / 2**24) * (256 + DEV_M) * 2**DEV_E     	|| where DEV_M is int8 and DEV_E is int3
# 															 or   (40e6 / 2**23) * DEV_M  						|| if DEV_E = 0
# peak_dev = modulation_index / (2*symboltime).                          peak_dev_physical = peak_dev * sr. accords to CC1125 (CC112X/CC1175) User's guide on page 26.
"""

#@njit(cache=True)
def make_samples(samples_per_symbol, bitstring, frequency_offset, power, modulation_index=0.5, shaper_BT_prod=0.5, n_silence_start=0, n_silence_end=0):
    """
    Main sample generation function used in the modem.

    The main difference between this and make_samples_alternative() is that this function uses an expanding FM modulator.

    params:
        samples_per_symbol: Output samples per symbol.
        bitstring: Array of +/- 1 values representing the bits to be modulated.
        frequency_offset: Normalized frequency offset (to sample rate) to apply to the modulated signal.
        power: Output power scaling factor.
        modulation_index:   Used modulation index or in other words the maximum frequency deviation relative to the symbol rate.
                            If this is not 0.5, the modulation is not minimum shift keying (MSK).
                            However, a value of 0.7 has been found to give better performance and can be used on the satellite.
        shaper_BT_prod: Gaussian filter BT product. Product of bandwidth and symbol time.
                        Lower values give narrower bandwidth, which is harder to demodulate.
                        There can also be inter-symbol interference if the value is too low.
                        Higher values give wider bandwidth, which is easier to demodulate. 0.5 is a commonly used balanced value.
        n_silence_start: Number of samples of silence to add to the start of the signal to account for hardware ramp-up times.
        n_silence_end: Number of samples of silence to add to the end of the signal to account for hardware ramp-down times.
    """
    # Make sure that frequency offset is normalized to sample rate.
    assert abs(frequency_offset) < 0.5, frequency_offset
    assert (shaper_BT_prod > 0) or (shaper_BT_prod == -1)
    
    # Bits need to be +/- 1 not 0/1.
    assert np.all(np.isclose(np.abs(bitstring[0:34]), 1))

    # Modulation index (h) = 2 * peak_frequency_deviation * symbol_time.
    # This is also normalized to the sample rate like the frequency offset.
    # Since samples_per_symbol = symbol_time * sample_rate, we have:
    peak_frequency_deviation	= modulation_index / (samples_per_symbol * 2.0)

    # Always use 14 samples per symbol.
    modulation_samples_per_symbol = 14.0
    nsamples 	= int(len(bitstring) * samples_per_symbol)

    # Different function called based on whether parallel modulator can be used. The parallel version requires Python 3.11+.
    modulator 	= make_f_modulating_waveform(bitstring, modulation_samples_per_symbol, shaper_BT_prod, int(modulation_samples_per_symbol) * 4 + 1)
    
    # Use expanding FM modulator to get desired number of samples.
    samples 	= fm_mod_expanding(frequency_offset, peak_frequency_deviation, modulator, nsamples)
    
    # Scale to desired power.
    if power != 1:
        samples = samples * (power**0.5)

    # Add silence to start or end. This is used to workaround hardware limitations such as PA ramp-up times.
    if (n_silence_start > 0) or (n_silence_end > 0):
        samples = np.concatenate( (np.zeros(n_silence_start, dtype=np.complex128), samples, np.zeros(n_silence_end, dtype=np.complex128)) )
    return samples, modulator


def make_samples_alternative(samples_per_symbol, bitstring, frequency_offset, power, modulation_index=0.5, shaper_BT_prod=0.5, shaper_n_taps=301, n_silence_start=0, n_silence_end=0):
    """
    Alternative sample generation function. make_samples() is currently used in the modem.
    """
    assert abs(frequency_offset) < 0.5, frequency_offset
    assert (shaper_BT_prod > 0) or (shaper_BT_prod == -1)
    assert np.all(np.isclose(np.abs(bitstring[0:34]), 1))
    peak_dev	= modulation_index / (samples_per_symbol*2.0)
    modulator 	= make_f_modulating_waveform(bitstring, samples_per_symbol, shaper_BT_prod, shaper_n_taps)
    samples 	= fm_mod(frequency_offset, peak_dev, modulator)
    if power != 1:
        samples 	= samples * (power**0.5)
    if (n_silence_start > 0) or (n_silence_end > 0):
        samples 	= np.concatenate( (np.zeros(n_silence_start, dtype=np.complex128), samples, np.zeros(n_silence_end, dtype=np.complex128)) )
    return samples, modulator





@njit(cache=True)
def bytes_to_bits(data):
    """
    Creates a int64 array of bits from a byte array.

    TODO: Why int64?
    """
    bits = np.zeros(len(data)*8, dtype=np.int64)
    for i in range(len(data)):
        for j in range(8):
            bits[i*8+j] = (data[i]>>j) & 1
    return bits


@njit(cache=True)
def ints_to_bits(int_arr, bits_per_int):
    """
    Creates a int64 array of bits from an integer array.
    
    TODO: Why int64?
    """
    bits = np.zeros(len(int_arr)*bits_per_int, dtype=np.int64)
    for i in range(len(int_arr)):
        for j in range(bits_per_int):
            bits[i*bits_per_int+j] = (int_arr[i]>>(bits_per_int-(j+1))) & 1
    return bits

@njit(cache=True)
def radionoise(n, sr, W_per_Hz):
    """
    :param n: number of samples
    :param sr: samplerate
    :param W_per_Hz: spectral power density (Watts per Hertz)
    :return: n samples of gaussian IQ noise
    To verify signal energy, and spectral power density:
            with df = sr/n
            n * absfft**2 * (sr/n)**2  = W_per_Hz * sr
            absfft * sqrt(n) * (sr/n) = sqrt(W_per_Hz * sr)
            absfft = sqrt(W_per_Hz * sr) * sqrt(n)/sr
            absfft = sqrt(W_per_Hz * n / sr)
            absfft * sqrt(sr/n) = sqrt(W_per_Hz)
            W_per_Hz = absfft**2 * (sr/n)
            W_per_Hz * n2*df = W =  absfft**2 * (sr/n)**2 * n2
            sum((fft(samples)*df)**2) ≈ W_per_Hz * sr
            - Each fft-bin is (2*f_Nyquist) / n  Hertz wide.
            - fft bins represent amplitudes of constituent component frquencies.
    """
    cc = (0.5*W_per_Hz*sr)**0.5
    return cc * (np.random.normal(0,1.0, n) + 1j*np.random.normal(0, 1.0, n))

def signal_energy(samples, sr):
    """
    returns exactly the same value as:   np.sum(np.abs(samples)**2 * dt)     | dt = 1/sr
        == np.sum(np.abs(np.fft.fft(samples))**2) * df / (sr**2)                 | df = sr/len(samples)   (valid for even and odd samplecounts)
            E = P*t,    t = n/sr = len(samples)/sr.
        => 	P = E/t = E * sr/n    = sumabsp2 * (1 / (len(samples) * sr))   * sr/len(samples)
        =>	P = sumabsp2 * 1 / len(samples)**2
    """
    return np.sum(np.abs(np.fft.fft(samples))**2) / (len(samples) * sr)

def signal_power(samples):
    # returns exactly the same value as:   np.sum(np.abs(samples)**2) * dt / (dt*len(samples))     | dt = 1/sr
    return np.sum(np.abs(np.fft.fft(samples))**2) / (len(samples)**2)

# SAMPLE GENERATION ==========================================================================================================================================================================
# SAMPLE GENERATION ==========================================================================================================================================================================








# FFT ========================================================================================================================================================================================
# FFT ========================================================================================================================================================================================

@njit(cache=True)
def njit_objmode_fft(x):
    """
    
    """
    with objmode(y='complex128[:]'):
        y = np.complex128(np.fft.fftshift(np.fft.fft(x)))
    return y


def time_fft_n(fftlen, nrep):
    """
    Times the FFT of length fftlen, nrep times, returning average time per FFT.
    """
    s = np.random.normal(0,1, fftlen) + np.random.normal(0,1,fftlen)*1j
    njit_objmode_fft(s)
    njit_objmode_fft(s)
    t0 = time.perf_counter()
    for _ in range(nrep):
        _ = njit_objmode_fft(s)
    dt = (time.perf_counter() - t0) / nrep
    return dt


def choose_fftlen(len_ideal, window_halfwidth):
    """
    Chooses the fastest FFT length near len_ideal.

    Candidates are chosen from the ideal length +/- window_halfwidth.
    """
    assert len_ideal > 12
    assert window_halfwidth >= 1
    assert window_halfwidth < len_ideal
    candidates = [x for x in range(int(len_ideal-window_halfwidth), int(len_ideal+window_halfwidth)) if (x%2)==0]
    best_speed = 0.0
    best_len = int(len_ideal)
    for l in candidates:
        dt = time_fft_n(fftlen=l, nrep=80)
        speed = l/dt
        if speed > best_speed:
            best_speed = speed
            best_len = l
    return best_len, best_speed
# FFT ========================================================================================================================================================================================
# FFT ========================================================================================================================================================================================








# FREQUENCY MANAGEMENT =======================================================================================================================================================================
# FREQUENCY MANAGEMENT =======================================================================================================================================================================
# ============================================================================================================================================================================================
#@njit(cache=True)
def create_freq_shifter_precomp(sr, fdelta, max_batchlen, fdelta_threshold):  # fdelta_threshold can be something like (1e-6 * sr).
    """
    
    """
    assert sr > 0.0
    assert abs(fdelta) < sr*0.5
    assert max_batchlen > 1
    assert 0.0 < fdelta_threshold < (0.5*sr)
    m = 4
    while True:
        m += 1
        assert m < 1e6
        n = int(round(fdelta / (sr/m)))
        fdelta_actual = n * (sr/m)
        err = abs(fdelta_actual - fdelta)
        if err >= fdelta_threshold:
            continue
        assert abs(fdelta-fdelta_actual) < fdelta_threshold
        shifter = np.exp(2j*np.pi * (n/m) * np.arange(max_batchlen + m + 2))   # (n/m) === (fdelta_actual/sr)
        # np.angle(arr[i]) === np.angle(arr[i+m]) === np.angle(arr[i%m])
        for _ in range(32):
            i = np.random.randint(len(shifter) - (m+1))
            d1 = (np.angle(shifter[i]) - np.angle(shifter[i+m])) % (np.pi*2)
            d2 = (np.angle(shifter[i]) - np.angle(shifter[i%m])) % (np.pi*2)
            b1 = np.isclose(d1, 0.0) or np.isclose(d1, np.pi*2)
            b2 = np.isclose(d2, 0.0) or np.isclose(d2, np.pi*2)
            assert b1, d1
            assert b2, d2
        return shifter, m, fdelta_actual


@njit(cache=True)
def freq_shift_phased_precomp(batch, shifter_arr, phase_idx0, phase_mod):
    """
    Precompiled frequency shifter.
    """
    shifted = np.zeros_like(batch)  # opening the vector multiplication above into a for-loop makes the numba-accelerated version ~10% faster.
    for i in range(len(shifted)):
        shifted[i] = batch[i] * shifter_arr[i + phase_idx0]
    phase_idx1 = (phase_idx0 + len(batch)) % phase_mod
    return shifted, phase_idx1
# ============================================================================================================================================================================================


@njit(cache=True)
def freq_shift_phased(batch, sr, fdelta, phase0):
    """
    
    """
    shifted = batch * np.exp(2j*np.pi*(fdelta/sr)*np.arange(len(batch)) + phase0*1j)
    phase1 = (phase0 + 2*np.pi*(fdelta/sr)*len(batch)) % (2*np.pi)
    return shifted, phase1


def get_doppler_low_high(f_center, v_relative):
    """
    Get the lowest and highest expected frequencies due to Doppler shift.
    """
    c = 299792458.0
    df_doppler = f_center * (((c+abs(v_relative))/c) - 1)
    f_center_min = f_center - df_doppler
    f_center_max = f_center + df_doppler
    return f_center_min, f_center_max


def doppler_correction(f_rx_received, f_rx_original, f_tx_at_target):
    """
    Perform doppler correction based on received and original frequencies.

    Calculates the velocity of the source and uses it to determine the required transmit frequency.
    """
    v_src = c * (f_rx_original/f_rx_received - 1)
    f_send = f_tx_at_target * (c+v_src)/c
    return f_send, v_src # v_src is the derivative of separating distance. (negative if satellite is approaching)

def doppler_correction_tle(uncorrected_tx_frequency):
    """
    Perform doppler correction based on current position based on TLE.
    This requires TLE to be new enough to be accurate. However it allows for frequency correction before reception.

    This requires skyfield to be installed and configured with TLE data.
    TODO: Automatic TLE updating, configuration files for GS, satellite etc.
    """
    current_time = skyfield_timescale.now()
    difference = satellite - groundstation
    topocentric = difference.at(current_time)
    range_rate = topocentric.frame_latlon_and_rates(groundstation)[5].km_per_s * 1e3
    doppler = range_rate / c * uncorrected_tx_frequency
    return doppler

def calculate_assumed_carrier_frequency(absolute_rx_frequency, uncorrected_tx_frequency):
    """
    WIP: Calculate the assumed carrier frequency based on absolute frequency of received signal and TLE based doppler correction.
    """
    doppler = doppler_correction_tle(uncorrected_tx_frequency)
    assumed_carrier_frequency = absolute_rx_frequency + doppler
    return assumed_carrier_frequency


def determine_ftune_and_min_sr(f_center_min, f_center_max, max_signal_bandwidth):
    """
    Determine frequency to tune to and minimum sample rate required to cover the desired frequency range.

    Tuned frequency is calculated to allow for doppler shift and signal bandwidth with some extra margin.
    10% margin + 25 kHz added to sideband calculation.

    Minimum sample rate is just based on Shannon-Nyquist theorem. (sampling_rate >= 2 * highest_frequency_component)
    """
    assert f_center_max >= f_center_min
    assert f_center_min > 0.5
    f_center_mid = (f_center_min + f_center_max) / 2
    f_center_span = f_center_max - f_center_min
    side_band = (max_signal_bandwidth/2 + f_center_span/2) * 1.1 + 25e3
    f_tune = f_center_mid - side_band

    # 2 * highest frequency component adjusted for tuning frequency.
    minimum_samplerate = (f_center_mid + side_band - f_tune) * 2  # == side_band * 4
    return f_tune, minimum_samplerate


def get_frequency_search_map(fftlen, f_min_nrm, f_max_nrm, assert_in_window=True):
    """
    
    """
    assert f_min_nrm <= f_max_nrm
    assert abs(f_max_nrm) < 1.0e3 #asserts the frequencies given were indeed normalized, not absolute.
    if assert_in_window:
        assert abs(f_min_nrm) <= 0.5
        assert abs(f_max_nrm) <= 0.5
    freqs = np.fft.fftshift( np.fft.fftfreq(fftlen, d=1.0) )
    df = freqs[1] - freqs[0]
    assert f_max_nrm > f_min_nrm
    mapping = np.zeros(fftlen, dtype=np.int64)
    for i,f in enumerate(freqs):
        if (f >= (f_min_nrm-df)) and (f <= (f_max_nrm+df)):
            mapping[i] = 1
    assert np.sum(mapping) > 0, mapping
    return mapping


def fractional_resampler_f_max_undisturbed(sr0, sr1, halflen, f_cutoff_coeff):
    f_slope_center = f_cutoff_coeff * (sr1/sr0) * sr0
    halfwidth      = 0.94 * sr0 / halflen
    return f_slope_center - halfwidth


# FREQUENCY MANAGEMENT =======================================================================================================================================================================
# FREQUENCY MANAGEMENT =======================================================================================================================================================================



def snr_dB(pl_power, noise_power):
    """
    Calculate SNR (Signal-to-Noise Ratio) in dB from signal power and noise power.
    """
    snr_linear = (pl_power-noise_power) / noise_power
    snr_dB_ = 10 * np.log10( max(1e-6, snr_linear) )
    return snr_dB_


# DEBUG PRINTING =============================================================================================================================================================================
from datetime import datetime as dtime
try:
    from mtools.zmq_printout import ZMQPIn
    #ZMQP_PRINTER = ZMQPIn(port=11001, hostname="localhost")
except:
    ZMQPIn = None

class DebugPrinter:
    def __init__(self, log_title:str, stdprint:bool=True, zmqprint_host_port=None):
        self.title = "[{}]".format(log_title)
        self.stdprint = stdprint
        self.zmqp = None
        self.zmqp_host = None
        self.zmqp_port = None
        if zmqprint_host_port:
            self.zmqp_host = zmqprint_host_port[0]
            self.zmqp_port = zmqprint_host_port[1]
            assert type(self.zmqp_host) == str
            assert type(self.zmqp_port) == int
            assert 1000 < self.zmqp_port < (2**16)
            if ZMQPIn:
                self.zmqp = ZMQPIn(port=self.zmqp_port, hostname=self.zmqp_host)
            else:
                print("Warning: ZMQPrint host configured, but no ZMQPrint library imported.")

    def DBGPRINT(self, first, *args):
        ts = "[{}]".format( dtime.now().isoformat()[11:] )
        ts += " "*(17-len(ts)) + self.title + " "
        ts += " "*(30-len(ts))
        #first, args = args[0], args[1:]
        if self.zmqp:
            self.zmqp.print(ts+str(first))
        if self.stdprint:
            print(ts+str(first), *args)


    def DBGPRINT_toggled(self, toggle, first, *args):
        if not toggle:
            return
        self.DBGPRINT(first, *args)
# DEBUG PRINTING =============================================================================================================================================================================














def pll_df_std0_polyfit(c_freq, c_limit):
    """
    This estimates pll-df-std0: the standard deviation of the first derivative of the frequency correction term of a nco-pll with pure noise input.
    This term can be used as an exceedinly sensitive transmission detector, with the following rules:
            - start of a transmission is indicated by the standard deviation (when measured on a sliding window) falling below C * pll-df-std0.
            - end of a transmission is indicated by the standard deviation returning back to >= pll-df-std0.
    """
    coeffs = [ -6.588834003638643e-10, -2.5411768833287384e-09, 4.981408860432077e-08, 1.868537663753007e-07,
                       -1.6876213332527186e-06, -6.0927590116583276e-06, 3.381339596106427e-05, 0.00011601629871765534,
                       -0.0004452159771682212, -0.001429960454287664, 0.0040467863424894085, 0.011952688145626354,
                       -0.025947547880166965, -0.06904834881024477, 0.11779091026188404, 0.2760086678268451,
                       -0.37435257361191565, -0.7542160313297891, 0.8113440477869422, 1.3790490653091885,
                       -1.1482083879741865, -1.6614950216935542, 0.9964447101954652, 1.4083888509528601,
                       -0.4914604337344185, -1.3722975909449475, 1.033787593289162, ]
    x = np.log10(c_freq/c_limit)
    y = 0
    for ip in range(27):
        y += coeffs[ip] * x**(27-(ip+1))
    y = 1.813*(x < np.log10(0.003)) + y*(np.log10(0.003) <= x)*(x < 3) + 0.0*(3 <= x)
    return y



def dt_array_report(dt_array, nsamples, sr0, dt_array_names):
    dt_total = np.sum(dt_array)
    speed = nsamples / dt_total
    overmatch = speed / sr0
    budget_fraction	= (1/overmatch) / 0.5
    cpu_fraction	= (1/overmatch) / 1.0
    S = "="*50
    S += "\n" + "speed:          {} Ms/s".format( round(1e-6 * speed, 2) )
    S += "\n" + "overmatch:      {}".format( round(overmatch, 2) )
    S += "\n" + "budget use:     {} %".format( round( 100*budget_fraction , 2) )
    S += "\n" + "cpu core use:   {} %".format( round( 100*cpu_fraction , 2) )
    for i_dt in range(len(dt_array)):
        txt1 = "  part {: ^{width1}} ({}):".format(i_dt, dt_array_names[i_dt], width1=2)
        txt2 = "{}{}".format(" "*(max(0, 27-len(txt1))), round( 100*dt_array[i_dt]/np.sum(dt_array) , 2))
        S += "\n" + txt1 + txt2
    S += "\n" + "parts of total:    {} %".format( round( 100*np.sum(dt_array)/dt_total , 2) )
    S += "\n" + "="*50
    return S
