import slbparser
import json
import emitter
import typesys
import util
from typing import NewType, Dict, Union, List, Any, Optional

# Can't make this more specific: https://github.com/python/mypy/issues/731
JsonObject = Dict[str, Any]

class HeaderMethodArgumentRepresentation:
    def __init__(self, name: str, type: str):
        self.name = name
        self.type = type

    def serialize(self) -> JsonObject:
        return {
            "type": "argument",
            "name": self.name,
            "argtype": self.type
        }

    @staticmethod
    def from_argument(argname: str, type: str) -> "HeaderMethodArgumentRepresentation":
        return HeaderMethodArgumentRepresentation(argname, type)

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderMethodArgumentRepresentation":
        if input["type"] != "argument":
            raise ValueError("Asked to unserialize type '%s' as argument" % input["type"])

        return HeaderMethodArgumentRepresentation(input["name"], input["argtype"])

class HeaderMethodRepresentation:
    def __init__(self, name: str, args: List[HeaderMethodArgumentRepresentation], returntype: str,
                containing_clazz: Optional[str], is_ctor: bool, entrypoint: bool, is_override: bool, is_abstract: bool):
        self.name = name
        self.args = args
        self.returntype = returntype
        self.containing_clazz = containing_clazz
        self.is_ctor = is_ctor
        self.entrypoint = entrypoint
        self.is_override = False
        self.is_abstract = is_abstract

    @property
    def numargs(self) -> int:
        return len(self.args)

    def serialize(self) -> JsonObject:
        return {
            "type": "method",
            "name": self.name,
            "arguments": [argument.serialize() for argument in self.args],
            "numargs": len(self.args),
            "returns": self.returntype,
            "containingclass": self.containing_clazz,
            "entrypoint": self.entrypoint,
            "ctor": self.is_ctor,
            "override": self.is_override,
            "abstract": self.is_abstract
        }

    @staticmethod
    def from_method(method: "emitter.MethodSegment", program: "emitter.Program") -> "HeaderMethodRepresentation":
        entrypoint_id = util.nonnull(program.get_entrypoint()).id if program.has_entrypoint() else None
        return HeaderMethodRepresentation(method.signature.name,
            [HeaderMethodArgumentRepresentation.from_argument(arg, argtype.name) for arg, argtype in zip(method.signature.argnames, method.signature.args)],
            method.signature.returntype.name,
            method.signature.containing_class.name if method.signature.containing_class is not None else None,
            is_ctor=method.signature.is_ctor,
            entrypoint=entrypoint_id == method.signature.id,
            is_override=method.signature.is_override,
            is_abstract=method.signature.is_abstract)

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderMethodRepresentation":
        if input["type"] != "method":
            raise ValueError("Asked to unserialize type '%s' as method" % input["type"])

        return HeaderMethodRepresentation(input["name"], [HeaderMethodArgumentRepresentation.unserialize(argument) for argument in input["arguments"]], input["returns"], input["containingclass"], is_ctor=input["ctor"], entrypoint=False, is_override=input["override"], is_abstract=input["abstract"])

class HeaderClazzFieldRepresentation:
    def __init__(self, name: str, type: str):
        self.name = name
        self.type = type

    def serialize(self) -> JsonObject:
        return {
            "type": "field",
            "name": self.name,
            "fieldtype": self.type
        }

    @staticmethod
    def from_field(field: "emitter.ClazzField") -> "HeaderClazzFieldRepresentation":
        return HeaderClazzFieldRepresentation(field.name, field.type.name)

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderClazzFieldRepresentation":
        if input["type"] != "field":
            raise ValueError("Asked to unserialize type '%s' as field" % input["type"])

        return HeaderClazzFieldRepresentation(input["name"], input["fieldtype"])

class HeaderClazzMethodRepresentation:
    def __init__(self, name: str):
        self.name = name

    def serialize(self) -> JsonObject:
        return {
            "type": "classmethod",
            "name": self.name
        }

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderClazzMethodRepresentation":
        if input["type"] != "classmethod":
            raise ValueError("Asked to unserialize type '%s' as a classmethod" % input["type"])
        return HeaderClazzMethodRepresentation(input["name"])

