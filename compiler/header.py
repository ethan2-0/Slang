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
    def from_argument(argname: str, typ: typesys.AbstractType) -> "HeaderMethodArgumentRepresentation":
        return HeaderMethodArgumentRepresentation(argname, typ.bytecode_name)

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderMethodArgumentRepresentation":
        if input["type"] != "argument":
            raise ValueError("Asked to unserialize type '%s' as argument" % input["type"])

        return HeaderMethodArgumentRepresentation(input["name"], input["argtype"])

class HeaderGenericParameterRepresentation:
    def __init__(self, name: str, extends: Optional[str], implements: List[str]):
        self.name = name
        self.extends = extends
        self.implements = implements

    def serialize(self) -> JsonObject:
        return {
            "type": "typeparam",
            "name": self.name,
            "extends": self.extends,
            "implements": self.implements
        }

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderGenericParameterRepresentation":
        if input["type"] != "typeparam":
            raise ValueError()
        return HeaderGenericParameterRepresentation(input["name"], input["extends"], input["implements"])

    @staticmethod
    def from_generic_type_parameter(parameter: typesys.GenericTypeArgument) -> "HeaderGenericParameterRepresentation":
        return HeaderGenericParameterRepresentation(
            parameter.name,
            parameter.extends.bytecode_name if parameter.extends is not None else None,
            [interface.bytecode_name for interface in parameter.implements]
        )

class HeaderMethodRepresentation:
    def __init__(self, name: str, args: List[HeaderMethodArgumentRepresentation], returntype: str,
                containing_clazz: Optional[str], containing_interface: Optional[str], is_ctor: bool,
                entrypoint: bool, is_override: bool, is_abstract: bool, type_params: List[HeaderGenericParameterRepresentation]):
        self.name = name
        self.args = args
        self.returntype = returntype
        self.containing_clazz = containing_clazz
        self.containing_interface = containing_interface
        self.is_ctor = is_ctor
        self.entrypoint = entrypoint
        self.is_override = False
        self.is_abstract = is_abstract
        self.type_params = type_params

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
            "containinginterface": self.containing_interface,
            "entrypoint": self.entrypoint,
            "ctor": self.is_ctor,
            "override": self.is_override,
            "abstract": self.is_abstract,
            "typeparams": [param.serialize() for param in self.type_params]
        }

    @staticmethod
    def from_method(method: "emitter.MethodSegment", program: "emitter.Program") -> "HeaderMethodRepresentation":
        entrypoint_id = util.nonnull(program.get_entrypoint()).id if program.has_entrypoint() else None
        return HeaderMethodRepresentation(method.signature.name,
            [HeaderMethodArgumentRepresentation.from_argument(arg, argtype) for arg, argtype in zip(method.signature.argnames, method.signature.args)],
            method.signature.returntype.bytecode_name,
            method.signature.containing_class.name if method.signature.containing_class is not None else None,
            method.signature.containing_interface.name if method.signature.containing_interface is not None else None,
            is_ctor=method.signature.is_ctor,
            entrypoint=entrypoint_id == method.signature.id,
            is_override=method.signature.is_override,
            is_abstract=method.signature.is_abstract,
            type_params=[HeaderGenericParameterRepresentation.from_generic_type_parameter(argument) for argument in method.signature.generic_type_context.arguments] if method.signature.generic_type_context is not None else []
        )

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderMethodRepresentation":
        if input["type"] != "method":
            raise ValueError("Asked to unserialize type '%s' as method" % input["type"])

        return HeaderMethodRepresentation(
            input["name"],
            [HeaderMethodArgumentRepresentation.unserialize(argument) for argument in input["arguments"]],
            input["returns"],
            input["containingclass"],
            input["containinginterface"],
            is_ctor=input["ctor"],
            entrypoint=False,
            is_override=input["override"],
            is_abstract=input["abstract"],
            type_params=[HeaderGenericParameterRepresentation.unserialize(param) for param in input["typeparams"]]
        )

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
        return HeaderClazzFieldRepresentation(field.name, field.type.bytecode_name)

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderClazzFieldRepresentation":
        if input["type"] != "field":
            raise ValueError("Asked to unserialize type '%s' as field" % input["type"])

        return HeaderClazzFieldRepresentation(input["name"], input["fieldtype"])

class HeaderMemberMethodRepresentation:
    def __init__(self, name: str):
        self.name = name

    def serialize(self) -> JsonObject:
        return {
            "type": "membermethod",
            "name": self.name
        }

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderMemberMethodRepresentation":
        if input["type"] != "membermethod":
            raise ValueError("Asked to unserialize type '%s' as a membermethod" % input["type"])
        return HeaderMemberMethodRepresentation(input["name"])

