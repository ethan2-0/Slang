# Import as another name since "opcodes" is used frequently as a variable name
import opcodes as opcodes_module
from opcodes import opcodes as ops
import header
import typesys
import parser
import util
import claims as clms
from typing import ClassVar, List, Optional, Union, TypeVar, Dict, cast, Tuple, Generic, NoReturn

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
    def __init__(self, variable_set: "StaticVariableSet", name: str, type: typesys.AbstractType, initializer: Union[None, int, List[int]]) -> None:
        self.variable_set = variable_set
        self.name = name
        self.type = type
        self.initializer = initializer

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
                self, name: str, args: "List[typesys.AbstractType]", argnames: "List[str]",
                returntype: "typesys.AbstractType", is_ctor: bool=False, containing_class: "ClazzSignature"=None,
                is_override: bool=False, is_entrypoint: bool=False) -> None:
        self.name = name
        self.args = args
        self.argnames = argnames
        self.returntype = returntype
        self.is_ctor = is_ctor
        self.id = MethodSignature.sequential_id
        self.containing_class = containing_class
        self.is_override = is_override
        self.is_entrypoint = is_entrypoint
        MethodSignature.sequential_id += 1

    @property
    def nargs(self) -> int:
        return len(self.args)

    def looks_like_method_handle(self) -> None:
        pass

    def __repr__(self) -> str:
        return "MethodSignature(name='%s', nargs=%s, id=%s)" % (self.name, len(self.args), self.id)

    def __str__(self) -> str:
        modifiers = ("override " if self.is_override else "") + ("entrypoint " if self.is_entrypoint else "")
        args_str = ", ".join(["%s: %s" % (name, typ.name) for typ, name in zip(self.args, self.argnames)])
        prefix = "%s." % self.containing_class.name if self.containing_class is not None else ""
        if self.is_ctor:
            return "%sctor(%s)" % (prefix, args_str)
        return "%sfn %s%s(%s): %s" % (modifiers, prefix, self.name, args_str, self.returntype.name)

    @staticmethod
    def from_node(method: parser.Node, program: "Program",
            this_type: "Optional[typesys.ClazzType]"=None, containing_class: "Optional[ClazzSignature]"=None) -> "MethodSignature":
        if containing_class is None and method["modifiers"].has_child("override"):
            method.compile_error("Can't both be an override method and not have a containing class")

        if method.i("fn"):
            signature: "List[typesys.AbstractType]" = cast(List[typesys.AbstractType], [] if this_type is None else [this_type]) + [program.types.resolve_strict(arg[1]) for arg in method[1]]
            argnames = ([] if this_type is None else ["this"]) + [arg[0].data_strict for arg in method[1]]
            is_override = method["modifiers"].has_child("override")
            is_entrypoint = method["modifiers"].has_child("entrypoint")
            if is_entrypoint and containing_class is not None:
                method.compile_error("Can't have an entrypoint inside a class")
            if is_entrypoint and len(argnames) > 0:
                method.compile_error("Can't have an entrypoint with arguments")
            name = util.nonnull(util.get_flattened(method[0]))
            if containing_class is None and program.namespace is not None:
                name = "%s.%s" % (program.namespace, name)
            return MethodSignature(name, signature, argnames, program.types.resolve_strict(method[2]), is_ctor=False, containing_class=containing_class, is_override=is_override, is_entrypoint=is_entrypoint)
        elif method.i("ctor"):
            signature = cast(List[typesys.AbstractType], [this_type]) + [program.types.resolve_strict(arg[1]) for arg in method[0]]
            argnames = ["this"] + [arg[0].data_strict for arg in method[0]]
            assert containing_class is not None
            assert type(containing_class.name) is str
            return MethodSignature("@@ctor.%s" % containing_class.name, signature, argnames, program.types.void_type, is_ctor=True, containing_class=containing_class, is_override=False)
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
            typ = self.types.decide_type(node[0], self.scope)
            if not node[1].i("ident"):
                raise ValueError("This is a compiler bug")
            if not typ.is_clazz():
                node.compile_error("Cannot perform access on something that isn't an instance of a class")
                # Unreachable
                return None # type: ignore
            assert isinstance(typ, typesys.ClazzType)
            if typ.type_of_property(node[1].data_strict) is not None:
                node.compile_error("Attempt to call a property")
                # Unreachable
                return None # type: ignore
            elif typ.method_signature(node[1].data_strict) is not None:
                reg = self.scope.allocate(typ)
                opcodes += self.emit_expr(node[0], reg)
                return typ.method_signature(node[1].data_strict), reg
            else:
                node.compile_error("Attempt to perform property access that doesn't make sense")
                # Unreachable
                return None # type: ignore
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
        elif node.i("as"):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs_reg)
            rhs_type = self.types.resolve_strict(node[1])
            if not lhs_reg.type.is_assignable_to(rhs_type) and not rhs_type.is_assignable_to(lhs_reg.type):
                raise typesys.TypingError(node, "Invalid cast: LHS is incompatible with RHS")
            if not rhs_type.is_clazz():
                node.compile_error("Cannot cast to a type that isn't a class.")
            if not lhs_reg.type.is_clazz():
                node.compile_error("Cannot cast something that isn't a class.");
            opcodes.append(ops["cast"].ins(lhs_reg, register))
        elif node.i("instanceof"):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs_reg)
            # result_reg = self.scope.allocate(self.types.bool_type)
            # TODO: Potentially emit warning if RHS is incompatible with LHS type
            rhs_type = self.types.resolve_strict(node[1])
            if lhs_reg.type.is_assignable_to(rhs_type):
                node.warn("Tautology: %s always instanceof %s" % (lhs_reg.type, rhs_type))
            if (not lhs_reg.type.is_assignable_to(rhs_type)) and (not rhs_type.is_assignable_to(lhs_reg.type)):
                node.warn("Contradiction: there cannot exist any %s instanceof %s" % (lhs_reg.type, rhs_type))
            opcodes.append(ops["instanceof"].ins(lhs_reg, register, rhs_type))
        elif node.of("+", "*", "^", "&", "|", "%", "/", "==", ">=", "<=", ">", "<", "!=", "and", "or"):
            lhs = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs)
            rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope))
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
                if not self.types.decide_type(node[0], self.scope).is_numerical():
                    raise typesys.TypingError(node, "Operator '-' needs numerical argument")
                opcodes += self.emit_expr(node[0], register)
                opcodes.append(ops["twocomp"].ins(register, node=node))
            else:
                lhs = self.scope.allocate(self.types.decide_type(node[0], self.scope))
                opcodes += self.emit_expr(node[0], lhs)
                rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope))
                opcodes += self.emit_expr(node[1], rhs)
                opcodes.append(ops["twocomp"].ins(rhs, node=node))
                opcodes.append(ops["add"].ins(lhs, rhs, register, node=node))
        elif node.i("call"):
            emitted_opcodes: List[opcodes_module.OpcodeInstance] = []
            signature, optional_lhs_reg = self.emit_to_method_signature(node[0], emitted_opcodes)
            opcodes += emitted_opcodes
            is_clazz_call = optional_lhs_reg is not None

            param_registers = []

            if len(node.children) - 1 != len(signature.args) + (-1 if is_clazz_call else 0):
                node.compile_error("Expected %s arguments, got %s" % (len(signature.args), len(node.children) - 1))
            for param, index in zip(node.children[1:], range(1 if is_clazz_call else 0, len(node.children) + (1 if is_clazz_call else 0))):
                inferred_type = self.types.decide_type(param, self.scope)
                declared_type = signature.args[index]
                if not inferred_type.is_assignable_to(declared_type):
                    raise typesys.TypingError(node, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                reg = self.scope.allocate(inferred_type)
                opcodes += self.emit_expr(param, reg)
                param_registers.append((reg, index + (-1 if is_clazz_call else 0)))
                # opcodes.append(ops["param"].ins(reg, index + (-1 if is_clazz_call else 0)))
            for reg, index in param_registers:
                opcodes.append(ops["param"].ins(reg, index, node=node))
            if is_clazz_call:
                opcodes.append(ops["classcall"].ins(util.nonnull(optional_lhs_reg), signature, register, node=node))
            else:
                opcodes.append(ops["call"].ins(signature, register, node=node))
        elif node.i("new"):
            typ = self.types.resolve_strict(node[0])
            if not isinstance(typ, typesys.ClazzType):
                node[0].compile_error("Attempt to instantiate a type that isn't a class")
            clazz_signature = typ.signature
            result_register = self.scope.allocate(self.types.decide_type(node, self.scope))
            opcodes.append(ops["new"].ins(clazz_signature, result_register, node=node))
            # TODO: Support multiple constructors and method overloading
            # TODO: A lot of this code is copy-pasted
            if len(clazz_signature.ctor_signatures) < 1:
                node.compile_error("Type has no constructors")
            ctor_signature = clazz_signature.ctor_signatures[0]
            if len(node.children) - 1 + 1 != len(ctor_signature.args):
                node.compile_error("Expected %s arguments, got %s" % (len(ctor_signature.args), len(node.children) - 1 + 1))
            for param, index in zip(node.children[1:], range(len(node.children) - 1)):
                inferred_type = self.types.decide_type(param, self.scope)
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
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs_reg)
            if not lhs_reg.type.is_array():
                raise typesys.TypingError(node[0], "Cannot do array access on something that isn't an array")
            if not cast(typesys.ArrayType, lhs_reg.type).parent_type.is_assignable_to(register.type):
                # This should have already been caught by typechecking further up
                raise ValueError("This is a compiler bug")
            index_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            if not index_reg.type.is_numerical():
                raise typesys.TypingError(node, "Index to array access must be numerical")
            opcodes += self.emit_expr(node[1], index_reg)
            opcodes.append(ops["arraccess"].ins(lhs_reg, index_reg, register, node=node))
        elif node.i("arrinst"):
            quantity_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            if not quantity_reg.type.is_numerical():
                raise typesys.TypingError(node, "Array lengths must be numerical")
            opcodes += self.emit_expr(node[1], quantity_reg)
            opcodes.append(ops["arralloc"].ins(register, quantity_reg, node=node))
        elif node.i("#"):
            arr_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            if not arr_reg.type.is_array():
                raise typesys.TypingError(node, "Can't decide length of something that isn't an array")
            opcodes += self.emit_expr(node[0], arr_reg)
            opcodes.append(ops["arrlen"].ins(arr_reg, register, node=node))
        elif node.i("."):
            if node[0].i("ident"):
                lhs_reg = self.scope.resolve(node[0].data_strict, node[0])
            else:
                # node, register
                lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
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
            opcodes.append(ops["staticvarset"].ins(value, static_variable.name))
        elif expr.i("ident"):
            lhs_reg = self.scope.resolve(expr.data_strict, expr)
            if not value.type.is_assignable_to(lhs_reg.type):
                raise typesys.TypingError(expr, "Attempt to make incompatible assignment: %s, %s" % (lhs_reg.type, value.type))
            opcodes.append(ops["mov"].ins(value, lhs_reg))
        elif expr.i("."):
            expr_lhs = expr[0]
            expr_lhs_type = self.types.decide_type(expr_lhs, self.scope)
            if not isinstance(expr_lhs_type, typesys.ClazzType):
                raise typesys.TypingError(expr, "Attempt to access property of something that isn't a class: %s" % expr_lhs_type)
            expr_lhs_reg = self.scope.allocate(expr_lhs_type)
            opcodes += self.emit_expr(expr_lhs, expr_lhs_reg)
            if not expr[1].i("ident"):
                raise ValueError("This is a compiler bug.")
            if not expr_lhs_type.type_of_property(expr[1].data_strict).is_assignable_from(value.type):
                raise typesys.TypingError(expr, "Incompatible types in class property assignment: %s, %s" % (expr_lhs_type, value.type))
            opcodes.append(ops["assign"].ins(expr_lhs_reg, expr[1].data_strict, value))
        elif expr.i("access"):
            expr_lhs = expr[0]
            expr_lhs_type = self.types.decide_type(expr_lhs, self.scope)
            if not isinstance(expr_lhs_type, typesys.ArrayType):
                raise typesys.TypingError(expr, "Attempt to index something that isn't an array: %s" % expr_lhs_type)
            if not expr_lhs_type.parent_type.is_assignable_from(value.type):
                raise typesys.TypingError(expr, "Attempt to assign incompatible type as array element: %s, %s" % (expr_lhs_type.parent_type, value.type))
            expr_lhs_reg = self.scope.allocate(expr_lhs_type)
            opcodes += self.emit_expr(expr_lhs, expr_lhs_reg)
            expr_index = expr[1]
            index_type = self.types.decide_type(expr_index, self.scope)
            if not isinstance(index_type, typesys.IntType):
                raise typesys.TypingError(expr_index, "Attempt to index an array by something that isn't an integer: %s" % index_type)
            index_reg = self.scope.allocate(index_type)
            opcodes += self.emit_expr(expr_index, index_reg)
            opcodes.append(ops["arrassign"].ins(expr_lhs_reg, index_reg, value))
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
            result = self.scope.allocate(self.types.decide_type(node if node.i("call") else node[0], self.scope))
            opcodes += self.emit_expr(node if node.i("call") else node[0], result)
        elif node.i("let"):
            # Notice that a variable may have only exactly one let statement
            # in a given scope. So we can typecheck just with that register's
            # type, we don't have to associate with the scope directly.
            if len(node) > 2:
                type = self.types.decide_type(node[2], self.scope)
                if not node[1].i("infer_type"):
                    inferred_type = type
                    # This is the declared type
                    type = self.types.resolve_strict(node[1])
                    if not inferred_type.is_assignable_to(type):
                        raise typesys.TypingError(node, "'%s' is not assignable to '%s'" % (inferred_type, type))
            else:
                if node[1].i("infer_type"):
                    node.compile_error("Variables declared without assignment must be given explicit type")
                type = self.types.resolve_strict(node[1])

            reg = self.scope.allocate(type)
            self.scope.let(node[0].data_strict, reg, node)

            if len(node) > 2:
                opcodes += annotate(self.emit_expr(node[2], reg), "let %s" % node[0].data)
        elif node.i("return"):
            if len(node.children) > 0:
                stated_return_type = self.types.decide_type(node[0], self.scope)
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
            reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], reg)
        elif node.i("assignment"):
            lhs_type = self.types.decide_type(node[0], self.scope)
            rhs_type = self.types.decide_type(node[1], self.scope)
            if not lhs_type.is_assignable_from(rhs_type):
                raise typesys.TypingError(node, "Need RHS to be assignable to LHS: %s, %s" % (lhs_type, rhs_type))
            value_reg = self.scope.allocate(rhs_type)
            opcodes += self.emit_expr(node[1], value_reg)
            opcodes += self.assign_to_expr(node[0], value_reg)
        elif node.of("+=", "*=", "-=", "/="):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs_reg)
            rhs_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
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
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope))
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

            condition_register = self.scope.allocate(self.types.decide_type(node[1], self.scope))
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
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope))
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
            # var = self.scope.resolve(node[0].data_strict, node)
            # if not var.type.is_numerical():
            #     raise typesys.TypingError(node, "Subject of increment/decrement must be numerical")
            # reg = self.scope.allocate(self.types.int_type)
            # opcodes.append(ops["load"].ins(reg, 1, node=node))
            # if node.i("--"):
            #     opcodes.append(ops["twocomp"].ins(reg, node=node))
            # opcodes.append(ops["add"].ins(var, reg, var, node=node))
            lhs_type = self.types.decide_type(node[0], self.scope)
            if not lhs_type.is_numerical():
                raise typesys.TypingError(node, "Attempt to increment something that isn't numerical: %s" % lhs_type)
            lhs_reg = self.scope.allocate(lhs_type)
            opcodes += annotate(self.emit_expr(node[0], lhs_reg), "++ expression")
            reg = self.scope.allocate(self.types.int_type)
            opcodes.append(ops["load"].ins(reg, 1, node=node))
            if node.i("--"):
                opcodes.append(ops["twocomp"].ins(reg, node=node))
            opcodes.append(ops["add"].ins(reg, lhs_reg, reg))
            opcodes += self.assign_to_expr(node[0], reg)
        else:
            node.compile_error("Unexpected (this is a compiler bug)")
        return opcodes, claims

    def convert_strings(self) -> None:
        def replace(parent: parser.Node, index: int) -> None:
            s = parent[index].data_strict
            array_node = parser.Node("[")
            nod = None
            if len(s) > 0:
                for ch in s:
                    array_node.add(parser.Node("number", data=str(ord(ch))))
                nod = parser.Node("new", parser.Node("ident", data="stdlib.String"), array_node, parser.Node("number", data="0"), parser.Node("number", data=str(len(s))))
            else:
                nod = parser.Node(
                    "new",
                    parser.Node("ident", data="stdlib.String"),
                    parser.Node(
                        "arrinst",
                        parser.Node("ident", data="int"),
                        parser.Node("number", data="0")
                    ),
                    parser.Node("number", data="0"),
                    parser.Node("number", data="0")
                )
            parent.children[index] = nod

        def convert_strings_inner(nod: parser.Node) -> None:
            for i in range(len(nod)):
                if nod[i].i("string"):
                    replace(nod, i)
                else:
                    convert_strings_inner(nod[i])
        # TODO: Technically, this doesn't work properly with functions with single-statement bodies.
        if self.top.has_child("statements"):
            convert_strings_inner(self.top["statements"])

    def emit(self) -> List[opcodes_module.OpcodeInstance]:
        self.return_type = self.signature.returntype
        # This caused a bug at some point
        assert self.top.i("fn") or self.top.i("ctor")
        self.convert_strings()
        for argtype, argname in zip(self.signature.args, self.signature.argnames):
            self.scope.let(argname, self.scope.allocate(argtype), self.top[1] if self.top.i("fn") else self.top[0])
        opcodes, claims = self.emit_statement(self.top[3] if self.top.i("fn") else self.top[1])
        if not claims.contains_equivalent(clms.ClaimReturns()):
            if self.signature.returntype.is_void():
                zeroreg = self.scope.allocate(self.types.int_type)
                opcodes.append(ops["zero"].ins(zeroreg))
                opcodes.append(ops["return"].ins(zeroreg))
            else:
                self.top[3].compile_error("Non-void method might not return")
        return opcodes

