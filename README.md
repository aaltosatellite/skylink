# Skylink
Skylink protocol is an designed for facilitating reliable packet transmission between a satellite and a ground station, over unreliable radio path, sharing transmission frequency in half-duplex fashion.
Designed to run on memory limited microcontroller such as ARM Cortex-M0 with only 32kB RAM.

Authors:
Petri Niemelä, Markus Hiltunen, Tatu Peltola, Baris Dinc

## Dependencies

Some of the compiled tests require zmq-library. On Debian-based linux distros, do:
```
$ sudo apt install libzmq3-dev
```




## Building

To build the for your PC, just type:
```
$ mkdir build
$ cd build
$ cmake .. -DSUO_GIT='~/suo.git'

$ make -j4
$ sudo make install
```

For embedded devices copy or symbolically link the `src` folder under your project. If you wish,
rename it for example `skylink`.
