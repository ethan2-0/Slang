from opcodes import opcodes as ops

class Chain:
    def __init__(self, node, scope, ops):
        self.node = node
        self.scope = scope
        self.ops = ops

    def decide_type(self):
        current_type = self.scope.resolve(self.node[0].data, self.node[0]).type
        for child in self.node.children[1:]:
            if child.i("."):
                if not current_type.is_clazz():
                    raise typesys.TypingError(child, "Cannot perform property access on something that isn't a class")
                current_type = current_type.type_of_property(child[0].data)
            elif child.i("call"):
                raise ValueError("Can't decide type of a chain that ends in a call (this is a compiler bug)")
            elif child.i("access"):
                if not current_type.is_array():
                    raise typesys.TypingError(child, "Cannot index something that isn't an array")
                current_type = current_type.parent_type
        return current_type

    def access(self, outputreg, emitter, no_output=False):
        # TODO: Copy-pasted from assign. Should split the bulk of this out.
        # TODO: Also, do single method calls ever go through this code path?
        current_reg = self.scope.resolve(self.node[0].data, self.node[0])
        res = None
        opcodes = []
        for child in self.node.children[1:]:
            if child.i("."):
                if not current_reg.type.is_clazz():
                    raise typesys.TypingError(child, "Cannot perform property access on something that isn't a class")
                new_reg = self.scope.allocate(current_reg.type.type_of_property(child[0].data))
                opcodes.append(self.ops["access"].ins(current_reg, child[0].data, new_reg))
                current_reg = new_reg
            elif child.i("call"):
                method_signature = current_reg.type.method_signature(child[0].data, child[0])
                if len(child.children) - 1 + 1 != len(method_signature.args):
                    node.compile_error("Expected %s arguments, got %s" % (len(method_signature.args), len(node.children) - 1 + 1))
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
                new_reg = self.scope.allocate(current_reg.type.parent_type)
                opcodes.append(self.ops["arraccess"].ins(current_reg, index_reg, new_reg))
                current_reg = new_reg
        if not no_output:
            opcodes.append(self.ops["mov"].ins(current_reg, outputreg))
        return opcodes

    def assign(self, resultreg, emitter):
        current_reg = self.scope.resolve(self.node[0].data, self.node[0])
        if len(self.node.children) == 1:
            return [self.ops["mov"].ins(resultreg, current_reg)]
        res = None
        opcodes = []
        for child in self.node.children[1:-1]:
            if child.i("."):
                if not current_reg.type.is_clazz():
                    raise typesys.TypingError(child, "Cannot perform property access on something that isn't a class")
                new_reg = self.scope.allocate(current_reg.type.type_of_property(child[0].data))
                opcodes.append(self.ops["access"].ins(current_reg, child[0].data, new_reg))
                current_reg = new_reg
            elif child.i("call"):
                method_signature = current_reg.type.method_signature(child[0].data, child[0])
                if len(child.children) - 1 + 1 != len(method_signature.args):
                    node.compile_error("Expected %s arguments, got %s" % (len(method_signature.args), len(node.children) - 1 + 1))
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
                new_reg = self.scope.allocate(current_reg.type.parent_type)
                index_reg = self.scope.allocate(emitter.types.decide_type(child[0], emitter.scope))
                opcodes += emitter.emit_expr(child[0], index_reg)
                opcodes.append(self.ops["arraccess"].ins(current_reg, index_reg, new_reg))
                current_reg = new_reg
        # if not self.node.children[-1].i("."):
        #     self.node.children[-1].compile_error("Cannot assign to property chain that doesn't end in property access")
        if self.node.children[-1].i("."):
            opcodes.append(self.ops["assign"].ins(current_reg, self.node.children[-1][0].data, resultreg))
        elif self.node.children[-1].i("access"):
            if not current_reg.type.is_array():
                raise typesys.TypingError(child, "Cannot index something that isn't an array")
            if not current_reg.type.parent_type.is_assignable_to(resultreg.type):
                raise typesys.TypingError("Value must be assignable to assignee array: %s not assignable to %s" % (current_reg.type.parent_type, resultreg.type))
            index_reg = self.scope.allocate(emitter.types.decide_type(self.node.children[-1][0], emitter.scope))
            if not index_reg.type.is_numerical():
                raise typesys.TypingError(child, "Index into array must be numerical")
            opcodes += emitter.emit_expr(self.node.children[-1][0], index_reg)
            opcodes.append(self.ops["arrassign"].ins(current_reg, index_reg, resultreg))
        else:
            self.node.children[-1].compile_error("Cannot assign to property chain that doesn't end in property access or index")
        return opcodes
