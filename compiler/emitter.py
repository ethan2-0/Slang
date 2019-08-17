# Import as another name since "opcodes" is used frequently as a variable name
import opcodes as opcodes_module
from opcodes import opcodes as ops
import header
import typesys
import parser
import util
import claims as clms
import interpreter
import hashlib
import itertools
from typing import ClassVar, List, Optional, Union, TypeVar, Dict, Tuple, Generic, NoReturn, Iterator, cast

class RegisterHandle:
    def __init__(self, id: int, type: "typesys.AbstractType") -> None:
        self.id = id
        self.type = type

    def __repr__(self) -> str:
        return "RegisterHandle(id=%s, type=%s)" % (self.id, self.type)

    def __str__(self) -> str:
        return "$%s" % self.id

    def looks_like_register_handle(self) -> None:
        pass

def flatten_ident_nodes(parent: parser.Node) -> str:
    return ".".join([child.data_strict for child in parent])

AnnotateOpcodesGenericType = TypeVar("AnnotateOpcodesGenericType", "List[opcodes_module.OpcodeInstance]", "opcodes_module.OpcodeInstance")
def annotate(opcode: AnnotateOpcodesGenericType, annotation: str) -> AnnotateOpcodesGenericType:
    if isinstance(opcode, list):
        if len(opcode) > 0:
            opcode[0].annotate(annotation)
        return opcode
    elif isinstance(opcode, opcodes_module.OpcodeInstance):
        opcode.annotate(annotation)
        return opcode
    else:
        # This is impossible
        raise ValueError()

class Scopes:
    def __init__(self) -> None:
        self.locals: List[Dict[str, RegisterHandle]] = [dict()]
        self.register_ids = 0
        self.registers: List[RegisterHandle] = []

    def push(self) -> None:
        self.locals.append(dict())

    def pop(self) -> None:
        self.locals.pop()

    def allocate(self, typ: "typesys.AbstractType") -> RegisterHandle:
        ret = RegisterHandle(self.register_ids, typ)
        self.registers.append(ret)
        self.register_ids += 1
        return ret

    def resolve(self, key: str, node: parser.Node) -> RegisterHandle:
        for local in reversed(self.locals):
            if key in local:
                return local[key]
        node.compile_error("Referenced local before declaration: %s" % key)
        # Unreachable
        return None # type: ignore

    def assign(self, key: str, register: RegisterHandle, node: parser.Node) -> None:
        for local in reversed(self.locals):
            if key in local:
                local[key] = register
                return
        node.compile_error("Assigned local before declaration: %s" % key)

    def exists(self, key: str) -> bool:
        for local in reversed(self.locals):
            if key in local:
                return True
        return False

    def let(self, key: str, register: RegisterHandle, node: parser.Node) -> None:
        # Note that Scopes doesn't know anything about static variables. So we
        # could theoretically let a static variable. Callers are responsible
        # for avoiding that.
        if not isinstance(node, parser.Node):
            raise ValueError("Expected Node, got %s" % type(node))

        if not isinstance(register, RegisterHandle):
            raise ValueError("Expected RegisterHandle, got %s" % type(register))

        if key in self.locals[len(self.locals) - 1]:
            node.compile_error("Redeclaring local: %s" % key)

        self.locals[len(self.locals) - 1][key] = register

class StaticVariable:
    def __init__(self, variable_set: "StaticVariableSet", name: str, type: typesys.AbstractType, initializer: interpreter.AbstractInterpreterValue, included: bool = False) -> None:
        self.variable_set = variable_set
        self.name = name
        self.type = type
        self.initializer = initializer
        self.included = included

    def __str__(self) -> str:
        return "static %s: %s" % (self.name, self.type.name)

class StaticVariableSet:
    def __init__(self, program: "Program") -> None:
        self.program = program
        self.variables: Dict[str, StaticVariable] = dict()

    def add_variable(self, variable: StaticVariable) -> None:
        self.variables[variable.name] = variable

    def has_variable(self, name: str) -> bool:
        for search_path in self.program.search_paths:
            if "%s%s" % (search_path, name) in self.variables:
                return True
        return False

    def resolve_variable(self, name: str, node: Optional[parser.Node] = None) -> StaticVariable:
        ret = self.resolve_variable_optional(name)
        if ret is None:
            if node is not None:
                node.compile_error("Attempt to resolve nonexistent static variable '%s'" % name)
            raise ValueError("Attempt to resolve nonexistent static variable '%s'" % name)
        return ret

    def resolve_variable_optional(self, name: str) -> Optional[StaticVariable]:
        for search_path in self.program.search_paths:
            if "%s%s" % (search_path, name) in self.variables:
                return self.variables["%s%s" % (search_path, name)]
        return None

class MethodSignature:
    sequential_id: ClassVar[int] = 0
    def __init__(
                self,
                name: str,
                args: "List[typesys.AbstractType]",
                argnames: "List[str]",
                returntype: "typesys.AbstractType",
                is_ctor: bool,
                containing_class: "Optional[ClazzSignature]",
                containing_interface: "Optional[Interface]",
                is_override: bool,
                is_entrypoint: bool,
                is_abstract: bool,
                generic_type_context: Optional[typesys.GenericTypeContext]) -> None:
        self.name = name
        self.args = args
        self.argnames = argnames
        self.returntype = returntype
        self.is_ctor = is_ctor
        self.id = MethodSignature.sequential_id
        self.containing_class = containing_class
        self.containing_interface = containing_interface
        self.is_override = is_override
        self.is_entrypoint = is_entrypoint
        self.is_abstract = is_abstract
        self.generic_type_context = generic_type_context
        MethodSignature.sequential_id += 1
        if self.is_reference_call and self.is_generic_method:
            raise ValueError("Class methods cannot have generic type parameters")

    @property
    def nargs(self) -> int:
        return len(self.args)

    @property
    def is_class_method(self) -> bool:
        return self.containing_class is not None

    @property
    def is_interface_method(self) -> bool:
        return self.containing_interface is not None

    @property
    def is_reference_call(self) -> bool:
        return self.is_class_method or self.is_interface_method

    @property
    def is_generic_method(self) -> bool:
        if self.generic_type_context is None:
            return False
        return len(self.generic_type_context.arguments) > 0

    def looks_like_method_handle(self) -> None:
        pass

    def __repr__(self) -> str:
        return "MethodSignature(name='%s', nargs=%s, id=%s)" % (self.name, len(self.args), self.id)

    def __str__(self) -> str:
        modifiers = ("override " if self.is_override else "") + ("entrypoint " if self.is_entrypoint else "")
        args_str = ", ".join(["%s: %s" % (name, typ.name) for typ, name in zip(self.args, self.argnames)])
        prefix = ""
        if self.containing_class is not None:
            prefix = "%s." % self.containing_class.name
        if self.containing_interface is not None:
            prefix = "%s." % self.containing_interface.name
        if self.is_ctor:
            return "%sctor(%s)" % (prefix, args_str)
        if self.generic_type_context is not None and len(self.generic_type_context.arguments) > 0:
            type_args_str = "<%s>" % ", ".join([t.name for t in self.generic_type_context.arguments])
        else:
            type_args_str = ""
        return "%sfn %s%s%s(%s): %s" % (modifiers, prefix, self.name, type_args_str, args_str, self.returntype.name)

    def reify(self, arguments: List[typesys.AbstractType], program: "Program") -> "MethodSignature":
        if self.generic_type_context is None:
            # TODO: Eventually every method should have a generic type context
            if len(arguments) > 0:
                raise ValueError("Attempt to provide generic type arguments to a method that doesn't have a generic type context")
            return self
        params = typesys.GenericTypeParameters(util.nonnull(self.generic_type_context), arguments, program.types)
        new_return_type = params.reify(self.returntype)
        argument_types = [params.reify(typ) for typ in self.args]
        return MethodSignature(
            name=self.name,
            args=argument_types,
            argnames=self.argnames,
            returntype=new_return_type,
            is_ctor=self.is_ctor,
            containing_class=self.containing_class,
            containing_interface=self.containing_interface,
            is_override=self.is_override,
            is_entrypoint=self.is_entrypoint,
            is_abstract=self.is_abstract,
            generic_type_context=None
        )

    @staticmethod
    def from_node(
            method: parser.Node,
            program: "Program",
            this_type: "Optional[typesys.AbstractType]" = None,
            containing_class: "Optional[ClazzSignature]" = None,
            containing_interface: "Optional[Interface]" = None) -> "MethodSignature":
        if containing_class is None and method["modifiers"].has_child("override"):
            method.compile_error("Can't both be an override method and not have a containing class")

        if method.i("fn"):
            generic_type_context = typesys.GenericTypeContext(parent=None)
            for type_arg in method["typeparams"]:
                generic_type_context.add_type_argument(type_arg.data_strict)
            if (containing_class is not None or containing_interface is not None) and len(generic_type_context.arguments) > 0:
                method.compile_error("Member methods cannot have type arguments")
            argtypes: "List[typesys.AbstractType]" = \
                cast(List[typesys.AbstractType], [] if this_type is None else [this_type])\
                 + [program.types.resolve_strict(arg[1], generic_type_context) for arg in method[1]]
            argnames = ([] if this_type is None else ["this"]) + [arg[0].data_strict for arg in method[1]]
            is_override = method["modifiers"].has_child("override")
            is_entrypoint = method["modifiers"].has_child("entrypoint")
            is_abstract = method["modifiers"].has_child("abstract")
            if is_entrypoint and containing_class is not None:
                method.compile_error("Can't have an entrypoint inside a class")
            if is_entrypoint and len(argnames) > 0:
                method.compile_error("Can't have an entrypoint with arguments")
            if is_abstract and is_override:
                method.compile_error("Modifiers abstract and override are incompatible")
            if is_abstract and containing_class is None:
                method.compile_error("Cannot have an abstract method outside of a class")
            if (is_override or is_entrypoint or is_abstract) and containing_interface is not None:
                method.compile_error("Interface methods cannot have modifiers")
            name = util.nonnull(util.get_flattened(method[0]))
            if containing_class is None and containing_interface is None and program.namespace is not None:
                name = "%s.%s" % (program.namespace, name)
            return MethodSignature(
                name,
                argtypes,
                argnames,
                returntype=program.types.resolve_strict(method[2], generic_type_context),
                generic_type_context=generic_type_context,
                is_ctor=False,
                containing_class=containing_class,
                containing_interface=containing_interface,
                is_override=is_override,
                is_entrypoint=is_entrypoint,
                is_abstract=is_abstract
            )
        elif method.i("ctor"):
            signature = cast(List[typesys.AbstractType], [this_type]) + [program.types.resolve_strict(arg[1], None) for arg in method[0]]
            argnames = ["this"] + [arg[0].data_strict for arg in method[0]]
            assert containing_class is not None
            assert type(containing_class.name) is str
            return MethodSignature(
                "@@ctor.%s" % containing_class.name,
                signature,
                argnames,
                returntype=program.types.void_type,
                is_ctor=True,
                containing_class=containing_class,
                is_override=False,
                is_entrypoint=False,
                is_abstract=False,
                containing_interface=None,
                generic_type_context=None
            )
        else:
            raise ValueError("This is a compiler bug.")
            # Unreachable
            return None # type: ignore

    @staticmethod
    def scan(top: parser.Node, program: "Program") -> "List[MethodSignature]":
        ret = []
        for method in top:
            if not method.i("fn"):
                continue
            ret.append(MethodSignature.from_node(method, program))
        return ret

