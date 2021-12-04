"""
    Virtual channel connector
"""
from __future__ import annotations

import struct
import asyncio
from typing import Any, Dict, Optional, Union

import zmq
import zmq.asyncio

from skylink import SkyStatus, SkyStatistics, parse_status, parse_stats

try:
    import pylink
except:
    pass # Allow starting without pylink installed


__all__ = [
    "RTTChannel",
    "ZMQChannel",
    "connect_to_vc"
]


#
# Virtual Channel Control message codes
#
VC_CTRL_TRANSMIT_VC0      = 0
VC_CTRL_TRANSMIT_VC1      = 1
VC_CTRL_TRANSMIT_VC2      = 2
VC_CTRL_TRANSMIT_VC3      = 3

VC_CTRL_RECEIVE_VC0       = 4
VC_CTRL_RECEIVE_VC1       = 5
VC_CTRL_RECEIVE_VC2       = 6
VC_CTRL_RECEIVE_VC3       = 7

VC_CTRL_GET_STATE         = 10
VC_CTRL_STATE_RSP         = 11
VC_CTRL_FLUSH_BUFFERS     = 12

VC_CTRL_GET_STATS         = 13
VC_CTRL_STATS_RSP         = 14
VC_CTRL_CLEAR_STATS       = 15

VC_CTRL_SET_CONFIG        = 16
VC_CTRL_GET_CONFIG        = 17
VC_CTRL_CONFIG_RSP        = 18

VC_CTRL_ARQ_CONNECT       = 20
VC_CTRL_ARQ_DISCONNECT    = 21
VC_CTRL_ARQ_TIMEOUT       = 22




class ARQTimeout(Exception):
    pass


class VCCommands:
    """
    General definitons of the control messages
    """
    vc: int
    frame_queue: asyncio.Queue[bytes]
    control_queue: asyncio.Queue[bytes]

    async def get_status(self) -> SkyStatus:
        """
        Request virtual buffer status.

        Returns:
            SkyStatus object containting the buffer statuses
        Raises:
            `asyncio.exceptions.TimeoutError`
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(VC_CTRL_GET_STATE)
        return parse_status(await self._wait_control_response(VC_CTRL_STATE_RSP))


    async def transmit(self, frame: Union[bytes, str]) -> None:
        """
        Send a frame to Virtual Channel buffer.

        Args:
            frame: Frame to be send. `str`, `bytes` or `SkyFrame`
        """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore')
        await self._send_command(VC_CTRL_TRANSMIT_VC0 + self.vc, frame)


    async def receive(self, timeout: Optional[float]=None) -> bytes:
        """
        Receive a frame from VC.

        Args:
            timeout: Optional timeout time in seconds.
                If None, receive will block.

        Raises:
            asyncio.TimeoutError in case of timeout.
        """
        if self.frame_queue is None:
            raise RuntimeError("Channel closed")

        frame = await asyncio.wait_for(self.frame_queue.get(), timeout)
        if isinstance(frame, ARQTimeout):
            raise frame


    async def arq_connect(self):
        """
        ARQ connect
        """
        await self._send_command(VC_CTRL_ARQ_CONNECT, struct.pack("B", self.vc))


    async def arq_disconnect(self):
        """
        ARQ disconnect
        """
        await self._send_command(VC_CTRL_ARQ_DISCONNECT, struct.pack("B", self.vc))



    async def get_free(self) -> int:
        """
        Get number of free bytes in the virtual channel buffer.
        """
        status = await self.get_status()
        return status.tx_free[self.vc]


    async def get_stats(self) -> SkyStatistics:
        """
        Request protocol statistics.

        Returns:
            SkyStatistics object containing the statistics.
        Raises:
            `asyncio.exceptions.TimeoutError`
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_command(VC_CTRL_GET_STATS)
        return parse_stats(await self._wait_control_response(VC_CTRL_STATS_RSP))


    async def clear_stats(self) -> None:
        """
        Clear Skylnk statistics
        """
        await self._send_command(VC_CTRL_CLEAR_STATS)



