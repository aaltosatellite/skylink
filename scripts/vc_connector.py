"""
    Virtual channel connector
"""
from __future__ import annotations

import asyncio
import json
import struct
#from collections import namedtuple
from enum import Enum
from typing import Optional, Tuple, Union

import zmq
import zmq.asyncio

#from skylink import SkyState, SkyStatistics, parse_state, parse_stats

try:
    # PyLink is used when connecting to a JLinkRTT device
    # Not necessary if using only ZMQChannel
    import pylink

except:     # pylint: disable=bare-except
    pass


__all__ = [
    "RTTChannel",
    "ZMQChannel",
    "connect_to_vc"
]


def wrap_cmd(cmd: str, **kwargs) -> dict:
    return {"metadata":{"cmd": cmd, **kwargs}}

# Datastructure idea from https://stackoverflow.com/questions/35567724/how-to-define-custom-properties-in-enumeration-in-python-javascript-like/35567824#35567824
# VCMessage = namedtuple('VCMessage', ['control_msg', 'response_msg'])

#
# Virtual Channel response message codes
#
# grep -oP '\["rsp"\] = \K"\w+"' vc_interface.cpp (29.3.2023)

class VCResponseMessage(Enum):
    arq_timeout: str     = "arq_timeout"   
    arq_connected: str   = "arq_connected" 
    state: str           = "state"         
    stats: str           = "stats"         
    config: str          = "config"  

#
# Virtual Channel Control message codes
#
# grep -oP 'ctrl_command == \K"\w+"' vc_interface.cpp (29.3.2023)
class VCControlMessage(Enum):
    get_config: str      = wrap_cmd("get_config")
    get_state: str       = wrap_cmd("get_state")
    get_stats: str       = wrap_cmd("get_stats")
    set_config: str      = wrap_cmd("set_config")
    arq_connect: str     = wrap_cmd("arq_connect")
    arq_disconnect: str  = wrap_cmd("arq_disconnect")
    clear_stats: str     = wrap_cmd("clear_stats")
    mac_reset: str       = wrap_cmd("mac_reset")
    flush: str           = wrap_cmd("flush")

#
# Skylink Binary protocol codes
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


class ReceptionTimeout(Exception):
    pass

class ControlTimeout(Exception):
    pass

class ARQTimeout(Exception):
    pass


class VCCommands:
    """
    General definitons of the control messages
    """
    vc: int
    frame_queue: asyncio.Queue[bytes]
    control_queue: asyncio.Queue[Tuple[int, bytes]]

    async def get_state(self) -> dict:
        """
        Request virtual buffer status.

        Returns:
            SkyState object containting the buffer states
        Raises:
            `asyncio.exceptions.TimeoutError`
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_json(VCControlMessage.get_state)
        return await self._wait_cmd_reply(VCResponseMessage.state)


    async def transmit(self, frame: Union[bytes, str]) -> None:
        """
        Send a data frame to Virtual Channel buffer.

        Args:
            frame: Frame to be send. `str`, `bytes` or `SkyFrame`
        """
        if isinstance(frame, str):
            frame = frame.encode('utf-8', 'ignore').hex()
        elif isinstance(frame, bytes):
            frame = frame.hex()
        else:
            raise Exception(f"Unsupported data type {type(frame)}")

        await self._send_json({"data": frame})


    async def receive(self, timeout: Optional[float]=None) -> bytes:
        """
        Receive a frame from VC.

        Args:
            timeout: Optional timeout time in seconds.
                If None, receive will block forever.

        Raises:
            ReceptionTimeout in case of timeout.
        """
        if self.frame_queue is None:
            raise RuntimeError("Channel closed")

        try:
            frame = await asyncio.wait_for(self.frame_queue.get(), timeout)
        except asyncio.TimeoutError as e:
            raise ReceptionTimeout() from e

        if isinstance(frame, ARQTimeout):
            raise frame
        return frame


    async def arq_connect(self):
        """
        ARQ connect
        """
        await self._send_json(VCControlMessage.arq_connect)


    async def arq_disconnect(self):
        """
        ARQ disconnect
        """
        await self._send_json(VCControlMessage.arq_disconnect)


    async def get_free(self) -> int:
        """
        Get number of free bytes in the virtual channel buffer.
        """
        # TODO: Update the parsing for SkyState
        status = await self.get_state()
        ret = status.vc[self.vc].buffer_free
        print("get_free returns:", ret)
        return ret


    async def get_stats(self) -> dict:
        """
        Request protocol statistics.

        Returns:
            SkyStatistics object containing the statistics.
        Raises:
            `asyncio.exceptions.TimeoutError`
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        await self._send_json(VCControlMessage.get_stats)
        return await self._wait_cmd_reply(VCResponseMessage.stats)


    async def clear_stats(self) -> None:
        """
        Clear Skylnk statistics
        """
        await self._send_json(VCControlMessage.clear_stats)


    async def _send_json(self, json_dict: Union[dict, VCControlMessage]):
        """
        Send control frame to VC buffer.
        KWArgs:
            cmd: Command code
            config: configuration to change
            value: Value to change for configuration
        """
        raise NotImplementedError()


    async def _wait_cmd_reply(self, expected_rsp: VCResponseMessage):
        raise NotImplementedError()



