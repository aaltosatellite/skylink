"""
    Virtual channel connector
"""
from __future__ import annotations

import asyncio
from datetime import datetime
import json
import struct
#from collections import namedtuple
from enum import Enum
import time
from typing import Optional, Tuple, Union

import zmq
import zmq.asyncio
import traceback

#from skylink import SkyState, SkyStatistics, parse_state, parse_stats

try:
    # PyLink is used when connecting to a JLinkRTT device
    # Not necessary if using only ZMQChannel
    # pip3 install pylink-square
    import pylink

except:     # pylint: disable=bare-except
    pass

__all__ = [
    "RTTChannel",
    "ZMQChannel",
    "connect_to_vc",
    "SkylinkDevice"
]

class SkylinkDevice(Enum):
    radio_dev: str = "STM32F446RE"
    FS1p_UHF: str = "VA10820"



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

def wrap_cmd(cmd: str, **kwargs) -> dict:
    return {"metadata":{"cmd": cmd, **kwargs}}

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
RDEV_CMD_WRITE_VC0      = 0
RDEV_CMD_WRITE_VC1      = 1
RDEV_CMD_WRITE_VC2      = 2
RDEV_CMD_WRITE_VC3      = 3

RDEV_CMD_READ_VC0       = 4
RDEV_CMD_READ_VC1       = 5
RDEV_CMD_READ_VC2       = 6
RDEV_CMD_READ_VC3       = 7

RDEV_CMD_GET_STATE         = 10
RDEV_CMD_STATE_RSP         = 11
RDEV_CMD_FLUSH_BUFFERS     = 12

RDEV_CMD_GET_STATS         = 13
RDEV_CMD_STATS_RSP         = 14
RDEV_CMD_CLEAR_STATS       = 15

RDEV_CMD_SET_CONFIG        = 16
RDEV_CMD_GET_CONFIG        = 17
RDEV_CMD_CONFIG_RSP        = 18

RDEV_CMD_ARQ_CONNECT       = 20
RDEV_CMD_ARQ_DISCONNECT    = 21
RDEV_CMD_ARQ_TIMEOUT       = 22


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
    config: dict[str, Union[bool, int]]
    print_trx: bool

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
            frame = frame.encode('utf-8', 'ignore')
        elif isinstance(frame, bytes):
            pass
        else:
            raise Exception(f"Unsupported data type {type(frame)}")

        if self.print_trx:
            print(f"\033[0;36mSending  data\033[0m: {frame.hex(sep='-')}")

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
        except asyncio.QueueEmpty:
            return None
        except asyncio.TimeoutError as e:
            raise ReceptionTimeout() from e

        if isinstance(frame, ARQTimeout):
            raise frame
        return frame

    async def receive_ctrl(self, expected_rsp: Union[VCResponseMessage, int]):
        reply = await self._wait_cmd_reply(expected_rsp)
        if self.print_trx:
            print(f"\033[0;32mRX ctrl reply:\033[0m {reply.hex(sep='-')}")
        return reply


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


    async def _wait_cmd_reply(self, expected_rsp: Union[VCResponseMessage, int]) -> bytes:
        """
        Wait for specific control message.

        Args:
            expected_cmd: Wait until a control message with this code is received.

        Raises:
            ControlTimeout is raised if no response from Skylink is received in 500ms.
        """
        if isinstance(expected_rsp, VCResponseMessage):
            expected_rsp = expected_rsp.name

        elif (isinstance(expected_rsp, bytes) and len(expected_rsp) == 1):
            expected_rsp = int.from_bytes(expected_rsp, 'little')

        elif isinstance(expected_rsp, int):
            pass

        else:
            raise Exception(f"Wrong expected type for {expected_rsp}: {type(expected_rsp)}")

        match_expected: bool = lambda found: found == expected_rsp

        try:
            while True:
                rsp_type, rsp_data = await asyncio.wait_for(self.control_queue.get(), timeout=0.5)
                if match_expected(rsp_type):
                    return rsp_data

        except asyncio.TimeoutError as e:
            raise ControlTimeout() from e


    async def _send_json(self, json_dict: Union[dict, VCControlMessage]) -> None:
        """
        Send control frame to VC buffer.
        KWArgs:
            cmd: Command code
            config: configuration to change
            value: Value to change for configuration
        """
        raise NotImplementedError()