class TypeReferenceSet:
    def __init__(self) -> None:
        self.references: List[typesys.AbstractType] = []

    def add_type_reference(self, typ: typesys.AbstractType) -> int:
        self.references.append(typ)
        return len(self.references) - 1

    def __len__(self) -> int:
        return len(self.references)

    def __iter__(self) -> Iterator[typesys.AbstractType]:
        return iter(self.references)

    def __str__(self) -> str:
        return "TypeReferenceSet(%s)" % ", ".join("%s=%s" % (i, self.references[i]) for i in range(len(self)))

class SegmentEmitter:
    pass

class MethodEmitter(SegmentEmitter):
    def __init__(self, top: parser.Node, program: "Program", signature: MethodSignature):
        self.top = top
        # This is used to cache the final list of opcodes after emitting the method.
        # This is empty until the moment the method has been emitted.
        self.opcodes: List[opcodes_module.OpcodeInstance] = []
        self.scope = Scopes()
        self.program = program
        self.types = program.types
        self.signature = signature
        self.generic_type_context = self.signature.generic_type_context
        self.type_references = TypeReferenceSet()
        # This is assigned to later. Technically this is uninitialized leaving
        # the constructor, but nothing that accesses it will be called before
        # it's initialized.
        self.return_type: typesys.AbstractType

    def emit_to_method_signature(self, node: parser.Node, opcodes: "List[opcodes_module.OpcodeInstance]") -> Tuple[MethodSignature, Optional[RegisterHandle]]:
        if util.get_flattened(node) is not None and self.program.get_method_signature_optional(util.nonnull(util.get_flattened(node))) is not None:
            return self.program.get_method_signature(util.nonnull(util.get_flattened(node))), None
        if node.i("ident"):
            node.compile_error("Attempt to call an identifier that doesn't resolve to a method")
        elif node.i("."):
            if node[0].i("super"):
                # We shouldn't ever get to emit_to_method_signature, instead we should handle it directly in emit_expr
                raise ValueError("This is a compiler bug.")
            typ = self.types.decide_type(node[0], self.scope, self.generic_type_context)
            if not node[1].i("ident"):
                raise ValueError("This is a compiler bug")
            if isinstance(typ, typesys.ClazzType):
                if typ.type_of_property_optional(node[1].data_strict) is not None:
                    node.compile_error("Attempt to call a property")
                    # Unreachable
                    return None # type: ignore
                elif typ.method_signature_optional(node[1].data_strict) is not None:
                    reg = self.scope.allocate(typ)
                    opcodes += self.emit_expr(node[0], reg)
                    return typ.method_signature(node[1].data_strict, node), reg
                else:
                    node.compile_error("Attempt to perform property access that doesn't make sense")
                    # Unreachable
                    return None # type: ignore
            elif isinstance(typ, typesys.InterfaceType):
                reg = self.scope.allocate(typ)
                opcodes += self.emit_expr(node[0], reg)
                return typ.method_signature(node[1].data_strict, node), reg
            else:
                node.compile_error("Cannot perform access on something that isn't an instance of a class or an interface")
        else:
            node.compile_error("Attempt to call something that can't be called")
            # Unreachable
            return None # type: ignore

    def emit_expr(self, node: parser.Node, register: RegisterHandle) -> List[opcodes_module.OpcodeInstance]:
        opcodes = []
        if node.i("number"):
            nt = int(node.data_strict)
            opcodes.append(ops["load"].ins(register, abs(nt), node=node))
            if nt < 0:
                opcodes.append(ops["twocomp"].ins(register))
        elif node.i("string"):
            string_val = node.data_strict

            numerical_values: List[interpreter.AbstractInterpreterValue] = []
            for ch in string_val:
                numerical_values.append(self.program.interpreter.create_int_value(ord(ch)))
            string_array_type = self.types.get_array_type(self.types.int_type)
            interpreter_value = interpreter.InterpreterValueArray(string_array_type, numerical_values)
            # If we have the same string going into the same register multiple
            # times then we'll end up with duplication here, but it's fine
            # since StaticVariableSet implements a last-value-wins mapping
            static_var_name = "--string-%s-%d-%s" % (self.signature.name, register.id, hashlib.md5(string_val.encode("utf8")).hexdigest())
            self.program.static_variables.add_variable(StaticVariable(self.program.static_variables, static_var_name, string_array_type, interpreter_value))

            string_array_register = self.scope.allocate(string_array_type)
            opcodes.append(annotate(ops["staticvarget"].ins(static_var_name, string_array_register, node=node), "string instantiation"))

            stdlib_string_type = self.types.get_string_type()
            if stdlib_string_type is None:
                node.compile_error("Cannot use string literal without an implementation of stdlib.String")
            opcodes.append(ops["new"].ins(stdlib_string_type.signature, register, node=node))

            opcodes.append(ops["param"].ins(string_array_register, 0))
            string_int_register = self.scope.allocate(self.types.int_type)
            opcodes.append(ops["load"].ins(string_int_register, 0))
            opcodes.append(ops["param"].ins(string_int_register, 1))
            opcodes.append(ops["load"].ins(string_int_register, len(string_val)))
            opcodes.append(ops["param"].ins(string_int_register, 2))
            string_ctor_signature = stdlib_string_type.signature.ctor_signatures[0]
            opcodes.append(ops["classcall"].ins(register, string_ctor_signature, self.scope.allocate(self.types.bool_type)))
        elif node.i("as"):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[0], lhs_reg)
            rhs_type = self.types.resolve_strict(node[1], self.generic_type_context)
            if lhs_reg.type.is_clazz() and rhs_type.is_clazz() and not lhs_reg.type.is_assignable_to(rhs_type) and not rhs_type.is_assignable_to(lhs_reg.type):
                node.warn("Impossible cast: LHS %s is incompatible with RHS %s" % (lhs_reg.type, rhs_type))
            if not rhs_type.is_clazz() and not rhs_type.is_interface():
                node.compile_error("Cannot cast to a type that isn't a class or interface.")
            if not lhs_reg.type.is_clazz() and not lhs_reg.type.is_interface():
                node.compile_error("Cannot cast something that isn't a class or interface.")
            opcodes.append(ops["cast"].ins(lhs_reg, register, node=node))
        elif node.i("instanceof"):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[0], lhs_reg)
            # result_reg = self.scope.allocate(self.types.bool_type)
            # TODO: Potentially emit warning if RHS is incompatible with LHS type
            rhs_type = self.types.resolve_strict(node[1], self.generic_type_context)
            if lhs_reg.type.is_generic_argument() or rhs_type.is_generic_argument():
                node.compile_error("Instanceof does not yet support generics")
            if lhs_reg.type.is_assignable_to(rhs_type):
                node.warn("Tautology: %s always instanceof %s" % (lhs_reg.type, rhs_type))
            if (not isinstance(lhs_reg.type, typesys.GenericTypeArgument)) and (not isinstance(rhs_type, typesys.GenericTypeArgument)) and (not lhs_reg.type.is_assignable_to(rhs_type)) and (not rhs_type.is_assignable_to(lhs_reg.type)):
                node.warn("Contradiction: there cannot exist any %s instanceof %s" % (lhs_reg.type, rhs_type))
            type_index = self.type_references.add_type_reference(rhs_type)
            opcodes.append(ops["instanceof"].ins(lhs_reg, register, type_index, node=node))
        elif node.of("+", "*", "^", "&", "|", "%", "/", "==", ">=", "<=", ">", "<", "!=", "and", "or"):
            lhs = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[0], lhs)
            rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[1], rhs)
            opcodes.append(ops[{
                "+": "add",
                "*": "mult",
                "^": "xor",
                "&": "and",
                "|": "or",
                "/": "div",
                "%": "modulo",
                "==": "equals",
                "<": "lt",
                ">": "gt",
                "<=": "lteq",
                ">=": "gteq",
                "!=": "equals",
                # TODO: What happens if x0 is 0x1 and x1 is 0x2? Specifically for "and", and acting on booleans
                "and": "and",
                "or": "or"
            }[node.type]].ins(lhs, rhs, register, node=node))
            if node.i("!="):
                opcodes.append(ops["invert"].ins(register, node=node))
        elif node.i("not"):
            if not register.type.is_boolean():
                raise typesys.TypingError(node, "Operator 'not' needs boolean argument")
            opcodes += self.emit_expr(node[0], register)
            opcodes.append(ops["invert"].ins(register, node=node))
        elif node.i("~"):
            if not register.type.is_numerical():
                raise typesys.TypingError(node, "Operator '~' needs numerical argument")
            opcodes += self.emit_expr(node[0], register)
            reg = self.scope.allocate(self.types.int_type)
            opcodes.append(ops["load"].ins(reg, 0xffffffff, node=node))
            opcodes.append(ops["xor"].ins(reg, register, register, node=node))
        # All of these conditions go on one line so as not to disturb the
        # if-else flow. This is here because, were it any lower, we'd get
        # problems in the .i("ident") part, for example.
        elif node.of("ident", ".") and util.get_flattened(node) is not None and self.program.static_variables.has_variable(util.nonnull(util.get_flattened(node))):
            opcodes.append(ops["staticvarget"].ins(self.program.static_variables.resolve_variable(util.nonnull(util.get_flattened(node))).name, register, node=node))
        elif node.i("ident"):
            opcodes.append(annotate(ops["mov"].ins(self.scope.resolve(node.data_strict, node), register, node=node), "access %s" % node.data))
        elif node.i("true"):
            opcodes.append(ops["load"].ins(register, 1, node=node))
        elif node.i("false"):
            opcodes.append(ops["load"].ins(register, 0, node=node))
        elif node.i("null"):
            opcodes.append(ops["zero"].ins(register, node=node))
        elif node.i("-"):
            if len(node.children) == 1:
                if not self.types.decide_type(node[0], self.scope, self.generic_type_context).is_numerical():
                    raise typesys.TypingError(node, "Operator '-' needs numerical argument")
                opcodes += self.emit_expr(node[0], register)
                opcodes.append(ops["twocomp"].ins(register, node=node))
            else:
                lhs = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
                opcodes += self.emit_expr(node[0], lhs)
                rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope, self.generic_type_context))
                opcodes += self.emit_expr(node[1], rhs)
                opcodes.append(ops["twocomp"].ins(rhs, node=node))
                opcodes.append(ops["add"].ins(lhs, rhs, register, node=node))
        elif node.i("call") and node[0].i(".") and node[0][0].i("super"):
            # Super call
            emitted_opcodes: List[opcodes_module.OpcodeInstance] = []
            containing_class = self.signature.containing_class
            if containing_class is None:
                node[0].compile_error("Cannot reference super outside of a class")
            superclass = containing_class.parent_signature
            if superclass is None:
                node[0].compile_error("Cannot reference super in a class without a superclass")
            method_name = node[0][1].data_strict
            method_signature = superclass.get_method_signature_by_name(method_name)
            if method_signature is None:
                node[0][1].compile_error("Attempt to call method of superclass that doesn't exist")
            if method_signature.is_abstract:
                node[0][1].compile_error("Attempt to perform super call to abstract method")
            if not len(node["params"]) == len(method_signature.args) - 1:
                node.compile_error("Expected %s arguments, got %s" % (len(node), len(method_signature.args)))
            param_reg: List[RegisterHandle] = []
            for param, index in zip(node["params"], itertools.count()):
                expected_type = method_signature.args[index + 1]
                reg = self.scope.allocate(self.types.decide_type(param, self.scope, self.generic_type_context))
                if not reg.type.is_assignable_to(expected_type):
                    raise typesys.TypingError(param, "Invalid parameter type: %s is not assignable to %s" % (reg.type, expected_type))
                emitted_opcodes += self.emit_expr(param, reg)
                param_reg.append(reg)
            for i in range(len(param_reg)):
                emitted_opcodes.append(ops["param"].ins(param_reg[i], i, node=node))
            # TODO: scope.resolve("this") is ugly
            emitted_opcodes.append(ops["classcallspecial"].ins(superclass, method_signature, register, self.scope.resolve("this", node), node=node))
            opcodes += annotate(emitted_opcodes, "super method call")
        elif node.i("call"):
            # Normal (not super) call
            emitted_opcodes = []
            signature, optional_lhs_reg = self.emit_to_method_signature(node[0], emitted_opcodes)
            opcodes += emitted_opcodes

            typeargs: Optional[List[typesys.AbstractType]] = None
            if len(node["typeargs"]) > 0:
                typeargs = [self.types.resolve_strict(typearg, self.generic_type_context) for typearg in node["typeargs"]]
                signature = signature.reify(typeargs, self.program)

            param_registers = []

            if len(node["params"]) != len(signature.args) + (-1 if signature.is_reference_call else 0):
                node.compile_error("Expected %s arguments, got %s for method %s" % (len(signature.args), len(node["params"]), signature))
            for param, index in zip(node["params"], itertools.count(1 if signature.is_reference_call else 0)):
                inferred_type = self.types.decide_type(param, self.scope, self.generic_type_context)
                declared_type = signature.args[index]
                if not inferred_type.is_assignable_to(declared_type):
                    raise typesys.TypingError(node, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                reg = self.scope.allocate(inferred_type)
                opcodes += self.emit_expr(param, reg)
                param_registers.append((reg, index + (-1 if signature.is_reference_call else 0)))
            for reg, index in param_registers:
                opcodes.append(ops["param"].ins(reg, index, node=node))
            if signature.is_class_method:
                opcodes.append(ops["classcall"].ins(util.nonnull(optional_lhs_reg), signature, register, node=node))
            elif signature.is_interface_method:
                opcodes.append(ops["interfacecall"].ins(util.nonnull(optional_lhs_reg), signature, register, node=node))
            else:
                typeargs_typerefs = [self.type_references.add_type_reference(typ) for typ in typeargs] if typeargs is not None else []
                opcodes.append(ops["call"].ins(signature, register, typeargs_typerefs, node=node))
        elif node.i("new"):
            typ = self.types.resolve_strict(node[0], self.generic_type_context)
            if not isinstance(typ, typesys.ClazzType):
                node[0].compile_error("Attempt to instantiate a type that isn't a class")
            if typ.signature.is_abstract:
                node[0].compile_error("Cannot instantiate an abstract class")
            clazz_signature = typ.signature
            result_register = self.scope.allocate(self.types.decide_type(node, self.scope, self.generic_type_context))
            opcodes.append(ops["new"].ins(clazz_signature, result_register, node=node))
            # TODO: Support multiple constructors and method overloading
            # TODO: A lot of this code is copy-pasted
            if len(clazz_signature.ctor_signatures) < 1:
                node.compile_error("Type has no constructors")
            ctor_signature = clazz_signature.ctor_signatures[0]
            if len(node["params"]) != len(ctor_signature.args) - 1:
                node.compile_error("Expected %s arguments, got %s" % (len(ctor_signature.args), len(node.children) - 1 + 1))
            for param, index in zip(node["params"], itertools.count()):
                inferred_type = self.types.decide_type(param, self.scope, self.generic_type_context)
                declared_type = ctor_signature.args[index + 1]
                if not inferred_type.is_assignable_to(declared_type):
                    raise typesys.TypingError(node, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                reg = self.scope.allocate(inferred_type)
                opcodes += self.emit_expr(param, reg)
                opcodes.append(ops["param"].ins(reg, index, node=node))
            opcodes.append(ops["classcall"].ins(result_register, ctor_signature, self.scope.allocate(self.types.bool_type), node=node))
            opcodes.append(ops["mov"].ins(result_register, register, node=node))
        elif node.i("["):
            if not register.type.is_array():
                raise typesys.TypingError(node, "Cannot assign an array to something that isn't an array")
            arrlen_reg = self.scope.allocate(self.types.int_type)
            opcodes.append(annotate(ops["load"].ins(arrlen_reg, len(node), node=node), "array instantiation length"))
            opcodes.append(ops["arralloc"].ins(register, arrlen_reg, node=node))
            index_reg = self.scope.allocate(self.types.int_type)
            element_reg = self.scope.allocate(cast(typesys.ArrayType, register.type).parent_type)
            for i, elm in zip(range(len(node)), node):
                opcodes.append(annotate(ops["load"].ins(index_reg, i, node=node), "array instantiation index"))
                opcodes += self.emit_expr(elm, element_reg)
                opcodes.append(ops["arrassign"].ins(register, index_reg, element_reg, node=node))
        elif node.i("access"):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[0], lhs_reg)
            if not lhs_reg.type.is_array():
                raise typesys.TypingError(node[0], "Cannot do array access on something that isn't an array")
            if not cast(typesys.ArrayType, lhs_reg.type).parent_type.is_assignable_to(register.type):
                # This should have already been caught by typechecking further up
                raise ValueError("This is a compiler bug")
            index_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope, self.generic_type_context))
            if not index_reg.type.is_numerical():
                raise typesys.TypingError(node, "Index to array access must be numerical")
            opcodes += self.emit_expr(node[1], index_reg)
            opcodes.append(ops["arraccess"].ins(lhs_reg, index_reg, register, node=node))
        elif node.i("arrinst"):
            quantity_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope, self.generic_type_context))
            if not quantity_reg.type.is_numerical():
                raise typesys.TypingError(node, "Array lengths must be numerical")
            opcodes += self.emit_expr(node[1], quantity_reg)
            opcodes.append(ops["arralloc"].ins(register, quantity_reg, node=node))
        elif node.i("#"):
            arr_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            if not arr_reg.type.is_array():
                raise typesys.TypingError(node, "Can't decide length of something that isn't an array")
            opcodes += self.emit_expr(node[0], arr_reg)
            opcodes.append(ops["arrlen"].ins(arr_reg, register, node=node))
        elif node.i("."):
            if node[0].i("ident"):
                lhs_reg = self.scope.resolve(node[0].data_strict, node[0])
            else:
                # node, register
                lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
                opcodes += self.emit_expr(node[0], lhs_reg)
            opcodes.append(ops["access"].ins(lhs_reg, node[1].data_strict, register, node=node))
        else:
            node.compile_error("Unexpected")
        return opcodes

    def assign_to_expr(self, expr: parser.Node, value: RegisterHandle) -> List[opcodes_module.OpcodeInstance]:
        opcodes: List[opcodes_module.OpcodeInstance] = []
        if expr.of("ident", ".") and util.get_flattened(expr) is not None \
                and self.program.static_variables.has_variable(util.nonnull(util.get_flattened(expr))):
            static_variable = self.program.static_variables.resolve_variable(util.nonnull(util.get_flattened(expr)))
            if not static_variable.type.is_assignable_from(value.type):
                raise typesys.TypingError(expr, "Attempt to make incompatible assignment to static variable: %s, %s" % (static_variable.type, value.type))
            opcodes.append(ops["staticvarset"].ins(value, static_variable.name, node=expr))
        elif expr.i("ident"):
            lhs_reg = self.scope.resolve(expr.data_strict, expr)
            if not value.type.is_assignable_to(lhs_reg.type):
                raise typesys.TypingError(expr, "Attempt to make incompatible assignment: %s, %s" % (lhs_reg.type, value.type))
            opcodes.append(ops["mov"].ins(value, lhs_reg, node=expr))
        elif expr.i("."):
            expr_lhs = expr[0]
            expr_lhs_type = self.types.decide_type(expr_lhs, self.scope, self.generic_type_context)
            if not isinstance(expr_lhs_type, typesys.ClazzType):
                raise typesys.TypingError(expr, "Attempt to access property of something that isn't a class: %s" % expr_lhs_type)
            expr_lhs_reg = self.scope.allocate(expr_lhs_type)
            opcodes += self.emit_expr(expr_lhs, expr_lhs_reg)
            if not expr[1].i("ident"):
                raise ValueError("This is a compiler bug.")
            if not expr_lhs_type.type_of_property(expr[1].data_strict, expr).is_assignable_from(value.type):
                raise typesys.TypingError(expr, "Incompatible types in class property assignment: %s, %s" % (expr_lhs_type, value.type))
            opcodes.append(ops["assign"].ins(expr_lhs_reg, expr[1].data_strict, value, node=expr))
        elif expr.i("access"):
            expr_lhs = expr[0]
            expr_lhs_type = self.types.decide_type(expr_lhs, self.scope, self.generic_type_context)
            if not isinstance(expr_lhs_type, typesys.ArrayType):
                raise typesys.TypingError(expr, "Attempt to index something that isn't an array: %s" % expr_lhs_type)
            if not expr_lhs_type.parent_type.is_assignable_from(value.type):
                raise typesys.TypingError(expr, "Attempt to assign incompatible type as array element: %s, %s" % (expr_lhs_type.parent_type, value.type))
            expr_lhs_reg = self.scope.allocate(expr_lhs_type)
            opcodes += self.emit_expr(expr_lhs, expr_lhs_reg)
            expr_index = expr[1]
            index_type = self.types.decide_type(expr_index, self.scope, self.generic_type_context)
            if not isinstance(index_type, typesys.IntType):
                raise typesys.TypingError(expr_index, "Attempt to index an array by something that isn't an integer: %s" % index_type)
            index_reg = self.scope.allocate(index_type)
            opcodes += self.emit_expr(expr_index, index_reg)
            opcodes.append(ops["arrassign"].ins(expr_lhs_reg, index_reg, value, node=expr))
        elif expr.i("call"):
            expr.compile_error("Cannot assign to a method call")
        else:
            raise ValueError("This is a compiler bug.")
        return opcodes

    def emit_statement(self, node: parser.Node) -> Tuple[List[opcodes_module.OpcodeInstance], clms.ClaimSpace]:
        opcodes: List[opcodes_module.OpcodeInstance] = []
        claims = clms.ClaimSpace()
        if node.i("statements"):
            self.scope.push()
            for statement in node:
                new_opcodes, new_claim_space = self.emit_statement(statement)
                opcodes += new_opcodes
                claims.add_claims_conservative(*new_claim_space.claims)
        elif node.i("call") or node.i("expr"):
            result = self.scope.allocate(self.types.decide_type(node if node.i("call") else node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node if node.i("call") else node[0], result)
        elif node.i("let"):
            # Notice that a variable may have only exactly one let statement
            # in a given scope. So we can typecheck just with that register's
            # type, we don't have to associate with the scope directly.
            if len(node) > 2:
                type = self.types.decide_type(node[2], self.scope, self.generic_type_context)
                if not node[1].i("infer_type"):
                    inferred_type = type
                    # This is the declared type
                    type = self.types.resolve_strict(node[1], self.generic_type_context)
                    if not inferred_type.is_assignable_to(type):
                        raise typesys.TypingError(node, "'%s' is not assignable to '%s'" % (inferred_type, type))
            else:
                if node[1].i("infer_type"):
                    node.compile_error("Variables declared without assignment must be given explicit type")
                type = self.types.resolve_strict(node[1], self.generic_type_context)

            reg = self.scope.allocate(type)
            self.scope.let(node[0].data_strict, reg, node)

            if len(node) > 2:
                opcodes += annotate(self.emit_expr(node[2], reg), "let %s" % node[0].data)
        elif node.i("return"):
            if len(node.children) > 0:
                stated_return_type = self.types.decide_type(node[0], self.scope, self.generic_type_context)
                if not self.return_type.is_assignable_from(stated_return_type):
                    raise typesys.TypingError(node, "Return type mismatch")
                reg = self.scope.allocate(stated_return_type)
                opcodes += self.emit_expr(node[0], reg)
            else:
                if not self.return_type.is_assignable_to(self.types.void_type):
                    raise typesys.TypingError(node, "Can't return nothing from a method unless the method returns void")
                reg = self.scope.allocate(self.types.int_type)
                opcodes.append(ops["zero"].ins(reg, node=node))
            claims.add_claims(clms.ClaimReturns())
            opcodes.append(ops["return"].ins(reg, node=node))
        elif node.i("expr"):
            reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[0], reg)
        elif node.i("assignment"):
            lhs_type = self.types.decide_type(node[0], self.scope, self.generic_type_context)
            rhs_type = self.types.decide_type(node[1], self.scope, self.generic_type_context)
            if not lhs_type.is_assignable_from(rhs_type):
                raise typesys.TypingError(node, "Need RHS to be assignable to LHS: %s, %s" % (lhs_type, rhs_type))
            value_reg = self.scope.allocate(rhs_type)
            opcodes += self.emit_expr(node[1], value_reg)
            opcodes += self.assign_to_expr(node[0], value_reg)
        elif node.of("+=", "*=", "-=", "/="):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[0], lhs_reg)
            rhs_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope, self.generic_type_context))
            opcodes += self.emit_expr(node[1], rhs_reg)
            if not lhs_reg.type.is_numerical() or not rhs_reg.type.is_numerical():
                raise typesys.TypingError(node, "LHS and RHS of arithmetic must be numerical, not %s and %s" % (lhs_reg.type, rhs_reg.type))
            if node.i("-="):
                opcodes.append(ops["twocomp"].ins(rhs_reg, node=node))
            opcodes.append(ops[{
                "+=": "add",
                "-=": "add",
                "*=": "mult",
                "/=": "div"
            }[node.type]].ins(lhs_reg, rhs_reg, lhs_reg, node=node))
            opcodes += self.assign_to_expr(node[0], lhs_reg)
        elif node.i("while"):
            result_opcodes: List[opcodes_module.OpcodeInstance] = []
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of a while loop must be boolean")
            conditional = self.emit_expr(node[0], condition_register)
            # For now, we can't be sure whether the content of a while loop is ever run
            statement_opcodes, _unused_claims = self.emit_statement(node[1])
            nop = ops["nop"].ins()
            result_opcodes += conditional
            result_opcodes.append(ops["jf"].ins(condition_register, nop, node=node))
            result_opcodes += statement_opcodes
            result_opcodes.append(ops["goto"].ins(conditional[0], node=node))
            result_opcodes.append(nop)
            opcodes += result_opcodes
        elif node.i("for"):
            result_opcodes = []
            setup_statement, setup_claim_space = self.emit_statement(node[0])
            result_opcodes += annotate(setup_statement, "for loop setup")
            claims.add_claims_conservative(*setup_claim_space.claims)

            condition_register = self.scope.allocate(self.types.decide_type(node[1], self.scope, self.generic_type_context))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of a for loop must be boolean")
            conditional = annotate(self.emit_expr(node[1], condition_register), "for loop condition")
            result_opcodes += conditional
            end_nop = ops["nop"].ins(node=node)
            result_opcodes.append(ops["jf"].ins(condition_register, end_nop, node=node))

            # For now, we don't know whether the body of a for loop will ever be run
            body_statement, _unused_body_claim_space = self.emit_statement(node[3])
            result_opcodes += annotate(body_statement, "for loop body")

            # For now, we don't know whether the each-time part of a for loop will ever be run
            each_time_statement, _unused_setup_claim_space = self.emit_statement(node[2])
            eachtime = annotate(each_time_statement, "for loop each time")
            result_opcodes += eachtime

            result_opcodes.append(annotate(ops["goto"].ins(conditional[0], node=node), "for loop loop"))
            result_opcodes.append(end_nop)

            opcodes += result_opcodes
        elif node.i("if"):
            result_opcodes = []
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope, self.generic_type_context))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of an if statement must be boolean")
            conditional = annotate(self.emit_expr(node[0], condition_register), "if predicate")
            true_path_statement, true_path_claims = self.emit_statement(node[1])
            statement_opcodes = annotate(true_path_statement, "if true path")
            nop = ops["nop"].ins(node=node)
            result_opcodes += conditional
            result_opcodes.append(ops["jf"].ins(condition_register, nop, node=node))
            result_opcodes += statement_opcodes
            else_end_nop = ops["NOP"].ins(node=node)
            if len(node) > 2:
                result_opcodes.append(ops["goto"].ins(else_end_nop, node=node)) #
            result_opcodes.append(nop)
            if len(node) > 2:
                false_path_statement, false_path_claims = self.emit_statement(node[2])
                result_opcodes += annotate(false_path_statement, "if false path")
                result_opcodes.append(else_end_nop)
                # We know that the intersection of the claims of the true and
                # false path will always be true.
                claims.add_claims_conservative(*true_path_claims.intersection(false_path_claims).claims)
            opcodes += result_opcodes
        elif node.of("++", "--"):
            lhs_type = self.types.decide_type(node[0], self.scope, self.generic_type_context)
            if not lhs_type.is_numerical():
                raise typesys.TypingError(node, "Attempt to increment something that isn't numerical: %s" % lhs_type)
            lhs_reg = self.scope.allocate(lhs_type)
            opcodes += annotate(self.emit_expr(node[0], lhs_reg), "++ expression")
            reg = self.scope.allocate(self.types.int_type)
            opcodes.append(ops["load"].ins(reg, 1, node=node))
            if node.i("--"):
                opcodes.append(ops["twocomp"].ins(reg, node=node))
            opcodes.append(ops["add"].ins(reg, lhs_reg, reg, node=node))
            opcodes += self.assign_to_expr(node[0], reg)
        elif node.i("super"):
            emitted_opcodes: List[opcodes_module.OpcodeInstance] = []
            # Note that we don't check if we're in a constructor. Though I can't
            # imagine why someone would want to call the superclass's
            # constructor outside of a constructor, it's not hurting anything.
            containing_class = self.signature.containing_class
            if containing_class is None:
                node.compile_error("Cannot invoke super constructor other than in a class method")
            clazz_signature = containing_class.parent_signature
            if clazz_signature is None:
                node.compile_error("Cannot invoke super constructor in class without a superclass")
            # For now, every class has exactly one constructor
            ctor_signature = clazz_signature.ctor_signatures[0]
            param_registers: List[RegisterHandle] = []
            if len(node["params"]) != len(ctor_signature.args) - 1:
                node.compile_error("Expected %s arguments, got %s" % (len(ctor_signature.args), len(node["params"]) + 1))
            for param, index in zip(node["params"], itertools.count()):
                reg = self.scope.allocate(self.types.decide_type(param, self.scope, self.generic_type_context))
                if not reg.type.is_assignable_to(ctor_signature.args[index + 1]):
                    raise typesys.TypingError(node, "Invalid parameter type: %s is not assignable to %s" % (reg.type, ctor_signature.args[index + 1]))
                emitted_opcodes += self.emit_expr(param, reg)
                param_registers.append(reg)
            for i in range(len(param_registers)):
                emitted_opcodes.append(ops["param"].ins(param_registers[i], i, node=node))
            # TODO: scope.resolve("this") is ugly
            emitted_opcodes.append(ops["classcallspecial"].ins(clazz_signature, ctor_signature, self.scope.allocate(self.types.bool_type), self.scope.resolve("this", node), node=node))
            opcodes += annotate(emitted_opcodes, "super cosntructor call")
        else:
            node.compile_error("Unexpected (this is a compiler bug)")
        return opcodes, claims

    def emit(self) -> List[opcodes_module.OpcodeInstance]:
        self.return_type = self.signature.returntype
        # This caused a bug at some point
        assert self.top.i("fn") or self.top.i("ctor")
        for argtype, argname in zip(self.signature.args, self.signature.argnames):
            self.scope.let(argname, self.scope.allocate(argtype), self.top[1] if self.top.i("fn") else self.top[0])
        opcodes, claims = self.emit_statement(self.top[3] if self.top.i("fn") else self.top[1])
        if not self.signature.is_abstract and self.signature.containing_interface is None and not claims.contains_equivalent(clms.ClaimReturns()):
            if self.signature.returntype.is_void():
                zeroreg = self.scope.allocate(self.types.int_type)
                opcodes.append(ops["zero"].ins(zeroreg))
                opcodes.append(ops["return"].ins(zeroreg))
            else:
                self.top[3].compile_error("Non-void method might not return")
        return opcodes