class RTTChannel(VCCommands):
    """
    Connect to embedded Skylink implementation via Segger RTT.
    """

    def __init__(self, vc: int = 0, rtt_init: bool=False, device: str = "STM32F446RE"):
        """
        Initialize J-Link RTT server's Telnet connection.

        Args:
            vc: Virtual Channel number
        """
        self.vc = vc
        self.frame_queue = asyncio.Queue()
        self.control_queue = asyncio.Queue()

        print(f"Connected to VC {vc} via RTT")
        print(f"Device: {device}")

        self.jlink = pylink.JLink()
        self.jlink.open()

        if rtt_init:
            self.jlink.set_tif(pylink.enums.JLinkInterfaces.JTAG)
            self.jlink.connect(device)
            self.jlink.rtt_start()

        loop = asyncio.get_event_loop()
        self.task = loop.create_task(self.receiver_task())

    def exit(self):
        if self.task:
            self.task.cancel()
        self.task = None

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

                    # Sync received, receive rest of the frame
                    # b_data_len = await self._read_exactly(1)
                    # b_data = await self._read_exactly(b_data_len)

                    # json_data: dict = json.loads(b_data)
                    # meta = json_data.get("metadata", {})
                    # cmd = meta.get("cmd", None)
                    # rsp = meta.get("rsp", None)

                    # if (data := json_data.get("data", None)) is not None:
                    #     await self.frame_queue.put(data.hex())

                    # elif rsp == VCResponseMessage.arq_timeout.name:
                    #     await self.frame_queue.put(ARQTimeout())
            
                    # elif rsp is not None:
                    #     await self.frame_queue.put(b_data)

                    # elif cmd is not None:
                    #     await self.control_queue.put((cmd, b_data))

                    # else:
                    #     print("Invalid packet format", json_data)

                sync = False


        # Kill the queues
        qf, qc = self.frame_queue, self.control_queue
        self.frame_queue, self.control_queue = None, None
        await qf.put(None)
        await qc.put(None)


    async def _send_json(self, json_dict: Union[dict,VCControlMessage]):
        """
        Send control frame to RTT VC buffer.
        KWArgs:
            cmd: Command code
            config: configuration to change
            value: Value to change for configuration
        """
        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")

        # TODO: Update to JSON format
        if isinstance(json_dict, dict):
            print('json_dict not a VCControlMessage. Assuming data')
            data = json.dumps(json_dict).encode('utf-8')

        elif isinstance(json_dict, VCControlMessage):
            data = json_dict.value.encode('utf-8')

        hdr = struct.pack("BBBB", 0xAB, 0xBA, len(data))
        self.jlink.rtt_write(self.vc + 1, hdr + data)

        await asyncio.sleep(0) # For the function to behave as a couroutine


    async def _wait_cmd_reply(self, expected_rsp: VCResponseMessage) -> bytes:
        """
        Wait for specific control message.

        Args:
            expected_cmd: Wait until a control message with this code is received.

        Raises:
            ControlTimeout is raised if no response from Skylink is received in 500ms.
        """

        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")

        try:
            while True:
                rsp_type, rsp_data = await asyncio.wait_for(self.control_queue.get(), timeout=0.5)
                if expected_rsp is None or rsp_type == expected_rsp.name:
                    return rsp_data
        except asyncio.TimeoutError as e:
            raise ControlTimeout() from e


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
        self.dl = ctx.socket(zmq.constants.PULL if pp else zmq.constants.SUB)
        self.dl.connect((dl_port := f"tcp://{host}:{port + vc*10}"))
        if not pp:
            self.dl.setsockopt(zmq.constants.SUBSCRIBE, b"")

        # Open uplink socket
        self.ul = ctx.socket(zmq.constants.PUSH if pp else zmq.constants.PUB)
        self.ul.connect((ul_port := f"tcp://{host}:{port + vc*10 + 1}"))
        self.ul.setsockopt(zmq.constants.SNDHWM, 10) # Set high water mark for outbound messages

        print(f"Connected to VC {vc} via ZMQ Channel")
        print(f"uplink:   {ul_port}", f"downlink: {dl_port}", sep="\n")

        loop = asyncio.get_event_loop()
        self.task = loop.create_task(self.receiver_task())

    def exit(self):
        if self.task:
            self.task.cancel()
        self.task = None

    async def receiver_task(self) -> None:
        """
        Background task runs receiver.
        """

        while True:
            # Wait for message from the ZMQ PUB
            raw_msg: bytes = await self.dl.recv()
            msg = json.loads(raw_msg.decode())
            meta = msg.get("metadata", {})
            cmd = meta.get("cmd", None)
            rsp = meta.get("rsp", None)


            if "data" in msg:
                # TODO: Parse data to nicer printable format?
                # (fs1p_gs/egse/interfaces/utils.py)
                # (fs1p_gs/egse/interfaces/skylink_bus.py)
                print("Receiced data:", (data := msg["data"].hex()))

                await self.frame_queue.put_nowait(data)

            elif rsp == VCResponseMessage.arq_timeout.name:
                await self.frame_queue.put(ARQTimeout())

            elif rsp is not None:
                await self.control_queue.put((rsp, raw_msg))

            elif cmd is not None:
                await self.control_queue.put((cmd, raw_msg))

            else:
                # Received nothing
                pass


    async def _send_json(self, json_dict: Union[dict, VCControlMessage]) -> None:
        """
        Send control frame to ZMQ VC buffer.
        KWArgs:
            data: Some hex values
            metadata: A dictionary with the following possible kwargs
                cmd: Command code
                config: configuration to change
                value: Value to change for configuration
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")
        
        if isinstance(json_dict, VCControlMessage):
            json_dict = json_dict.value

        await self.ul.send(json.dumps(json_dict).encode())


    async def _wait_cmd_reply(self, expected_rsp: VCResponseMessage) -> bytes:
        """
        Wait for specific control message.
        Args:
            expected_rsp: Response command code to be waited.
        Returns:
            bytes
        """
        if self.control_queue is None:
            raise RuntimeError("Channel closed")

        while True:
            rsp_type, rsp_data = await asyncio.wait_for(self.control_queue.get(), timeout=0.5)
            if rsp_type == expected_rsp.name:
                return rsp_data




def connect_to_vc(host: str="127.0.0.1", port: int=7100, rtt: bool=False, vc: int=0, rtt_init: bool=False, **kwargs):
    """
    Connect to Skylink implementation over ZMQ or RTT
    """
    if not rtt:
        return ZMQChannel(host, port, vc)
    else:
        return RTTChannel(vc, rtt_init)


if __name__ == "__main__":
    async def testing():

        #vc = RTTChannel(vc=0)
        vc = ZMQChannel("127.0.0.1", 7100, vc=0)
        await asyncio.sleep(1) # Wait for the ZMQ connection

        #await vc.arq_connect()

        #while True:
        if 1:
            #await vc.transmit(b"Hello world")
            print(await vc.get_stats())
            print(await vc.get_state())
            await asyncio.sleep(1)

    loop = asyncio.get_event_loop()
    loop.run_until_complete(testing())
