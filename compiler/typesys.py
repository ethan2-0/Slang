# TODO: Change TypingError to Node.compile_error

class TypingError(Exception):
    def __init__(self, node, error):
        Exception.__init__(self, "At line %s: %s" % (node.line, error))

class AbstractType:
    def __init__(self, name):
        self.name = name

    def is_assignable_to(self, other):
        # RuntimeException isAssignableTo Exception
        pass

    def is_assignable_from(self, other):
        # Exception isAssignableFrom RuntimeException
        pass

    def is_numerical(self):
        return False

    def is_boolean(self):
        return False

    def __str__(self):
        return ":%s" % self.name

    def __repr__(self):
        return "Type(name=%s)" % self.name

class IntType(AbstractType):
    def __init__(self):
        AbstractType.__init__(self, "int")

    def is_assignable_to(self, other):
        return isinstance(other, IntType)

    def is_assignable_from(self, other):
        return isinstance(other, IntType)

    def is_numerical(self):
        return True

class BoolType(AbstractType):
    def __init__(self):
        AbstractType.__init__(self, "bool")

    def is_assignable_to(self, other):
        return isinstance(other, BoolType)

    def is_assignable_from(self, other):
        return isinstance(other, BoolType)

    def is_boolean(self):
        return True

class TypeSystem:
    def __init__(self):
        self.int_type = IntType()
        self.bool_type = BoolType()
        self.types = [self.int_type, self.bool_type]

    def decide_type(self, expr, scope):
        if expr.of("+", "*", "-", "^", "&", "|"):
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
        elif expr.i("~"):
            type = self.decide_type(expr[0], scope)
            if not type.is_numerical():
                raise TypingError(expr[0], "Type %s is not numerical" % type)
            return self.int_type
        elif expr.i("ident"):
            return scope.resolve(expr.data, expr).type
        else:
            return None