class Segment:
    def __init__(self, humantype: str, realtype: int) -> None:
        self.humantype = humantype
        self.realtype = realtype

    def __str__(self) -> str:
        return "Segment(type='%s', code=%s)" % (self.humantype, self.realtype)

    def print_(self) -> None:
        print(str(self))

    def evaluate(self, program: "Program") -> None:
        pass

class MethodSegment(Segment):
    def __init__(self, method: parser.Node, signature: MethodSignature) -> None:
        Segment.__init__(self, "method", 0x00)
        self.method = method
        self.signature = signature
        self.opcodes: List[opcodes_module.OpcodeInstance]
        self.num_registers: int
        self.scope: Scopes
        self.emitter: MethodEmitter

    def emit_opcodes(self, program: "Program") -> List[opcodes_module.OpcodeInstance]:
        emitter = MethodEmitter(self.method, program, self.signature)
        ret = emitter.emit()
        # TODO: This is spaghetti code
        self.emitter = emitter
        self.num_registers = emitter.scope.register_ids
        self.scope = emitter.scope
        self.opcodes = ret
        return ret

    def __str__(self) -> str:
        return "MethodSegment(type='%s', typecode=%s, signature=%s)" % (self.humantype, self.realtype, self.signature)

    def print_(self) -> None:
        Segment.print_(self)
        print("Registers:", ", ".join([repr(register) for register in self.emitter.scope.registers]))
        print("Type references:", self.emitter.type_references)
        for opcode, index in zip(self.opcodes, range(len(self.opcodes))):
            print("%6d: %s" % (index, str(opcode)))

    def evaluate(self, program: "Program") -> None:
        self.opcodes = self.emit_opcodes(program)