SegmentEmitterTypeVariable = TypeVar("SegmentEmitterTypeVariable", bound=SegmentEmitter)

class Segment(Generic[SegmentEmitterTypeVariable]):
    def __init__(self, humantype: str, realtype: int) -> None:
        self.humantype = humantype
        self.realtype = realtype

    def __str__(self) -> str:
        return "Segment(type='%s', code=%s)" % (self.humantype, self.realtype)

    def print_(self, emitter: SegmentEmitterTypeVariable) -> None:
        print(str(self))

    def evaluate(self, program: "Program") -> None:
        pass

class MethodSegment(Segment[MethodEmitter]):
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

    def print_(self, emitter: MethodEmitter) -> None:
        Segment.print_(self, emitter)
        print("Registers:", ", ".join([repr(register) for register in self.emitter.scope.registers]))
        for opcode, index in zip(self.opcodes, range(len(self.opcodes))):
            print("%6d: %s" % (index, str(opcode)))

    def evaluate(self, program: "Program") -> None:
        self.opcodes = self.emit_opcodes(program)

# We need to provide an argument to Segment but there's no MetadataEmitter
class MetadataSegment(Segment[SegmentEmitter]):
    def __init__(self, entrypoint: Optional[MethodSignature]=None, headers: header.HeaderRepresentation=header.HeaderRepresentation.HIDDEN) -> None:
        Segment.__init__(self, "metadata", 0x01)
        self.entrypoint = entrypoint
        self.headers = headers

    def __str__(self) -> str:
        return "MetadataSegment(type='%s', typecode=%s, entrypoint=%s)" % (self.humantype, self.realtype, self.entrypoint)

    def print_(self, emitter: SegmentEmitter) -> None:
        Segment.print_(self, emitter)
        print("    entrypoint=%s" % self.entrypoint)
        print("    headers omitted for brevity")

    def has_entrypoint(self) -> bool:
        return self.entrypoint is not None

    def evaluate(self, program: "Program") -> None:
        Segment.evaluate(self, program)