class HeaderClazzRepresentation:
    def __init__(
            self,
            name: str,
            fields: List[HeaderClazzFieldRepresentation],
            methods: List[HeaderMemberMethodRepresentation],
            ctors: List[HeaderMemberMethodRepresentation],
            parent: Optional[str],
            interfaces: List[str],
            is_abstract: bool,
            type_params: List[HeaderGenericParameterRepresentation]):
        self.name = name
        self.fields = fields
        self.methods = methods
        self.ctors = ctors
        self.parent = parent
        self.interfaces = interfaces
        self.is_abstract = is_abstract
        self.type_params = type_params

    def serialize(self) -> JsonObject:
        return {
            "type": "class",
            "name": self.name,
            "fields": [field.serialize() for field in self.fields],
            "methods": [method.serialize() for method in self.methods],
            "ctors": [method.serialize() for method in self.ctors],
            "interfaces": self.interfaces,
            "parent": self.parent,
            "abstract": self.is_abstract,
            "type_params": [param.serialize() for param in self.type_params]
        }

    @staticmethod
    def from_clazz_signature(signature: "emitter.ClazzSignature") -> "HeaderClazzRepresentation":
        return HeaderClazzRepresentation(
            signature.name,
            [HeaderClazzFieldRepresentation.from_field(field) for field in signature.fields],
            [HeaderMemberMethodRepresentation(methodsignature.name) for methodsignature in signature.method_signatures],
            [HeaderMemberMethodRepresentation(methodsignature.name) for methodsignature in signature.ctor_signatures],
            signature.parent_signature.bytecode_name if signature.parent_signature is not None else None,
            [interface.bytecode_name for interface in signature.implemented_interfaces],
            signature.is_abstract,
            [HeaderGenericParameterRepresentation.from_generic_type_parameter(param) for param in signature.generic_type_context.arguments] if signature.generic_type_context is not None else []
        )

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderClazzRepresentation":
        if input["type"] != "class":
            raise ValueError("Asked to unserialize type '%s' as a class" % input["type"])
        return HeaderClazzRepresentation(
            input["name"],
            [HeaderClazzFieldRepresentation.unserialize(field) for field in input["fields"]],
            [HeaderMemberMethodRepresentation.unserialize(method) for method in input["methods"]],
            [HeaderMemberMethodRepresentation.unserialize(method) for method in input["ctors"]],
            input["parent"],
            input["interfaces"],
            input["abstract"],
            [HeaderGenericParameterRepresentation.unserialize(param) for param in input["type_params"]]
        )

class HeaderStaticVariableRepresentation:
    def __init__(self, name: str, typ: str):
        self.name = name
        self.type = typ

    def serialize(self) -> JsonObject:
        return {
            "name": self.name,
            "variabletype": self.type,
            "type": "staticvar"
        }

    @staticmethod
    def from_static_variable(variable: "emitter.StaticVariable") -> "HeaderStaticVariableRepresentation":
        return HeaderStaticVariableRepresentation(variable.name, variable.type.bytecode_name)

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderStaticVariableRepresentation":
        return HeaderStaticVariableRepresentation(input["name"], input["variabletype"])

class HeaderInterfaceRepresentation:
    def __init__(self, name: str, methods: List[HeaderMemberMethodRepresentation]):
        self.name = name
        self.methods = methods

    def serialize(self) -> JsonObject:
        return {
            "name": self.name,
            "methods": [method.serialize() for method in self.methods],
            "type": "interface"
        }

    @staticmethod
    def from_interface(interface: "emitter.Interface") -> "HeaderInterfaceRepresentation":
        return HeaderInterfaceRepresentation(interface.name, [HeaderMemberMethodRepresentation(method.name) for method in interface.method_signatures])

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderInterfaceRepresentation":
        return HeaderInterfaceRepresentation(input["name"], [HeaderMemberMethodRepresentation.unserialize(method) for method in input["methods"]])

class HeaderRepresentation:
    # Filled in below
    HIDDEN: "HeaderRepresentation" = None # type: ignore
    def __init__(self, methods: List[HeaderMethodRepresentation], clazzes: List[HeaderClazzRepresentation], static_variables: List[HeaderStaticVariableRepresentation], interfaces: List[HeaderInterfaceRepresentation], hidden: bool=False):
        self.methods = methods
        self.clazzes = clazzes
        self.static_variables = static_variables
        self.interfaces = interfaces
        self.hidden = hidden

    def serialize(self) -> JsonObject:
        if self.hidden:
            return {
                "hidden": True
            }

        return {
            "methods": [method.serialize() for method in self.methods],
            "classes": [clazz.serialize() for clazz in self.clazzes],
            "staticvars": [var.serialize() for var in self.static_variables],
            "interfaces": [interface.serialize() for interface in self.interfaces],
            "type": "metadata"
        }

    @staticmethod
    def from_program(program: "emitter.Program") -> "HeaderRepresentation":
        return HeaderRepresentation(
            [HeaderMethodRepresentation.from_method(method, program) for method in program.methods],
            [HeaderClazzRepresentation.from_clazz_signature(clazz) for clazz in program.clazz_signatures if not clazz.is_included],
            [HeaderStaticVariableRepresentation.from_static_variable(var) for var in program.static_variables.variables.values() if not var.included],
            [HeaderInterfaceRepresentation.from_interface(interface) for interface in program.interfaces if not interface.included]
        )

    @staticmethod
    def unserialize(input: JsonObject) -> "HeaderRepresentation":
        if len(input) <= 0 or "hidden" in input and input["hidden"] == True:
            raise ValueError("Asked to unserialize missing or hidden input")

        if input["type"] != "metadata":
            raise ValueError("Asked to unserialize type '%s' as metadata" % input["type"])

        return HeaderRepresentation(
            [HeaderMethodRepresentation.unserialize(method) for method in input["methods"]],
            [HeaderClazzRepresentation.unserialize(clazz) for clazz in input["classes"]],
            [HeaderStaticVariableRepresentation.unserialize(var) for var in input["staticvars"]],
            [HeaderInterfaceRepresentation.unserialize(interface) for interface in input["interfaces"]]
        )

HeaderRepresentation.HIDDEN = HeaderRepresentation([], [], [], [], hidden=True)

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