class MetadataSegment(Segment):
    def __init__(self, entrypoint: Optional[MethodSignature]=None, headers: header.HeaderRepresentation=header.HeaderRepresentation.HIDDEN) -> None:
        Segment.__init__(self, "metadata", 0x01)
        self.entrypoint = entrypoint
        self.headers = headers

    def __str__(self) -> str:
        return "MetadataSegment(type='%s', typecode=%s, entrypoint=%s)" % (self.humantype, self.realtype, self.entrypoint)

    def print_(self) -> None:
        Segment.print_(self)
        print("    entrypoint=%s" % self.entrypoint)
        print("    headers omitted for brevity (pass --headers to show anyway)")

    def has_entrypoint(self) -> bool:
        return self.entrypoint is not None

    def evaluate(self, program: "Program") -> None:
        Segment.evaluate(self, program)

class Interface:
    def __init__(self, name: str, node: Optional[parser.Node], included: bool = False) -> None:
        self.name = name
        self.node = node
        self.included = included
        self.method_signatures: List[MethodSignature] = []
        self.type: Optional[typesys.InterfaceType]

    def get_signature_by_name(self, name: str) -> Optional[MethodSignature]:
        for signature in self.method_signatures:
            if signature.name == name:
                return signature
        return None

    def add_to_typesys(self, types: typesys.TypeSystem) -> None:
        self.type = types.accept_interface(self)

    def add_signature(self, signature: MethodSignature) -> None:
        self.method_signatures.append(signature)

    def add_signatures_from_node(self, program: "Program") -> None:
        if self.node is None:
            raise ValueError("This is a compiler bug.")
        for node in self.node["interfacebody"]:
            if not node.i("fn"):
                raise ValueError("This is a compiler bug.")
            self.add_signature(MethodSignature.from_node(node, program, containing_interface=self, this_type=util.nonnull(self.type)))

    def __str__(self) -> str:
        return "interface %s" % self.name

    @staticmethod
    def create_from_node(node: parser.Node, program: "Program") -> "Interface":
        name = node["ident"].data_strict
        if program.namespace is not None:
            name = "%s.%s" % (program.namespace, name)
        return Interface(name, node)

