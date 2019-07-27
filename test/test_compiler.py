import os
import os.path
import sys

# Pytest maintains the same interpreter instance throughout all test suites as
# long as it's in the same invocation of pytest. So we need to clear out `util`
# (from ../compiler) so we can use our own `util`.
try:
    del sys.modules["util"]
except AttributeError:
    pass

import util

os.chdir(os.path.dirname(__file__))

class TestInterpreterCommandLineArgs:
    def test_command_line_arguments_returnval(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_command_line_options/returnval.slg")
        assert "0x00000005" in util.interpret(path, "--print-return-value")
        assert "0x00000005" not in util.interpret(path)

    def test_invalid_arg(self):
        util.clean_tmp()
        assert "Invalid command line argument" in util.interpret("--does-not-exist", expect_fail=True)

class TestInterpreterIfSemantics:
    def test_ifsemantics(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_ifsemantics/ifsemantics.slg")
        assert "0x00000234" in util.interpret(path, "--print-return-value")

class TestInterpreterMethodCall:
    def test_methodcall(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_methodcall/methodcall.slg")
        assert "0x0000002a" in util.interpret(path, "--print-return-value")

class TestInterpreterMinimal:
    def test_minimal(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_minimal/minimal.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

class TestInterpreterRecursion:
    def test_recursion(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/interpreter_recursion/recursion.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

class TestIntegrationInclude:
    def test_include(self):
        util.clean_tmp()
        lib = util.assert_compile_succeeds("resources/integration_include/lib.slg")
        main = util.assert_compile_succeeds("resources/integration_include/main.slg", include=[lib])
        assert "0x00000003" in util.interpret(lib, main, "--print-return-value")

class TestIntegrationKitchenSink:
    def test_kitchensink(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/integration_kitchensink/kitchensink.slg")
        assert "0x00100000" in util.interpret(path, "--print-return-value")

    def test_linkedlist(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/integration_kitchensink/linkedlist.slg")
        assert "0x0000000a" in util.interpret(path, "--print-return-value")

    def test_forloop(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/integration_kitchensink/forloop.slg")
        assert "0x0000000a" in util.interpret(path, "--print-return-value")

    def test_whileloop(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/integration_kitchensink/whileloop.slg")
        assert "0x00000032" in util.interpret(path, "--print-return-value")

    def test_nested_for_loop(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/integration_kitchensink/nestedforloop.slg")
        assert "0x000007e9" in util.interpret(path, "--print-return-value")

class TestClazzesBasic:
    def test_clazz(self):
        util.clean_tmp()
        util.assert_compile_succeeds("resources/classes_basic/class.slg")

    def test_clazz_interpreter(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_basic/class.slg")
        util.interpret(path)

    def test_clazz_new_unknown_type(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/classes_basic/class_new_unknown_type.slg")

    def test_clazz_new_primitive_type(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/classes_basic/class_new_primitive_type.slg", message="Type :bool is not a class")

    def test_interreferencing_clazzes(self):
        util.clean_tmp()
        util.assert_compile_succeeds("resources/classes_basic/interreferencing_classes.slg")

    def test_interreferencing_clazzes_interpreter(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_basic/interreferencing_classes.slg")
        util.interpret(path)

    def test_property_access(self):
        util.clean_tmp()
        util.assert_compile_succeeds("resources/classes_basic/class_property_access.slg")

    def test_property_access_interpreter(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_basic/class_property_access.slg")
        util.interpret(path)

    def test_property_access_advanced(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_basic/class_property_access_advanced.slg")
        util.interpret(path)

    def test_clazz_nullptr(self):
        # This isn't permanent behavior by any stretch, but I'll codify it anyway.
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_basic/class_nullptr.slg")
        assert "Null pointer" in util.interpret(path, expect_fail=True)

    def test_clazz_call(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_basic/class_call.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

class TestClassesMethods:
    def test_classes_method(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/classes_methods/class_method.slg")
        util.interpret(path)

    def test_method_and_property_name(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/classes_methods/method_and_property_name.slg", "Class property and method with the same name")

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

    def test_return_nothing_on_nonvoid(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_method_types/return_nothing_on_nonvoid.slg", "Can't return nothing from a method unless the method returns void")

    def test_return_null_on_nonvoid(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/types_method_types/return_null_on_nonvoid.slg")
        assert "0x00000000" in util.interpret(path, "--print-return-value")

    def test_return_something_on_void(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_method_types/return_something_on_void.slg", "Return type mismatch")

    def test_no_return_on_void(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/types_method_types/no_return_on_void.slg")
        assert "0x00000000" in util.interpret(path, "--print-return-value")

    def test_no_return_on_nonvoid(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/types_method_types/no_return_on_nonvoid.slg", "Non-void method might not return")

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

class TestTypesNull:
    def test_types_null(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/types_null/types_null.slg")
        util.interpret(path)

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
        util.assert_compile_fails("resources/types_wrong_operator_types/increment_bool.slg", "to increment something that isn't numerical")

    def test_inplace_with_bool(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/types_wrong_operator_types/inplace_with_bool.slg", "must be numerical")

class TestArrays:
    def test_arrays_basic(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/arrays/basic.slg")
        assert "0x0000000f" in util.interpret(path, "--print-return-value")

    def test_arrays_misc(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/arrays/misc.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_arrays_out_of_bounds(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/arrays/out_of_bounds.slg")
        assert "Array index out of bounds" in util.interpret(path, expect_fail=True)

    def test_arrays_2d(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/arrays/test_2d.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_arrays_zerofill(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/arrays/zerofill.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_arrays_literal_inference_inheritance(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/arrays/literal_inference_inheritance.slg")
        assert "0x00000003" in util.interpret(path, "--print-return-value")

    def test_arrays_literal_of_nulls(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/arrays/literla_of_nulls.slg", "Cannot have an array literal comprising only null values, use arbitrary-length instantiation syntax instead")

    def test_arrays_no_such_member_type(self):
        util.clean_tmp()
        util.assert_compile_fails("resources/arrays/no_such_member_type.slg", "Could not resolve type")

class TestInheritance:
    def test_inheritance_basic(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/inheritance/basic.slg")
        assert "0x0000000c" in util.interpret(path, "--print-return-value")

    def test_inheritance_polymorphism(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/inheritance/polymorphism.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_inheritance_polymorphism(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/inheritance/polymorphism_advanced.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_inheritance_overriding_missing_override(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/inheritance/overriding_missing_override.slg", message="overriding rules")

    def test_inheritance_overriding_override_on_parent(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/inheritance/overriding_override_on_parent.slg", message="overriding rules")

    def test_inheritance_overriding_no_corresponding(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/inheritance/overriding_no_corresponding.slg", message="overriding rules")

    def test_inheritance_overriding_mismatched_arguments(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/inheritance/overriding_mismatched_arguments.slg", message="overriding rules")

    def test_inheritance_duplicate_field_same_type(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/inheritance/duplicate_field_same_type.slg", message="Duplicate field in both child and parent class named 'number'")

    def test_inheritance_duplicate_field_different_type(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/inheritance/duplicate_field_different_type.slg", message="Duplicate field in both child and parent class named 'number'")

class TestMethods:
    def test_methods_single_call(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/methods/single_call.slg")
        assert "0x00000005" in util.interpret(path, "--print-return-value")

    def test_inheritance_duplicate_field_different_type(self):
        util.clean_tmp()
        path = util.assert_compile_fails("resources/methods/override_outside_class.slg", message="Can't both be an override method and not have a containing class")

    def test_nested_calls(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/methods/single_call.slg")
        util.interpret(path)

    def test_methods_dot_name(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/methods/dot_name.slg")
        util.interpret(path)

class TestNamespaces:
    def test_basic(self):
        util.clean_tmp()
        lib = util.assert_compile_succeeds("resources/namespaces/basic_lib.slg")
        main = util.assert_compile_succeeds("resources/namespaces/basic_main.slg", include=[lib])
        assert "0x0000000a" in util.interpret(lib, main, "--print-return-value")

    def test_method(self):
        util.clean_tmp()
        lib = util.assert_compile_succeeds("resources/namespaces/method_lib.slg")
        main = util.assert_compile_succeeds("resources/namespaces/method_main.slg", include=[lib])
        assert "0x00000005" in util.interpret(lib, main, "--print-return-value")

class TestParser:
    def test_underscore_in_name(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/parser/underscore_in_name.slg")
        assert "0x00000005" in util.interpret(path, "--print-return-value")

    def test_escapes(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/parser/escapes.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_string_literal(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/parser/string_literal.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_call_expr_in_parens(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/parser/call_expr_in_parens.slg")
        util.interpret(path)

    def test_no_semicolons(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/parser/no_semicolons.slg")
        util.interpret(path)

class TestStdlib:
    def test_basic(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/basic.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_string_basic(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/string_basic.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_conversion(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/conversion.slg")
        util.interpret(path)

    def test_die(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/die.slg")
        assert "This is the fatal message." in util.interpret(path, expect_fail=True)

    def test_strconcat_all(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/strconcat_all.slg")
        util.interpret(path)

    def test_strjoin(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/strjoin.slg")
        util.interpret(path)

    def test_streams(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/streams.slg")
        util.interpret(path)

    def test_stdin_input_stream(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/stdin_input_stream.slg")
        assert "3\n8" in util.interpret(path, stdin=b"1\n2\n3\n5")

    def test_string_advanced(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/stdlib/string_advanced.slg")
        util.interpret(path)


class TestReplacedMethods:
    def test_print(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/replaced_methods/print.slg")
        assert "Printed" in util.interpret(path)

    def test_exit(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/replaced_methods/exit.slg")
        util.interpret(path, expect_fail=True)

class TestInstanceofCasts:
    def test_instanceof(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/types_instanceof_casts/instanceof.slg", no_warnings=False)
        util.interpret(path)

    def test_instanceof_incompatible_types(self):
        util.clean_tmp()
        util.assert_compile_succeeds(
            "resources/types_instanceof_casts/instanceof_incompatible_types.slg",
            message=[
                ":StringSubclass always instanceof :stdlib.String",
                ":stdlib.String always instanceof :stdlib.String",
                "there cannot exist any :stdlib.String instanceof :stdlib.InputStream"
            ],
            no_warnings=False
        )

class TestInterpreterDuplicateMethodOrClass:
    def test_duplicate_class(self):
        util.clean_tmp()
        main_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/main.slg")
        duplicate_class_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/duplicate_class.slg")
        assert "Duplicate class name: 'DuplicateClass'" in util.interpret(main_path, duplicate_class_path, expect_fail=True)

    def test_duplicate_method(self):
        util.clean_tmp()
        main_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/main.slg")
        duplicate_method_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/duplicate_method.slg")
        assert "Duplicate method name and containing class: 'duplicate_method'" in util.interpret(main_path, duplicate_method_path, expect_fail=True)

    def test_not_duplicate_method(self):
        util.clean_tmp()
        main_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/main.slg")
        not_duplicate_method_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/not_duplicate_method.slg")
        util.interpret(main_path, not_duplicate_method_path, expect_fail=False)

class TestStaticVariables:
    def test_basic(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/static_variables/basic.slg", no_warnings=False)
        util.interpret(path)

    def test_initializer(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/static_variables/initializer.slg", no_warnings=False)
        util.interpret(path)

    def test_initializer_garbage_collection(self):
        util.clean_tmp()
        path = util.assert_compile_succeeds("resources/static_variables/initializer_garbage_collection.slg", no_warnings=False)
        assert "Garbage collecting" in util.interpret(path, "--gc-verbose")
