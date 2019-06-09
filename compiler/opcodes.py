opcodes = None

import os.path
import json
import emitter


class OpcodeInstance:
    def __init__(self, opcode, params, node, annotation=None):
        self.opcode = opcode
        self.params = params
        self.node = node
        self.annotations = [annotation] if annotation is not None else []
        # This index is used in the bytecode emitter
        self.index = -1

    def annotate(self, annotation):
        self.annotations.append(annotation)

    def __str__(self):
        firstpart = str(self.opcode)
        secondpart = ", ".join(str(param) if not isinstance(param, OpcodeInstance) else str(param.opcode) for param in self.params)
        lastpart = (", annotation=%s" % self.annotations) if len(self.annotations) > 0 else ""
        lastpart += (", index=%s" % self.index) if self.index > 0 else ""
        return "OpcodeInstance(opcode=%s, params=[%s]%s)" % (firstpart, secondpart, lastpart)

class Opcode:
    def __init__(self, data):
        self.data = data
        self.mneumonic = data["mneumonic"]
        self.params = data["params"]
        self.writes = data["writes"] if "writes" in data else []
        self.reads = data["reads"] if "reads" in data else []
        self.code = int(data["code"], 0)

    def __str__(self):
        return "Opcode(mneumonic=%s)" % self.mneumonic

    def instantiate(self, *args, node=None):
        args = list(args)
        def complain_about_typechecking():
            raise ValueError("Argument has invalid type (%s ; %s)" % (args, self.params))

        if len(args) != len(self.params):
            complain_about_typechecking()

        for arg, param_type in zip(args, self.params):
            if param_type == "register":
                try:
                    arg.looks_like_register_handle()
                except:
                    complain_about_typechecking()
            elif param_type == "int" or param_type == "paramreg":
                None if isinstance(arg, int) else complain_about_typechecking()
            elif param_type == "property":
                None if isinstance(arg, str) else complain_about_typechecking()
            elif param_type == "instruction":
                None if isinstance(arg, OpcodeInstance) else complain_about_typechecking()
            elif param_type == "method":
                try:
                    arg.looks_like_method_handle()
                except:
                    complain_about_typechecking()
            elif param_type == "class":
                try:
                    arg.looks_like_class_signature()
                except:
                    complain_about_typechecking()
            elif param_type == "type":
                try:
                    arg.looks_like_type()
                except:
                    complain_about_typechecking()
            else:
                raise ValueError()

        return OpcodeInstance(self, args, node)

    def ins(self, *args, node=None):
        return self.instantiate(*args, node=node)

    def __call__(self, *args):
        return self.instantiate(*args)

opcodes_list = []
opcodes = dict()
with open(os.path.join(os.path.dirname(__file__), "opcodes.json")) as f:
    for data in json.loads(f.read()):
        cod = Opcode(data)
        opcodes_list.append(cod)
        opcodes[cod.mneumonic] = cod
        opcodes[cod.mneumonic.lower()] = cod
