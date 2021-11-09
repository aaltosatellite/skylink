import sys
import time
import zmq
import pmt


ctx = zmq.Context()

up = ctx.socket(zmq.PUB)
up.bind("tcp://127.0.0.1:6001")

dw = ctx.socket(zmq.SUB)
dw.connect("tcp://127.0.0.1:7001")


def uplink(data: bytes) -> None:
    """
    Transmit a frame to GNU radio graph
    """
    if isinstance(data, str):
        data = data.encode()
    pdu = pmt.cons(pmt.PMT_NIL, pmt.init_u8vector(len(data), data))
    up.send(pmt.serialize_str(pdu))


def downlink(flags: int=0) -> bytes:
    """
    Receive a frame from GNU radio graph
    """
    pdu = pmt.deserialize_str(dw.recv(flags))
    print("pdu", pdu)
    print("msg", pmt.cdr(pdu))
    assert(pmt.is_u8vector(pmt.cdr(pdu)))
    return pmt.to_python(pmt.cdr(pdu))


if __name__ == "__main__":
    if 1:
        while True:
            uplink(b"Hello world")
            print(".", end="", flush=True)
            time.sleep(1)

    else:
        while True:
            print(downlink())