class ClazzField:
    def __init__(self, name: str, type: typesys.AbstractType) -> None:
        self.name = name
        self.type = type

class ClazzSignature:
    def __init__(self, name: str, fields: List[ClazzField], method_signatures: List[MethodSignature],
                ctor_signatures: List[MethodSignature], parent_signature: "Optional[ClazzSignature]", is_included: bool=False) -> None:
        self.name = name
        self.fields = fields
        self.method_signatures = method_signatures
        self.ctor_signatures = ctor_signatures
        self.is_included = is_included
        if parent_signature is not None and type(parent_signature) is not ClazzSignature:
            raise ValueError("Invalid type for parent_signature %s (expected ClazzSignature or NoneType)" % type(parent_signature))
        self.parent_signature = parent_signature

    def validate_overriding_rules(self) -> None:
        if self.is_included:
            return

        def throw(signature: MethodSignature) -> NoReturn:
            raise ValueError("Method '%s' violates overriding rules on class '%s'" % (signature.name, self.name))

        for signature in self.method_signatures:
            if self.get_field_by_name(signature.name) is not None:
                raise ValueError("Class property and method with the same name: '%s' (from class '%s'" % (signature.name, self.name))
            if not signature.is_override:
                if self.parent_signature is None:
                    continue
                if self.parent_signature.get_method_signature_by_name(signature.name) is not None:
                    throw(signature)
            else:
                if self.parent_signature is None:
                    throw(signature)
                # `parent_sig` is a method signature (as opposed to a class signature)
                parent_sig = util.nonnull(self.parent_signature).get_method_signature_by_name(signature.name)
                if parent_sig is None:
                    throw(signature)
                assert parent_sig is not None
                if not parent_sig.returntype.is_assignable_to(signature.returntype):
                    throw(signature)
                if len(signature.args) != len(parent_sig.args):
                    throw(signature)
                for i in range(len(signature.args) - 1):
                    # Why - 1 and + 1? Because `this` is an argument
                    if not parent_sig.args[i + 1].is_assignable_to(signature.args[i + 1]):
                        throw(signature)

        if self.parent_signature is not None:
            for field in self.fields:
                if self.parent_signature.get_field_by_name(field.name) is not None:
                    raise ValueError("Duplicate field in both child and parent class named '%s'" % field.name)

    def get_method_signature_by_name(self, name: str) -> Optional[MethodSignature]:
        for signature in self.method_signatures:
            if signature.name == name:
                return signature

        if self.parent_signature is not None:
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
        return repr(self)

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
        return ClazzField(node[0].data_strict, self.program.types.resolve_strict(node[1]))

    def emit_signature_no_fields(self) -> ClazzSignature:
        self.is_signature_no_fields = True
        self.signature = ClazzSignature(self.name, [], [], [], None)
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
        self.signature = ClazzSignature(self.name, self.fields, self.method_signatures, self.ctor_signatures, None)
        # We need to replace the containing_class with the new signature,
        # just so we don't have different signatures floating around.
        for signature in self.method_signatures:
            signature.containing_class = self.signature
        self.is_signature_no_fields = False
        return self.signature

    def add_superclass_to_signature(self) -> None:
        if len(self.top["extends"]) > 0:
            # Since we have a reference to this class's signature, we can
            # just update it in-place.
            parent_type = self.program.types.resolve(self.top["extends"][0])
            if parent_type is None or not isinstance(parent_type, typesys.ClazzType):
                self.top["extends"].compile_error("Tried to extend a nonexisting class")
            self.signature.parent_signature = parent_type.signature
        # Otherwise, the superclass remains as `None`.

    def emit(self) -> "ClazzSegment":
        self.emit_signature()
        return ClazzSegment(self)

