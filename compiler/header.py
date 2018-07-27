class HeaderMethodArgumentRepresentation:
    def __init__(self, name):
        self.name = name

    def serialize(self):
        return {
            "type": "argument",
            "name": self.name,
            "argtype": "int"
        }

    @staticmethod
    def from_argument(argname):
        return HeaderMethodArgumentRepresentation(argname)

    @staticmethod
    def unserialize(input):
        if input["type"] != "method":
            raise ValueError("Asked to unserialize type '%s' as argument" % input["type"])

        return HeaderMethodArgumentRepresentation(input["name"])

class HeaderMethodRepresentation:
    def __init__(self, name, args, entrypoint=False):
        self.name = name
        self.args = args
        self.entrypoint = entrypoint

    @property
    def numargs(self):
        return len(self.args)

    def serialize(self):
        return {
            "type": "method",
            "name": self.name,
            "arguments": [argument.serialize() for argument in self.args],
            "numargs": len(self.args),
            "returns": "int",
            "entrypoint": self.entrypoint
        }

    @staticmethod
    def from_method(method, entrypoint_id):
        return HeaderMethodRepresentation(method.signature.name, [HeaderMethodArgumentRepresentation.from_argument(arg) for arg in method.signature.argnames], entrypoint_id == method.signature.id)

    @staticmethod
    def unserialize(input):
        if input["type"] != "method":
            raise ValueError("Asked to unserialize type '%s' as method" % input["type"])

        return HeaderMethodRepresentation(input["name"], [HeaderMethodArgumentRepresentation.unserialize(argument) for argument in input["arguments"]])

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
        return HeaderRepresentation([HeaderMethodRepresentation.from_method(method, program.get_entrypoint().id) for method in program.methods])

    @staticmethod
    def unserialize(input):
        if len(input) <= 0 or "hidden" in input and input["hidden"] == True:
            raise ValueError("Asked to unserialize missing or hidden input")

        if input["type"] != "metadata":
            raise ValueError("Asked to unserialize type '%s' as metadata" % input["type"])

        return HeaderRepresentation([HeaderMethodRepresentation.unserialize(method) for method in input["methods"]])

HeaderRepresentation.HIDDEN = HeaderRepresentation([], hidden=True)
