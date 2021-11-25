# Skylink
Skylink protocol is an designed for.
Designed to run on memory limited microcontroller such as ARM Cortex-M0 with only 32kB RAM.

Authors:
Petri Niemelä, Markus Hiltunen, Tatu Peltola, Baris Dinc


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