class RTTChannel(VCCommands):
    """
    Connect to embedded Skylink implementation via Segger RTT.
    """
    # TODO: add terminal logging capability

    def __init__(self, vc: int = 0, rtt_init: bool=False, device: SkylinkDevice = SkylinkDevice.radio_dev, print_trx: bool=True):
        """
        Initialize J-Link RTT server's Telnet connection.

        Args:
            vc: Virtual Channel number
            rtt_init: Initializes the JLink cinterface (Not needed if RTTViewer is active)
        """
        self.vc = vc
        self.frame_queue = asyncio.Queue()
        self.control_queue = asyncio.Queue()
        self.print_trx = print_trx

        self.device = device

        if device == SkylinkDevice.radio_dev:
            self.syncword_A = b'\xAB'  # For Radio_dev protocol (directly speak to skylink) 
            self.syncword_B = b'\xBA'
            self.rtt_VC = self.vc + 1
        elif device == SkylinkDevice.FS1p_UHF:
            self.syncword_A = b'\x5A'   # \xAB  # For Radio_dev protocol (directly speak to skylink) 
            self.syncword_B = b'\xCE'   # \xBA
            self.rtt_VC = 1
        else:
            raise Exception(f'Device syncword not specified! {device}')

        print(f"Connected to VC {vc} via RTT")
        print(f"Device: {device}")

        self.jlink = pylink.JLink()
        self.jlink.open()

        # This code looks like it should always run?
        if rtt_init:
            self.jlink.set_tif(pylink.enums.JLinkInterfaces.JTAG)
            self.jlink.connect(device.value)
            self.jlink.rtt_start()

        loop = asyncio.get_event_loop()
        self.task = loop.create_task(self.receiver_task())

    def exit(self):
        if self.task:
            self.task.cancel()
        self.task = None

    def _get_pkt_header_no_cmd(self, len_data) -> bytes:
        if self.device == SkylinkDevice.radio_dev:
            return struct.pack("BBB", 0xAB, 0xBA, len_data)

        elif self.device == SkylinkDevice.FS1p_UHF:            # src , UHD_bus_ID
            assert 0 < len_data
            return struct.pack(">BBHBB", 0x5A, 0xCE, len_data, 0x69, 0x03)    # Since cmd is included in the data, +1

    async def _rtt_parse(self) -> Tuple[bytes, bytes]:
        if self.device == SkylinkDevice.radio_dev:
            data_len, cmd = await self._read_exactly(2)
            data = await self._read_exactly(data_len)
            return cmd, data

        elif self.device == SkylinkDevice.FS1p_UHF:
            hdr = await self._read_exactly(5)
            data_len, _, _, cmd = struct.unpack(">HBBB", hdr)
            data = await self._read_exactly(data_len)
            return cmd, data

    async def _read_exactly(self, num: int) -> bytes:
        """
        Read exactly N bytes from the RTT buffer.

        Args:
            num: Number of bytes to be read
        """
        buf: bytes = b""
        while len(buf) < num:
            ret: bytes = self.jlink.rtt_read(self.rtt_VC, num - len(buf)) # TODO: self.vc + 1 for STM32
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
                if b == self.syncword_A:
                    sync = True

            else:
                if b == self.syncword_B:
                    # Sync received, receive rest of the frame
                    cmd, data = await self._rtt_parse()

                    # print(f'RX Task | cmd: {cmd} - {data}')

                    if RDEV_CMD_READ_VC0 <= cmd <= RDEV_CMD_READ_VC3:
                        await self.frame_queue.put(data)
                    elif cmd == RDEV_CMD_ARQ_TIMEOUT:
                        await self.frame_queue.put(ARQTimeout())
                    else:
                        await self.control_queue.put((cmd, data))

                sync = False


        # Kill the queues
        qf, qc = self.frame_queue, self.control_queue
        self.frame_queue, self.control_queue = None, None
        await qf.put(None)
        await qc.put(None)


    async def _send_json(self, json_dict: Union[dict,VCControlMessage]) -> None:
        """
        Send control frame to RTT VC buffer.
        KWArgs:
            cmd: Command code
            config: configuration to change
            value: Value to change for configuration
        """
        if self.control_queue is None or not self.jlink.connected():
            raise RuntimeError("Channel closed")

        if isinstance(json_dict, dict):
            data = json_dict['data']

        elif isinstance(json_dict, VCControlMessage):
            raise NotImplementedError('RTTChannel cannot send JSON packets')

        hdr = self._get_pkt_header_no_cmd(len(data))
        snt_data = hdr + data + b'\x00'
        #print('RTT_SEND_JSON:',hdr.hex(sep='-'), data.hex(sep='-'))
        #print("written to RTT:", snt_data, f"({len(snt_data)} bytes)")
        self.jlink.rtt_write(self.rtt_VC, snt_data)   # self.vc + 1 for STM32

        await asyncio.sleep(0) # For the function to behave as a couroutine


    async def _wait_cmd_reply(self, expected_rsp: Union[VCResponseMessage, int]) -> bytes:
        if not self.jlink.connected():
            raise RuntimeError("Channel closed")
        reply = await super()._wait_cmd_reply(expected_rsp)
        return reply



