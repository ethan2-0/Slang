from typing import List, Generic, TypeVar
import typesys
import emitter
import parser
import abc

T = TypeVar("T", bound=typesys.AbstractType, covariant=True)
# Not inheriting from abc.ABC: https://stackoverflow.com/a/48554367
class AbstractInterpreterValue(Generic[T]):
    def __init__(self, typ: T) -> None:
        self.type: T = typ

    @abc.abstractmethod
    def equals(self, other: "AbstractInterpreterValue") -> bool:
        raise NotImplementedError

    @abc.abstractmethod
    def human_representation(self) -> str:
        raise NotImplementedError

InterpreterValueAny = AbstractInterpreterValue[typesys.AbstractType]

class InterpreterValueInteger(AbstractInterpreterValue[typesys.IntType]):
    def __init__(self, typ: typesys.IntType, value: int) -> None:
        AbstractInterpreterValue.__init__(self, typ)
        self.value = value

    def equals(self, other: AbstractInterpreterValue) -> bool:
        if not isinstance(other, InterpreterValueInteger):
            return False
        return other.value == self.value

    def human_representation(self) -> str:
        return str(self.value)

class InterpreterValueBoolean(AbstractInterpreterValue[typesys.BoolType]):
    def __init__(self, typ: typesys.BoolType, value: bool) -> None:
        AbstractInterpreterValue.__init__(self, typ)
        self.value = value

    def equals(self, other: AbstractInterpreterValue) -> bool:
        if not isinstance(other, InterpreterValueBoolean):
            return False
        return other.value == self.value

    def human_representation(self) -> str:
        return "true" if self.value else "false"

class InterpreterValueNull(AbstractInterpreterValue[typesys.VoidType]):
    def __init__(self, typ: typesys.VoidType) -> None:
        AbstractInterpreterValue.__init__(self, typ)

    def equals(self, other: AbstractInterpreterValue) -> bool:
        return isinstance(other, InterpreterValueNull)

    def human_representation(self) -> str:
        return "null"

class InterpreterValueArray(AbstractInterpreterValue[typesys.ArrayType]):
    def __init__(self, typ: typesys.ArrayType, value: List[AbstractInterpreterValue]) -> None:
        AbstractInterpreterValue.__init__(self, typ)
        self.value = value

    def equals(self, other: AbstractInterpreterValue) -> bool:
        return other is self

    def human_representation(self) -> str:
        return "[%s]" % ", ".join(value.human_representation() for value in self.value)

