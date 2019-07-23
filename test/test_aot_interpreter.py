import sys
import os.path

initial_sys_path = sys.path
sys.path = [os.path.join(os.path.dirname(__file__), "../compiler")] + sys.path

import parser
import emitter
import interpreter

sys.path = initial_sys_path


def parse(text):
    # Appending a newline is due to a quirk in the expression parser.
    # I could fix it but it's really not worth it.
    return parser.Parser(parser.Toker("%s\n" % text)).parse_expr()

def dummy_program():
    return emitter.Emitter(parser.Node("top")).emit_program()

def evaluate(text):
    return interpreter.Interpreter(dummy_program()).eval_expr(parse(text))

def assert_has_bool_value(text, value):
    val = evaluate(text)
    assert isinstance(val, interpreter.InterpreterValueBoolean)
    assert not (val.value ^ value)

def assert_has_integer_value(text, value):
    val = evaluate(text)
    assert isinstance(val, interpreter.InterpreterValueInteger)
    assert val.value == value

def test_int_literal():
    assert_has_integer_value("123", 123)

def test_true_literal():
    assert_has_bool_value("true", True)

def test_false_literal():
    assert_has_bool_value("false", False)

def test_null_literal():
    val = evaluate("null")
    assert isinstance(val, interpreter.InterpreterValueNull)

def test_addition_multiplication():
    assert_has_integer_value("1 + 2 * (3 + 4)", 15)

def test_subtraction():
    assert_has_integer_value("23 - 4", 19)
    assert_has_integer_value("4 - 23", (4 - 23) & 0xffffffffffffffff)

def test_division():
    assert_has_integer_value("19 / 4", 4)

def test_arithmetic_kitchen_sink():
    assert_has_integer_value("((123 ^ 456) & (789 | -10)) / 16 * 6 + 5 - (24 & -15)", 151)

def test_bools():
    # This is by no means exhaustive, but it's good enough
    assert_has_bool_value("false and false", False)
    assert_has_bool_value("true and false", False)
    assert_has_bool_value("true and true", True)
    assert_has_bool_value("true or false", True)
    assert_has_bool_value("false or false", False)
    assert_has_bool_value("not false", True)

def test_equality():
    assert_has_bool_value("1 == 2", False)
    assert_has_bool_value("2 == 2", True)
    assert_has_bool_value("-2 == -2", True)
    assert_has_bool_value("2 != 2", False)
    assert_has_bool_value("2 != 3", True)

def test_array_literal():
    val = evaluate("[[1, 2], [3, 4], [5, 6]]")
    assert isinstance(val, interpreter.InterpreterValueArray)
    assert isinstance(val.value[0], interpreter.InterpreterValueArray)
    assert isinstance(val.value[0].value[0], interpreter.InterpreterValueInteger)
    assert val.value[0].value[0].value == 1
    assert val.value[2].value[1].value == 6

def test_array_instantiation():
    val = evaluate("[[bool]: 4 * 4]")
    assert isinstance(val, interpreter.InterpreterValueArray)
    assert isinstance(val.value[0], interpreter.InterpreterValueNull)

def test_array_access():
    assert_has_integer_value("([1, 2, 3])[2]", 3)
    assert_has_bool_value("([bool: 5])[2]", False)

def test_array_length():
    assert_has_integer_value("#[1, 2, 3, 4, 5]", 5)

def test_comparison():
    assert_has_bool_value("3 > 2", True)
    assert_has_bool_value("3 >= 3", True)
    assert_has_bool_value("3 < 3", False)
    assert_has_bool_value("3 <= 4", True)