class InterfaceSegment(Segment):
    def __init__(self, interface: Interface) -> None:
        Segment.__init__(self, "interface", 0x04)
        self.interface = interface

    def __str__(self) -> str:
        return "InterfaceSegment(type='%s', typecode=%s, interface=%s)" % (self.humantype, self.realtype, self.interface)

    def print_(self) -> None:
        Segment.print_(self)
        for signature in self.interface.method_signatures:
            print("    %s" % signature)

    def evaluate(self, program: "Program") -> None:
        Segment.evaluate(self, program)
        if self.interface.node is None:
            return
        for node in self.interface.node["interfacebody"]:
            if not node.i("fn"):
                raise ValueError("This is a compiler bug")
            method = program.emit_method(node, util.nonnull(self.interface.get_signature_by_name(node["ident"].data_strict)))
            program.segments.append(method)
            program.methods.append(method)

class ClazzField:
    def __init__(self, name: str, type: typesys.AbstractType) -> None:
        self.name = name
        self.type = type

class ClazzSignature:
    def __init__(
                self,
                name: str,
                fields: List[ClazzField],
                method_signatures: List[MethodSignature],
                ctor_signatures: List[MethodSignature],
                parent_type: Optional[typesys.ClazzType],
                is_abstract: bool,
                implemented_interfaces: List[typesys.InterfaceType],
                is_included: bool=False) -> None:
        self.name = name
        self.fields = fields
        self.method_signatures = method_signatures
        self.ctor_signatures = ctor_signatures
        self.is_abstract = is_abstract
        self.is_included = is_included
        self.parent_type = parent_type
        self.implemented_interfaces = implemented_interfaces

    @property
    def parent_signature(self) -> "Optional[ClazzSignature]":
        return self.parent_type.signature if self.parent_type is not None else None

    def validate_overriding_rules(self) -> None:
        if self.is_included:
            return

        def throw(signature: MethodSignature) -> NoReturn:
            raise ValueError("Method '%s' violates overriding rules on class '%s'" % (signature.name, self.name))

        # The `override` modifier can mean either that a method is an interface
        # implementation method or an override from a superclass. In the latter
        # case, we'll first see that it's marked as override and add it to this
        # set. Later, if it didn't end up being an interface implementation, we
        # throw that signature because it was marked as override improperly.
        must_be_interface_implementation = set()

        for signature in self.method_signatures:
            if self.get_field_by_name(signature.name) is not None:
                raise ValueError("Class property and method with the same name: '%s' (from class '%s'" % (signature.name, self.name))
            if signature.is_abstract and not self.is_abstract:
                raise ValueError("Class '%s' is not abstract but has abstract method '%s'" % (self.name, signature.name))
            if not signature.is_override:
                if self.parent_signature is not None and self.parent_signature.get_method_signature_by_name(signature.name) is not None:
                    throw(signature)
            else:
                if self.parent_signature is None:
                    throw(signature)
                # `parent_sig` is a method signature (as opposed to a class signature)
                parent_sig = util.nonnull(self.parent_signature).get_method_signature_by_name(signature.name)
                if parent_sig is None:
                    must_be_interface_implementation.add(signature)
                else:
                    assert parent_sig is not None
                    if not parent_sig.returntype.is_assignable_to(signature.returntype):
                        throw(signature)
                    if len(signature.args) != len(parent_sig.args):
                        throw(signature)
                    for i in range(len(signature.args) - 1):
                        # Why - 1 and + 1? Because `this` is an argument
                        if not parent_sig.args[i + 1].is_assignable_to(signature.args[i + 1]):
                            throw(signature)

        if self.parent_signature is not None and not self.is_abstract:
            for parent_sig in self.parent_signature.method_signatures:
                if not parent_sig.is_abstract:
                    continue
                if self.get_method_signature_by_name(parent_sig.name, recursive=False) is None:
                    raise ValueError("Abstract method '%s' is not overridden by subclass '%s'" % (parent_sig.name, self.name))

        if self.parent_signature is not None:
            for field in self.fields:
                if self.parent_signature.get_field_by_name(field.name) is not None:
                    raise ValueError("Duplicate field in both child and parent class named '%s'" % field.name)

        for interface in self.implemented_interfaces:
            for signature in interface.interface.method_signatures:
                implementation_sig = self.get_method_signature_by_name(signature.name)
                if implementation_sig in must_be_interface_implementation:
                    must_be_interface_implementation.remove(implementation_sig)
                if implementation_sig is None:
                    raise ValueError("Method '%s' of interface '%s' is not implemented by class '%s'" % (signature.name, interface.name, self.name))
                if not implementation_sig.is_override:
                    raise ValueError("Method '%s' of class '%s' is an implementation from interface '%s' and must be marked as override" % (signature.name, self.name, interface.name))
                if not implementation_sig.returntype.is_assignable_to(signature.returntype) or (implementation_sig.returntype.is_void() and not signature.returntype.is_void()):
                    raise ValueError("Method '%s' of class '%s' has return type %s that isn't assignable to return type %s of the corresponding method from interface %s"
                        % (signature.name, self.name, implementation_sig.returntype, signature.returntype, interface.name))
                if len(implementation_sig.args) != len(signature.args):
                    raise ValueError("Method '%s' of class '%s' has %s arguments, but expected %s to match the corresponding method from interface %s"
                        % (signature.name, self.name, len(implementation_sig.args), len(signature.args), interface.name))
                for i in range(1, len(signature.args)):
                    if not signature.args[i].is_assignable_to(implementation_sig.args[i]):
                        raise ValueError("Method '%s' of class '%s': argument #%s (including this) has type %s which is not assignable from %s from corresponding method of interface %s"
                        % (signature.name, self.name, i, implementation_sig.args[i], signature.args[i], interface.name))
        # This is a for loop for conceptual clarity, even though it'll never
        # hit a second iteration
        for signature in must_be_interface_implementation:
            throw(signature)

    def get_method_signature_by_name(self, name: str, recursive: bool = True) -> Optional[MethodSignature]:
        for signature in self.method_signatures:
            if signature.name == name:
                return signature

        if self.parent_signature is not None and recursive:
            return self.parent_signature.get_method_signature_by_name(name)

        return None

    def get_field_by_name(self, name: str) -> Optional[ClazzField]:
        for field in self.fields:
            if field.name == name:
                return field

        if self.parent_signature is not None:
            return self.parent_signature.get_field_by_name(name)

        return None

    def __str__(self) -> str:
        ret = ""
        if self.is_included:
            ret += ("included ")
        if self.is_abstract:
            ret += ("abstract ")
        ret += "class "
        ret += self.name
        if self.parent_signature is None:
            ret += " extends null"
        else:
            ret += " extends %s" % self.parent_signature.name
        if len(self.implemented_interfaces) > 0:
            ret += " implements "
            interface_names: List[str] = []
            for interface in self.implemented_interfaces:
                interface_names.append(interface.name)
            ret += ", ".join(interface_names)
        return ret

    def __repr__(self) -> str:
        return "ClazzSignature(name='%s')" % self.name

    def looks_like_class_signature(self) -> None:
        pass