class Interpreter:
    def __init__(self, program: "emitter.Program") -> None:
        self.program: "emitter.Program" = program
        self.null = InterpreterValueNull(self.program.types.void_type)
        self.true = InterpreterValueBoolean(self.program.types.bool_type, True)
        self.false = InterpreterValueBoolean(self.program.types.bool_type, False)
        self.dummy_scope = emitter.Scopes()

    def create_int_value(self, value: int) -> InterpreterValueInteger:
        return InterpreterValueInteger(self.program.types.int_type, value)

    def create_array_value(self, type: typesys.ArrayType, values: List[AbstractInterpreterValue]) -> InterpreterValueArray:
        return InterpreterValueArray(type, values)

    def normalize_negative(self, num: int) -> int:
        bitmask = 0xffffffffffffffff
        if num & 0x8000000000000000 != 0:
            num = (((~num) & bitmask) + 1) & bitmask
        return num

    def eval_expr(self, node: parser.Node) -> InterpreterValueAny:
        # TODO: Support referencing other static variables
        # Bitmask for 64-bit computation
        bitmask = 0xffffffffffffffff
        if node.i("number"):
            return self.create_int_value(int(node.data_strict))
        elif node.i("true"):
            return self.true
        elif node.i("false"):
            return self.false
        elif node.i("null"):
            return self.null
        elif node.of("+", "*", "^", "&", "|"):
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not isinstance(lhs, InterpreterValueInteger) or not isinstance(rhs, InterpreterValueInteger):
                raise typesys.TypingError(node, "Attempt to perform arithmetic on something that isn't an integers")
            if node.i("+"):
                ret = lhs.value + rhs.value
            elif node.i("*"):
                ret = lhs.value * rhs.value
            elif node.i("^"):
                ret = lhs.value ^ rhs.value
            elif node.i("&"):
                ret = lhs.value & rhs.value
            elif node.i("|"):
                ret = lhs.value | rhs.value
            return self.create_int_value(ret & bitmask)
        elif node.of("and", "or"):
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not isinstance(lhs, InterpreterValueBoolean) or not isinstance(rhs, InterpreterValueBoolean):
                raise typesys.TypingError(node, "Attempt to perform logical operation on something that isn't an integers")
            if node.i("and"):
                ret = lhs.value and rhs.value
            elif node.i("or"):
                ret = lhs.value or rhs.value
            return self.true if ret else self.false
        elif node.i("not"):
            val = self.eval_expr(node[0])
            if not isinstance(val, InterpreterValueBoolean):
                raise typesys.TypingError(node, "Attempt to perform logical operation on something that isn't an integers")
            return self.false if val.value else self.true
        elif node.of(">=", "<=", "<", ">"):
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not isinstance(lhs, InterpreterValueInteger) or not isinstance(rhs, InterpreterValueInteger):
                raise typesys.TypingError(node, "Attempt to perform arithmetic on something that isn't an integers")
            lhs_value = self.normalize_negative(lhs.value)
            rhs_value = self.normalize_negative(rhs.value)
            if node.i(">="):
                ret = lhs_value >= rhs_value
            elif node.i("<="):
                ret = lhs_value <= rhs_value
            elif node.i("<"):
                ret = lhs_value < rhs_value
            elif node.i(">"):
                ret = lhs_value > rhs_value
            else:
                raise ValueError("This is a compiler bug")
            return self.true if ret else self.false
        elif node.of("==", "!="):
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not lhs.type.is_assignable_to(rhs.type) and not rhs.type.is_assignable_to(lhs.type):
                raise typesys.TypingError(node, "Incomparable types: '%s' and '%s'" % (lhs.type, rhs.type))
            return self.true if lhs.equals(rhs) ^ (True if node.i("!=") else False) else self.false
        elif node.i("-") and len(node) == 2:
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not isinstance(lhs, InterpreterValueInteger) or not isinstance(rhs, InterpreterValueInteger):
                raise typesys.TypingError(node, "Attempt to perform arithmetic on something that isn't an integers")
            return self.create_int_value((lhs.value - rhs.value) & bitmask)
        elif (node.i("-") and len(node) == 1) or node.i("~"):
            val = self.eval_expr(node[0])
            if not isinstance(val, InterpreterValueInteger):
                raise typesys.TypingError(node, "Attempt to negate something that isn't an integer")
            return self.create_int_value((-val.value if node.i("-") else ~val.value) & bitmask)
        elif node.i("/"):
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not isinstance(lhs, InterpreterValueInteger) or not isinstance(rhs, InterpreterValueInteger):
                raise typesys.TypingError(node, "Attempt to perform arithmetic on something that isn't an integers")
            lhs_value = lhs.value
            rhs_value = rhs.value
            # Make sure, if our value is negative, we're dividing by a
            # negative rather than a very large value (due to two's
            # complement)
            lhs_value = self.normalize_negative(lhs_value)
            rhs_value = self.normalize_negative(rhs_value)
            res = lhs_value // rhs_value
            if res * rhs.value != lhs.value:
                # Python rounds toward negative infinity, whereas C
                # (and thus our language) rounds toward 0.
                if res < 0:
                    res += 1
            return self.create_int_value(res & bitmask)
        elif node.i("["):
            typ = self.program.types.decide_type(node, emitter.Scopes())
            if not isinstance(typ, typesys.ArrayType):
                raise ValueError("This is a compiler bug.")
            # All of our typechecking has already been taken care of in
            # the logic in typesys there
            result_values: List[AbstractInterpreterValue] = []
            for child in node:
                result_values.append(self.eval_expr(child))
            return self.create_array_value(typ, result_values)
        elif node.i("arrinst"):
            typ = self.program.types.decide_type(node, emitter.Scopes())
            if not isinstance(typ, typesys.ArrayType):
                raise ValueError("This is a compiler bug.")
            # Same thing, all of our typechecking is delegated to typesys
            array_len = self.eval_expr(node[1])
            if not isinstance(array_len, InterpreterValueInteger):
                raise typesys.TypingError(node[1], "Type of array length must be an integer")
            if array_len.value > 1024:
                node.warn("Statically creating a very large array, this will make your bytecode file very large: %d" % array_len.value)
            arrinst_result_values: List[AbstractInterpreterValue]
            if isinstance(typ.parent_type, typesys.IntType):
                arrinst_result_values = [self.create_int_value(0)] * array_len.value
            elif isinstance(typ.parent_type, typesys.BoolType):
                arrinst_result_values = [self.false] * array_len.value
            else:
                arrinst_result_values = [self.null] * array_len.value
            return self.create_array_value(typ, arrinst_result_values)
        elif node.i("#"):
            val = self.eval_expr(node[0])
            if not isinstance(val, InterpreterValueArray):
                raise typesys.TypingError(node[0], "Cannot find length of something that isn't an array")
            return self.create_int_value(len(val.value))
        elif node.i("access"):
            lhs = self.eval_expr(node[0])
            rhs = self.eval_expr(node[1])
            if not isinstance(lhs, InterpreterValueArray):
                raise typesys.TypingError(node[0], "Can't access an element of something that isn't an array")
            if not isinstance(rhs, InterpreterValueInteger):
                raise typesys.TypingError(node[1], "Array indices must be integers")
            return lhs.value[rhs.value]
        else:
            node.compile_error("Cannot evaluate expression at compile-time")