class RTTChannel(VCCommands):
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


            # Hunt the sync word
            if not sync:
                if b == b"\xAB":
                    sync = True

            else:
                if b == b"\xBA":

                    # Sync received, receive rest of the frame
                    data_len, cmd = await self._read_exactly(2)
                    data = await self._read_exactly(data_len)

                    if VC_CTRL_RECEIVE_VC0 <= cmd <= VC_CTRL_RECEIVE_VC3:
                        await self.frame_queue.put(data)
                    elif cmd == VC_CTRL_ARQ_TIMEOUT:
                        await self.frame_queue.put(ARQTimeout())
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
        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")

        hdr = struct.pack("BBBB", 0xAB, 0xBA, len(data), cmd)
        self.jlink.rtt_write(self.vc + 1, hdr + data)
        await asyncio.sleep(0) # For the function to behave as a couroutine


    async def _wait_control_response(self, cmd: int) -> bytes:
        """
        Wait for specific control message.
        """
        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")
        while True:
            ctrl = await asyncio.wait_for(self.control_queue.get(), timeout=0.5)
            if ctrl[0] == cmd:
                return ctrl[1]



class ZMQChannel(VCCommands):
    """
    Connect to Skylink implementation over ZeroMQ
    """

    def __init__(self, host: str, port: int, vc: int, ctx=None, pp: bool=False):
        """ Initialize ZMQ connection. """
        self.vc = vc
        if ctx is None:
            ctx = zmq.asyncio.Context()

        self.frame_queue = asyncio.Queue()
        self.control_queue = asyncio.Queue()

        # Open downlink socket
        self.dl = ctx.socket(zmq.PULL if pp else zmq.SUB)
        self.dl.connect("tcp://%s:%d" % (host, port + vc))
        if not pp:
            self.dl.setsockopt(zmq.SUBSCRIBE, b"")

        # Open uplink socket
        self.ul = ctx.socket(zmq.PUSH if pp else zmq.PUB)
        self.ul.connect("tcp://%s:%d" % (host, port + vc + 100))
        self.ul.setsockopt(zmq.SNDHWM, 10) # Set high water mark for outbound messages

        print(f"Connected to VC {vc} via ZMQ {host}:{port+vc}")

        loop = asyncio.get_event_loop()
        loop.create_task(self.receiver_task())


    async def receiver_task(self) -> None:
        """
        Background task run receiver to receive and
        """

        while True:
            # Wait for message from the ZMQ PUB
            msg = await self.dl.recv()

            if VC_CTRL_RECEIVE_VC0 <= msg[0] <= VC_CTRL_RECEIVE_VC3:
                await self.frame_queue.put(msg[1:])
            elif msg[0] == VC_CTRL_ARQ_TIMEOUT:
                await self.frame_queue.put(ARQTimeout())
            else:
                await self.control_queue.put((msg[0], msg[1:]))



    async def _send_command(self, cmd: int, data: bytes = b"") -> None:
        """
        Send general command to ZMQ VC buffer.
        Args:
            cmd: Command code
            data: Data/payload for the command
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        hdr = bytes([ cmd ])
        await self.ul.send(hdr + data)


    async def _wait_control_response(self, cmd: int) -> bytes:
        """
        Wait for specific control message.
        Args:
            cmd: Response command code to be waited.
        Returns:
            bytes
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        while True:
            ctrl = await asyncio.wait_for(self.control_queue.get(), timeout=0.5)
            if ctrl[0] == cmd:
                return ctrl[1:]




def connect_to_vc(host: str="127.0.0.1", port: int=5000, rtt: bool=False, vc: int=0, **kwargs):
    """
    Connect to Skylink implementation over ZMQ or RTT
    """
    if not rtt:
        return ZMQChannel(host, port, vc)
    else:
        return RTTChannel(vc)


if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    #vc = RTTChannel(vc=0)
    vc = ZMQChannel("127.0.0.1", 5000, vc=0)

    async def testing():
        while True:
            await vc.transmit(b"Hello world")
            #print(await vc.get_status())
            #print(await vc.get_stats())
            await asyncio.sleep(1.1)

    loop.run_until_complete(testing())
