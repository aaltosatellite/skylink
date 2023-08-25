import json
import time
import struct
from typing import Dict, List, Optional, Union

import zmq
import zmq.asyncio

# Metadata names
METADATA_IDENTS = {
    1: "id",
    10: "mode",
    20: "power",
    21: "rssi",
    30: "cfo",
    40: "sync_errors",
    41: "golay_coded",
    42: "golay_errors",
    43: "rs_bit_errors",
    44: "rs_octet_errors",
}


# Suo message identifiers
SUO_MSG_RECEIVE          = 0x0001
SUO_MSG_TRANSMIT         = 0x0002
SUO_MSG_TIMING           = 0x0003

# Suo message flags
SUO_FLAGS_HAS_TIMESTAMP  = 0x0100
SUO_FLAGS_TX_ACTIVE      = 0x0200
SUO_FLAGS_RX_ACTIVE      = 0x0400
SUO_FLAGS_RX_LOCKED      = 0x0800



class SuoFrame:
    """
    Class for serializing and deserializing Suo frames to/from bytes
    """

    id: int
    flags: int
    data: bytes
    timestamp: int
    metadata: Dict[str, Union[int, float, bytes]]

    def __init__(self):
        """
        Initialize an empty frame object
        """
        self.id = 2
        self.flags = 0
        self.timestamp = 0
        self.data = b""
        self.metadata = {}


    def to_bytes(self) -> bytes:
        """
        Dump SuoFrame to bytes which can be sent over a socket.
        """
        if len(self.metadata) > 0:
            print("Warning: metadata dumping not supported")

        hdr = struct.pack("@IIQ", self.id, self.flags, self.timestamp)
        assert len(hdr) == 16
        metadata = b""
        return hdr, metadata, self.data


    def _parse_metadata(self, raw: bytes) -> None:
        """
        Parse metadata section from raw bytes
        """
        assert (len(raw) % 20) == 0
        self.metadata = {}

        chunks: List[bytes] = [ raw[i:i+20] for i in range(0, len(raw), 20) ]
        for meta_chunk in chunks:

            mlen, mtype, mident = struct.unpack("BBH", meta_chunk[0:4])
            mvalue = meta_chunk[4:]

            name = METADATA_IDENTS.get(mident, f"unknown_{mident}")

            if mtype == 1: # Float
                mvalue = struct.unpack("f", mvalue[:4])[0]
            elif mtype == 2: # Double
                mvalue = struct.unpack("d", mvalue[:8])[0]
            elif mtype == 3: # Int
                mvalue = struct.unpack("i", mvalue[:4])[0]
            elif mtype == 4: # Uint
                mvalue = struct.unpack("I", mvalue[:4])[0]
            elif mtype == 5: # time
                mvalue = struct.unpack("I", mvalue[:4])[0]

            self.metadata[name] = mvalue


    def _parse_header(self, raw: bytes) -> None:
        """
        Parse
        """
        assert len(raw) == 16
        self.id, self.flags, self.timestamp \
            = struct.unpack("@IIQ", raw)


    @classmethod
    def from_bytes(cls,
            header: bytes,
            metadata: Optional[bytes]=None,
            data: Optional[bytes]=None
        ) -> 'SuoFrame':
        """
        Deserialize Suo frame from byte string
        """
        self = cls()
        self._parse_header(header)
        if metadata is not None:
            self._parse_metadata(metadata)
        if self.data is not None:
            self.data = data
        return self


    @property
    def meta_str(self) -> str:
        """
        Return all metadata formated as a string.
        """
        return ", ".join([ f"{n}: {v!r}" for n,v in self.metadata.items() ])


    def __str__(self) -> str:
        """
        Format a readable string from the frame.
        """
        return f"SuoFrame({self.id}, {self.timestamp}, {self.data!r})"


class SuoInterface:

    def __init__(self,
            base: int=4000,
            ticks: bool=False
        ):
        """
        Connect to Suo modem over ZMQ socket
        """

        self.ctx = zmq.Context()

        self.dl = self.ctx.socket(zmq.SUB)
        self.dl.connect(f"tcp://localhost:{base}")
        self.dl.setsockopt(zmq.SUBSCRIBE, b"")

        # Flush
        try:
            while self.dl.recv(zmq.NOBLOCK):
                pass
        except zmq.error.Again:
            pass

        self.up = self.ctx.socket(zmq.PUB)
        self.up.connect(f"tcp://localhost:{base+1}")


    def transmit(self,
            frame: dict
        ) -> None:
        """
        Transmit a frame
        """

        cmd = json.dumps(frame)
        print(time.time_ns(), "before")
        self.up.send(bytes(cmd,'utf-8'))
        # self.up.send_json(frame)
        print(time.time_ns(), "after")


    def receive(self,
            timeout: float=0
        ) -> SuoFrame:
        """
        Receive a frame

        Args:
            timeout: Receiving timeout in seconds. 0 for infinite

        Returns:
            Received SuoFrame object

        Raises:
            TimeoutError is case no frames is received.
        """
        return self.dl.recv()


if __name__ == "__main__":
    import sys, time
    port = 7100
    suo = SuoInterface(base=port)
    print(f'suo_connector: Created port {port} (SUB), {port+1} (PUB)')
    if len(sys.argv) > 1 and sys.argv[1] == "tx":
        for i in range(4):
            msg = {"metadata":{"cmd":"get_state"}}
            
            print("Transmitting", msg , " ", time.time_ns())
            suo.transmit(msg)
            time.sleep(3)
            print("slept")

    else:
        pass
        while True:
            print(suo.receive())