class StaticVariableSegment(Segment[SegmentEmitter]):
    def __init__(self, static_variables: StaticVariableSet) -> None:
        Segment.__init__(self, "staticvars", 0x03)
        self.static_variables = static_variables

    def __str__(self) -> str:
        return "StaticVariableSegment()"

    def print_(self, emitter: SegmentEmitter) -> None:
        Segment.print_(self, emitter)
        for variable in self.static_variables.variables.values():
            print("    %s" % variable)

    def evaluate(self, program: "Program") -> None:
        Segment.evaluate(self, program)

class ClazzSegment(Segment[ClazzEmitter]):
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
        return "ClazzSegment(type='%s', typecode=%s, name='%s', nfields=%s, nmethods=%s, superclass=%s)" \
            % (self.humantype,
            self.realtype,
            self.name,
            len(self.fields),
            len(self.method_signatures),
            "'%s'" % self.parent_signature.name if self.parent_signature is not None else "None")

    def print_(self, emitter: ClazzEmitter) -> None:
        Segment.print_(self, emitter)
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
                self.static_variables.add_variable(
                    StaticVariable(
                        self.static_variables,
                        "%s.%s" % (self.namespace, unqualified_variable_name) if self.namespace is not None else unqualified_variable_name,
                        self.types.resolve_strict(toplevel[1]),
                        None
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
            if not toplevel.i("class"):
                continue
            clazz_emitters.append(ClazzEmitter(toplevel, self))
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
        for emitter in clazz_emitters:
            emitter.add_superclass_to_signature()
        self.clazz_emitters = clazz_emitters
        self.method_signatures += MethodSignature.scan(self.emitter.top, self)
        self.prescan_static_variables()

    def add_include(self, headers: header.HeaderRepresentation) -> None:
        for clazz in headers.clazzes:
            self.types.accept_skeleton_clazz(ClazzSignature(clazz.name, [], [], [], None, is_included=True))
        for method in headers.methods:
            # elw
            self.method_signatures.append(MethodSignature(
                method.name,
                [self.types.resolve_strict(parser.parse_type(arg.type)) for arg in method.args],
                [arg.name for arg in method.args],
                self.types.resolve_strict(parser.parse_type(method.returntype))
            ))
        for clazz in headers.clazzes:
            methods = []
            for clazz_method in clazz.methods:
                methods.append(self.get_method_signature(clazz_method.name))
            ctors = []
            for ctor in clazz.ctors:
                ctors.append(self.get_method_signature(ctor.name))
            fields = []
            for field in clazz.fields:
                fields.append(ClazzField(field.name, self.types.resolve_strict(parser.parse_type(field.type))))
            parent_type = self.types.resolve_strict(parser.parse_type(clazz.parent)) if clazz.parent is not None else None
            assert isinstance(parent_type, typesys.ClazzType) or parent_type is None
            # util.nonnull is correct here because of the previous two lines
            clazz_signature = ClazzSignature(clazz.name, fields, methods, ctors, util.nonnull(parent_type).signature if clazz.parent is not None else None, is_included=True)
            self.clazz_signatures.append(clazz_signature)
            self.types.update_signature(clazz_signature)

class Emitter:
    def __init__(self, top: parser.Node) -> None:
        self.top = top

    def emit_program(self) -> Program:
        return Program(self)

    def emit_bytes(self, program: Program) -> bytes:
        import bytecode
        return bytecode.emit(program.segments)
