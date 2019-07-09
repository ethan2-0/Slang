from typing import List, Dict, Optional, cast
from opcodes import opcodes as ops
import typesys
import parser
import emitter
import opcodes

# This file is peppered with type: ignore comments and unchecked casts
# It's ultimately a huge mess of spaghetti code and I think my effort
# would bebetter spent rewriting it outright than typechecking it.

class Chain:
    def __init__(self, node: parser.Node, scope: "emitter.Scopes", ops: Dict[str, opcodes.Opcode]) -> None:
        self.node = node
        self.scope = scope
        self.ops = ops

    def decide_type(self) -> typesys.AbstractType:
        current_type = self.scope.resolve(self.node[0].data_strict, self.node[0]).type
        for child in self.node.children[1:]:
            if child.i("."):
                if not current_type.is_clazz():
                    raise typesys.TypingError(child, "Cannot perform property access on something that isn't a class")
                current_type = current_type.type_of_property(child[0].data) # type: ignore
                if current_type is None:
                    child[0].compile_error("Cannot find property")
            elif child.i("call"):
                raise ValueError("Can't decide type of a chain that ends in a call (this is a compiler bug)")
            elif child.i("access"):
                if not current_type.is_array():
                    raise typesys.TypingError(child, "Cannot index something that isn't an array")
                current_type = current_type.parent_type # type: ignore
        return current_type

    def access(self, outputreg: "Optional[emitter.RegisterHandle]", emitter: "emitter.MethodEmitter", no_output: bool=False) -> List[opcodes.OpcodeInstance]:
        # TODO: Copy-pasted from assign. Should split the bulk of this out.
        # TODO: Also, do single method calls ever go through this code path?
        current_reg = self.scope.resolve(self.node[0].data_strict, self.node[0])
        res = None
        opcodes = []
        for child in self.node.children[1:]:
            if child.i("."):
                if not current_reg.type.is_clazz():
                    raise typesys.TypingError(child, "Cannot perform property access on something that isn't a class")
                new_reg = self.scope.allocate(cast(typesys.ClazzType, current_reg.type).type_of_property(child[0].data_strict))
                opcodes.append(self.ops["access"].ins(current_reg, child[0].data_strict, new_reg))
                current_reg = new_reg
            elif child.i("call"):
                method_signature = cast(typesys.ClazzType, current_reg.type).method_signature(child[0].data_strict, child[0])
                if len(child.children) - 1 + 1 != len(method_signature.args):
                    child.compile_error("Expected %s arguments, got %s" % (len(method_signature.args), len(child.children) - 1 + 1))
                for param, index in zip(child.children[1:], range(len(child.children) - 1)):
                    inferred_type = emitter.types.decide_type(param, self.scope)
                    declared_type = method_signature.args[index + 1]
                    if not inferred_type.is_assignable_to(declared_type):
                        raise typesys.TypingError(child, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                    reg = self.scope.allocate(inferred_type)
                    opcodes += emitter.emit_expr(param, reg)
                    opcodes.append(self.ops["param"].ins(reg, index))
                new_reg = self.scope.allocate(method_signature.returntype)
                opcodes.append(self.ops["classcall"].ins(current_reg, method_signature, new_reg))
                current_reg = new_reg
            elif child.i("access"):
                if not current_reg.type.is_array():
                    raise typesys.TypingError(child, "Cannot index something that isn't an array")
                index_reg = self.scope.allocate(emitter.types.decide_type(child[0], emitter.scope))
                opcodes += emitter.emit_expr(child[0], index_reg)
                new_reg = self.scope.allocate(cast(typesys.ArrayType, current_reg.type).parent_type)
                opcodes.append(self.ops["arraccess"].ins(current_reg, index_reg, new_reg))
                current_reg = new_reg
        if not no_output:
            opcodes.append(self.ops["mov"].ins(current_reg, outputreg)) # type: ignore
        return opcodes

    def assign(self, resultreg: "emitter.RegisterHandle", emitter: "emitter.MethodEmitter") -> List[opcodes.OpcodeInstance]:
        current_reg = self.scope.resolve(self.node[0].data_strict, self.node[0])
        if len(self.node.children) == 1:
            return [self.ops["mov"].ins(resultreg, current_reg)]
        res = None
        opcodes = []
        for child in self.node.children[1:-1]:
            if child.i("."):
                if not current_reg.type.is_clazz():
                    raise typesys.TypingError(child, "Cannot perform property access on something that isn't a class")
                new_reg = self.scope.allocate(cast(typesys.ClazzType, current_reg.type).type_of_property(child[0].data_strict))
                opcodes.append(self.ops["access"].ins(current_reg, child[0].data_strict, new_reg))
                current_reg = new_reg
            elif child.i("call"):
                method_signature = cast(typesys.ClazzType, current_reg.type).method_signature(child[0].data_strict, child[0])
                if len(child.children) - 1 + 1 != len(method_signature.args):
                    child.compile_error("Expected %s arguments, got %s" % (len(method_signature.args), len(child.children) - 1 + 1))
                for param, index in zip(child.children[1:], range(len(child.children) - 1)):
                    inferred_type = emitter.types.decide_type(param, self.scope) # type: ignore
                    declared_type = method_signature.args[index + 1]
                    if not inferred_type.is_assignable_to(declared_type):
                        raise typesys.TypingError(child, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                    reg = self.scope.allocate(inferred_type)
                    opcodes += emitter.emit_expr(param, reg)
                    opcodes.append(self.ops["param"].ins(reg, index))
                new_reg = self.scope.allocate(method_signature.returntype)
                opcodes.append(self.ops["classcall"].ins(current_reg, method_signature, new_reg))
                current_reg = new_reg
            elif child.i("access"):
                if not current_reg.type.is_array():
                    raise typesys.TypingError(child, "Cannot index something that isn't an array")
                new_reg = self.scope.allocate(cast(typesys.ArrayType, current_reg.type).parent_type)
                index_reg = self.scope.allocate(emitter.types.decide_type(child[0], emitter.scope)) # type: ignore
                opcodes += emitter.emit_expr(child[0], index_reg)
                opcodes.append(self.ops["arraccess"].ins(current_reg, index_reg, new_reg))
                current_reg = new_reg
        # if not self.node.children[-1].i("."):
        #     self.node.children[-1].compile_error("Cannot assign to property chain that doesn't end in property access")
        if self.node.children[-1].i("."):
            opcodes.append(self.ops["assign"].ins(current_reg, self.node.children[-1][0].data_strict, resultreg))
        elif self.node.children[-1].i("access"):
            if not current_reg.type.is_array():
                raise typesys.TypingError(self.node, "Cannot index something that isn't an array")
            if not cast(typesys.ArrayType, current_reg.type).parent_type.is_assignable_from(resultreg.type):
                raise typesys.TypingError(self.node, "Value must be assignable from assignee array: %s not assignable from %s" % (cast(typesys.ArrayType, current_reg.type).parent_type, resultreg.type))
            index_reg = self.scope.allocate(emitter.types.decide_type(self.node.children[-1][0], emitter.scope)) # type: ignore
            if not index_reg.type.is_numerical():
                raise typesys.TypingError(child, "Index into array must be numerical")
            opcodes += emitter.emit_expr(self.node.children[-1][0], index_reg)
            opcodes.append(self.ops["arrassign"].ins(current_reg, index_reg, resultreg))
        else:
            self.node.children[-1].compile_error("Cannot assign to property chain that doesn't end in property access or index")
        return opcodes
