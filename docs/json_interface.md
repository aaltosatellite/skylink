# SkyModem JSON interface

For communication with the mission control system Skymodem uses JSON formatted packets over ZMQ PUB/SUB sockets. (Implementation of the interface can be found from: skymodem/vc_interface.cpp.) By default, skymodem will create a new binding ZMQ socket for each virtual channel and communication direction (tx/rx). Skymodem subcribes all messages incoming to rx socket and published responses and other asyncronoush messages such as received packets to tx automatically.

The ZMQ port numbers are numbered using following equation `base_port + 10 * vc_index + (tx_port ? 1 : 0)`.
For example, if the base_port is 7100 (default value) the port numbers are following:

| TCP Port Number | Skymodems's role | Description             |
| --------------- | ---------------- | ----------------------- |
|       7100      |     ZMQ PUB      | VC0 Receive/Downlink    |
|       7101      |     ZMQ SUB      | VC0 Transmit/Uplink     |
|       7110      |     ZMQ PUB      | VC1 Receive/Downlink    |
|       7111      |     ZMQ SUB      | VC1 Transmit/Uplink     |
|       7120      |     ZMQ PUB      | VC2 Receive/Downlink    |
|       7121      |     ZMQ SUB      | VC2 Transmit/Uplink     |
|       7130      |     ZMQ PUB      | VC3 Receive/Downlink    |
|       7132      |     ZMQ SUB      | VC3 Transmit/Uplink     |



# JSON packet formats

## Transmitting a packet

To transmit (aka push a new packet to Skylink's buffer where Skylink will transmit it) following message
```
{
    "packet_type": "tc",
    "data": "123456789abcdef",
    "vc": 0,
}
```

Where:
- `packet_type` field is `tc` to indicate that this is telecommand packet. (optional)
- `data` field contains the transmitted data as a hexadecimal string.
  For example, `bytes(b"test").hex()` can be used to produced hexedecimal strings in Python.
- `vc` is optional virtual channel index. If the field is given the frame is pushed to specified virtual channel else virtual channel by the used port number is used.


## Receiving packet

When ever a frame is received by the modem and Skylink accepts it, skymodem will automatically transfer the packet to publishing virtual channels socket in following format.

```
{
    "packet_type": "tm",
    "timestamp": "2023-04-20T12:01:00Z",
    "data": "123456789abcdef",
    "vc": 1,
    "metadata": { }
}
```

Where:
- `packet_type` field is always `tm` to indicate that this is telemetry packet.
- `timestamp` field contains frame receiving time as ISO 8601 datetime string in UTC timezone.
- `data` field contains the received packet as a hexadecimal string.
   For example, `bytes.fromhex("abcd")` can be used to convert string hexedecimal strings to bytes-object in Python.
- `vc` field contains the virtual channel number.
- `metadata` field contains a dict of possible other metadata.


## ARQ connected/timeout messages

When the ARQ process changes state following message is output publish socket.

```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "vc": 0,
    "metadata": {
        "rsp": "arq_connected" | "arq_timeout"
        "vc": 0,
        "session_identifier": 123
    }
}
``` 


## Control commands:

In control commands, the `packet_type` must always be `control` and the actual command/response code with relevant fields are inside the `metadata` dict.  


### Get status

To request Skylink's current state following command is transmitted to the virtual channels subscribing socket.
```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "get_state"
    }
}
```

After handling the command Skymodem responses following message containing state of the all virtual channels. 
```
{ 
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": { 
        "rsp": "state",
        "state": [
            { "arq_state": 2, "buffer_free": 8, "tx_frames": 0, "rx_frames": 0 },
            { "arq_state": 0, "buffer_free": 10, "tx_frames": 0, "rx_frames": 0 },
            { "arq_state": 0, "buffer_free": 10, "tx_frames": 0, "rx_frames": 0 },
            { "arq_state": 0, "buffer_free": 10, "tx_frames": 0, "rx_frames": 0 },
        ]
    }
}
``` 

Where:
- `state` field contains list dict about each virtual channels state. In side each dict:
    - `arq_state` field is the . 0 = off, 1 = handshaking, 2 = on
    - `buffer_free` field contains number of free slots for packets in the transmit buffer.
    - `tx_frames` field ...
    - `rx_frames` field ...


### Get statistics

Command:
```
{
    "packet_type": "control",
    "metadata": { 
        "rsp": "get_stats",
    }
}
```

Response:
```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": {
        "rsp": "stats", 
        "rx_frames": 0,
        "rx_arq_resets": 0,
        "tx_frames": 0,
        "tx_bytes": 0
    }
}
``` 

Where:
 - `rx_frames` field is total number of transmitted,
 - `rx_arq_resets` field ...
 - `tx_frames` field ...
 - `tx_bytes` field ...

### Clear statistics

Command:
```
{
    "packet_type": "control",
    "metadata": { 
        "rsp": "clear_stats",
    }
}
```

No response


### Set config

Command:
```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "set_config",
        "config": "blaa",
        "value": 123 
    }
}
```

No response


### Get config

Command:
```
{ 
    "packet_type": "control",
    "metadata": { 
        "cmd": "get_config",
        "config": "blaa"
    }
}
```

Response:
```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": {
        "rsp": "config",
        "config": "blaa",
        "value": 123
    }
}
``` 



### ARQ connect

```
{ 
    "packet_type": "control",
    "metadata": {
        "cmd": "arq_connect"
    }
}
```

Response:
```
{
    "packet_type": "control",
    "timestamp": "2023-04-20T12:01:00Z",
    "metadata": {
        "rsp": "arq_connecting",
        "session_identifier": 123,
    }
}
``` 


### ARQ disconnect

```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "arq_disconnect"
    }
}
``` 


### MAC reset

```
{
    "packet_type": "control",
    "metadata": {
        "cmd": "arq_disconnect"
    }
}
``` 



