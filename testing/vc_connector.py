"""
    Virtual channel connector
"""
import struct
import asyncio
import zmq
import zmq.asyncio
from typing import Union, Any

try:
    import pylink
except:
    pass


class RTTChannel:
    """
        Connect to embedded Skylink implementation via RTT and Telnet
    """

    def __init__(self, host: str = "127.0.0.1", port: int = 19021, vc: int = 0):
        """ Initialize J-Link RTT server's Telnet connection. """
        self.vc = vc
        self.frames = asyncio.Queue()
        self.statuses = asyncio.Queue()

        self.jlink = pylink.JLink()
        self.jlink.open()

        loop = asyncio.get_event_loop()
        loop.create_task(self.receiver_task())


    async def _read_exactly(self, num) -> bytes:
        """ Read exactly N bytes from the RTT buffer. """
        buf = b""
        while len(buf) < num:
            ret = self.jlink.rtt_read(self.vc + 1, num - len(buf))
            if len(ret) == 0:
                await asyncio.sleep(0.01)
            else:
                buf += bytes(ret)
        return buf


    async def receiver_task(self):
        """ Background task run receiver """

        sync = False
        while True:

            b = await self._read_exactly(1)

            if not self.jlink.connected():
                print("DISCONNECTED")
                await asyncio.sleep(0)
                break

            if len(b) == 0: # Socket disconnect
                await asyncio.sleep(0.01)
                continue

            if not sync:
                if b == b"\xAB":
                    sync = True

            else:
                if b == b"\xBA":

                    cmd, data_len = await self._read_exactly(2)
                    data = await self._read_exactly(data_len)
                    if cmd == 0x00:
                        await self.frames.put(data)
                    elif cmd == 0x01:
                        await self.statuses.put(data)
                sync = False


        # Kill the queues
        qf, qs = self.frames, self.statuses
        self.frames, self.statuses = None, None
        await qf.put(None)
        await qs.put(None)


    async def send_command(self, cmd: int, data: bytes):
        """ Send general command """
        hdr = struct.pack("BBBB", 0xAB, 0xBA, len(data), cmd)
        self.jlink.rtt_write(self.vc + 1, hdr + data)
        await asyncio.sleep(0) # For the function to behave as a couroutine


    async def transmit(self, frame: Union[bytes, str]) -> None:
        """ Send a frame to Virtual Channel buffer. """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self.send_command(0x02, frame)


    async def receive(self, timeout=None) -> bytes:
        """ Wait for a frame from VC.
            Raises asyncio.TimeoutError in case of timeout
        """
        if self.frames is None:
            return None
        await asyncio.wait_for(self.frames.get(), timeout)


    async def get_status(self):
        """ Request virtual buffer status. """
        if self.statuses is None:
            return None
        await self.send_command(0x00, b"")
        r = await asyncio.wait_for(self.statuses.get(), timeout=1)
        status = struct.unpack("BB", r)
        return status



class ZMQChannel:
    """
        Connect to Skylink implementation over ZeroMQ
    """

    def __init__(self, host: str, port: int, vc: int, ctx=None, pp: bool=False):
        """ Initialize ZMQ connection. """
        if ctx is None:
            ctx = zmq.asyncio.Context()

        # Open downlink socket
        self.dl = ctx.socket(zmq.PULL if pp else zmq.SUB)
        self.dl.connect("tcp://%s:%d" % (host, port + vc))
        if not pp:
            self.dl.setsockopt(zmq.SUBSCRIBE, b"")

        # Open uplink socket
        self.ul = ctx.socket(zmq.PUSH if pp else zmq.PUB)
        self.ul.connect("tcp://%s:%d" % (host, port + vc + 100))
        self.ul.setsockopt(zmq.SNDHWM, 10) # Set high water mark for outbound messages


    async def transmit(self, frame):
        """ Transmit a frame to virtual channel. """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self.ul.send(frame)


    async def receive(self, timeout=None):
        """ Receive a frame from the virtual channel. """
        return await self.dl.recv()


    async def get_status(self):
        """ Request virtual buffer status. """
        pass # TODO
        return (0, 0, 0, 0)


if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    rtt = RTTChannel("127.0.0.1", 19021, 0)

    async def hello():
        await rtt.transmit(b"Hello world")
        await asyncio.sleep(0.1)
    loop.run_until_complete(hello())
