#!/usr/bin/env python3
"""
    Simple radio link simulator which transfers frames between two Skylink protocol
    implementations mimicing the suo modem interface.
"""

import zmq, threading, time, random, struct

second_per_octet = 8.0 / 9600.0

ctx = zmq.Context()

ul_tx = ctx.socket(zmq.SUB)
ul_tx.bind("tcp://*:43701")
ul_tx.setsockopt(zmq.SUBSCRIBE, b"")

dl_rx = ctx.socket(zmq.PUB)
dl_rx.bind("tcp://*:43700")

dl_tx = ctx.socket(zmq.SUB)
dl_tx.bind("tcp://*:53701")
dl_tx.setsockopt(zmq.SUBSCRIBE, b"")

ul_rx = ctx.socket(zmq.PUB)
ul_rx.bind("tcp://*:53700")

tick_pub = ctx.socket(zmq.PUB)
tick_pub.bind("tcp://*:43703")
tick_pub.bind("tcp://*:53703")

cs = False # Global flag to indice is the radio medium busy

def link(tx, rx, name, frame_loss):
    """
    Worker thread to behave as one end of the link.

    Args:
        rx: Receiving socket
        tx: Transmission socket
        name: Name identifier for the node
        frame_loss: Probability that the frame send by this node is lost.
    """

    global cs
    while True:

        # Wait for new frame to be transmitted
        d = tx.recv()
        t_now = time.time()
        t_frame_ns , = struct.unpack("Q", d[8:16])
        t_frame = 1e-9 * t_frame_ns

        # Sleep
        cs1 = cs
        cs = True
        time.sleep(second_per_octet * (len(d) - 64))
        cs = False

        # Determine was the transmission successful
        if cs1:
            text = "Collision"
        elif random.random() < frame_loss:
            text = "Lost"
        else:
            # Send the frame to socket only if link heurestic was successful
            rx.send(d)
            text = ""

        print("%10.2f %10.2f %s %s" % (t_now, t_frame, name, text))


dlthread = threading.Thread(target=link, daemon=True, args = (dl_tx, dl_rx, "DL", 0.1))
dlthread.start()
ulthread = threading.Thread(target=link, daemon=True, args = (ul_tx, ul_rx, "UL", 0.4))
ulthread.start()

while True:
    # Publish ticks (required by the protocol implementation)
    tick_pub.send(struct.pack("IIQ", 0, 0, int(time.time() * 1e9)))
    time.sleep(0.05)
