# These tests are for tests that don't ever touch the interpreter.
import util

class TestInterpreterIfSemantics:
    def test_ifsemantics(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_ifsemantics/ifsemantics.slg")
        assert "0x00000234" in util.interpret(path)

class TestInterpreterMethodCall:
    def test_methodcall(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_methodcall/methodcall.slg")
        assert "0x0000002a" in util.interpret(path)

class TestInterpreterMinimal:
    def test_minimal(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_minimal/minimal.slg")
        assert "0x00000001" in util.interpret(path)

class TestInterpreterRecursion:
    def test_recursion(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_recursion/recursion.slg")
        assert "0x00000001" in util.interpret(path)

class TestIntegrationInclude:
    def test_include(self):
        util.clean_tmp()
        lib = util.assert_compile_succeeds("resources/integration_include/lib.slg")
        main = util.assert_compile_succeeds("resources/integration_include/main.slg", include=lib)
        assert "0x00000003" in util.interpret(lib, main)

class TestIntegrationKitchenSink:
    def test_kitchensink(self):
        util.clean_tmp()
        util.assert_compile_succeeds("resources/integration_kitchensink/kitchensink.slg")

class TestMethodTypes:
    def test_method_types(self):
        util.clean_tmp()
        util.assert_compile_succeeds("resources/types_method_types/method_types.slg")

    def test_wrong_parameter_type(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_method_types/wrong_parameter_type.slg", "not assignable")

    def test_wrong_return_type(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_method_types/wrong_return_type.slg", "Return type mismatch")

class TestOperatorTypes:
    def test_operator_types(self):
        util.clean_tmp()
        util.assert_compile_succeeds("resources/types_operator_types/operator_types.slg")

class TestWrongExplicitType:
    def test_bool_as_int(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_explicit_type/bool_as_int.slg", "not assignable")

    def test_int_as_bool(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_explicit_type/bool_as_int.slg", "not assignable")

class TestTypesWrongOperatorTypes:
    def test_arithmetic_needs_int(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/arithmetic_needs_int.slg", "is not numerical")

    def test_arithmetic_oneint(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/arithmetic_oneint.slg", "is not numerical")

    def test_equality_makes_boolean(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/equality_makes_boolean.slg", "is not numerical")

    def test_equality_needs_assignable(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/equality_needs_assignable.slg", "Incomparable types")

    def test_if_needs_boolean(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/if_needs_boolean.slg", "must be boolean")

    def test_increment_bool(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/increment_bool.slg", "must be numerical")

    def test_inplace_with_bool(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/inplace_with_bool.slg", "must be numerical")
