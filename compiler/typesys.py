# TODO: Change TypingError to Node.compile_error

import util
import parser
import emitter
from typing import Optional, List, Dict, cast

class TypingError(Exception):
    def __init__(self, node: parser.Node, error: str) -> None:
        Exception.__init__(self, "Near line %s: %s" % (node.line, error))

class AbstractType:
    def __init__(self, name: str) -> None:
        self.name = name

    def is_assignable_to(self, other: "AbstractType") -> bool:
        # RuntimeException isAssignableTo Exception
        raise NotImplementedError

    def is_assignable_from(self, other: "AbstractType") -> bool:
        # Exception isAssignableFrom RuntimeException
        return other.is_assignable_to(self)

    def is_numerical(self) -> bool:
        return False

    def is_boolean(self) -> bool:
        return False

    def is_void(self) -> bool:
        return False

    def is_clazz(self) -> bool:
        # Contract: This returns true iff isinstance(self, ClazzType)
        return False

    def is_interface(self) -> bool:
        return False

    def is_array(self) -> bool:
        # Contract: This returns true iff isinstance(self, ArrayType)
        return False

    def is_generic_argument(self) -> bool:
        return False

    def resolves(self, node: parser.Node, program: "emitter.Program") -> bool:
        def flatten_qualified(nod: parser.Node) -> str:
            if nod.i("ident"):
                return nod.data_strict
            else:
                return "%s.%s" % (flatten_qualified(nod[0]), flatten_qualified(nod[1]))

        if not node.i("ident") and not node.i("."):
            return False

        name = flatten_qualified(node)

        for search_path in program.search_paths:
            if "%s%s" % (search_path, name) == self.name:
                return True
        return False

    def get_supertype(self) -> "Optional[AbstractType]":
        return None

    def __str__(self) -> str:
        return ":%s" % self.name

    def __repr__(self) -> str:
        return "Type(name=%s)" % self.name

class IntType(AbstractType):
    def __init__(self) -> None:
        AbstractType.__init__(self, "int")

    def is_assignable_to(self, other: AbstractType) -> bool:
        return isinstance(other, IntType)

    def is_numerical(self) -> bool:
        return True

class BoolType(AbstractType):
    def __init__(self) -> None:
        AbstractType.__init__(self, "bool")

    def is_assignable_to(self, other: AbstractType) -> bool:
        return isinstance(other, BoolType)

    def is_boolean(self) -> bool:
        return True

class VoidType(AbstractType):
    def __init__(self) -> None:
        AbstractType.__init__(self, "void")

    def is_void(self) -> bool:
        return True

    def is_assignable_to(self, other: AbstractType) -> bool:
        return True

class ClazzType(AbstractType):
    def __init__(self, signature: "emitter.ClazzSignature", typesys: "TypeSystem") -> None:
        AbstractType.__init__(self, signature.name)
        self.typesys = typesys
        self.signature = signature

    def is_assignable_to(self, other: AbstractType) -> bool:
        if isinstance(other, InterfaceType):
            return self.implements(other)
        if not isinstance(other, ClazzType):
            return False

        if other.name == self.name:
            return True

        supertype = self.get_supertype()
        if supertype is not None:
            return supertype.is_assignable_to(other)

        return False

    def get_supertype(self) -> "Optional[ClazzType]":
        if self.signature.parent_signature is not None:
            return self.typesys.get_clazz_type_by_signature(self.signature.parent_signature)
        return None

    def type_of_property(self, name: str, node: parser.Node) -> AbstractType:
        ret = self.type_of_property_optional(name, node)
        if ret is None:
            node.compile_error("Invalid property name '%s' for type '%s'" % (name, self.name))
        return ret

    def type_of_property_optional(self, name: str, node: parser.Node=None) -> Optional[AbstractType]:
        for field in self.signature.fields:
            if field.name == name:
                return field.type
        supertype = self.get_supertype()
        if supertype is not None:
            ret = supertype.type_of_property_optional(name)
            if ret is not None:
                return ret
        if node is not None:
            node.compile_error("Invalid property name '%s' for type '%s'" % (name, self.name))
        return None

    def method_signature(self, name: str, node: parser.Node) -> "emitter.MethodSignature":
        ret = self.method_signature_optional(name, node)
        if ret is None:
            node.compile_error("Invalid class method name '%s' for type '%s'" % (name, self.name))
        return ret

    def method_signature_optional(self, name: str, node: parser.Node=None) -> "Optional[emitter.MethodSignature]":
        for method in self.signature.method_signatures:
            if method.name == name:
                return method
        supertype = self.get_supertype()
        if supertype is not None:
            ret = supertype.method_signature_optional(name)
            if ret is not None:
                return ret
        if node is not None:
            node.compile_error("Invalid class method name '%s' for type '%s'" % (name, self.name))
        return None

    def is_clazz(self) -> bool:
        return True

    def implements(self, other: "InterfaceType") -> bool:
        for interface in self.signature.implemented_interfaces:
            if other == interface:
                return True
        supertype = self.get_supertype()
        if supertype is not None and supertype.implements(other):
            return True
        return False

