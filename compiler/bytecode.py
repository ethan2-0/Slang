from typing import List, TypeVar, Generic
import struct
import binascii
import json
import opcodes
import emitter
import typesys
import util
import interpreter

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
                params += encode_str(param.bytecode_name)
            elif paramtype == "type":
                assert isinstance(param, int)
                params += struct.pack("!I", param)
            elif paramtype == "types":
                assert isinstance(param, list)
                params += struct.pack("!I", len(param))
                for i in param:
                    assert isinstance(i, int)
                    params += struct.pack("!I", i)
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
        header = struct.pack("!IIII", segment.num_registers, segment.signature.nargs, len(segment.opcodes), len(segment.emitter.type_references)) + encode_str(segment.signature.name)
        body_header = b""
        if segment.signature.is_class_method:
            assert segment.signature.containing_class is not None
            body_header += encode_str(segment.signature.containing_class.name)
        elif segment.signature.is_interface_method:
            assert segment.signature.containing_interface is not None
            body_header += encode_str(segment.signature.containing_interface.name)
        else:
            body_header += encode_str("")
        if segment.signature.generic_type_context is not None:
            body_header += struct.pack("!I", segment.signature.generic_type_context.num_arguments)
            for argument in segment.signature.generic_type_context.arguments:
                body_header += encode_str(argument.name)
                body_header += encode_str(argument.extends.name if argument.extends is not None else "")
                body_header += struct.pack("!I", len(argument.implements))
                for interface in argument.implements:
                    body_header += encode_str(interface.name)
        else:
            body_header += struct.pack("!I", 0)
        body_header += encode_str(segment.signature.returntype.bytecode_name)
        for register in segment.scope.registers:
            body_header += encode_str(register.type.bytecode_name)
        for typ in segment.emitter.type_references:
            body_header += encode_str(typ.bytecode_name)
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
        body += encode_str(segment.signature.parent_signature.bytecode_name if segment.signature.parent_signature is not None else "")
        body += struct.pack("!I", len(segment.signature.implemented_interfaces))
        for interface in segment.signature.implemented_interfaces:
            body += encode_str(interface.bytecode_name)
        if segment.signature.generic_type_context is not None:
            body += struct.pack("!I", segment.signature.generic_type_context.num_arguments)
            for argument in segment.signature.generic_type_context.arguments:
                body += encode_str(argument.bytecode_name)
                body += encode_str(argument.extends.bytecode_name if argument.extends is not None else "")
                body += struct.pack("!I", len(argument.implements))
                for interface in argument.implements:
                    body += encode_str(interface.bytecode_name)
        else:
            body += struct.pack("!I", 0)
        body += struct.pack("!I", len(segment.fields))
        for field in segment.fields:
            body += encode_str(field.name)
            body += encode_str(field.type.bytecode_name)
        return ret + struct.pack("!I", len(body)) + body

class SegmentEmitterStaticVariables(SegmentEmitter[emitter.StaticVariableSegment]):
    def __init__(self) -> None:
        SegmentEmitter.__init__(self, "staticvars")

    def emit_interpreter_value(self, value: interpreter.AbstractInterpreterValue) -> bytes:
        # Note that this encoding is self-terminating
        if isinstance(value, interpreter.InterpreterValueInteger):
            return struct.pack("!Q", value.value)
        elif isinstance(value, interpreter.InterpreterValueBoolean):
            return struct.pack("!Q", 1) if value.value else struct.pack("!Q", 0)
        elif isinstance(value, interpreter.InterpreterValueNull):
            return struct.pack("!Q", 0)
        elif isinstance(value, interpreter.InterpreterValueArray):
            ret = struct.pack("!Q", len(value.value))
            for elm in value.value:
                ret += self.emit_interpreter_value(elm)
            return ret
        else:
            raise ValueError("This is a compiler bug")

    def emit(self, segment: emitter.StaticVariableSegment) -> bytes:
        ret = SegmentEmitter.emit(self, segment)
        serialized_variables = bytes()
        variables_counted = 0
        for variable in segment.static_variables.variables.values():
            if variable.included:
                continue
            variables_counted += 1
            serialized_variables += encode_str(variable.name) + encode_str(variable.type.bytecode_name) + self.emit_interpreter_value(variable.initializer)
        body = struct.pack("!I", variables_counted) + serialized_variables
        return ret + struct.pack("!I", len(body)) + body

class SegmentEmitterInterface(SegmentEmitter[emitter.InterfaceSegment]):
    def __init__(self) -> None:
        SegmentEmitter.__init__(self, "interface")

    def emit(self, segment: emitter.InterfaceSegment) -> bytes:
        ret = SegmentEmitter.emit(self, segment)
        body = encode_str(segment.interface.name)
        return ret + body

emitters: List[SegmentEmitter] = [
    SegmentEmitterMetadata(),
    SegmentEmitterMethod(),
    SegmentEmitterClazz(),
    SegmentEmitterStaticVariables(),
    SegmentEmitterInterface()
]

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