class ClazzEmitter(SegmentEmitter):
    def __init__(self, top: parser.Node, program: "Program") -> None:
        self.top = top
        self.program = program
        self.signature: ClazzSignature
        self.is_signature_no_fields = False
        self.fields: List[ClazzField]
        self.method_signatures: List[MethodSignature]
        self.ctor_signatures: List[MethodSignature]

    @property
    def unqualified_name(self) -> str:
        return self.top[0].data_strict

    @property
    def name(self) -> str:
        return ("%s." % self.program.namespace if self.program.namespace is not None else "") + self.unqualified_name

    def _emit_field(self, node: parser.Node) -> ClazzField:
        return ClazzField(node[0].data_strict, self.program.types.resolve_strict(node[1], None))

    def emit_signature_no_fields(self) -> ClazzSignature:
        self.is_signature_no_fields = True
        self.signature = ClazzSignature(self.name, [], [], [], None, self.top["modifiers"].has_child("abstract"), [])
        return self.signature

    def emit_signature(self) -> ClazzSignature:
        if self.signature is not None and not self.is_signature_no_fields:
            return self.signature
        self.fields = []
        self.method_signatures = []
        self.ctor_signatures = []
        for field in self.top["classbody"]:
            if field.i("classprop"):
                self.fields.append(self._emit_field(field))
            elif field.i("fn"):
                signature = MethodSignature.from_node(
                    field,
                    self.program,
                    this_type=self.program.types.get_clazz_type_by_signature(self.signature),
                    # At this point in execution, `this.signature` is the blank
                    # signature from emit_signature_no_fields(...).
                    containing_class=self.signature
                )
                self.method_signatures.append(signature)
                field.xattrs["signature"] = signature
            elif field.i("ctor"):
                signature = MethodSignature.from_node(
                    field,
                    self.program,
                    this_type=self.program.types.get_clazz_type_by_signature(self.signature),
                    # Same comment as above
                    containing_class=self.signature
                )
                self.method_signatures.append(signature)
                self.ctor_signatures.append(signature)
                field.xattrs["signature"] = signature
        self.signature = ClazzSignature(
            self.name,
            self.fields,
            self.method_signatures,
            self.ctor_signatures,
            None,
            self.top["modifiers"].has_child("abstract"),
            []
        )
        # We need to replace the containing_class with the new signature,
        # just so we don't have different signatures floating around.
        for signature in self.method_signatures:
            signature.containing_class = self.signature
        self.is_signature_no_fields = False
        return self.signature

    def add_superclass_to_signature(self) -> None:
        if len(self.top["extends"]) <= 0:
            # Implicitly extending stdlib.Object
            object_type = self.program.types.get_object_type()
            if object_type is None:
                self.top["extends"].compile_error("Cannot implicitly extend stdlib.Object without an implementation of stdlib.Object")
            self.signature.parent_type = object_type
        elif self.top["extends"][0].i("ident") and self.top["extends"][0].data_strict == "void":
            self.signature.parent_type = None
        else:
            # Since we have a reference to this class's signature, we can
            # just update it in-place.
            parent_type = self.program.types.resolve(self.top["extends"][0], None)
            if parent_type is None or not isinstance(parent_type, typesys.ClazzType):
                self.top["extends"].compile_error("Tried to extend a nonexisting class")
            self.signature.parent_type = parent_type
        # Otherwise, the superclass remains as `None`.

    def add_interfaces_to_signature(self) -> None:
        for interface in self.top["implements"]:
            typ = self.program.types.resolve_strict(interface, None)
            if not isinstance(typ, typesys.InterfaceType):
                interface.compile_error("Can't implement a type that isn't an interface")
            self.signature.implemented_interfaces.append(typ)

    def emit(self) -> "ClazzSegment":
        self.emit_signature()
        return ClazzSegment(self)

