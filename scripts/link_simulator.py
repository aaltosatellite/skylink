#!/usr/bin/env python3
"""
    Simple radio link simulator which transfers frames between two Skylink protocol
    implementations mimicing the suo modem interface.
"""

import time
import random
import struct
import asyncio

import zmq
import zmq.asyncio

plot = None

#from realtime_plot.drawer import create_realplot
#plot = create_realplot(10, 1)


propagation_delay = 0.010 # [s]
second_per_octet = 8.0 / 9600.0 # [s]
overhead = 6 # [bytes]


ctx = zmq.asyncio.Context()

ul_tx = ctx.socket(zmq.SUB)
ul_tx.bind("tcp://*:4001")
ul_tx.setsockopt(zmq.SUBSCRIBE, b"")

dl_rx = ctx.socket(zmq.PUB)
dl_rx.bind("tcp://*:4000")

ul_ticks = ctx.socket(zmq.PUB)
ul_ticks.bind("tcp://*:4002")
ul_ticks.bind("tcp://*:4003")


dl_tx = ctx.socket(zmq.SUB)
dl_tx.bind("tcp://*:4101")
dl_tx.setsockopt(zmq.SUBSCRIBE, b"")

ul_rx = ctx.socket(zmq.PUB)
ul_rx.bind("tcp://*:4100")

dl_ticks = ctx.socket(zmq.PUB)
dl_ticks.bind("tcp://*:4102")
dl_ticks.bind("tcp://*:4103")



epoch = time.time()

def get_timestamp() -> int:
    """ Get SDR timestamp in nanoseconds """
    global epoch
    return int((time.time() - epoch) * 1e9)

def corrupt(raw: bytes, n: int) -> bytes:
    """ Flip N bits in the frame """
    raw = bytearray(raw)
    for _ in range(n):
        raw[random.randint(0, len(raw) - 1)] ^= (1 << random.randint(0, 7))
    return bytes(raw)


cs = False # Global flag to indice is the radio medium busy

async def link(tx, rx, ticks, name: str, frame_loss: float) -> None:
    """
    Worker thread to behave as one end of the link.

    Args:
        rx: Receiving socket
        tx: Transmission socket
        name: Name identifier for the node
        frame_loss: Probability that the frame send by this node is lost.
    """

    global cs
    state = 0

    t_idle = 0
    data = None
    collision: bool = False

    while True:

        flags = 0x0100 | 0x0400 # Has timestamp + RX active

        if state == 0:
            #
            # Receiving
            #

            try:
                hdr, meta, data = await tx.recv_multipart(flags=zmq.NOBLOCK)
                id, flags, timestamp = struct.unpack("@IIQ", hdr)

                collision = cs
                cs = True
                state = 1

                t_idle = time.time() + propagation_delay + second_per_octet * (len(data) + overhead)
                flags |= 0x0200

            except zmq.error.Again:
                pass

            if state == 0 and cs:
                flags |= 0x0800 # RX locked

        else:
            #
            # Transmitting
            #

            if t_idle < time.time():

                # Determine was the transmission successful
                if collision:
                    text = "\u001b[31mCollision"

                elif random.random() < frame_loss:
                    text = "\u001b[33mLost"

                else:

                    data = corrupt(data, random.randint(0, 8))

                    # Send the frame to socket only if link heurestic was successful
                    hdr = struct.pack("@IIQ", 1, 0, get_timestamp())
                    rx.send_multipart((hdr, b"", data))
                    text = ""

                color = "\u001b[36m" if name == "UL" else "\u001b[34m"
                print(f"{color}{get_timestamp() * 1e-9:10.3f} {name} {len(data)} bytes {text}\u001b[0m")

                data = None
                cs = False
                collision = False
                state = 0

            else:
                flags |= 0x0200  # TX active

        #
        # Send tick message
        #
        ticks.send(struct.pack("@IIQ", 3, flags, get_timestamp() ))

        if plot is not None:
            plot.put((name, state))

        await asyncio.sleep(0.001)





if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.create_task(link(dl_tx, dl_rx, dl_ticks, "DL", 0.01))
    loop.create_task(link(ul_tx, ul_rx, ul_ticks, "UL", 0.06))
    loop.run_forever()