class HeaderClazzRepresentation:
    def __init__(self, name: str, fields: List[HeaderClazzFieldRepresentation], methods: List[HeaderClazzMethodRepresentation],
                ctors: List[HeaderClazzMethodRepresentation], parent: Optional[str], is_abstract: bool):
        self.name = name
        self.fields = fields
        self.methods = methods
        self.ctors = ctors
        self.parent = parent
        self.is_abstract = is_abstract

    def serialize(self) -> JsonObject:
        return {
            "type": "class",
            "name": self.name,
            "fields": [field.serialize() for field in self.fields],
            "methods": [method.serialize() for method in self.methods],
            "ctors": [method.serialize() for method in self.ctors],
            "parent": self.parent,
            "abstract": self.is_abstract
        }

    @staticmethod
    def from_clazz_signature(signature: "emitter.ClazzSignature") -> "HeaderClazzRepresentation":
        return HeaderClazzRepresentation(
            signature.name,
            [HeaderClazzFieldRepresentation.from_field(field) for field in signature.fields],
            [HeaderClazzMethodRepresentation(methodsignature.name) for methodsignature in signature.method_signatures],
            [HeaderClazzMethodRepresentation(methodsignature.name) for methodsignature in signature.ctor_signatures],
            signature.parent_signature.name if signature.parent_signature is not None else None,
            signature.is_abstract
        )

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderClazzRepresentation":
        if input["type"] != "class":
            raise ValueError("Asked to unserialize type '%s' as a class" % input["type"])
        return HeaderClazzRepresentation(
            input["name"],
            [HeaderClazzFieldRepresentation.unserialize(field) for field in input["fields"]],
            [HeaderClazzMethodRepresentation.unserialize(method) for method in input["methods"]],
            [HeaderClazzMethodRepresentation.unserialize(method) for method in input["ctors"]],
            input["parent"],
            input["abstract"]
        )


class HeaderRepresentation:
    # Filled in below
    HIDDEN: "HeaderRepresentation" = None # type: ignore
    def __init__(self, methods: List[HeaderMethodRepresentation], clazzes: List[HeaderClazzRepresentation], hidden: bool=False):
        self.methods = methods
        self.clazzes = clazzes
        self.hidden = hidden

    def serialize(self) -> JsonObject:
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
    def from_program(program: "emitter.Program") -> "HeaderRepresentation":
        return HeaderRepresentation(
            [HeaderMethodRepresentation.from_method(method, program) for method in program.methods],
            [HeaderClazzRepresentation.from_clazz_signature(clazz) for clazz in program.clazz_signatures if not clazz.is_included]
        )

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderRepresentation":
        if len(input) <= 0 or "hidden" in input and input["hidden"] == True:
            raise ValueError("Asked to unserialize missing or hidden input")

        if input["type"] != "metadata":
            raise ValueError("Asked to unserialize type '%s' as metadata" % input["type"])

        return HeaderRepresentation(
            [HeaderMethodRepresentation.unserialize(method) for method in input["methods"]],
            [HeaderClazzRepresentation.unserialize(clazz) for clazz in input["classes"]]
        )

HeaderRepresentation.HIDDEN = HeaderRepresentation([], [], hidden=True)

def from_json(fname: str) -> "HeaderRepresentation":
    file_data = None
    with open(fname, "r") as f:
        file_data = f.read()
    return HeaderRepresentation.unserialize(json.loads(file_data))

def from_slb(fname: str) -> HeaderRepresentation:
    file_data = None
    with open(fname, "rb") as f:
        file_data = slbparser.extract_headers(f.read())
    if file_data is None:
        raise ValueError("Attempt to read headers from slb that has no metadata segment")

    return HeaderRepresentation.unserialize(json.loads(file_data))