class InterfaceType(AbstractType):
    def __init__(self, interface: "emitter.Interface") -> None:
        AbstractType.__init__(self, interface.name)
        self.interface = interface

    def is_interface(self) -> bool:
        return True

    def get_supertype(self) -> Optional[AbstractType]:
        return None

    def method_signature(self, name: str, node: parser.Node) -> "emitter.MethodSignature":
        ret = self.method_signature_optional(name)
        if ret is None:
            node.compile_error("Invalid interface method name '%s' for type '%s'" % (name, self.name))
        return ret

    def method_signature_optional(self, name: str) -> "Optional[emitter.MethodSignature]":
        return self.interface.get_signature_by_name(name)

    def is_assignable_to(self, other: AbstractType) -> bool:
        if not isinstance(other, InterfaceType):
            return False
        return other == self

class ArrayType(AbstractType):
    def __init__(self, parent_type: AbstractType) -> None:
        AbstractType.__init__(self, "[%s]" % parent_type.name)
        self.parent_type = parent_type

    def is_assignable_to(self, other: AbstractType) -> bool:
        return isinstance(other, ArrayType) and other.parent_type == self.parent_type

    def resolves(self, node: parser.Node, program: "emitter.Program") -> bool:
        return node.i("[]") and self.parent_type.resolves(node[0], program)

    def is_array(self) -> bool:
        return True

class GenericTypeArgument(AbstractType):
    def __init__(self, name: str):
        AbstractType.__init__(self, name)

    def is_assignable_to(self, other: AbstractType) -> bool:
        return other == self

class GenericTypeContext:
    def __init__(self, parent: "Optional[GenericTypeContext]") -> None:
        self.parent = parent
        # Note that order is significant here
        self.arguments: "List[GenericTypeArgument]" = []

    @property
    def num_arguments(self) -> int:
        return len(self.arguments)

    def is_generic_argument(self) -> bool:
        return False

    def resolve_optional(self, node: parser.Node, program: "emitter.Program") -> Optional[GenericTypeArgument]:
        for typ in self.arguments:
            if typ.resolves(node, program):
                return typ
        if self.parent is not None:
            return self.parent.resolve_optional(node, program)
        return None

    def add_type_argument(self, name: str) -> None:
        self.arguments.append(GenericTypeArgument(name))

    def __repr__(self) -> str:
        return "GenericTypeContext(parent=%s, arguments=[%s])" % (self.parent, ", ".join(str(arg) for arg in self.arguments))

    def __str__(self) -> str:
        return repr(self)

class GenericTypeParameters:
    def __init__(self, context: GenericTypeContext, parameters: List[AbstractType], types: "TypeSystem") -> None:
        if len(parameters) != len(context.arguments):
            raise ValueError("Wrong number of type parameters. This is a compiler bug.")
        self.context = context
        self.parameters = parameters
        self.types = types
        self.num_params = len(parameters)
        self.mapping: Dict[GenericTypeArgument, AbstractType] = dict()
        for i in range(self.num_params):
            self.mapping[context.arguments[i]] = parameters[i]

    def reify(self, parameter: AbstractType) -> AbstractType:
        if isinstance(parameter, ArrayType):
            return self.types.get_array_type(self.reify(parameter.parent_type))
        if not isinstance(parameter, GenericTypeArgument):
            return parameter
        return self.mapping[parameter]

