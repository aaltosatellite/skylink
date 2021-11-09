
import struct
import zmq
import time

"""
uint32_t id, flags;
uint64_t time;
uint32_t metadata[11];
uint32_t len;
uint8_t data[RADIOFRAME_MAXLEN];
"""

ctx = zmq.Context()

dl = ctx.socket(zmq.SUB)
dl.connect("tcp://localhost:43700")
dl.setsockopt(zmq.SUBSCRIBE, b"")

# Flush
try:
    while dl.recv(zmq.NOBLOCK):
        pass
except zmq.error.Again: 
    pass

up = ctx.socket(zmq.PUB)
up.connect("tcp://localhost:43701")

def suo_transmit(frame, fid=1, flags=6, time=None, metadata=None):
    if time is None:
        time = 0
    if metadata is None:
        metadata = 11 * [0]

    hdr = struct.pack("IIQ11II", fid, flags, time, *metadata, len(frame))
    up.send(hdr + frame)
    print("TX", frame)


def suo_receive():

    frame = dl.recv()
    print(frame)
    hdr = struct.unpack("IIQ11II", frame[:64])
    #fid, flags, time, metadata,
    frame = frame[64:]

    print("RX", frame)

if __name__ == "__main__":
    import sys, time

    if sys.argv[0] == "tx":
        while True:
            suo_transmit(b"hello")
            time.sleep(1)

    else:
        while True:
            suo_receive()
