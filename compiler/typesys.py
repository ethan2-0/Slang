# TODO: Change TypingError to Node.compile_error

class TypingError(Exception):
    def __init__(self, node, error):
        Exception.__init__(self, "Near line %s: %s" % (node.line, error))

class AbstractType:
    def __init__(self, name):
        self.name = name

    def is_assignable_to(self, other):
        # RuntimeException isAssignableTo Exception
        pass

    def is_assignable_from(self, other):
        # Exception isAssignableFrom RuntimeException
        return other.is_assignable_to(self)

    def is_numerical(self):
        return False

    def is_boolean(self):
        return False

    def is_void(self):
        return False

    def is_clazz(self):
        return False

    def is_array(self):
        return False

    def resolves(self, node):
        return node.i("ident") and node.data == self.name

    def __str__(self):
        return ":%s" % self.name

    def __repr__(self):
        return "Type(name=%s)" % self.name

class IntType(AbstractType):
    def __init__(self):
        AbstractType.__init__(self, "int")

    def is_assignable_to(self, other):
        return isinstance(other, IntType)

    def is_numerical(self):
        return True

class BoolType(AbstractType):
    def __init__(self):
        AbstractType.__init__(self, "bool")

    def is_assignable_to(self, other):
        return isinstance(other, BoolType)

    def is_boolean(self):
        return True

class VoidType(AbstractType):
    def __init__(self):
        AbstractType.__init__(self, "void")

    def is_void(self):
        return True

    def is_assignable_to(self, other):
        return True

class ClazzType(AbstractType):
    def __init__(self, signature):
        AbstractType.__init__(self, signature.name)
        self.signature = signature

    def is_assignable_to(self, other):
        return isinstance(other, ClazzType) and other.name == self.name

    def type_of_property(self, name, node=None):
        for field in self.signature.fields:
            if field.name == name:
                return field.type
        if node is not None:
            node.compile_error("Invalid property name '%s' for type '%s'" % (name, self.name))

    def method_signature(self, name, node=None):
        for method in self.signature.method_signatures:
            if method.name == name:
                return method
        if node is not None:
            node.compile_error("Invalid class method name '%s' for type '%s'" % (name, self.type))

    def is_clazz(self):
        return True

class ArrayType(AbstractType):
    def __init__(self, parent_type):
        AbstractType.__init__(self, "[%s]" % parent_type.name)
        self.parent_type = parent_type

    def is_assignable_to(self, other):
        return isinstance(other, ArrayType) and other.parent_type == self.parent_type

    def resolves(self, node):
        return node.i("[]") and self.parent_type.resolves(node[0])

    def is_array(self):
        return True

