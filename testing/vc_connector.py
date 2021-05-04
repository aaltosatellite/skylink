"""
    Virtual channel connector
"""
import struct
import asyncio
from typing import Union, Any, Optional

import zmq
import zmq.asyncio

from skylink import SkyFrame, SkyStatus, SkyStatistics, parse_status, parse_stats

try:
    import pylink
except:
    pass


class RTTChannel:
    """
    Connect to embedded Skylink implementation via Segger RTT.
    """

    def __init__(self, vc: int = 0):
        """
        Initialize J-Link RTT server's Telnet connection.

        Args:
            vc: Virtual Channel number
        """
        self.vc = vc
        self.frame_queue = asyncio.Queue()
        self.control_queue = asyncio.Queue()

        print(f"Connected to VC {vc} via RTT")

        self.jlink = pylink.JLink()
        self.jlink.open()

        loop = asyncio.get_event_loop()
        loop.create_task(self.receiver_task())


    async def _read_exactly(self, num: int) -> bytes:
        """
        Read exactly N bytes from the RTT buffer.

        Args:
            num: Number of bytes to be read
        """
        buf: bytes = b""
        while len(buf) < num:
            ret: bytes = self.jlink.rtt_read(self.vc + 1, num - len(buf))
            if len(ret) == 0:
                await asyncio.sleep(0.01)
            else:
                buf += bytes(ret)
        return buf


    async def receiver_task(self) -> None:
        """
        Background task run receiver to receive and
        """

        sync = False
        while True:

            # Wait for frame sync word
            b = await self._read_exactly(1)

            if not self.jlink.connected():
                print("DISCONNECTED")
                await asyncio.sleep(0)
                break

            if len(b) == 0: # Socket disconnect
                await asyncio.sleep(0.01)
                continue

            # Hunt the sync word
            if not sync:
                if b == b"\xAB":
                    sync = True
            else:
                if b == b"\xBA":

                    # Sync received, receive rest of the frame
                    cmd, data_len = await self._read_exactly(2)
                    data = await self._read_exactly(data_len)

                    if cmd == 0x01:
                        await self.frame_queue.put(data)
                    else:
                        await self.control_queue.put((cmd, data))

                sync = False


        # Kill the queues
        qf, qc = self.frame_queue, self.control_queue
        self.frame_queue, self.control_queue = None, None
        await qf.put(None)
        await qc.put(None)


    async def _send_command(self, cmd: int, data: bytes = b""):
        """
        Send general command to RTT VC buffer.

        Args:
            cmd: Command code
            data:
        """
        hdr = struct.pack("BBBB", 0xAB, 0xBA, len(data), cmd)
        self.jlink.rtt_write(self.vc + 1, hdr + data)
        await asyncio.sleep(0) # For the function to behave as a couroutine

    async def _wait_control_response(self, cmd: int) -> bytes:
        """
        Wait for specific control message.
        """
        while True:
            ctrl = await asyncio.wait_for(self.control_queue.get(), timeout=0.5)
            if ctrl[0] == cmd:
                return ctrl[1]


    async def transmit(self, frame: Union[SkyFrame, bytes, str]) -> None:
        """ Send a frame to Virtual Channel buffer. """
        if isinstance(frame, SkyFrame):
            frame = frame.data
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self._send_command(0x02, frame)


    async def receive(self, timeout: Optional[float]=None) -> SkyFrame:
        """
        Receive a frame from VC.

        Args:
            timeout: Optional timeout time in seconds.
                If None, receive will block.

        raises:
            asyncio.TimeoutError in case of timeout.
        """
        if self.frame_queue is None:
            raise RuntimeError("Channel closed")

        return await asyncio.wait_for(self.frame_queue.get(), timeout)


    async def get_status(self) -> SkyStatus:
        """
        Request virtual buffer status.
        Returns:
            SkyStatus object containting the buffer statuses
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(0x00)
        return parse_status(await self._wait_control_response(0x00))


    async def get_free(self) -> int:
        """
        Get number of free bytes in the virtual channel buffer.
        """
        status = self.get_status()
        return status.vc[self.vc] # TODO: real definition


    async def get_stats(self) -> SkyStatistics:
        """
        Request protocol statistics.

        Returns:
            SkyStatistics object containing the statistics.
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(0x00)
        return parse_stats(await self._wait_control_response(0x00))



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

        self.control = None

        # Open uplink socket
        self.ul = ctx.socket(zmq.PUSH if pp else zmq.PUB)
        self.ul.connect("tcp://%s:%d" % (host, port + vc + 100))
        self.ul.setsockopt(zmq.SNDHWM, 10) # Set high water mark for outbound messages

        print(f"Connected to VC {vc} via ZMQ {host}:{port}")


    async def transmit(self, frame: Union[SkyFrame, bytes, str]) -> None:
        """ Transmit a frame to virtual channel. """
        if isinstance(frame, SkyFrame):
            frame = frame.data
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self.ul.send(frame)


    async def receive(self, timeout: Optional[int] = None) -> SkyFrame:
        """ Receive a frame from the virtual channel. """
        return await self.dl.recv()


    async def get_status(self) -> SkyStatus:
        """ Request virtual buffer status. """
        self.ul.send
        pass # TODO
        return (0, 0, 0, 0)

    async def get_stats(self) -> SkyStatistics:
        """ Request statistics """
        return # parse_stats()


def connect_to_vc(host: str = "127.0.0.1", port: int = 5200, rtt: bool=False, vc: int = 0, **kwargs):
    """
    Connect to Skylink implementation over ZMQ or RTT
    """
    if not rtt:
        return ZMQChannel(host, port, vc)
    else:
        return RTTChannel(vc)


if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    vc = RTTChannel(vc=0)
    #vc = ZMQChannel(vc=0)

    async def testing():
        await vc.transmit(b"Hello world")
        print(await vc.get_status())
        print(await vc.get_stats())
        await asyncio.sleep(0.1)

    loop.run_until_complete(testing())