class ZMQChannel(VCCommands):
    """
    Connect to Skylink implementation over ZeroMQ
    """

    def __init__(self, host: str, port: int, vc: int, ctx=None, pp: bool=False, print_trx: bool=True):
        """ Initialize ZMQ connection. """
        self.vc = vc
        if ctx is None:
            ctx = zmq.asyncio.Context()

        self.frame_queue = asyncio.Queue()
        self.control_queue = asyncio.Queue()
        self.config: dict[str, Union[bool, int]] = {}
        self.print_trx = print_trx

        # Open downlink socket
        self.dl = ctx.socket(zmq.PULL if pp else zmq.SUB)
        self.dl.connect((dl_port := f"tcp://{host}:{port + vc*10}"))
        if not pp:
            self.dl.setsockopt(zmq.SUBSCRIBE, b"")

        # Open uplink socket
        self.ul = ctx.socket(zmq.constants.PUSH if pp else zmq.PUB)
        self.ul.connect((ul_port := f"tcp://{host}:{port + vc*10 + 1}"))
        self.ul.setsockopt(zmq.SNDHWM, 10) # Set high water mark for outbound messages

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
        print("Started receiver task")
        try:
            while True:
                # Wait for message from the ZMQ PUB
                raw_msg: bytes = await self.dl.recv()
                msg = json.loads(raw_msg.decode())

                ts = msg.get("timestamp", None)

                if ts is not None:
                    now = datetime.now()
                    delay = now - datetime.strptime(ts,'%Y-%m-%dT%H:%M:%S.%fZ' )   # '2023-04-20T11:29:28.497Z'

                meta = msg.get("metadata", {})
                cmd = meta.get("cmd", None)
                rsp = meta.get("rsp", None)



                #print(raw_msg, msg, sep='\n')


                if "data" in msg:
                    from textwrap import wrap
                    data = msg['data']
                    print('ZMQ received', data)
                    data = bytes([int(i,16) for i in wrap(data,2)])
                    print('now', data)

                    if self.config.get("show_delay", None) is True:
                        data = data + struct.pack(">I", delay.microseconds)
                    self.frame_queue.put_nowait(data)

                    #print("Received data:", data)

                    if ts is not None:
                        print("    delay (us): ", delay.microseconds)
                        pass

                elif rsp == VCResponseMessage.arq_timeout.name:
                    await self.frame_queue.put(ARQTimeout())

                elif rsp is not None:
                    await self.control_queue.put((rsp, msg))

                elif cmd is not None:
                    await self.control_queue.put((cmd, msg))

                else:
                    # Received nothing
                    pass

        except: # pylint: disable=bare-except
            traceback.print_exc()
            raise Exception('Receiver go CrAsHeDy cRaSh')


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

        if isinstance(( data := json_dict.get('data', None)), bytes):
            json_dict['data'] = data.hex()

        await self.ul.send(json.dumps(json_dict).encode())