class TypeSystem:
    def __init__(self, program):
        self.program = program
        self.int_type = IntType()
        self.bool_type = BoolType()
        self.void_type = VoidType()
        self.types = [self.int_type, self.bool_type, self.void_type]
        self.array_types = []
        self.clazz_signatures = []

    def resolve(self, node, fail_silent=False):
        for type in self.types:
            if type.resolves(node):
                return type
        if node.i("["):
            return self.get_array_type(self.resolve(node[0]))
        if not fail_silent:
            raise TypingError(node, "Could not resolve type: '%s'" % node)

    def accept_skeleton_clazz(self, signature):
        self.clazz_signatures.append(signature)
        self.types.append(ClazzType(signature))

    def update_signature(self, signature):
        # TODO: This makes class resolution n^2
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

    def get_clazz_type_by_signature(self, signature):
        for type in self.types:
            if isinstance(type, ClazzType) and type.signature == signature:
                return type
        raise KeyError()

    def get_array_type(self, parent_type):
        for typ in self.array_types:
            if typ.parent_type == parent_type:
                return typ
        typ = ArrayType(parent_type)
        self.types.append(typ)
        self.array_types.append(typ)
        return typ

    def decide_type(self, expr, scope):
        if expr.of("+", "*", "-", "^", "&", "|", "%") and len(expr) == 2:
            lhs_type = self.decide_type(expr[0], scope)
            rhs_type = self.decide_type(expr[1], scope)
            if not lhs_type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % lhs_type)

            if not rhs_type.is_numerical():
                raise TypingError(expr[1], "Type %s is not numerical" % rhs_type)

            if not lhs_type.is_assignable_from(rhs_type) or not lhs_type.is_assignable_to(rhs_type):
                raise TypingError(expr, "Types %s and %s are incompatible for arithmetic operation" % (lhs_type, rhs_type))

            return lhs_type
        elif expr.of(">=", "<=", ">", "<"):
            lhs_type = self.decide_type(expr[0], scope)
            rhs_type = self.decide_type(expr[1], scope)
            if not lhs_type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % lhs_type)

            if not rhs_type.is_numerical():
                raise TypingError(expr[1], "Type %s is not numerical" % rhs_type)

            if not lhs_type.is_assignable_from(rhs_type) or not lhs_type.is_assignable_to(rhs_type):
                raise TypingError(expr, "Types %s and %s are incompatible for arithmetic comparison" % (lhs_type, rhs_type))

            return self.bool_type
        elif expr.of("==", "!="):
            lhs_type = self.decide_type(expr[0], scope)
            rhs_type = self.decide_type(expr[1], scope)
            if not lhs_type.is_assignable_to(rhs_type) and not rhs_type.is_assignable_to(lhs_type):
                raise TypingError(expr, "Incomparable types: '%s' and '%s'" % (lhs_type, rhs_type))
            return self.bool_type
        elif expr.i("number"):
            return self.int_type
        elif expr.of("and", "or"):
            lhs_type = self.decide_type(expr[0], scope)
            rhs_type = self.decide_type(expr[1], scope)
            if not lhs_type.is_boolean():
                raise TypingError(expr[0], "Type %s is not boolean" % lhs_type)

            if not rhs_type.is_boolean():
                raise TypingError(expr[0], "Type %s is not boolean" % rhs_type)

            return self.bool_type
        elif expr.i("not"):
            type = self.decide_type(expr[0], scope)
            if not type.is_boolean():
                raise TypingError(expr[0], "Type %s is not boolean" % type)
            return self.bool_type
        elif expr.of("~", "-") and len(expr) == 1:
            type = self.decide_type(expr[0], scope)
            if not type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % type)
            return self.int_type
        elif expr.i("ident"):
            return scope.resolve(expr.data, expr).type
        elif expr.of("true", "false"):
            return self.bool_type
        elif expr.i("null"):
            return self.void_type
        elif expr.i("["):
            # For now, everything must be the same type, except for nulls
            # interspersed. Later this will change.
            current_type = None
            for child in expr:
                current_child_type = self.decide_type(child, scope)
                if current_type is None:
                    current_type = current_child_type

                if not current_child_type.is_assignable_to(current_type):
                    raise TypingError(child, "Incompatible types for array: %s and %s" % (current_type, current_child_type))

            return self.get_array_type(current_type)
        elif expr.i("access"):
            lhs_type = self.decide_type(expr[0], scope)
            if not lhs_type.is_array():
                raise TypingError(child, "Attempt to access element of something that isn't an array")
            return lhs_type.parent_type
        elif expr.i("call"):
            # TODO: Move method call typechecking in here from emitter.py.
            signature = None
            if expr[0].i("."):
                dot_node = expr[0]
                lhs_type = self.decide_type(dot_node[0], scope)
                if not lhs_type.is_clazz():
                    raise TypingError(dot_node[1], "Attempt to call a method on non-class type '%s'" % lhs_type)
                signature = lhs_type.method_signature(dot_node[1].data)
            else:
                signature = self.program.get_method_signature(expr[0].data)
            if signature is None:
                expr.compile_error("Unknown method name '%s'" % expr[0].data)
            return signature.returntype
        elif expr.i("new"):
            ret = self.resolve(expr[0])
            if not ret.is_clazz():
                raise TypingError(expr[0], "Type %s is not a class" % ret)
            return ret
        elif expr.i("."):
            lhs_type = None
            if expr[0].i("ident"):
                lhs_type = scope.resolve(expr[0].data, expr[0]).type
            else:
                lhs_type = self.decide_type(expr[0], scope)
            return lhs_type.type_of_property(expr[1].data)
        else:
            print("Unrecognized expression getting type of None", expr)
            return None
