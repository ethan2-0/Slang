from typing import List, TypeVar, Generic
import struct
import binascii
import json
import opcodes
import emitter
import typesys
import util

def encode_str(s: str) -> bytes:
    return struct.pack("!I", len(s.encode("utf8"))) + s.encode("utf8")

class MethodBytecodeEmitter:
    def __init__(self, opcodes: List[opcodes.OpcodeInstance]):
        self.opcodes = opcodes
        self.opcodes_numbered = False
        self.current_line_num = 0

    def number_opcodes(self) -> None:
        if self.opcodes_numbered:
            return
        self.opcodes_numbered = True

        for i in range(len(self.opcodes)):
            self.opcodes[i].index = i

    def emit_opcode(self, opcode: opcodes.OpcodeInstance) -> bytes:
        self.number_opcodes()
        params = b""
        for param, paramtype in zip(opcode.params, opcode.opcode.params):
            if paramtype == "int":
                assert isinstance(param, int)
                params += struct.pack("!Q", param)
            if paramtype == "paramreg":
                assert isinstance(param, int)
                params += struct.pack("!B", param)
            elif paramtype == "register":
                assert isinstance(param, emitter.RegisterHandle)
                params += struct.pack("!I", param.id)
            elif paramtype == "method":
                assert isinstance(param, emitter.MethodSignature)
                params += encode_str(param.name)
            elif paramtype == "class":
                assert isinstance(param, emitter.ClazzSignature)
                params += encode_str(param.name)
            elif paramtype == "type":
                assert isinstance(param, typesys.AbstractType)
                params += encode_str(param.name)
            elif paramtype == "property":
                assert isinstance(param, str)
                params += encode_str(param)
            elif paramtype == "instruction":
                assert isinstance(param, opcodes.OpcodeInstance)
                params += struct.pack("!I", param.index)
            elif paramtype == "staticvar":
                assert isinstance(param, str)
                params += encode_str(param)

        if opcode.node is not None:
            self.current_line_num = opcode.node.line
        ret = struct.pack("!B", opcode.opcode.code) + params
        return ret

    def emit(self) -> bytes:
        ret = b""
        for opcode in self.opcodes:
            ret += self.emit_opcode(opcode)
        current_line_num = 0
        for opcode in self.opcodes:
            if opcode.node is not None:
                current_line_num = opcode.node.line
            ret += struct.pack("!I", current_line_num)
        return ret

SegmentType = TypeVar("SegmentType", bound=emitter.Segment)
class SegmentEmitter(Generic[SegmentType]):
    def __init__(self, humantype: str) -> None:
        self.humantype = humantype

    def handles(self, segment: SegmentType) -> bool:
        return self.humantype == segment.humantype

    def emit(self, segment: SegmentType) -> bytes:
        return struct.pack("!B", segment.realtype)

class SegmentEmitterMethod(SegmentEmitter[emitter.MethodSegment]):
    def __init__(self) -> None:
        SegmentEmitter.__init__(self, "method")

    def emit(self, segment: emitter.MethodSegment) -> bytes:
        ret = SegmentEmitter.emit(self, segment)
        header = struct.pack("!III", segment.num_registers, segment.signature.nargs, len(segment.opcodes)) + encode_str(segment.signature.name)
        body_header = b""
        body_header += encode_str("" if segment.signature.containing_class is None else segment.signature.containing_class.name)
        body_header += encode_str(segment.signature.returntype.name)
        for register in segment.scope.registers:
            body_header += encode_str(register.type.name)
        body = body_header + MethodBytecodeEmitter(segment.opcodes).emit()
        length = struct.pack("!I", len(header + body))
        return ret + length + header + body

class SegmentEmitterMetadata(SegmentEmitter[emitter.MetadataSegment]):
    def __init__(self) -> None:
        SegmentEmitter.__init__(self, "metadata")

    def emit(self, segment: emitter.MetadataSegment) -> bytes:
        ret = SegmentEmitter.emit(self, segment)
        body = None
        if segment.has_entrypoint():
            body = encode_str(util.nonnull(segment.entrypoint).name)
        else:
            body = struct.pack("!I", 0)
        headers_serialized = json.dumps(segment.headers.serialize(), separators=(",", ":"))
        body += encode_str(headers_serialized)
        return ret + body

class SegmentEmitterClazz(SegmentEmitter[emitter.ClazzSegment]):
    def __init__(self) -> None:
        SegmentEmitter.__init__(self, "class")

    def emit(self, segment: emitter.ClazzSegment) -> bytes:
        ret = SegmentEmitter.emit(self, segment)
        body = encode_str(segment.name)
        body += encode_str(segment.signature.parent_signature.name if segment.signature.parent_signature is not None else "")
        body += struct.pack("!I", len(segment.fields))
        for field in segment.fields:
            body += encode_str(field.name)
            body += encode_str(field.type.name)
        return ret + struct.pack("!I", len(body)) + body

class SegmentEmitterStaticVariables(SegmentEmitter[emitter.StaticVariableSegment]):
    def __init__(self) -> None:
        SegmentEmitter.__init__(self, "staticvars")

    def emit(self, segment: emitter.StaticVariableSegment) -> bytes:
        ret = SegmentEmitter.emit(self, segment)
        body = bytes()
        body += struct.pack("!I", len(segment.static_variables.variables))
        for variable in segment.static_variables.variables.values():
            body += encode_str(variable.name) + encode_str(variable.type.name)
        return ret + struct.pack("!I", len(body)) + body

emitters: List[SegmentEmitter] = [SegmentEmitterMetadata(), SegmentEmitterMethod(), SegmentEmitterClazz(), SegmentEmitterStaticVariables()]

def emit(segments: List[emitter.Segment]) -> bytes:
    ret = binascii.unhexlify(b"cf702b56")
    for segment in segments:
        handled_segment = False
        for emitter in emitters:
            if not emitter.handles(segment):
                continue
            handled_segment = True
            ret += emitter.emit(segment)
            break
        if not handled_segment:
            raise ValueError("This is a compiler bug.")
    return ret