class StaticVariableSegment(Segment):
    def __init__(self, static_variables: StaticVariableSet) -> None:
        Segment.__init__(self, "staticvars", 0x03)
        self.static_variables = static_variables

    def __str__(self) -> str:
        return "StaticVariableSegment()"

    def print_(self) -> None:
        Segment.print_(self)
        for variable in self.static_variables.variables.values():
            if variable.included:
                continue
            print("    %s" % variable)

    def evaluate(self, program: "Program") -> None:
        Segment.evaluate(self, program)

class ClazzSegment(Segment):
    def __init__(self, emitter: ClazzEmitter) -> None:
        Segment.__init__(self, "class", 0x02)
        self.emitter = emitter
        self.signature = emitter.signature
        self.name = self.signature.name
        self.fields = self.signature.fields
        self.method_signatures = self.signature.method_signatures

    @property
    def parent_signature(self) -> Optional[ClazzSignature]:
        return self.signature.parent_signature

    def __str__(self) -> str:
        return "ClazzSegment(type='%s', typecode=%s,nfields=%s, nmethods=%s, signature=%s)" \
            % (self.humantype,
            self.realtype,
            len(self.fields),
            len(self.method_signatures),
            self.signature)

    def print_(self) -> None:
        Segment.print_(self)
        for method in self.method_signatures:
            print("    %s" % method)
        for field in self.fields:
            print("    %s: %s" % (field.name, field.type.name))

    def evaluate(self, program: "Program") -> None:
        Segment.evaluate(self, program)
        for field in self.emitter.top["classbody"]:
            if field.i("fn") or field.i("ctor"):
                signature = field.xattrs["signature"]
                method = program.emit_method(field, signature=signature)
                program.segments.append(method)
                program.methods.append(method)

