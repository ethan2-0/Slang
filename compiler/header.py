import slbparser
import json

class HeaderMethodArgumentRepresentation:
    def __init__(self, name, type):
        self.name = name
        self.type = type

    def serialize(self):
        return {
            "type": "argument",
            "name": self.name,
            "argtype": self.type
        }

    @staticmethod
    def from_argument(argname, type):
        return HeaderMethodArgumentRepresentation(argname, type)

    @staticmethod
    def unserialize(input):
        if input["type"] != "argument":
            raise ValueError("Asked to unserialize type '%s' as argument" % input["type"])

        return HeaderMethodArgumentRepresentation(input["name"], input["argtype"])

class HeaderMethodRepresentation:
    def __init__(self, name, args, returntype, entrypoint=False):
        self.name = name
        self.args = args
        self.entrypoint = entrypoint
        self.returntype = returntype

    @property
    def numargs(self):
        return len(self.args)

    def serialize(self):
        return {
            "type": "method",
            "name": self.name,
            "arguments": [argument.serialize() for argument in self.args],
            "numargs": len(self.args),
            "returns": self.returntype,
            "entrypoint": self.entrypoint
        }

    @staticmethod
    def from_method(method, program):
        entrypoint_id = program.get_entrypoint().id if program.has_entrypoint() else None
        return HeaderMethodRepresentation(method.signature.name,
            [HeaderMethodArgumentRepresentation.from_argument(arg, argtype.name) for arg, argtype in zip(method.signature.argnames, method.signature.args)],
            method.signature.returntype.name,
            entrypoint_id == method.signature.id)

    @staticmethod
    def unserialize(input):
        if input["type"] != "method":
            raise ValueError("Asked to unserialize type '%s' as method" % input["type"])

        return HeaderMethodRepresentation(input["name"], [HeaderMethodArgumentRepresentation.unserialize(argument) for argument in input["arguments"]], input["returns"])

class HeaderRepresentation:
    def __init__(self, methods, hidden=False):
        self.methods = methods
        self.hidden = hidden

    def serialize(self):
        if self.hidden:
            return {
                "hidden": True
            }

        methods = []
        for method in self.methods:
            methods.append(method.serialize())

        return {
            "methods": methods,
            "type": "metadata"
        }

    @staticmethod
    def from_program(program):
        return HeaderRepresentation([HeaderMethodRepresentation.from_method(method, program) for method in program.methods])

    @staticmethod
    def unserialize(input):
        if len(input) <= 0 or "hidden" in input and input["hidden"] == True:
            raise ValueError("Asked to unserialize missing or hidden input")

        if input["type"] != "metadata":
            raise ValueError("Asked to unserialize type '%s' as metadata" % input["type"])

        return HeaderRepresentation([HeaderMethodRepresentation.unserialize(method) for method in input["methods"]])

HeaderRepresentation.HIDDEN = HeaderRepresentation([], hidden=True)

def from_slb(fname):
    file_data = None
    with open(fname, "rb") as f:
        file_data = slbparser.extract_headers(f.read())

    return HeaderRepresentation.unserialize(json.loads(file_data))
