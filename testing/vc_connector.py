"""
    Virtual channel connector
"""
import struct
import asyncio
import zmq
import zmq.asyncio
from typing import Union, Any


class RTTChannel:
    """
        Connect to embedded Skylink implementation via RTT and Telnet
    """

    def __init__(self, host: str, port: int, vc: int):
        """ Initialize J-Link RTT server's Telnet connection. """
        self.frames = asyncio.Queue()
        self.statuses = asyncio.Queue()

        async def connect():
            """ Coroutine for connecting to RTT Telnet server """

            print("Connecting...")
            self.reader, self.writer = await asyncio.open_connection(host, port)

            # Send config string
            self.writer.write(b"$$SEGGER_TELNET_ConfigStr=RTTCh;%d$$" % (vc + 1))
            await asyncio.sleep(0.1)
            print("OK")

        loop = asyncio.get_event_loop()
        loop.run_until_complete(connect())
        loop.create_task(self.receiver_task())


    async def receiver_task(self):
        """ Background task run receiver """

        sync = False
        while True:

            b = await self.reader.read(1)
            if len(b) == 0: # Socket disconnect
                print("DISCONNECTED")
                break

            if not sync:
                if b == b"\xAB":
                    sync = True

            else:
                if b == b"\xBA":

                    cmd, data_len = await self.reader.read(2)
                    data = await self.reader.readexactly(data_len)
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
        self.writer.write(hdr + data)
        await asyncio.sleep(0) # For the function to behave as a couroutine


    async def transmit(self, frame: Union[bytes, str]) -> None:
        """ Send a frame to Virtual Channel buffer. """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self.send_command(0x0, frame)


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
        await self.send_command(0x0, b"")
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
