# At some point, it might be necessary to flesh this out
# As it is, I'm leaving this pretty minimal

from typing import Optional
import struct

SEGMENT_TYPE_METHOD = 0x00
SEGMENT_TYPE_METADATA = 0x01
SEGMENT_TYPE_CLASS = 0x02


class BytesReader:
    """
    This class is analagous to the fr_* methods in bytecode.c.
    """
    def __init__(self, input: bytes) -> None:
        self.input = input
        self.ptr = 0

    def get_uint32(self) -> int:
        fmt = "!I"
        size = struct.calcsize(fmt)
        data = struct.unpack_from(fmt, self.input[self.ptr:self.ptr + size])[0]
        self.ptr += size
        return data

    def get_uint8(self) -> int:
        fmt = "!B"
        size = struct.calcsize(fmt)
        data = struct.unpack_from(fmt, self.input[self.ptr:self.ptr + size])[0]
        self.ptr += size
        return data

    def get_str(self) -> str:
        size = self.get_uint32()
        data = self.input[self.ptr:self.ptr + size].decode("utf8")
        self.ptr += size
        return data

    def is_eof(self) -> bool:
        return self.ptr >= len(self.input)

def extract_headers(slb_bytes: bytes) -> Optional[str]:
    reader = BytesReader(slb_bytes)
    magic_number = reader.get_uint32()
    if magic_number != 0xcf702b56:
        raise ValueError("Invalid slb input: invalid magic number %02x" % magic_number)
    while not reader.is_eof():
        segment_type = reader.get_uint8()
        if segment_type == SEGMENT_TYPE_METHOD:
            length = reader.get_uint32()
            reader.ptr += length
        elif segment_type == SEGMENT_TYPE_METADATA:
            reader.get_str() # entrypoint
            return reader.get_str() # headers
        elif segment_type == SEGMENT_TYPE_CLASS:
            length = reader.get_uint32()
            reader.ptr += length
        else:
            raise ValueError("Invalid slb input: unknown segment type %02x" % segment_type)
    return None
