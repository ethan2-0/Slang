from opcodes import opcodes as ops
import header
import typesys
import parser
import claims as clms
from chain import Chain

class RegisterHandle:
    def __init__(self, id, type):
        self.id = id
        self.type = type

    def __repr__(self):
        return "RegisterHandle(id=%s, type=%s)" % (self.id, self.type)

    def __str__(self):
        return "$%s" % self.id

    def looks_like_register_handle(self):
        pass

def flatten_ident_nodes(parent):
    return ".".join([child.data for child in parent])

def annotate(opcodes, annotation):
    if type(opcodes) is list:
        if len(opcodes) > 0:
            opcodes[0].annotate(annotation)
        return opcodes
    else:
        opcodes.annotate(annotation)
        return opcodes

class Scopes:
    def __init__(self):
        self.locals = [dict()]
        self.register_ids = 0
        self.registers = []

    def push(self):
        self.locals.append(dict())

    def pop(self):
        self.locals.pop()

    def allocate(self, type=None):
        ret = RegisterHandle(self.register_ids, type)
        self.registers.append(ret)
        self.register_ids += 1
        return ret

    def resolve(self, key, node):
        for local in reversed(self.locals):
            if key in local:
                return local[key]
        node.compile_error("Referenced local before declaration: %s" % key)

    def assign(self, key, register, node):
        for local in reversed(self.locals):
            if key in local:
                local[key] = register
                return
        node.compile_error("Assigned local before declaration: %s" % key)

    def exists(self, key):
        for local in reversed(self.locals):
            if key in local:
                return True
        return False

    def let(self, key, register, node):
        if not isinstance(node, parser.Node):
            raise ValueError("Expected Node, got %s" % type(node))

        if not isinstance(register, RegisterHandle):
            raise ValueError("Expected RegisterHandle, got %s" % type(register))

        if key in self.locals[len(self.locals) - 1]:
            node.compile_error("Redeclaring local: %s" % key)

        self.locals[len(self.locals) - 1][key] = register

class MethodSignature:
    def __init__(self, name, args, argnames, returntype, is_ctor=False, containing_class=None, is_override=False, is_entrypoint=False):
        assert type(is_ctor) is type(False)
        self.name = name
        self.args = args
        self.argnames = argnames
        self.returntype = returntype
        self.is_ctor = is_ctor
        self.id = MethodSignature.sequential_id
        self.containing_class = containing_class
        self.is_override = is_override
        self.is_entrypoint = is_entrypoint
        MethodSignature.sequential_id += 1

    @property
    def nargs(self):
        return len(self.args)

    def looks_like_method_handle(self):
        pass

    def __repr__(self):
        return "MethodSignature(name='%s', nargs=%s, id=%s)" % (self.name, len(self.args), self.id)

    def __str__(self):
        modifiers = ("override " if self.is_override else "") + ("entrypoint " if self.is_entrypoint else "")
        args_str = ", ".join(["%s: %s" % (name, typ.name) for typ, name in zip(self.args, self.argnames)])
        prefix = "%s." % self.containing_class.name if self.containing_class is not None else ""
        if self.is_ctor:
            return "%sctor(%s)" % (prefix, args_str)
        return "%sfn %s%s(%s): %s" % (modifiers, prefix, self.name, args_str, self.returntype.name)

    @staticmethod
    def from_node(method, types, program, this_type=None, containing_class=None):
        if containing_class is None and method["modifiers"].has_child("override"):
            method.compile_error("Can't both be an override method and not have a containing class")

        if method.i("fn"):
            signature = ([] if this_type is None else [this_type]) + [types.resolve(arg[1]) for arg in method[1]]
            argnames = ([] if this_type is None else ["this"]) + [arg[0].data for arg in method[1]]
            is_override = method["modifiers"].has_child("override")
            is_entrypoint = method["modifiers"].has_child("entrypoint")
            if is_entrypoint and containing_class is not None:
                method.compile_error("Can't have an entrypoint inside a class")
            if is_entrypoint and len(argnames) > 0:
                method.compile_error("Can't have an entrypoint with arguments")
            name = method[0].data
            if containing_class is None and program.namespace is not None:
                name = "%s.%s" % (program.namespace, name)
            return MethodSignature(name, signature, argnames, types.resolve(method[2]), is_ctor=False, containing_class=containing_class, is_override=is_override, is_entrypoint=is_entrypoint)
        elif method.i("ctor"):
            signature = [this_type] + [types.resolve(arg[1]) for arg in method[0]]
            argnames = ["this"] + [arg[0].data for arg in method[0]]
            assert type(containing_class.name) is str
            return MethodSignature("@@ctor.%s" % containing_class.name, signature, argnames, types.void_type, is_ctor=True, containing_class=containing_class, is_override=False)

    @staticmethod
    def scan(top, types):
        # TODO: Fix this when removing top.xattrs["program"]
        program = top.xattrs["program"]
        if program is None:
            raise ValueError("This is a compiler bug.")
        ret = []
        for method in top:
            if not method.i("fn"):
                continue
            ret.append(MethodSignature.from_node(method, types, program))
        return ret
