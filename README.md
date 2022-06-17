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

Main authors: Petri Niemelä, Markus Hiltunen

Special thanks: Tatu Peltola, Baris Dinc


# Building for PC

Skylink implementation itself doesn't depend on any external libraries. However, to run the implementation
on a PC a hosting application is required to handle the communication between protocol and modem/radio and
between protocol and user application. For this purpose, the PC build includes `gs` (ground station) application
which interfaces with [Suo Modem library](https://github.com/petrinm/suo) and other user applications via
ZMQ sockets.

Before starting the compiling process, the ZeroMQ library shall be installed on the system.
For example from apt on Debian based distros.
```
$ sudo apt install libzmq3-dev
```

Also, download and install [Suo Modem library](https://github.com/petrinm/suo) according to its instructions.
If the So library is installed to the system directories, the build system should be capable of locating it
automatically. Otherwise, the location of the Suo git repository needs to be hinted to the Cmake by giving
`SUO_GIT` define as shown in following instructions.

```
$ git clone https://github.com/aaltosatellite/skylink
$ mkdir build
$ cd build
$ cmake .. -DSUO_GIT='~/suo.git'
$ make
```

This compiles the `libskylink.so`, `gs` application and various unit tests.

In the PC configuration, the software stack including Skylink can look for example like this:
<img alt="Skylink implemented in PC application" src="https://docs.google.com/drawings/d/e/2PACX-1vTS7-NUPl7c-zPaWXcuNd-l_SF0DxkMJqoaIFHb-g0cniG5JG1Z52R-qUsmzHVAcu8i_zmya1HMt28Z/pub?w=1042&h=340" width="60%" />

After this the compiled gs host application located in `build/gs/gs` and the suo modem application can
be launched. The interfacing with Skylink's virtual channels over ZMQ sockets can be done for example
using interface library `scripts/vc_connector.py`.


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
