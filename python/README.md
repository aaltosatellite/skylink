# Independent Python parser library for the Skylink protocol

```shell
$ pip3 install -e .
```


## Usage example


```python
>>> import skylink
>>> frame = skylink.parse_frame(b'fOH2F1S*\x05\x00\x00T\x01\xf4\x01\xeb\xab!Hello world')
>>> print(frame)
Container:
    header = Container:
        version = 12
        identifier = b'OH2F1S' (total 6)
        flags = Container:
            has_payload = True
            arq_on = False
            is_authenticated = False
        vc = 2
        sequence = 0
        extensions = b'T\x01\xf4\x01\xeb' (total 5)
    payload = b'\xab!Hello world' (total 13)
    auth = b'' (total 0)
```
