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
        path = util.assert_compile_succeeds("resources/interpreter_command_line_options/returnval.slg")
        assert "0x00000005" in util.interpret(path, "--print-return-value")
        assert "0x00000005" not in util.interpret(path)

    def test_invalid_arg(self):
        assert "Invalid command line argument" in util.interpret("--does-not-exist", expect_fail=True)

class TestInterpreterIfSemantics:
    def test_ifsemantics(self):
        path = util.assert_compile_succeeds("resources/interpreter_ifsemantics/ifsemantics.slg")
        assert "0x00000234" in util.interpret(path, "--print-return-value")

class TestInterpreterMethodCall:
    def test_methodcall(self):
        path = util.assert_compile_succeeds("resources/interpreter_methodcall/methodcall.slg")
        assert "0x0000002a" in util.interpret(path, "--print-return-value")

class TestInterpreterMinimal:
    def test_minimal(self):
        path = util.assert_compile_succeeds("resources/interpreter_minimal/minimal.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

class TestInterpreterRecursion:
    def test_recursion(self):
        path = util.assert_compile_succeeds("resources/interpreter_recursion/recursion.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

class TestIntegrationInclude:
    def test_include(self):
        lib = util.assert_compile_succeeds("resources/integration_include/lib.slg")
        main = util.assert_compile_succeeds("resources/integration_include/main.slg", include=[lib])
        assert "0x00000003" in util.interpret(lib, main, "--print-return-value")

class TestIntegrationKitchenSink:
    def test_kitchensink(self):
        path = util.assert_compile_succeeds("resources/integration_kitchensink/kitchensink.slg")
        assert "0x00100000" in util.interpret(path, "--print-return-value")

    def test_linkedlist(self):
        path = util.assert_compile_succeeds("resources/integration_kitchensink/linkedlist.slg")
        assert "0x0000000a" in util.interpret(path, "--print-return-value")

    def test_forloop(self):
        path = util.assert_compile_succeeds("resources/integration_kitchensink/forloop.slg")
        assert "0x0000000a" in util.interpret(path, "--print-return-value")

    def test_whileloop(self):
        path = util.assert_compile_succeeds("resources/integration_kitchensink/whileloop.slg")
        assert "0x00000032" in util.interpret(path, "--print-return-value")

    def test_nested_for_loop(self):
        path = util.assert_compile_succeeds("resources/integration_kitchensink/nestedforloop.slg")
        assert "0x000007e9" in util.interpret(path, "--print-return-value")

class TestClazzesBasic:
    def test_clazz(self):
        util.assert_compile_succeeds("resources/classes_basic/class.slg")

    def test_clazz_interpreter(self):
        path = util.assert_compile_succeeds("resources/classes_basic/class.slg")
        util.interpret(path)

    def test_clazz_new_unknown_type(self):
        util.assert_compile_fails("resources/classes_basic/class_new_unknown_type.slg", message="Could not resolve type")

    def test_clazz_new_primitive_type(self):
        util.assert_compile_fails("resources/classes_basic/class_new_primitive_type.slg", message="Type :bool is not a class")

    def test_interreferencing_clazzes(self):
        util.assert_compile_succeeds("resources/classes_basic/interreferencing_classes.slg")

    def test_interreferencing_clazzes_interpreter(self):
        path = util.assert_compile_succeeds("resources/classes_basic/interreferencing_classes.slg")
        util.interpret(path)

    def test_property_access(self):
        util.assert_compile_succeeds("resources/classes_basic/class_property_access.slg")

    def test_property_access_interpreter(self):
        path = util.assert_compile_succeeds("resources/classes_basic/class_property_access.slg")
        util.interpret(path)

    def test_property_access_advanced(self):
        path = util.assert_compile_succeeds("resources/classes_basic/class_property_access_advanced.slg")
        util.interpret(path)

    def test_clazz_nullptr(self):
        # This isn't permanent behavior by any stretch, but I'll codify it anyway.
        path = util.assert_compile_succeeds("resources/classes_basic/class_nullptr.slg")
        assert "Null pointer" in util.interpret(path, expect_fail=True)

    def test_clazz_call(self):
        path = util.assert_compile_succeeds("resources/classes_basic/class_call.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_extend_included(self):
        path = util.assert_compile_succeeds("resources/classes_basic/extend_included.slg")
        util.interpret(path)

class TestClassesMethods:
    def test_classes_method(self):
        path = util.assert_compile_succeeds("resources/classes_methods/class_method.slg")
        util.interpret(path)

    def test_method_and_property_name(self):
        util.assert_compile_fails("resources/classes_methods/method_and_property_name.slg", "Class property and method with the same name")

class TestMethodTypes:
    def test_method_types(self):
        util.assert_compile_succeeds("resources/types_method_types/method_types.slg")

    def test_wrong_parameter_type(self):
        util.assert_compile_fails("resources/types_method_types/wrong_parameter_type.slg", "not assignable")

    def test_wrong_return_type(self):
        util.assert_compile_fails("resources/types_method_types/wrong_return_type.slg", "Return type mismatch")

    def test_return_nothing_on_nonvoid(self):
        util.assert_compile_fails("resources/types_method_types/return_nothing_on_nonvoid.slg", "Can't return nothing from a method unless the method returns void")

    def test_return_null_on_nonvoid(self):
        path = util.assert_compile_succeeds("resources/types_method_types/return_null_on_nonvoid.slg")
        assert "0x00000000" in util.interpret(path, "--print-return-value")

    def test_return_something_on_void(self):
        util.assert_compile_fails("resources/types_method_types/return_something_on_void.slg", "Return type mismatch")

    def test_no_return_on_void(self):
        path = util.assert_compile_succeeds("resources/types_method_types/no_return_on_void.slg")
        assert "0x00000000" in util.interpret(path, "--print-return-value")

    def test_no_return_on_nonvoid(self):
        path = util.assert_compile_fails("resources/types_method_types/no_return_on_nonvoid.slg", "Non-void method might not return")

class TestOperatorTypes:
    def test_operator_types(self):
        util.assert_compile_succeeds("resources/types_operator_types/operator_types.slg")

class TestWrongExplicitType:
    def test_bool_as_int(self):
        util.assert_compile_fails("resources/types_wrong_explicit_type/bool_as_int.slg", "not assignable")

    def test_int_as_bool(self):
        util.assert_compile_fails("resources/types_wrong_explicit_type/bool_as_int.slg", "not assignable")

class TestTypesNull:
    def test_types_null(self):
        path = util.assert_compile_succeeds("resources/types_null/types_null.slg")
        util.interpret(path)

class TestTypesWrongOperatorTypes:
    def test_arithmetic_needs_int(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/arithmetic_needs_int.slg", "is not numerical")

    def test_arithmetic_oneint(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/arithmetic_oneint.slg", "is not numerical")

    def test_equality_makes_boolean(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/equality_makes_boolean.slg", "is not numerical")

    def test_equality_needs_assignable(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/equality_needs_assignable.slg", "Incomparable types")

    def test_if_needs_boolean(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/if_needs_boolean.slg", "must be boolean")

    def test_increment_bool(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/increment_bool.slg", "to increment something that isn't numerical")

    def test_inplace_with_bool(self):
        util.assert_compile_fails("resources/types_wrong_operator_types/inplace_with_bool.slg", "must be numerical")

class TestArrays:
    def test_arrays_basic(self):
        path = util.assert_compile_succeeds("resources/arrays/basic.slg")
        assert "0x0000000f" in util.interpret(path, "--print-return-value")

    def test_arrays_misc(self):
        path = util.assert_compile_succeeds("resources/arrays/misc.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_arrays_out_of_bounds(self):
        path = util.assert_compile_succeeds("resources/arrays/out_of_bounds.slg")
        assert "Array index out of bounds" in util.interpret(path, expect_fail=True)

    def test_arrays_2d(self):
        path = util.assert_compile_succeeds("resources/arrays/test_2d.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_arrays_zerofill(self):
        path = util.assert_compile_succeeds("resources/arrays/zerofill.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_arrays_literal_inference_inheritance(self):
        path = util.assert_compile_succeeds("resources/arrays/literal_inference_inheritance.slg")
        assert "0x00000003" in util.interpret(path, "--print-return-value")

    def test_arrays_literal_of_nulls(self):
        util.assert_compile_fails("resources/arrays/literla_of_nulls.slg", "Cannot have an array literal comprising only null values, use arbitrary-length instantiation syntax instead")

    def test_arrays_no_such_member_type(self):
        util.assert_compile_fails("resources/arrays/no_such_member_type.slg", "Could not resolve type")

class TestInheritance:
    def test_inheritance_basic(self):
        path = util.assert_compile_succeeds("resources/inheritance/basic.slg")
        assert "0x0000000c" in util.interpret(path, "--print-return-value")

    def test_inheritance_polymorphism(self):
        path = util.assert_compile_succeeds("resources/inheritance/polymorphism.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_inheritance_polymorphism(self):
        path = util.assert_compile_succeeds("resources/inheritance/polymorphism_advanced.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_inheritance_overriding_missing_override(self):
        path = util.assert_compile_fails("resources/inheritance/overriding_missing_override.slg", message="overriding rules")

    def test_inheritance_overriding_override_on_parent(self):
        path = util.assert_compile_fails("resources/inheritance/overriding_override_on_parent.slg", message="overriding rules")

    def test_inheritance_overriding_no_corresponding(self):
        path = util.assert_compile_fails("resources/inheritance/overriding_no_corresponding.slg", message="overriding rules")

    def test_inheritance_overriding_mismatched_arguments(self):
        path = util.assert_compile_fails("resources/inheritance/overriding_mismatched_arguments.slg", message="overriding rules")

    def test_inheritance_duplicate_field_same_type(self):
        path = util.assert_compile_fails("resources/inheritance/duplicate_field_same_type.slg", message="Duplicate field in both child and parent class named 'number'")

    def test_inheritance_duplicate_field_different_type(self):
        path = util.assert_compile_fails("resources/inheritance/duplicate_field_different_type.slg", message="Duplicate field in both child and parent class named 'number'")

class TestMethods:
    def test_methods_single_call(self):
        path = util.assert_compile_succeeds("resources/methods/single_call.slg")
        assert "0x00000005" in util.interpret(path, "--print-return-value")

    def test_inheritance_duplicate_field_different_type(self):
        path = util.assert_compile_fails("resources/methods/override_outside_class.slg", message="Can't both be an override method and not have a containing class")

    def test_nested_calls(self):
        path = util.assert_compile_succeeds("resources/methods/single_call.slg")
        util.interpret(path)

    def test_methods_dot_name(self):
        path = util.assert_compile_succeeds("resources/methods/dot_name.slg")
        util.interpret(path)

class TestNamespaces:
    def test_basic(self):
        lib = util.assert_compile_succeeds("resources/namespaces/basic_lib.slg")
        main = util.assert_compile_succeeds("resources/namespaces/basic_main.slg", include=[lib])
        assert "0x0000000a" in util.interpret(lib, main, "--print-return-value")

    def test_method(self):
        lib = util.assert_compile_succeeds("resources/namespaces/method_lib.slg")
        main = util.assert_compile_succeeds("resources/namespaces/method_main.slg", include=[lib])
        assert "0x00000005" in util.interpret(lib, main, "--print-return-value")

class TestParser:
    def test_underscore_in_name(self):
        path = util.assert_compile_succeeds("resources/parser/underscore_in_name.slg")
        assert "0x00000005" in util.interpret(path, "--print-return-value")

    def test_escapes(self):
        path = util.assert_compile_succeeds("resources/parser/escapes.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_string_literal(self):
        path = util.assert_compile_succeeds("resources/parser/string_literal.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_call_expr_in_parens(self):
        path = util.assert_compile_succeeds("resources/parser/call_expr_in_parens.slg")
        util.interpret(path)

    def test_no_semicolons(self):
        path = util.assert_compile_succeeds("resources/parser/no_semicolons.slg")
        util.interpret(path)

class TestStdlib:
    def test_basic(self):
        path = util.assert_compile_succeeds("resources/stdlib/basic.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_string_basic(self):
        path = util.assert_compile_succeeds("resources/stdlib/string_basic.slg")
        assert "0x00000001" in util.interpret(path, "--print-return-value")

    def test_conversion(self):
        path = util.assert_compile_succeeds("resources/stdlib/conversion.slg")
        util.interpret(path)

    def test_die(self):
        path = util.assert_compile_succeeds("resources/stdlib/die.slg")
        assert "This is the fatal message." in util.interpret(path, expect_fail=True)

    def test_strconcat_all(self):
        path = util.assert_compile_succeeds("resources/stdlib/strconcat_all.slg")
        util.interpret(path)

    def test_strjoin(self):
        path = util.assert_compile_succeeds("resources/stdlib/strjoin.slg")
        util.interpret(path)

    def test_streams(self):
        path = util.assert_compile_succeeds("resources/stdlib/streams.slg")
        util.interpret(path)

    def test_stdin_input_stream(self):
        path = util.assert_compile_succeeds("resources/stdlib/stdin_input_stream.slg")
        assert "3\n8" in util.interpret(path, stdin=b"1\n2\n3\n5")

    def test_string_advanced(self):
        path = util.assert_compile_succeeds("resources/stdlib/string_advanced.slg")
        util.interpret(path)

    def test_numerical(self):
        path = util.assert_compile_succeeds("resources/stdlib/numerical.slg")
        util.interpret(path)

    def test_array_methods(self):
        path = util.assert_compile_succeeds("resources/stdlib/array_methods.slg")
        util.interpret(path)

class TestReplacedMethods:
    def test_print(self):
        path = util.assert_compile_succeeds("resources/replaced_methods/print.slg")
        assert "Printed" in util.interpret(path)

    def test_exit(self):
        path = util.assert_compile_succeeds("resources/replaced_methods/exit.slg")
        util.interpret(path, expect_fail=True)

class TestInstanceofCasts:
    def test_instanceof(self):
        path = util.assert_compile_succeeds("resources/types_instanceof_casts/instanceof.slg", no_warnings=False)
        util.interpret(path)

    def test_instanceof_incompatible_types(self):
        util.assert_compile_succeeds(
            "resources/types_instanceof_casts/instanceof_incompatible_types.slg",
            message=[
                ":StringSubclass always instanceof :stdlib.String",
                ":stdlib.String always instanceof :stdlib.String",
                "there cannot exist any :stdlib.String instanceof :stdlib.InputStream"
            ],
            no_warnings=False
        )

    def test_impossible_cast(self):
        util.assert_compile_succeeds(
            "resources/types_instanceof_casts/impossible_cast.slg",
            message=[
                "Impossible cast: LHS :Class1 is incompatible with RHS :Class2"
            ],
            no_warnings=False
        )

    def test_array_instanceof(self):
        path = util.assert_compile_succeeds("resources/types_instanceof_casts/array_instanceof.slg", no_warnings=False)
        util.interpret(path)

class TestInterpreterDuplicateMethodOrClass:
    def test_duplicate_class(self):
        main_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/main.slg")
        duplicate_class_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/duplicate_class.slg")
        assert "Duplicate class name: 'DuplicateClass'" in util.interpret(main_path, duplicate_class_path, expect_fail=True)

    def test_duplicate_method(self):
        main_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/main.slg")
        duplicate_method_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/duplicate_method.slg")
        assert "Duplicate method name and containing class: 'duplicate_method'" in util.interpret(main_path, duplicate_method_path, expect_fail=True)

    def test_not_duplicate_method(self):
        main_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/main.slg")
        not_duplicate_method_path = util.assert_compile_succeeds("resources/interpreter_duplicate_method_or_class/not_duplicate_method.slg")
        util.interpret(main_path, not_duplicate_method_path, expect_fail=False)

class TestStaticVariables:
    def test_basic(self):
        path = util.assert_compile_succeeds("resources/static_variables/basic.slg", no_warnings=False)
        util.interpret(path)

    def test_initializer(self):
        path = util.assert_compile_succeeds("resources/static_variables/initializer.slg", no_warnings=False)
        util.interpret(path)

    def test_initializer_garbage_collection(self):
        path = util.assert_compile_succeeds("resources/static_variables/initializer_garbage_collection.slg", no_warnings=False)
        assert "Garbage collecting" in util.interpret(path, "--gc-verbose")

class TestObject:
    def test_object_extension(self):
        path = util.assert_compile_succeeds("resources/object/object_extension.slg", no_warnings=False)
        util.interpret(path)

class TestSupercalls:
    def test_supercalls(self):
        path = util.assert_compile_succeeds("resources/supercalls/supercalls.slg")
        util.interpret(path)

    def test_supercall_statement(self):
        path = util.assert_compile_succeeds("resources/supercalls/supercall_statement.slg")

    def test_assign_to_supercall_statement(self):
        path = util.assert_compile_fails("resources/supercalls/assign_to_supercall_statement.slg", message="Cannot assign to a method call")

class TestAbstractClasses:
    def test_basic(self):
        path = util.assert_compile_succeeds("resources/abstract_classes/basic.slg")
        util.interpret(path)

    def test_instantiate_abstract_class(self):
        path = util.assert_compile_fails("resources/abstract_classes/instantiate_abstract_class.slg", message="Cannot instantiate an abstract class")

    def test_dont_override_abstract_method(self):
        path = util.assert_compile_fails("resources/abstract_classes/dont_override_abstract_method.slg", message="is not overridden")

    def test_supercall_to_abstract_method(self):
        path = util.assert_compile_fails("resources/abstract_classes/supercall_to_abstract_method.slg", message="Attempt to perform super call to abstract method")

class TestInterfaces:
    def test_basic(self):
        path = util.assert_compile_succeeds("resources/interfaces/basic.slg")
        util.interpret(path)

    def test_multiple_interfaces(self):
        path = util.assert_compile_succeeds("resources/interfaces/multiple_interfaces.slg")
        util.interpret(path)

    def test_method_not_implemented(self):
        path = util.assert_compile_fails("resources/interfaces/method_not_implemented.slg", message="Method 'method2' of interface 'Interface' is not implemented by class 'Class'")

    def test_wrong_return_type(self):
        path = util.assert_compile_fails("resources/interfaces/wrong_return_type.slg", message="Method 'method' of class 'Class' has return type :stdlib.String that isn't assignable to return type :int of the corresponding method from interface Interface")

    def test_wrong_return_type_void(self):
        path = util.assert_compile_fails("resources/interfaces/wrong_return_type_void.slg", message="Method 'method' of class 'Class' has return type :void that isn't assignable to return type :int of the corresponding method from interface Interface")

    def test_inherit_interface_implementations(self):
        path = util.assert_compile_succeeds("resources/interfaces/inherit_interface_implementations.slg")
        util.interpret(path)

    def test_instanceof(self):
        path = util.assert_compile_succeeds("resources/interfaces/instanceof.slg", no_warnings=False)
        util.interpret(path)

class TestGenericMethods:
    def test_basic(self):
        path = util.assert_compile_succeeds("resources/generic_methods/basic.slg")
        util.interpret(path)

    def test_generic_instanceof_object(self):
        path = util.assert_compile_succeeds("resources/generic_methods/generic_instanceof_object.slg")
        util.interpret(path)

    def test_generic_instanceof_primitive(self):
        path = util.assert_compile_succeeds("resources/generic_methods/generic_instanceof_primitive.slg")
        util.interpret(path)

    def test_generic_recursion(self):
        path = util.assert_compile_succeeds("resources/generic_methods/generic_recursion.slg")
        util.interpret(path)

    def test_reification_reuse(self):
        path = util.assert_compile_succeeds("resources/generic_methods/reification_reuse.slg")
        # TODO: Figure out some way of calibrating this or do it based on memory use or something
        util.interpret(path, timeout=0.5)

    def test_generic_instanceof_type_parameter(self):
        path = util.assert_compile_succeeds("resources/generic_methods/generic_instanceof_type_parameter.slg")
        util.interpret(path)

    def test_generic_bounds(self):
        path = util.assert_compile_succeeds("resources/generic_methods/generic_bounds.slg")
        util.interpret(path)

    def test_unsatisfied_extends_bound(self):
        path = util.assert_compile_fails("resources/generic_methods/unsatisfied_extends_bound.slg", message="Could not reify generic method: ':stdlib.Object' does not satisfy ':T extends stdlib.String'")

    def test_unsatisfied_implements_bound(self):
        path = util.assert_compile_fails("resources/generic_methods/unsatisfied_implements_bound.slg", message="Could not reify generic method: ':stdlib.Object' does not satisfy ':T implements stdlib.Hashable, stdlib.ToString, Interface'")

    def test_generic_member_method(self):
        path = util.assert_compile_fails("resources/generic_methods/generic_member_method.slg", message="Member methods cannot have type arguments")

class TestGenericClasses:
    def test_basic(self):
        path = util.assert_compile_succeeds("resources/generic_classes/basic.slg")
        util.interpret(path)

    def test_bounds(self):
        path = util.assert_compile_succeeds("resources/generic_classes/bounds.slg")
        util.interpret(path)

    def test_type_annotation_raw_type(self):
        util.assert_compile_fails("resources/generic_classes/type_annotation_raw_type.slg", message="Cannot use raw types directly")

    def test_instantiate_raw_type(self):
        util.assert_compile_fails("resources/generic_classes/instantiate_raw_type.slg", message="Expected type arguments")

    def test_not_assignable(self):
        util.assert_compile_fails("resources/generic_classes/not_assignable.slg", message="is not assignable to")

    def test_inherit_from_specialization(self):
        path = util.assert_compile_succeeds("resources/generic_classes/inherit_from_specialization.slg")
        util.interpret(path)

    def test_inherit_from_generic(self):
        path = util.assert_compile_succeeds("resources/generic_classes/inherit_from_generic.slg")
        util.interpret(path)

    def test_include(self):
        lib = util.assert_compile_succeeds("resources/generic_classes/include_lib.slg")
        main = util.assert_compile_succeeds("resources/generic_classes/include_main.slg", include=[lib])
        util.interpret(lib, main)
        util.interpret(main, lib)
