Ground station software

# Modem
The radio modem is based on suo. Get it from https://github.com/tejeez/suo

# Link layer: fs\_ll
The fs\_ll.elf program runs the Aalto Amateur Protocol stack.

## Interfaces
fs\_ll interfaces in every direction using ZeroMQ publish/subscribe sockets
which also work for buffering packets.

The interface to modem consists of sockets for transmitted (uplink)
and received (downlink) physical layer frames. The modem binds the sockets
and fs\_ll connects into them.
An additional socket for timing and commanding of the modem may be added once
the modem gets support for it.

The interface to application layers is a separate ZeroMQ publish/subscribe
socket for each uplink and downlink virtual channel. fs\_ll binds to the
sockets and application layer connects to them.



## Implementation notes

The same protocol implementation is used both in the UHF subsystem
and the ground station, so here is mostly code to glue the implementation
to the suo modem running on Linux.

Reed-Solomon FEC is now included in the AAP implementation, so the modem passes
raw uncorrected codewords to fs\_ll.
A simplified version of libfec, including only a fixed Reed-Solomon codec,
is used both in the UHF subsystem and the ground station script.




# How to use it

To run the ground station:

Run make to compile fs\_ll.elf.
Start both both `./start-modem.sh` and `./fs_ll.elf`,
preferably in different terminals to see what they print.

# Tests and simulations

To test the protocol, use `start-test.sh`, which starts the following processes:

* `./fs_ll.elf` - works as the slave
* `./sim.py` - simulates delay and packet loss between ends
* `./fs_ll.elf m 53700 62000` - works as the master, simulating the spacecraft
* `./test-terminal.py` - "chat" program
* `./test-terminal.py 62000` - another end of the "chat"
* `./test-terminal.py 52001` - another virtual channel
* `./test-terminal.py 62001` - another end again

If everything works, you should be able to reliably send messages between
these test-terminals. Also look for debug messages from fs\_ll.
