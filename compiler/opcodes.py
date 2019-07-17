import os.path
import json
from typing import List, Optional, Union, cast, Dict, Any, NoReturn
import emitter
import typesys
import parser

OpcodeParamType = Union[int, str, "emitter.MethodSignature", "emitter.ClazzSignature", "emitter.RegisterHandle", typesys.AbstractType, "OpcodeInstance"]

class OpcodeInstance:
    def __init__(self, opcode: "Opcode", params: List[OpcodeParamType], node: Optional[parser.Node], annotation: Optional[str]=None):
        self.opcode = opcode
        self.params = params
        self.node = node
        self.annotations = [annotation] if annotation is not None else []
        # This index is used in the bytecode emitter
        self.index = -1

    def annotate(self, annotation: str) -> None:
        self.annotations.append(annotation)

    def __str__(self) -> str:
        firstpart = str(self.opcode)
        secondpart = ", ".join(str(param) if not isinstance(param, OpcodeInstance) else str(cast(OpcodeInstance, param).opcode) for param in self.params)
        lastpart = (", annotation=%s" % self.annotations) if len(self.annotations) > 0 else ""
        lastpart += (", index=%s" % self.index) if self.index > 0 else ""
        return "OpcodeInstance(opcode=%s, params=[%s]%s)" % (firstpart, secondpart, lastpart)

class Opcode:
    def __init__(self, data: Dict[str, Any]):
        self.data = data
        self.mneumonic: str = data["mneumonic"]
        self.params: List[str] = data["params"]
        self.writes: List[int] = data["writes"] if "writes" in data else []
        self.reads: List[int] = data["reads"] if "reads" in data else []
        self.code: int = int(data["code"], base=0)

    def __str__(self) -> str:
        return "Opcode(mneumonic=%s)" % self.mneumonic

    def instantiate(self, *args_: OpcodeParamType, node: parser.Node=None) -> OpcodeInstance:
        args: List[OpcodeParamType] = list(args_)
        def complain_about_typechecking() -> NoReturn:
            raise ValueError("Argument has invalid type (%s ; %s)" % (args, self.params))

        if len(args) != len(self.params):
            complain_about_typechecking()

        for arg, param_type in zip(args, self.params):
            if param_type == "register":
                if not isinstance(arg, emitter.RegisterHandle):
                    print(type(arg), emitter.RegisterHandle)
                    complain_about_typechecking()
            elif param_type == "int" or param_type == "paramreg":
                if not isinstance(arg, int):
                    complain_about_typechecking()
            elif param_type == "property":
                if not isinstance(arg, str):
                    complain_about_typechecking()
            elif param_type == "instruction":
                if not isinstance(arg, OpcodeInstance):
                    complain_about_typechecking()
            elif param_type == "method":
                if not isinstance(arg, emitter.MethodSignature):
                    complain_about_typechecking()
            elif param_type == "class":
                if not isinstance(arg, emitter.ClazzSignature):
                    complain_about_typechecking()
            elif param_type == "type":
                if not isinstance(arg, typesys.AbstractType):
                    complain_about_typechecking()
            elif param_type == "staticvar":
                if not isinstance(arg, str):
                    complain_about_typechecking()
            else:
                raise ValueError("This is a compiler bug.")

        return OpcodeInstance(self, args, node)

    def ins(self, *args: OpcodeParamType, node: parser.Node=None) -> OpcodeInstance:
        return self.instantiate(*args, node=node)

    def __call__(self, *args: OpcodeParamType) -> OpcodeInstance:
        return self.instantiate(*args)

opcodes_list: List[Opcode] = []
opcodes: Dict[str, Opcode] = dict()
with open(os.path.join(os.path.dirname(__file__), "opcodes.json")) as f:
    for data in json.loads(f.read()):
        cod = Opcode(data)
        opcodes_list.append(cod)
        opcodes[cod.mneumonic] = cod
        opcodes[cod.mneumonic.lower()] = cod