def connect_to_vc(host: str="127.0.0.1", port: int=7100, vc: int=0, rtt: bool=False, rtt_init: bool=False, print_trx:bool = True, **kwargs):
    """
    Connect to Skylink implementation over ZMQ or RTT
    """
    if not rtt:
        return ZMQChannel(host=host, port=port, vc=vc, print_trx=print_trx)
    else:
        return RTTChannel(vc=vc, rtt_init=rtt_init, device=kwargs['device'], print_trx=print_trx)


async def test_skymodem():

    #vc = RTTChannel(vc=0)
    vc = ZMQChannel("127.0.0.1", 7100, vc=0)
    await asyncio.sleep(1) # Wait for the ZMQ connection

    #await vc.arq_connect()
    def pretty(data: bytes) -> str:
        return json.dumps(json.loads(data),indent=4)

    def pprint(data: bytes) -> None:
        p = pretty(data)
        w = [len(l) for l in p.split('\n')]
        w = max(w)
        print(f"{' Incoming Data ':{'='}^{w}}",
                pretty(data),
                f"{'':{'='}^{w}}",
                "",
                sep='\n')

    #while True:
    if 1:
        #await vc.transmit(b"Hello world")
        stats = await vc.get_stats()
        state = await vc.get_state()
        
        pprint(stats)
        pprint(state)
        await asyncio.sleep(1)

# Service packet
# flag      = '\xAB'
# cmd       = []
# payload   = []

UHF_SERVICE_ECHO = b'\xAB\x21'
UHF_SERVICE_GET_SKY_CONFIG = b'\xAB\x13'
UHF_SERVICE_COPY_TO_FRAM = b'\xAB\x18'

UHF_CMD_PING = b'\x15'

async def test_tx_to_uhf():
    vc = ZMQChannel("127.0.0.1", 7100, vc=2)
    await asyncio.sleep(1) # Wait for the ZMQ connection

    for i in range(10):
        await vc.transmit(UHF_SERVICE_ECHO)
        await asyncio.sleep(1)


async def test_trx_to_uhf():
    vc = ZMQChannel("127.0.0.1", 7100, vc=2)
    await asyncio.sleep(1) # Wait for the ZMQ connection

    for i in range(30):
        await vc.transmit(UHF_SERVICE_GET_SKY_CONFIG)
        try:
            await vc.receive(1)
        except ReceptionTimeout:
            print('RX timed out!')

        await asyncio.sleep(0.5)


async def test_zmq_rx():
    duration = 12
    print(f"Starting rx ({duration}s)")

    vc = ZMQChannel("127.0.0.1", 7100, vc=2)
    await asyncio.sleep(1) # Wait for the ZMQ connection

    t_s = time.time()
    t_f = t_s + duration
    
    while time.time() < t_f:
        try:
            await vc.receive(1)
        except ReceptionTimeout:
            print('RX timed out!')

        await asyncio.sleep(0.1)
    

async def test_rtt():
    vc = RTTChannel(2, False, device=SkylinkDevice.FS1p_UHF)
    await asyncio.sleep(1)

    for _ in range(1):
        await vc.transmit(UHF_CMD_PING)
        await vc.receive_ctrl(UHF_CMD_PING)
        await asyncio.sleep(0.1)

async def test_skylink_rx():
    vc = ZMQChannel("127.0.0.1", 7100, vc=2)

    for _ in range(5000):
        await asyncio.sleep(1)


if __name__ == "__main__":

    loop = asyncio.get_event_loop()
    loop.run_until_complete(test_tx_to_uhf())