class Program:
    def __init__(self, emitter: "Emitter") -> None:
        self.methods: List[MethodSegment] = []
        self.emitter = emitter
        self.top = emitter.top
        self.segments: List[Segment] = []
        self.types: typesys.TypeSystem = typesys.TypeSystem(self)
        self.clazzes: List[ClazzSegment] = []
        self.clazz_signatures: List[ClazzSignature] = []
        self.using_directives: List[str] = []
        self.namespace: Optional[str] = None
        self.clazz_emitters: List[ClazzEmitter]
        self.method_signatures: List[MethodSignature] = []
        self.static_variables: StaticVariableSet = StaticVariableSet(self)
        self.interpreter: interpreter.Interpreter = interpreter.Interpreter(self)
        self.interfaces: List[Interface] = []

    @property
    def search_paths(self) -> List[str]:
        search_paths = [""]
        if self.namespace is not None:
            search_paths.append("%s." % self.namespace)
        for using_directive in self.using_directives:
            search_paths.append("%s." % using_directive)
        return search_paths

    def get_entrypoint(self) -> Optional[MethodSignature]:
        for method in self.methods:
            if method.signature.is_entrypoint:
                return method.signature
        return None

    def get_method_signature(self, name: str) -> MethodSignature:
        ret = self.get_method_signature_optional(name)
        if ret is None:
            raise ValueError("Could not find method signature '%s'" % name)
        return ret

    def get_method_signature_optional(self, name: str) -> Optional[MethodSignature]:
        search_paths = self.search_paths
        for signature in self.method_signatures:
            if signature.name in ["%s%s" % (path, name) if path != "" else name for path in search_paths]:
                return signature
        return None
        # return get_method_signature_optional(name, self.top, is_real_top=True)

    def has_entrypoint(self) -> bool:
        return self.get_entrypoint() is not None

    def emit_method(self, method: parser.Node, signature: MethodSignature=None) -> MethodSegment:
        # util.get_flattened(method[0]) is method name
        # TODO: Remove signature parameter and make caller call get_method_signature
        return MethodSegment(method, signature if signature is not None else self.get_method_signature(util.nonnull(util.get_flattened(method[0]))))

    def validate_overriding_rules(self) -> None:
        for signature in self.clazz_signatures:
            signature.validate_overriding_rules()

    def evaluate(self) -> None:
        for emitter in self.clazz_emitters:
            segment = emitter.emit()
            self.clazzes.append(segment)
            self.segments.append(segment)
        for toplevel in self.top:
            if toplevel.i("fn"):
                method = self.emit_method(toplevel)
                self.methods.append(method)
                self.segments.append(method)
        self.validate_overriding_rules()
        for clazz in self.clazzes:
            clazz.evaluate(self)
        for interface in self.interfaces:
            if interface.included:
                continue
            interface_segment = InterfaceSegment(interface)
            self.segments.append(interface_segment)
            interface_segment.evaluate(self)
        for method in self.methods:
            method.evaluate(self)
        self.segments.append(StaticVariableSegment(self.static_variables))

    def get_header_representation(self) -> header.HeaderRepresentation:
        return header.HeaderRepresentation.from_program(self)

    def add_metadata(self) -> None:
        if self.has_entrypoint():
            self.segments.append(MetadataSegment(entrypoint=self.get_entrypoint(), headers=self.get_header_representation()))
        else:
            self.segments.append(MetadataSegment(headers=self.get_header_representation()))

    def prescan_static_variables(self) -> None:
        for toplevel in self.top:
            if toplevel.i("static"):
                unqualified_variable_name = util.nonnull(util.get_flattened(toplevel[0]))
                value: interpreter.AbstractInterpreterValue = self.interpreter.null
                if len(toplevel) == 3:
                    value = self.interpreter.eval_expr(toplevel[2])
                self.static_variables.add_variable(
                    StaticVariable(
                        self.static_variables,
                        "%s.%s" % (self.namespace, unqualified_variable_name) if self.namespace is not None else unqualified_variable_name,
                        self.types.resolve_strict(toplevel[1], None),
                        value
                    )
                )

    def prescan(self) -> None:
        for toplevel in self.top:
            if toplevel.i("namespace"):
                self.namespace = flatten_ident_nodes(toplevel)
            elif toplevel.i("using"):
                self.using_directives.append(flatten_ident_nodes(toplevel))
        clazz_emitters: List[ClazzEmitter] = []
        for toplevel in self.top:
            if toplevel.i("class"):
                clazz_emitters.append(ClazzEmitter(toplevel, self))
            elif toplevel.i("interface"):
                self.interfaces.append(Interface.create_from_node(toplevel, self))
        # This is the indices in self.clazz_signatures of the class signature
        # associated with its respective emitter
        indices = dict()
        for emitter in clazz_emitters:
            signature = emitter.emit_signature_no_fields()
            indices[id(emitter)] = len(self.clazz_signatures)
            self.clazz_signatures.append(signature)
            self.types.accept_skeleton_clazz(signature)
        for emitter in clazz_emitters:
            signature = emitter.emit_signature()
            self.types.update_signature(signature)
            self.clazz_signatures[indices[id(emitter)]] = signature
        for interface in self.interfaces:
            interface.add_to_typesys(self.types)
        for emitter in clazz_emitters:
            emitter.add_superclass_to_signature()
            emitter.add_interfaces_to_signature()
        for interface in self.interfaces:
            if interface.included:
                continue
            interface.add_signatures_from_node(self)
        self.clazz_emitters = clazz_emitters
        self.method_signatures += MethodSignature.scan(self.emitter.top, self)
        self.prescan_static_variables()

    def add_include(self, headers: header.HeaderRepresentation) -> None:
        for clazz in headers.clazzes:
            self.types.accept_skeleton_clazz(ClazzSignature(clazz.name, [], [], [], None, clazz.is_abstract, [], is_included=True))
        for interface_header in headers.interfaces:
            interface = Interface(interface_header.name, None, included=True)
            self.interfaces.append(interface)
            self.types.accept_interface(interface)
        for method in headers.methods:
            self.method_signatures.append(MethodSignature(
                method.name,
                [self.types.resolve_strict(parser.parse_type(arg.type), None) for arg in method.args],
                [arg.name for arg in method.args],
                self.types.resolve_strict(parser.parse_type(method.returntype), None),
                containing_class=util.nonnull(self.types.get_clazz_type_by_name(method.containing_clazz)).signature if method.containing_clazz is not None else None,
                containing_interface=util.nonnull(self.types.get_interface_type_by_name(method.containing_interface)).interface if method.containing_interface is not None else None,
                is_abstract=method.is_abstract,
                is_override=method.is_override,
                is_ctor=method.is_ctor,
                generic_type_context=None,
                is_entrypoint=False
            ))
        for interface_header in headers.interfaces:
            interface = util.nonnull(self.types.get_interface_type_by_name(interface_header.name)).interface
            method_names = set([member_method.name for member_method in interface_header.methods])
            for method_signature in self.method_signatures:
                if method_signature.containing_interface == interface and method_signature.name in method_names:
                    interface.add_signature(method_signature)
        for clazz in headers.clazzes:
            methods = []
            for clazz_method in clazz.methods:
                methods.append(self.get_method_signature(clazz_method.name))
            ctors = []
            for ctor in clazz.ctors:
                ctors.append(self.get_method_signature(ctor.name))
            fields = []
            for field in clazz.fields:
                fields.append(ClazzField(field.name, self.types.resolve_strict(parser.parse_type(field.type), None)))
            parent_type = self.types.resolve_strict(parser.parse_type(clazz.parent), None) if clazz.parent is not None else None
            assert isinstance(parent_type, typesys.ClazzType) or parent_type is None
            # util.nonnull is correct here because of the previous two lines
            clazz_signature = ClazzSignature(
                clazz.name,
                fields,
                methods,
                ctors,
                util.nonnull(parent_type) if clazz.parent is not None else None,
                clazz.is_abstract,
                [cast(typesys.InterfaceType, self.types.resolve_strict(parser.parse_type(name), None)) for name in clazz.interfaces],
                is_included=True
            )
            self.clazz_signatures.append(clazz_signature)
            self.types.update_signature(clazz_signature)
            for method_signature in methods + ctors:
                method_signature.containing_class = clazz_signature
        for staticvar in headers.static_variables:
            self.static_variables.add_variable(StaticVariable(self.static_variables, staticvar.name, self.types.resolve_strict(parser.parse_type(staticvar.type), None), self.interpreter.null, included=True))

class Emitter:
    def __init__(self, top: parser.Node) -> None:
        self.top = top

    def emit_program(self) -> Program:
        return Program(self)

    def emit_bytes(self, program: Program) -> bytes:
        import bytecode
        return bytecode.emit(program.segments)
