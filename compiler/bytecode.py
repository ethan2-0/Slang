import struct
import binascii
import json

def encode_str(s):
    return struct.pack("!I", len(s.encode("utf8"))) + s.encode("utf8")

class MethodBytecodeEmitter:
    def __init__(self, opcodes):
        self.opcodes = opcodes
        self.opcodes_numbered = False

    def number_opcodes(self):
        if self.opcodes_numbered:
            return
        self.opcodes_numbered = True

        for i in range(len(self.opcodes)):
            self.opcodes[i].index = i

    def emit_opcode(self, opcode):
        self.number_opcodes()
        params = b""
        for param, paramtype in zip(opcode.params, opcode.opcode.params):
            if paramtype == "int":
                params += struct.pack("!Q", param)
            if paramtype == "paramreg":
                params += struct.pack("!B", param)
            elif paramtype == "register":
                params += struct.pack("!I", param.id)
            elif paramtype == "method":
                params += encode_str(param.name)
            elif paramtype == "instruction":
                params += struct.pack("!I", param.index)
        ret = struct.pack("!B", opcode.opcode.code) + params
        return ret

    def emit(self):
        ret = b""
        for opcode in self.opcodes:
            ret += self.emit_opcode(opcode)
        return ret

class SegmentEmitter:
    def __init__(self, humantype):
        self.humantype = humantype

    def handles(self, segment):
        return self.humantype == segment.humantype

    def emit(self, segment):
        return struct.pack("!B", segment.realtype)

class SegmentEmitterMethod(SegmentEmitter):
    def __init__(self):
        SegmentEmitter.__init__(self, "method")

    def emit(self, segment):
        ret = SegmentEmitter.emit(self, segment)
        header = struct.pack("!III", segment.num_registers, segment.signature.nargs, len(segment.signature.name)) + segment.signature.name.encode("utf8")
        body_header = b""
        body_header += encode_str(segment.signature.returntype.name)
        for argtype in segment.signature.args:
            body_header += encode_str(argtype.name)
        body = body_header + MethodBytecodeEmitter(segment.opcodes).emit()
        length = struct.pack("!I", len(header + body))
        return ret + length + header + body

class SegmentEmitterMetadata(SegmentEmitter):
    def __init__(self):
        SegmentEmitter.__init__(self, "metadata")

    def emit(self, segment):
        ret = SegmentEmitter.emit(self, segment)
        body = None
        if segment.has_entrypoint():
            body = struct.pack("!I", len(segment.entrypoint.name)) + segment.entrypoint.name.encode("utf8")
        else:
            body = struct.pack("!I", 0)
        headers_serialized = json.dumps(segment.headers.serialize(), separators=(",", ":"))
        body += encode_str(headers_serialized)
        return ret + body

emitters = [SegmentEmitterMetadata(), SegmentEmitterMethod()]

def emit(segments):
    ret = binascii.unhexlify(b"cf702b56")
    for segment in segments:
        for emitter in emitters:
            if not emitter.handles(segment):
                continue
            ret += emitter.emit(segment)
            break
    return ret
