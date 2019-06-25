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
    def __init__(self, name, args, returntype, containing_clazz, is_ctor, entrypoint=False, is_override=False):
        self.name = name
        self.args = args
        self.returntype = returntype
        self.containing_clazz = containing_clazz
        self.is_ctor = is_ctor
        self.entrypoint = entrypoint
        self.is_override = False

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
            "containingclass": self.containing_clazz,
            "entrypoint": self.entrypoint,
            "ctor": self.is_ctor,
            "override": self.is_override
        }

    @staticmethod
    def from_method(method, program):
        entrypoint_id = program.get_entrypoint().id if program.has_entrypoint() else None
        return HeaderMethodRepresentation(method.signature.name,
            [HeaderMethodArgumentRepresentation.from_argument(arg, argtype.name) for arg, argtype in zip(method.signature.argnames, method.signature.args)],
            method.signature.returntype.name,
            method.signature.containing_class.name if method.signature.containing_class is not None else None,
            is_ctor=method.signature.is_ctor,
            # is_ctor=False,
            entrypoint=entrypoint_id == method.signature.id,
            is_override=method.signature.is_override)

    @staticmethod
    def unserialize(input):
        if input["type"] != "method":
            raise ValueError("Asked to unserialize type '%s' as method" % input["type"])

        return HeaderMethodRepresentation(input["name"], [HeaderMethodArgumentRepresentation.unserialize(argument) for argument in input["arguments"]], input["returns"], input["containingclass"], is_ctor=input["ctor"], entrypoint=False, is_override=input["override"])

class HeaderClazzFieldRepresentation:
    def __init__(self, name, type):
        self.name = name
        self.type = type

    def serialize(self):
        return {
            "type": "field",
            "name": self.name,
            "fieldtype": self.type.name
        }

    @staticmethod
    def from_field(field):
        return HeaderClazzFieldRepresentation(field.name, field.type)

    @staticmethod
    def unserialize(input):
        if input["type"] != "field":
            raise ValueError("Asked to unserialize type '%s' as field" % input["type"])

        return HeaderClazzFieldRepresentation(input["name"], input["fieldtype"])

class HeaderClazzMethodRepresentation:
    def __init__(self, name):
        self.name = name

    def serialize(self):
        return {
            "type": "classmethod",
            "name": self.name
        }

    @staticmethod
    def unserialize(input):
        if input["type"] != "classmethod":
            raise ValueError("Asked to unserialize type '%s' as a classmethod" % input["type"])
        return HeaderClazzMethodRepresentation(input["name"])

class HeaderClazzRepresentation:
    def __init__(self, name, fields, methods, ctors, parent):
        self.name = name
        self.fields = fields
        self.methods = methods
        self.ctors = ctors
        self.parent = parent

    def serialize(self):
        return {
            "type": "class",
            "name": self.name,
            "fields": [field.serialize() for field in self.fields],
            "methods": [method.serialize() for method in self.methods],
            "ctors": [method.serialize() for method in self.ctors],
            "parent": self.parent
        }

    @staticmethod
    def from_clazz_signature(signature):
        return HeaderClazzRepresentation(
            signature.name,
            [HeaderClazzFieldRepresentation.from_field(field) for field in signature.fields],
            [HeaderClazzMethodRepresentation(methodsignature.name) for methodsignature in signature.method_signatures],
            [HeaderClazzMethodRepresentation(methodsignature.name) for methodsignature in signature.ctor_signatures],
            signature.parent_signature.name if signature.parent_signature is not None else None
        )

    @staticmethod
    def unserialize(input):
        if input["type"] != "class":
            raise ValueError("Asked to unserialize type '%s' as a class" % input["type"])
        return HeaderClazzRepresentation(
            input["name"],
            [HeaderClazzFieldRepresentation.unserialize(field) for field in input["fields"]],
            [HeaderClazzMethodRepresentation.unserialize(method) for method in input["methods"]],
            [HeaderClazzMethodRepresentation.unserialize(method) for method in input["ctors"]],
            input["parent"]
        )


class HeaderRepresentation:
    # Filled in below
    HIDDEN: "HeaderRepresentation" = None # type: ignore
    def __init__(self, methods, clazzes, hidden=False):
        self.methods = methods
        self.clazzes = clazzes
        self.hidden = hidden

    def serialize(self):
        if self.hidden:
            return {
                "hidden": True
            }

        return {
            "methods": [method.serialize() for method in self.methods],
            "classes": [clazz.serialize() for clazz in self.clazzes],
            "type": "metadata"
        }

    @staticmethod
    def from_program(program):
        return HeaderRepresentation(
            [HeaderMethodRepresentation.from_method(method, program) for method in program.methods],
            [HeaderClazzRepresentation.from_clazz_signature(clazz) for clazz in program.clazz_signatures if not clazz.is_included]
        )

    @staticmethod
    def unserialize(input):
        if len(input) <= 0 or "hidden" in input and input["hidden"] == True:
            raise ValueError("Asked to unserialize missing or hidden input")

        if input["type"] != "metadata":
            raise ValueError("Asked to unserialize type '%s' as metadata" % input["type"])

        return HeaderRepresentation(
            [HeaderMethodRepresentation.unserialize(method) for method in input["methods"]],
            [HeaderClazzRepresentation.unserialize(clazz) for clazz in input["classes"]]
        )

HeaderRepresentation.HIDDEN = HeaderRepresentation([], [], hidden=True)

def from_json(fname):
    file_data = None
    with open(fname, "r") as f:
        file_data = f.read()
    return HeaderRepresentation.unserialize(json.loads(file_data))

def from_slb(fname):
    file_data = None
    with open(fname, "rb") as f:
        file_data = slbparser.extract_headers(f.read())

    return HeaderRepresentation.unserialize(json.loads(file_data))