class TypeSystem:
    def __init__(self, program: "emitter.Program"):
        self.program = program
        self.int_type = IntType()
        self.bool_type = BoolType()
        self.void_type = VoidType()
        self.types = [self.int_type, self.bool_type, self.void_type]
        self.array_types: List[ArrayType] = []
        self.clazz_signatures: "List[emitter.ClazzSignature]" = []

    def resolve(self, node: parser.Node, generic_type_context: Optional[GenericTypeContext],  fail_silent: bool=False) -> Optional[AbstractType]:
        for typ in self.types:
            if typ.resolves(node, self.program):
                return typ
        if node.i("["):
            element_type = self.resolve(node[0], generic_type_context, fail_silent=fail_silent)
            if element_type is None:
                if not fail_silent:
                    # TypeSystem.resolve will never return None if fail_silent == True
                    raise ValueError("This is a compiler bug")
                return None
            return self.get_array_type(element_type)
        if generic_type_context is not None:
            ret = generic_type_context.resolve_optional(node, self.program)
            if ret is not None:
                return ret
        if not fail_silent:
            raise TypingError(node, "Could not resolve type: '%s' ('%s')" % (node, util.get_flattened(node)))
        else:
            return None

    def accept_interface(self, interface: "emitter.Interface") -> InterfaceType:
        typ = InterfaceType(interface)
        self.types.append(typ)
        return typ

    def get_interface_type_by_name(self, name: str) -> Optional[InterfaceType]:
        typ = self.resolve(parser.parse_type(name), None, fail_silent=True)
        if typ is None:
            return typ
        if not isinstance(typ, InterfaceType):
            return None
        return typ

    def get_clazz_type_by_name(self, name: str) -> Optional[ClazzType]:
        typ = self.resolve(parser.parse_type(name), None, fail_silent=True)
        if typ is None:
            return typ
        if not isinstance(typ, ClazzType):
            return None
        return typ

    def get_string_type(self) -> Optional[ClazzType]:
        return self.get_clazz_type_by_name("stdlib.String")

    def get_object_type(self) -> Optional[ClazzType]:
        return self.get_clazz_type_by_name("stdlib.Object")

    def resolve_strict(self, node: parser.Node, generic_type_context: Optional[GenericTypeContext]) -> AbstractType:
        ret = self.resolve(node, generic_type_context, fail_silent=False)
        if ret is None:
            # TypeSystem.resolve will never return None if fail_silent == True
            raise ValueError("This is a compiler bug.")
        return ret

    def accept_skeleton_clazz(self, signature: "emitter.ClazzSignature") -> None:
        self.clazz_signatures.append(signature)
        self.types.append(ClazzType(signature, self))

    def update_signature(self, signature: "emitter.ClazzSignature") -> None:
        # TODO: This is n^2 in the number of classes
        found = False
        for i in range(len(self.clazz_signatures)):
            if self.clazz_signatures[i].name == signature.name:
                self.clazz_signatures[i] = signature
                found = True
        if not found:
            raise KeyError()

        found = False
        for type in self.types:
            if isinstance(type, ClazzType) and type.name == signature.name:
                type.signature = signature
                found = True
        if not found:
            raise KeyError()

    def get_clazz_type_by_signature(self, signature: "emitter.ClazzSignature") -> ClazzType:
        for type in self.types:
            if isinstance(type, ClazzType) and type.signature == signature:
                return type
        raise KeyError()

    def get_array_type(self, parent_type: AbstractType) -> ArrayType:
        for typ in self.array_types:
            if typ.parent_type == parent_type:
                return typ
        typ = ArrayType(parent_type)
        self.types.append(typ)
        self.array_types.append(typ)
        return typ

    def resolve_to_signature(self, node: parser.Node, scope: "emitter.Scopes", generic_type_context: Optional[GenericTypeContext]) -> "emitter.MethodSignature":
        if util.get_flattened(node) is not None and self.program.get_method_signature_optional(util.nonnull(util.get_flattened(node))) is not None:
            return self.program.get_method_signature(util.nonnull(util.get_flattened(node)))
        if node.i("ident"):
            node.compile_error("Attempt to call an identifier that doesn't resolve to a method")
            # Unreachable
            return None # type: ignore
        elif node.i("."):
            typ = self.decide_type(node[0], scope, generic_type_context)
            if not node[1].i("ident"):
                raise ValueError("This is a compiler bug")
            if isinstance(typ, ClazzType):
                if typ.type_of_property_optional(node[1].data_strict) is not None:
                    node.compile_error("Attempt to call a property")
                    # Unreachable
                    return None # type: ignore
                elif typ.method_signature_optional(node[1].data_strict) is not None:
                    return typ.method_signature(node[1].data_strict, node)
                else:
                    node.compile_error("Attempt to perform property access that doesn't make sense (perhaps the property doesn't exist or is spelled wrong?)")
                    # Unreachabe
                    return None # type: ignore
            elif isinstance(typ, InterfaceType):
                return typ.method_signature(node[1].data_strict, node)
            else:
                node.compile_error("Cannot perform access on something that isn't an instance of a class or an interface")
                # Unreachable
                return None # type: ignore
        else:
            node.compile_error("Attempt to call something that can't be called")
            # Unreachable
            return None # type: ignore

    def decide_type(self, expr: parser.Node, scope: "emitter.Scopes", generic_type_context: Optional[GenericTypeContext]) -> AbstractType:
        if expr.i("as"):
            return self.resolve_strict(expr[1], generic_type_context)
        elif expr.i("instanceof"):
            return self.bool_type
        elif expr.of("+", "*", "-", "^", "&", "|", "%", "/") and len(expr) == 2:
            lhs_type = self.decide_type(expr[0], scope, generic_type_context)
            rhs_type = self.decide_type(expr[1], scope, generic_type_context)
            if not lhs_type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % lhs_type)

            if not rhs_type.is_numerical():
                raise TypingError(expr[1], "Type %s is not numerical" % rhs_type)

            if not lhs_type.is_assignable_from(rhs_type) or not lhs_type.is_assignable_to(rhs_type):
                raise TypingError(expr, "Types %s and %s are incompatible for arithmetic operation" % (lhs_type, rhs_type))

            return lhs_type
        elif expr.of(">=", "<=", ">", "<"):
            lhs_type = self.decide_type(expr[0], scope, generic_type_context)
            rhs_type = self.decide_type(expr[1], scope, generic_type_context)
            if not lhs_type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % lhs_type)

            if not rhs_type.is_numerical():
                raise TypingError(expr[1], "Type %s is not numerical" % rhs_type)

            if not lhs_type.is_assignable_from(rhs_type) or not lhs_type.is_assignable_to(rhs_type):
                raise TypingError(expr, "Types %s and %s are incompatible for arithmetic comparison" % (lhs_type, rhs_type))

            return self.bool_type
        elif expr.of("==", "!="):
            lhs_type = self.decide_type(expr[0], scope, generic_type_context)
            rhs_type = self.decide_type(expr[1], scope, generic_type_context)
            if not lhs_type.is_assignable_to(rhs_type) and not rhs_type.is_assignable_to(lhs_type):
                raise TypingError(expr, "Incomparable types: '%s' and '%s'" % (lhs_type, rhs_type))
            return self.bool_type
        elif expr.i("number"):
            return self.int_type
        elif expr.of("and", "or"):
            lhs_type = self.decide_type(expr[0], scope, generic_type_context)
            rhs_type = self.decide_type(expr[1], scope, generic_type_context)
            if not lhs_type.is_boolean():
                raise TypingError(expr[0], "Type %s is not boolean" % lhs_type)

            if not rhs_type.is_boolean():
                raise TypingError(expr[0], "Type %s is not boolean" % rhs_type)

            return self.bool_type
        elif expr.i("not"):
            type = self.decide_type(expr[0], scope, generic_type_context)
            if not type.is_boolean():
                raise TypingError(expr[0], "Type %s is not boolean" % type)
            return self.bool_type
        elif expr.of("~", "-") and len(expr) == 1:
            type = self.decide_type(expr[0], scope, generic_type_context)
            if not type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % type)
            return self.int_type
        elif expr.of("ident", ".") \
                and util.get_flattened(expr) is not None \
                and self.program.static_variables.has_variable(util.nonnull(util.get_flattened(expr))):
            return self.program.static_variables.resolve_variable(util.nonnull(util.get_flattened(expr))).type
        elif expr.i("ident"):
            return scope.resolve(expr.data_strict, expr).type
        elif expr.of("true", "false"):
            return self.bool_type
        elif expr.i("null"):
            return self.void_type
        elif expr.i("["):
            # For now, everything must be the same type, except for nulls
            # interspersed. Later this will change.
            current_type = None
            if len(expr) == 0:
                expr.compile_error("Zero-length arrays must be instantiated with the arbitrary-length instantiation syntax")
            for child in expr:
                current_child_type: AbstractType = self.decide_type(child, scope, generic_type_context)
                if current_type is None:
                    if not current_child_type.is_void():
                        current_type = current_child_type
                    else:
                        continue

                # This is to make the typechecker happy
                # We know this is safe because of the previous if-statement
                current_type_not_none: AbstractType = current_type

                while current_type_not_none.get_supertype() is not None and not current_child_type.is_assignable_to(current_type_not_none):
                    current_type = current_type_not_none.get_supertype()
                if not current_child_type.is_assignable_to(current_type_not_none):
                    raise TypingError(child, "Could not reconcile type %s" % (current_child_type))

            if current_type is None:
                expr.compile_error("Cannot have an array literal comprising only null values, use arbitrary-length instantiation syntax instead")
                # Unreachable
                return None # type: ignore
            else:
                return self.get_array_type(current_type)
        elif expr.i("arrinst"):
            return self.get_array_type(self.resolve_strict(expr[0], generic_type_context))
        elif expr.i("#"):
            if not self.decide_type(expr[0], scope, generic_type_context).is_array():
                raise TypingError(expr[0], "Can't decide length of something that isn't an array")
            return self.int_type
        elif expr.i("access"):
            lhs_type = self.decide_type(expr[0], scope, generic_type_context)
            if not lhs_type.is_array():
                raise TypingError(expr, "Attempt to access element of something that isn't an array")
            assert isinstance(lhs_type, ArrayType)
            return lhs_type.parent_type
        elif expr.i("call"):
            # TODO: Move method call typechecking in here from emitter.py.
            signature = self.resolve_to_signature(
                expr[0],
                scope,
                generic_type_context
            )
            if len(expr["typeargs"]) != (len(signature.generic_type_context.arguments) if signature.generic_type_context is not None else 0):
                expr.compile_error("Wrong number of type arguments (expected %s, got %s)" % (len(signature.generic_type_context.arguments), len(expr["typeargs"])))
            signature = signature.reify([self.resolve_strict(typ, generic_type_context) for typ in expr["typeargs"]], self.program)
            if signature is not None:
                return signature.returntype
            if expr[0].i("."):
                dot_node = expr[0]
                lhs_type = self.decide_type(dot_node[0], scope)
                if not lhs_type.is_clazz():
                    raise TypingError(dot_node[1], "Attempt to call a method on non-class type '%s'" % lhs_type)
                signature = lhs_type.method_signature(dot_node[1].data)
            if signature is None:
                expr.compile_error("Unknown method name '%s'" % expr[0].data)
            return signature.returntype
        elif expr.i("new"):
            ret = self.resolve_strict(expr[0], generic_type_context)
            if not ret.is_clazz():
                raise TypingError(expr[0], "Type %s is not a class" % ret)
            return ret
        elif expr.i("."):
            # Ternary operator to satisfy typechecker (if-else statement would effectively require phi node which is ugly)
            lhs_typ: AbstractType = \
                (scope.resolve(expr[0].data_strict, expr[0]).type) if expr[0].i("ident") \
                else self.decide_type(expr[0], scope, generic_type_context)
            if not lhs_typ.is_clazz():
                expr.compile_error("Attempt to access an attribute of something that isn't a class")
            assert isinstance(lhs_typ, ClazzType)
            return lhs_typ.type_of_property(expr[1].data_strict, expr)
        elif expr.i("string"):
            string_type = self.get_string_type()
            if string_type is None:
                expr.compile_error("Cannot use string literal without an implementation of stdlib.String")
            return string_type
        elif expr.i("super"):
            # TODO: This is horribly ugly
            typ = scope.resolve("this", expr).type
            if not isinstance(typ, ClazzType):
                expr.compile_error("Variable `this` isn't a class type")
            parent_type = typ.get_supertype()
            if parent_type is None:
                expr.compile_error("Attempt to reference superclass in a class without a superclass")
            return parent_type
        else:
            raise ValueError("Expression not accounted for in typesys. This is a compiler bug.")
