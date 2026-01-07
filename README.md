# Skylink

Skylink protocol is a point-to-point communication protocol designed for small satellite applications
operating over radio amateur band. The protocol has been designed to facilitate an efficient and reliable
packet transmission between a satellite and ground station over a narrowband half-duplex channel and
can be used for example operating a small satellite.

The protocol implements for example features such as:
* Four logical virtual channels for mission specific purposes
* Windowed Time Division Duplexing (TDD)
* Reliable data transfer using automatic retransmission (ARQ)
* Uplink and downlink data authentication

More detailed protocol specification can found [/docs/Skylink_protocol_Specification_v1.pdf](/docs/Skylink_protocol_Specification_v1.pdf).

This repository contains the protocol implementation, PC host application and various test scripts. The implementation (found from `/src`) is written in pure C and has been designed to run on memory limited microcontrollers in space.

The source code is available under LGPL license, see `LICENSE` for the license text.

Main authors: Markus Hiltunen, Petri Niemelä

Additional help: Klaus Kivirikko, Topi Räty

# Dependencies

A pythonic skymodem was made which uses Cython linking to the C-based Skylink. First install the UHD drivers for
the USRP B200. This can be done by running the following commands in the terminal:

```
$ sudo add-apt-repository -y ppa:ettusresearch/uhd
$ sudo apt install uhd-host && sudo uhd_images_downloader && sudo usermod -a -G usrp $USERNAME
$ sudo apt install soapysdr-module-uhd soapysdr-tools
```

Once the drivers are installed, execute 
```
$ uhd_usrp_probe
```

in the terminal. This should upload the FPGA image to the USRP and then
print out some device info. If this succeeds, connecting to the USRP was successful. 

Next install python dependencies:
```
$ sudo apt install python3-soapysdr 
$ pip install numpy numba Cython
```

Next compile cython skylink wrapper by running the following commands:
```
$ cd skylink/skymodem/skylink_wrapper/cython_skylink
$ python _run_compile.py
```

Now you should be able to run the application, but you have to update the HMAC key in the application.py file.
It is located at the start of the __main__ function, near the end of the file. 

# Running the application

Run the `skymodem` application by:

```
$ cd skylink/skymodem/
$ python application.py
```

There are some preset configurations for running the modem.
These presets can be found in skylink/skymodem/modem_configs/presets.json
Currently available presets are: fm_default, fm_backup, dev_default, fms_default, fm_multimode, dev_multimode, fms_multimode, fm_calculated_doppler

These can be ran by using:
```
$ python application.py --config ${preset_name_here}
```

Preset configurations can also be overriden by specifying other configs such as:
```
$ python application.py --config ${preset_name_here} --rx_gain 50
```

# Configuration:
Doppler calculation configurations can be added interactively for a new satellite/ground station using:
```
$ python skylink/skymodem/TLE_doppler_configs/create_doppler_config.py
```

This new configuration can be then used by creating a new preset with:
```
$ python skylink/skymodem/modem_configs/add_preset.py
```
This will ask for configuration values in addition to a doppler config filename which will be used.

The new preset can then be used as specified in the "Running the application" section of this README.


# Including Skylink into an embedded application

Because no well standardized method for cross compiling libraries for embedded applications doesn't
exist, the easiest method to include the Skylink implementation into a project is by symbolically
linking the `src` folder under your project or by copying the whole `src` folder under the project.
The Skylink project expects to find its includes located inside the src, so you need to add the Skylink
implementation directory to the compiler's include directory listing.

In an embedded applications, the software architecture could look for example like this:

<img alt="Skylink implemented in an embedded application" src="https://docs.google.com/drawings/d/e/2PACX-1vQdUlA0Cfm3j24T89MKLFS9DPoLoOzKoz1c5jPob4sIAoblqsb_bjOYe0W6mpQfJOds6nkqm6ddAZY1/pub?w=933&h=336" width="60%" />

*TODO:* Example implementation.


# Python parser

The `python` folder includes an (almost) independent parser for handling Skylink frames. The
implementation can be used to parse and construct individual Skylink radio frames but it doesn't
include the required logic to drive the protocol implementation in real-time for two-way communication.