MethodSignature.sequential_id = 0

def get_method_signature(name, top_, is_real_top=False):
    # I really need to change this. This should instead accept a reference to
    # the Program object, which should in turn have a list of method
    # signatures. But everything involved here is too tightly coupled, so it's
    # a major chunk of work.
    top = top_
    if not is_real_top:
        top = top_.xattrs["top"]

    program = top.xattrs["program"]
    search_paths = [""]
    search_paths.append(program.namespace)
    search_paths += program.using_directives
    for signature in top.xattrs["signatures"]:
        if signature.name in ["%s.%s" % (path, name) if path != "" else name for path in search_paths]:
            return signature

class MethodEmitter:
    def __init__(self, top, program, signature):
        self.top = top
        self.opcodes = []
        self.scope = Scopes()
        self.program = program
        self.types = program.types
        self.signature = signature

    def emit_expr(self, node, register):
        opcodes = []
        if node.i("number"):
            nt = int(node.data)
            opcodes.append(ops["load"].ins(register, abs(nt)))
            if nt < 0:
                opcodes.append(ops["twocomp"].ins(register))
        elif node.of("+", "*", "^", "&", "|", "%", "==", ">=", "<=", ">", "<", "!=", "and", "or"):
            lhs = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs)
            rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            opcodes += self.emit_expr(node[1], rhs)
            opcodes.append(ops[{
                "+": "add",
                "*": "mult",
                "^": "xor",
                "&": "and",
                "|": "or",
                "%": "modulo",
                "==": "equals",
                "<": "lt",
                ">": "gt",
                "<=": "lteq",
                ">=": "gteq",
                "!=": "equals",
                # TODO: What happens if x0 is 0x1 and x1 is 0x2? Specifically for "and".
                "and": "and",
                "or": "or"
            }[node.type]].ins(lhs, rhs, register))
            if node.i("!="):
                opcodes.append(ops["invert"].ins(register))
        elif node.i("not"):
            if not register.type.is_boolean():
                raise typesys.TypingError(node, "Operator 'not' needs boolean argument")
            opcodes += self.emit_expr(node[0], register)
            opcodes.append(ops["invert"].ins(register))
        elif node.i("~"):
            if not register.type.is_numerical():
                raise typesys.TypingError(node, "Operator '~' needs numerical argument")
            opcodes += self.emit_expr(node[0], register)
            reg = self.scope.allocate(self.types.int_type)
            opcodes.append(ops["load"].ins(reg, 0xffffffff))
            opcodes.append(ops["xor"].ins(reg, register, register))
        elif node.i("ident"):
            opcodes.append(annotate(ops["mov"].ins(self.scope.resolve(node.data, node), register), "access %s" % node.data))
        elif node.i("true"):
            opcodes.append(ops["load"].ins(register, 1))
        elif node.i("false"):
            opcodes.append(ops["load"].ins(register, 0))
        elif node.i("null"):
            opcodes.append(ops["zero"].ins(register))
        elif node.i("-"):
            if len(node.children) == 1:
                if not self.types.decide_type(node[0], self.scope).is_numerical():
                    raise typesys.TypingError(node, "Operator '-' needs numerical argument")
                opcodes += self.emit_expr(node[0], register)
                opcodes.append(ops["twocomp"].ins(register))
            else:
                lhs = self.scope.allocate(self.types.decide_type(node[0], self.scope))
                opcodes += self.emit_expr(node[0], lhs)
                rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope))
                opcodes += self.emit_expr(node[1], rhs)
                opcodes.append(ops["twocomp"].ins(rhs))
                opcodes.append(ops["add"].ins(lhs, rhs, register))
        elif node.i("call"):
            signature = None
            is_clazz_call = False
            if node[0].i("."):
                is_clazz_call = True
                dot_node = node[0]
                lhs_reg = self.scope.allocate(self.types.decide_type(dot_node[0], self.scope))
                opcodes += self.emit_expr(dot_node[0], lhs_reg)
                if not lhs_reg.type.is_clazz():
                    lhs_reg.compile_error("Attempt to call a method of something that has type '%s', despite it not being a class" % lhs_reg.type)
                signature = lhs_reg.type.method_signature(dot_node[1].data)
            else:
                signature = get_method_signature(node[0].data, self.top)

            if len(node.children) - 1 != len(signature.args) + (-1 if is_clazz_call else 0):
                node.compile_error("Expected %s arguments, got %s" % (len(signature.args), len(node.children) - 1))
            for param, index in zip(node.children[1:], range(len(node.children) - 1)):
                inferred_type = self.types.decide_type(param, self.scope)
                declared_type = signature.args[index]
                if not inferred_type.is_assignable_to(declared_type):
                    raise typesys.TypingError(node, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                reg = self.scope.allocate(inferred_type)
                opcodes += self.emit_expr(param, reg)
                opcodes.append(ops["param"].ins(reg, index))
            if is_clazz_call:
                opcodes.append(ops["classcall"].ins(lhs_reg, signature, register))
            else:
                opcodes.append(ops["call"].ins(signature, register))
        elif node.i("new"):
            # clazz_signature = self.program.get_clazz_signature(node[0].data)
            clazz_signature = self.types.resolve(node[0]).signature
            result_register = self.scope.allocate(self.types.decide_type(node, self.scope))
            opcodes.append(ops["new"].ins(clazz_signature, result_register))
            # TODO: Support multiple constructors and method overloading
            # TODO: A lot of this code is copy-pasted
            if len(clazz_signature.ctor_signatures) < 1:
                node.compile_error("Type has no constructors")
            ctor_signature = clazz_signature.ctor_signatures[0]
            if len(node.children) - 1 + 1 != len(ctor_signature.args):
                node.compile_error("Expected %s arguments, got %s" % (len(ctor_signature.args), len(node.children) - 1 + 1))
            for param, index in zip(node.children[1:], range(len(node.children) - 1)):
                inferred_type = self.types.decide_type(param, self.scope)
                declared_type = ctor_signature.args[index + 1]
                if not inferred_type.is_assignable_to(declared_type):
                    raise typesys.TypingError(node, "Invalid parameter type: '%s' is not assignable to '%s'" % (inferred_type, declared_type))
                reg = self.scope.allocate(inferred_type)
                opcodes += self.emit_expr(param, reg)
                opcodes.append(ops["param"].ins(reg, index))
            opcodes.append(ops["classcall"].ins(result_register, ctor_signature, self.scope.allocate(self.types.bool_type)))
            opcodes.append(ops["mov"].ins(result_register, register))
        elif node.i("["):
            if not register.type.is_array():
                raise typesys.TypingError(node, "Cannot assign an array to something that isn't an array")
            arrlen_reg = self.scope.allocate(self.types.int_type)
            opcodes.append(annotate(ops["load"].ins(arrlen_reg, len(node)), "array instantiation length"))
            opcodes.append(ops["arralloc"].ins(register, arrlen_reg))
            index_reg = self.scope.allocate(self.types.int_type)
            for i, elm in zip(range(len(node)), node):
                opcodes.append(annotate(ops["load"].ins(index_reg, i), "array instantiation index"))
                element_reg = self.scope.allocate(self.types.decide_type(elm, self.scope))
                opcodes += self.emit_expr(elm, element_reg)
                opcodes.append(ops["arrassign"].ins(register, index_reg, element_reg))
        elif node.i("access"):
            lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            opcodes += self.emit_expr(node[0], lhs_reg)
            if not lhs_reg.type.is_array():
                raise typesys.TypingError(node[0], "Cannot do array access on something that isn't an array")
            if not lhs_reg.type.parent_type.is_assignable_to(register.type):
                # This should have already been caught by typechecking further up
                raise ValueError("This is a compiler bug")
            index_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            if not index_reg.type.is_numerical():
                raise typesys.TypingError(child, "Index to array access must be numerical")
            opcodes += self.emit_expr(node[1], index_reg)
            opcodes.append(ops["arraccess"].ins(lhs_reg, index_reg, register))
        elif node.i("arrinst"):
            quantity_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            if not quantity_reg.type.is_numerical():
                raise typesys.TypingError("Array lengths must be numerical")
            opcodes += self.emit_expr(node[1], quantity_reg)
            opcodes.append(ops["arralloc"].ins(register, quantity_reg))
        elif node.i("#"):
            arr_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            if not arr_reg.type.is_array():
                raise typesys.TypingError("Can't decide length of something that isn't an array")
            opcodes += self.emit_expr(node[0], arr_reg)
            opcodes.append(ops["arrlen"].ins(arr_reg, register))
        elif node.i("."):
            lhs_reg = None
            if node[0].i("ident"):
                lhs_reg = self.scope.resolve(node[0].data, node[0])
            else:
                # node, register
                lhs_reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
                opcodes += self.emit_expr(node[0], lhs_reg)
            opcodes.append(ops["access"].ins(lhs_reg, node[1].data, register))
        else:
            node.compile_error("Unexpected")
        return opcodes

    def emit_statement(self, node):
        opcodes = []
        claims = clms.ClaimSpace()
        if node.i("statements"):
            self.scope.push()
            for statement in node:
                new_opcodes, new_claim_space = self.emit_statement(statement)
                opcodes += new_opcodes
                claims.add_claims_conservative(*new_claim_space.claims)
        elif node.i("call") or node.i("expr"):
            result = self.scope.allocate(self.types.decide_type(node if node.i("call") else node[0], self.scope))
            opcodes += self.emit_expr(node if node.i("call") else node[0], result)
        elif node.i("let"):
            # Notice that a variable may have only exactly one let statement
            # in a given scope. So we can typecheck just with that register's
            # type, we don't have to associate with the scope directly.
            type = None
            if len(node) > 2:
                type = self.types.decide_type(node[2], self.scope)
                if not node[1].i("infer_type"):
                    inferred_type = type
                    # This is the declared type
                    type = self.types.resolve(node[1])
                    if not inferred_type.is_assignable_to(type):
                        raise typesys.TypingError(node, "'%s' is not assignable to '%s'" % (inferred_type, type))
            else:
                if node[1].i("infer_type"):
                    node.compile_error("Variables declared without assignment must be given explicit type")
                type = self.types.resolve(node[1])

            reg = self.scope.allocate(type)
            self.scope.let(node[0].data, reg, node)

            if len(node) > 2:
                opcodes += annotate(self.emit_expr(node[2], reg), "let %s" % node[0].data)
        elif node.i("return"):
            reg = None
            if len(node.children) > 0:
                stated_return_type = self.types.decide_type(node[0], self.scope)
                if not self.return_type.is_assignable_from(stated_return_type):
                    raise typesys.TypingError(node, "Return type mismatch")
                reg = self.scope.allocate(stated_return_type)
                opcodes += self.emit_expr(node[0], reg)
            else:
                if not self.return_type.is_assignable_to(self.types.void_type):
                    raise typesys.TypingError(node, "Can't return nothing from a method unless the method returns void")
                reg = self.scope.allocate(self.types.int_type)
                opcodes.append(ops["zero"].ins(reg))
            claims.add_claims(clms.ClaimReturns())
            opcodes.append(ops["return"].ins(reg))
        elif node.i("chain"):
            opcodes += Chain(node, self.scope, ops).access(None, self, no_output=True)
        elif node.i("assignment"):
            rhs_type = self.types.decide_type(node[1], self.scope)
            rhs_reg = self.scope.allocate(rhs_type)
            opcodes += self.emit_expr(node[1], rhs_reg)
            chain = Chain(node[0], self.scope, ops)
            lhs_type = chain.decide_type()
            if not lhs_type.is_assignable_from(rhs_type):
                raise typesys.TypingError(node, "Need RHS to be assignable to LHS: %s, %s" % (lhs_type, rhs_type))
            opcodes += chain.assign(rhs_reg, self)
        elif node.of("+=", "*=", "-="):
            # TODO: This does the first (n - 1) parts of the chain twice
            rhs_reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            opcodes += self.emit_expr(node[1], rhs_reg)
            chain = Chain(node[0], self.scope, ops)
            resultreg = self.scope.allocate(chain.decide_type())
            opcodes += chain.access(resultreg, self)
            if not resultreg.type.is_numerical() or not rhs_reg.type.is_numerical():
                raise typesys.TypingError(node, "LHS and RHS of arithmetic must be numerical, not '%s' and '%s'" % (resultreg.type.name, rhs_reg.type.name))
            if node.i("-="):
                opcodes.append(ops["twocomp"].ins(rhs_reg))
            opcodes.append(ops[{
                "+=": "add",
                "*=": "mult",
                "-=": "add"
            }[node.type]].ins(resultreg, rhs_reg, resultreg))
            opcodes += chain.assign(resultreg, self)
        elif node.i("while"):
            result = []
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of a while loop must be boolean")
            conditional = self.emit_expr(node[0], condition_register)
            # For now, we can't be sure whether the content of a while loop is ever run
            statement, _unused_claims = self.emit_statement(node[1])
            nop = ops["nop"].ins()
            result += conditional
            result.append(ops["jf"].ins(condition_register, nop))
            result += statement
            result.append(ops["goto"].ins(conditional[0]))
            result.append(nop)
            opcodes += result
        elif node.i("for"):
            result = []
            setup_statement, setup_claim_space = self.emit_statement(node[0])
            result += annotate(setup_statement, "for loop setup")
            claims.add_claims_conservative(*setup_claim_space.claims)

            condition_register = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of a for loop must be boolean")
            conditional = annotate(self.emit_expr(node[1], condition_register), "for loop condition")
            result += conditional
            end_nop = ops["nop"].ins()
            result.append(ops["jf"].ins(condition_register, end_nop))

            # For now, we don't know whether the body of a for loop will ever be run
            body_statement, _unused_body_claim_space = self.emit_statement(node[3])
            result += annotate(body_statement, "for loop body")

            # For now, we don't know whether the each-time part of a for loop will ever be run
            each_time_statement, _unused_setup_claim_space = self.emit_statement(node[2])
            eachtime = annotate(each_time_statement, "for loop each time")
            result += eachtime

            result.append(annotate(ops["goto"].ins(conditional[0]), "for loop loop"))
            result.append(end_nop)

            opcodes += result
        elif node.i("if"):
            result = []
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of an if statement must be boolean")
            conditional = annotate(self.emit_expr(node[0], condition_register), "if predicate")
            true_path_statement, true_path_claims = self.emit_statement(node[1])
            statement = annotate(true_path_statement, "if true path")
            nop = ops["nop"].ins()
            result += conditional
            result.append(ops["jf"].ins(condition_register, nop))
            result += statement
            else_end_nop = ops["NOP"].ins()
            if len(node) > 2:
                result.append(ops["goto"].ins(else_end_nop)) #
            result.append(nop)
            if len(node) > 2:
                false_path_statement, false_path_claims = self.emit_statement(node[2])
                result += annotate(false_path_statement, "if false path")
                result.append(else_end_nop)
                # We know that the intersection of the claims of the true and
                # false path will always be true.
                claims.add_claims_conservative(*true_path_claims.intersection(false_path_claims).claims)
            opcodes += result
        elif node.of("++", "--"):
            var = self.scope.resolve(node[0].data, node)
            if not var.type.is_numerical():
                raise typesys.TypingError(node, "Subject of increment/decrement must be numerical")
            reg = self.scope.allocate(self.types.int_type)
            opcodes.append(ops["load"].ins(reg, 1))
            if node.i("--"):
                opcodes.append(ops["twocomp"].ins(reg))
            opcodes.append(ops["add"].ins(var, reg, var))
        else:
            node.compile_error("Unexpected (this is a compiler bug)")
        return opcodes, claims

    def encode(self, opcodes):
        pass

    def emit(self):
        self.return_type = self.signature.returntype
        # To codify an assumption used in this method:
        assert self.top.i("fn") or self.top.i("ctor")
        for argtype, argname in zip(self.signature.args, self.signature.argnames):
            self.scope.let(argname, self.scope.allocate(argtype), self.top[1] if self.top.i("fn") else self.top[0])
        opcodes, claims = self.emit_statement(self.top[3] if self.top.i("fn") else self.top[1])
        if not claims.contains_equivalent(clms.ClaimReturns()):
            if self.signature.returntype.is_void():
                zeroreg = self.scope.allocate(self.types.int_type)
                opcodes.append(ops["zero"].ins(zeroreg))
                opcodes.append(ops["return"].ins(zeroreg))
            else:
                self.top[3].compile_error("Non-void method might not return")
        return opcodes

class Method:
    def __init__(self, root, emitter):
        self.root = root
        self.emitter = emitter

class Segment:
    def __init__(self, humantype, realtype):
        self.humantype = humantype
        self.realtype = realtype

    def __str__(self):
        return "Segment(type='%s', code=%s)" % (self.humantype, self.realtype)

    def print_(self, emitter):
        print(str(self))

    def evaluate(self, program):
        pass

class MethodSegment(Segment):
    def __init__(self, method, signature):
        Segment.__init__(self, "method", 0x00)
        self.method = method
        self.signature = signature
        self.opcodes = None
        self.num_registers = None
        self.scope = None
        self.opcodes = None

    def emit_opcodes(self, program):
        emitter = MethodEmitter(self.method, program, self.signature)
        ret = emitter.emit()
        # TODO: This is spaghetti code
        self.emitter = emitter
        self.num_registers = emitter.scope.register_ids
        self.scope = emitter.scope
        self.opcodes = ret
        return ret

    def __str__(self):
        return "MethodSegment(type='%s', typecode=%s, signature=%s)" % (self.humantype, self.realtype, self.signature)

    def print_(self, emitter):
        Segment.print_(self, emitter)
        print("Registers:", ", ".join([repr(register) for register in self.emitter.scope.registers]))
        for opcode, index in zip(self.opcodes, range(len(self.opcodes))):
            print("    %2d: %s" % (index, str(opcode)))

    def evaluate(self, program):
        self.opcodes = self.emit_opcodes(program)

class MetadataSegment(Segment):
    def __init__(self, entrypoint=None, headers=header.HeaderRepresentation.HIDDEN):
        Segment.__init__(self, "metadata", 0x01)
        self.entrypoint = entrypoint
        self.headers = headers

    def __str__(self):
        return "MetadataSegment(type='%s', typecode=%s, entrypoint=%s)" % (self.humantype, self.realtype, self.entrypoint)

    def print_(self, emitter):
        Segment.print_(self, emitter)
        print("    entrypoint=%s" % self.entrypoint)
        print("    headers omitted for brevity")

    def has_entrypoint(self):
        return self.entrypoint is not None

    def evaluate(self, program):
        Segment.evaluate(self, program)

class ClazzField:
    def __init__(self, name, type):
        self.name = name
        self.type = type

class ClazzSignature:
    def __init__(self, name, fields, method_signatures, ctor_signatures, parent_signature):
        self.name = name
        self.fields = fields
        self.method_signatures = method_signatures
        self.ctor_signatures = ctor_signatures
        if parent_signature is not None and type(parent_signature) is not ClazzSignature:
            raise ValueError("Invalid type for parent_signature %s (expected ClazzSignature or NoneType)" % type(parent_signature))
        self.parent_signature = parent_signature

    def validate_overriding_rules(self):
        def throw(signature):
            raise ValueError("Method '%s' violates overriding rules on class '%s'" % (signature.name, self.name))

        for signature in self.method_signatures:
            if not signature.is_override:
                if self.parent_signature is None:
                    continue
                if self.parent_signature.get_method_signature_by_name(signature.name) is not None:
                    throw(signature)
            else:
                if self.parent_signature is None:
                    throw(signature)
                # `parent_sig` is a method signature (as opposed to a class signature)
                parent_sig = self.parent_signature.get_method_signature_by_name(signature.name)
                if parent_sig is None:
                    throw(signature)
                if not parent_sig.returntype.is_assignable_to(signature.returntype):
                    throw(signature)
                if len(signature.args) != len(parent_sig.args):
                    throw(signature)
                for i in range(len(signature.args) - 1):
                    # Why - 1 and + 1? Because `this` is an argument
                    if not parent_sig.args[i + 1].is_assignable_to(signature.args[i + 1]):
                        throw(signature)

        if self.parent_signature is not None:
            for field in self.fields:
                if self.parent_signature.get_field_by_name(field.name) is not None:
                    raise ValueError("Duplicate field in both child and parent class named '%s'" % field.name)

    def get_method_signature_by_name(self, name):
        for signature in self.method_signatures:
            if signature.name == name:
                return signature

        if self.parent_signature is not None:
            return self.parent_signature.get_method_signature_by_name(name)

        return None

    def get_field_by_name(self, name):
        for field in self.fields:
            if field.name == name:
                return field

        if self.parent_signature is not None:
            return self.parent_signature.get_field_by_name(name)

        return None

    def __str__(self):
        return repr(self)

    def __repr__(self):
        return "ClazzSignature(name='%s')" % self.name

    def looks_like_class_signature(self):
        pass

class ClazzSegment(Segment):
    def __init__(self, emitter):
        Segment.__init__(self, "class", 0x02)
        self.emitter = emitter
        self.signature = emitter.signature
        self.name = self.signature.name
        self.fields = self.signature.fields
        self.method_signatures = self.signature.method_signatures

    @property
    def parent_signature(self):
        return self.signature.parent_signature

    def __str__(self):
        return "ClazzSegment(type='%s', typecode=%s, name='%s', nfields=%s, nmethods=%s, superclass=%s)" \
            % (self.humantype,
            self.realtype,
            self.name,
            len(self.fields),
            len(self.method_signatures),
            "'%s'" % self.parent_signature.name if self.parent_signature is not None else "None")

    def print_(self, emitter):
        Segment.print_(self, emitter)
        for method in self.method_signatures:
            print("    %s" % method)
        for field in self.fields:
            print("    %s: %s" % (field.name, field.type.name))

    def evaluate(self, program):
        Segment.evaluate(self, program)
        for field in self.emitter.top["classbody"]:
            if field.i("fn") or field.i("ctor"):
                signature = field.xattrs["signature"]
                method = program.emit_method(field, signature=signature)
                program.segments.append(method)
                program.methods.append(method)

class ClazzEmitter:
    def __init__(self, top, program):
        self.top = top
        self.program = program
        self.signature = None
        self.is_signature_no_fields = False

    @property
    def unqualified_name(self):
        return self.top[0].data

    @property
    def name(self):
        return ("%s." % self.program.namespace if self.program.namespace is not None else "") + self.unqualified_name

    def _emit_field(self, node):
        return ClazzField(node[0].data, self.program.types.resolve(node[1]))

    def emit_signature_no_fields(self):
        self.is_signature_no_fields = True
        self.signature = ClazzSignature(self.name, [], [], [], None)
        return self.signature

    def emit_signature(self):
        if self.signature is not None and not self.is_signature_no_fields:
            return self.signature
        self.fields = []
        self.method_signatures = []
        self.ctor_signatures = []
        for field in self.top["classbody"]:
            if field.i("classprop"):
                self.fields.append(self._emit_field(field))
            elif field.i("fn"):
                signature = MethodSignature.from_node(
                    field,
                    self.program.types,
                    self.program,
                    this_type=self.program.types.get_clazz_type_by_signature(self.signature),
                    # At this point in execution, `this.signature` is the blank
                    # signature from emit_signature_no_fields(...).
                    containing_class=self.signature
                )
                self.method_signatures.append(signature)
                field.xattrs["signature"] = signature
            elif field.i("ctor"):
                signature = MethodSignature.from_node(
                    field,
                    self.program.types,
                    self.program,
                    this_type=self.program.types.get_clazz_type_by_signature(self.signature),
                    # Same comment as above
                    containing_class=self.signature
                )
                self.method_signatures.append(signature)
                self.ctor_signatures.append(signature)
                field.xattrs["signature"] = signature
        self.signature = ClazzSignature(self.name, self.fields, self.method_signatures, self.ctor_signatures, None)
        # We need to replace the containing_class with the new signature,
        # just so we don't have different signatures floating around.
        for signature in self.method_signatures:
            signature.containing_class = self.signature
        self.is_signature_no_fields = False
        return self.signature

    def add_superclass_to_signature(self):
        if len(self.top["extends"]) > 0:
            # Since we have a reference to this class's signature, we can
            # just update it in-place.
            parent_type = self.program.types.resolve(self.top["extends"][0])
            if parent_type is None or not parent_type.is_clazz():
                self.top["extends"].compile_error("Tried to extend a nonexisting class")
            self.signature.parent_signature = parent_type.signature
        # Otherwise, the superclass remains as `None`.

    def emit(self):
        self.emit_signature()
        return ClazzSegment(self)

class Program:
    def __init__(self, emitter):
        self.methods = []
        self.emitter = emitter
        self.top = emitter.top
        self.top.xattrs["program"] = self
        self.segments = []
        self.types = typesys.TypeSystem(self)
        self.clazzes = []
        self.clazz_signatures = []
        self.using_directives = []
        self.namespace = None

    @property
    def search_paths(self):
        search_paths = [""]
        if self.namespace is not None:
            search_paths.append("%s." % self.namespace)
        for using_directive in self.using_directives:
            search_paths.append("%s." % using_directive)
        return search_paths

    def get_entrypoint(self):
        for method in self.methods:
            if method.signature.is_entrypoint:
                return method.signature
        return None

    def get_method_signature(self, name):
        return get_method_signature(name, self.top, is_real_top=True)

    def get_clazz_signature(self, name):
        for signature in self.clazz_signatures:
            if signature.name == name:
                return signature

    def has_entrypoint(self):
        return self.get_entrypoint() is not None

    def emit_method(self, method, signature=None):
        # method[0].data is method.name
        # TODO: Remove signature parameter and make caller call get_method_signature
        return MethodSegment(method, signature if signature is not None else get_method_signature(method[0].data, method))

    def validate_overriding_rules(self):
        for signature in self.clazz_signatures:
            signature.validate_overriding_rules()

    def evaluate(self):
        for emitter in self.clazz_emitters:
            segment = emitter.emit()
            self.clazzes.append(segment)
            self.segments.append(segment)
        for toplevel in self.top:
            if toplevel.i("fn"):
                method = self.emit_method(toplevel)
                self.methods.append(method)
                self.segments.append(method)
        # for clazz in self.clazzes:
        #     methods = clazz.emit_methods()
        #     self.methods += methods
        #     self.segments += methods
        self.validate_overriding_rules()
        for clazz in self.clazzes:
            clazz.evaluate(self)
        for method in self.methods:
            method.evaluate(self)

    def get_header_representation(self):
        return header.HeaderRepresentation.from_program(self)

    def add_metadata(self):
        if self.has_entrypoint():
            self.segments.append(MetadataSegment(entrypoint=self.get_entrypoint(), headers=self.get_header_representation()))
        else:
            self.segments.append(MetadataSegment(headers=self.get_header_representation()))

    def prescan(self):
        for toplevel in self.top:
            if toplevel.i("namespace"):
                self.namespace = flatten_ident_nodes(toplevel)
            elif toplevel.i("using"):
                self.using_directives.append(flatten_ident_nodes(toplevel))
        clazz_emitters = []
        for toplevel in self.top:
            if not toplevel.i("class"):
                continue
            clazz_emitters.append(ClazzEmitter(toplevel, self))
        for emitter in clazz_emitters:
            signature = emitter.emit_signature_no_fields()
            self.clazz_signatures.append(signature)
            self.types.accept_skeleton_clazz(signature)
        for emitter, index in zip(clazz_emitters, range(len(clazz_emitters))):
            signature = emitter.emit_signature()
            self.types.update_signature(signature)
            self.clazz_signatures[index] = signature
        for emitter in clazz_emitters:
            emitter.add_superclass_to_signature()
        self.clazz_emitters = clazz_emitters
        if "signatures" not in self.emitter.top.xattrs:
            self.emitter.top.xattrs["signatures"] = []
        self.emitter.top.xattrs["signatures"] += MethodSignature.scan(self.emitter.top, self.types)

    def add_include(self, headers):
        for clazz in headers.clazzes:
            self.types.accept_skeleton_clazz(ClazzSignature(clazz.name, [], [], [], None))
        for method in headers.methods:
            self.emitter.top.xattrs["signatures"].append(MethodSignature(
                method.name,
                [self.types.resolve(parser.parse_type(arg.type)) for arg in method.args],
                [arg.name for arg in method.args],
                self.types.resolve(parser.parse_type(method.returntype))
            ))
        for clazz in headers.clazzes:
            methods = []
            for method in clazz.methods:
                methods.append(get_method_signature(method.name, self.top, is_real_top=True))
            ctors = []
            for ctor in clazz.ctors:
                ctors.append(get_method_signature(ctor.name, self.top, is_real_top=True))
            fields = []
            for field in clazz.fields:
                fields.append(ClazzField(field.name, self.types.resolve(parser.parse_type(field.type))))
            clazz = ClazzSignature(clazz.name, fields, methods, ctors, self.types.resolve(parser.parse_type(clazz.parent)) if clazz.parent is not None else None)
            self.clazz_signatures.append(clazz)
            self.types.update_signature(clazz)

class Emitter:
    def __init__(self, top):
        self.top = top
        self.top.xattrs["signatures"] = []

    def emit_program(self):
        for method in self.top:
            method.xattrs["top"] = self.top

        program = Program(self)

        return program

    def emit_bytes(self, program):
        import bytecode
        return bytecode.emit(program.segments)

if __name__ == "__main__":
    import argparse
    argparser = argparse.ArgumentParser()
    argparser.add_argument("file")
    argparser.add_argument("--no-metadata", action="store_true", help="don't include metadata")
    argparser.add_argument("--ast", action="store_true", help="display AST (for debugging)")
    argparser.add_argument("--ast-after", action="store_true", help="display AST after bytecode generation (for debugging)")
    argparser.add_argument("--segments", action="store_true", help="display segments (for debugging)")
    argparser.add_argument("--hexdump", action="store_true", help="display hexdump (for debugging)")
    argparser.add_argument("--headers", action="store_true", help="display headers (for debugging)")
    argparser.add_argument("--signatures", action="store_true", help="display signatures (for debugging)")
    argparser.add_argument("--directives", action="store_true", help="display using and namespace directives (for debugging)")
    argparser.add_argument("--parse-only", action="store_true", help="parse only, don't compile (for debugging)")
    argparser.add_argument("-o", "--output", metavar="file", help="file for bytecode output")
    argparser.add_argument("-i", "--include", metavar="file", action="append", help="files to link against")
    args = argparser.parse_args()

    if args.output is None:
        args.output = "%s.slb" % args.file[:-4]

    with open(args.file, "r") as f:
        tree = parser.Parser(parser.Toker("\n" + f.read())).parse()

    if args.ast:
        tree.output()

    if args.parse_only:
        import sys
        sys.exit(0)

    emitter = Emitter(tree)
    program = emitter.emit_program()
    if args.include:
        for include in args.include:
            program.add_include(header.from_slb(include))
    program.prescan()
    if args.directives:
        if program.namespace is not None:
            print("Namespace: '%s'" % program.namespace)
        for using_directive in program.using_directives:
            print("Using: '%s'" % using_directive)
    if args.signatures:
        for signature in program.top.xattrs["signatures"]:
            print("    %s" % signature)
    program.evaluate()

    if not args.no_metadata:
        program.add_metadata()

    if args.segments:
        for segment in program.segments:
            segment.print_(emitter)

    if args.headers:
        import json
        print(json.dumps(program.get_header_representation().serialize(), indent=4))
    if args.ast_after:
        tree.output()

    outbytes = emitter.emit_bytes(program)

    if args.hexdump:
        import binascii
        print(binascii.hexlify(outbytes).decode("ascii"))

    with open(args.output, "wb") as f:
        f.write(outbytes)
