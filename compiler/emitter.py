from opcodes import opcodes as ops
import header
import typesys

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
        print("Allocating with type %s" % type)
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
    def __init__(self, name, args, argnames):
        self.name = name
        self.args = args
        self.argnames = argnames
        self.id = MethodSignature.sequential_id
        MethodSignature.sequential_id += 1

    @property
    def nargs(self):
        return len(self.args)

    def looks_like_method_handle(self):
        pass

    def __repr__(self):
        return "MethodSignature(name='%s', nargs=%s, id=%s)" % (self.name, len(self.args), self.id)

    def __str__(self):
        return repr(self)

    @staticmethod
    def scan(top):
        ret = []
        for method in top:
            signature = ["int"] * len(list(method[1]))
            argnames = [arg.data for arg in method[1]]
            ret.append(MethodSignature(method[0].data, signature, argnames))
        return ret
MethodSignature.sequential_id = 0

def get_method_signature(name, top_):
    top = top_.xattrs["top"]

    for signature in top.xattrs["signatures"]:
        if signature.name == name:
            return signature

class MethodEmitter:
    def __init__(self, top, program):
        self.top = top
        self.opcodes = []
        self.scope = Scopes()
        self.program = program
        self.types = program.types

    def emit_expr(self, node, register):
        opcodes = []
        if node.i("number"):
            nt = int(node.data)
            opcodes.append(ops["load"].ins(register, abs(nt)))
            if nt < 0:
                opcodes.append(ops["twocomp"].ins(register))
        elif node.of("+", "*", "^", "&", "|", "==", ">=", "<=", ">", "<", "!=", "and", "or"):
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
        elif node.i("-"):
            if len(node.children) == 1:
                if not self.types.decide_type(node[0], self.scope).is_numerical():
                    raise typesys.TypingError(node, "Operator '-' needs numerical argument")
                opcodes += self.emit_expr(node[0], register)
                opcodes.append(ops["twocomp"].ins(register))
            else:
                lhs = self.scope.allocate(self.typesys.decide_type(node[0], self.scope))
                opcodes += self.emit_expr(node[0], lhs)
                rhs = self.scope.allocate(self.types.decide_type(node[1], self.scope))
                opcodes += self.emit_expr(node[1], rhs)
                opcodes.append(ops["twocomp"].ins(rhs))
                opcodes.append(ops["add"].ins(lhs, rhs, register))
        elif node.i("call"):
            for param, index in zip(node.children[1:], range(len(node.children) - 1)):
                reg = self.scope.allocate(self.types.decide_type(param, self.scope))
                opcodes += self.emit_expr(param, reg)
                opcodes.append(ops["param"].ins(reg, index))
            signature = get_method_signature(node[0].data, self.top)
            if signature is None or signature.nargs != len(node.children) - 1:
                node.compile_error("Invalid or nonexistent method signature '%s'" % signature)
            opcodes.append(ops["call"].ins(signature, register))
        else:
            node.compile_error("Unexpected")
        return opcodes

    def emit_statement(self, node):
        opcodes = []
        if node.i("statements"):
            self.scope.push()
            for statement in node:
                opcodes += self.emit_statement(statement)
        elif node.i("call"):
            # TODO: Hardcoded type
            result = self.scope.allocate(self.types.int_type)
            opcodes += self.emit_expr(node, result)
        elif node.i("let"):
            # Notice that a variable may have only exactly one let statement
            # in a given scope. So we can typecheck just with that register's
            # type, we don't have to associate with the scope directly.
            reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            self.scope.let(node[0].data, reg, node)
            opcodes += annotate(self.emit_expr(node[1], reg), "let %s" % node[0].data)
        elif node.i("return"):
            reg = None
            if len(node.children) > 0:
                reg = self.scope.allocate(self.types.decide_type(node[0], self.scope))
                opcodes += self.emit_expr(node[0], reg)
            else:
                reg = self.scope.allocate(self.types.int_type)
                opcodes.append(ops["zero"].ins(reg))
            opcodes.append(ops["return"].ins(reg))
        elif node.i("assignment"):
            reg = self.scope.resolve(node[0].data, node[0])
            if not reg.type.isAssignableFrom(self.types.decide_type(node[1]), self.scope):
                raise typesys.TypingError(node, "Need RHS to be assignable to LHS")
            opcodes += self.emit_expr(node[1], reg)
        elif node.of("+=", "*=", "-="):
            reg = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            opcodes += self.emit_expr(node[1], reg)
            varreg = self.scope.resolve(node[0].data, node)
            if not reg.type.is_numerical() or not varreg.type.is_numerical():
                raise typesys.TypingError(node, "LHS and RHS of arithmetic must be numerical")
            opcodes.append(ops[{
                "+=": "add",
                "*=": "mult",
                "-=": "add"
            }[node.type]].ins(varreg, reg, varreg))
            if node.i("-="):
                opcodes.append(ops["twocomp"].ins(varreg))
        elif node.i("while"):
            result = []
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of a while loop must be boolean")
            conditional = self.emit_expr(node[0], condition_register)
            statement = self.emit_statement(node[1])
            nop = ops["nop"].ins()
            result += conditional
            result.append(ops["jf"].ins(condition_register, nop))
            result += statement
            result.append(ops["goto"].ins(conditional[0]))
            result.append(nop)
            opcodes += result
        elif node.i("for"):
            result = []
            result += annotate(self.emit_statement(node[0]), "for loop setup")
            eachtime = annotate(self.emit_statement(node[2]), "for loop each time")
            result += eachtime
            condition_register = self.scope.allocate(self.types.decide_type(node[1], self.scope))
            if not condition_register.is_boolean():
                raise typesys.TypingError(node, "Condition of a for loop must be boolean")
            conditional = annotate(self.emit_expr(node[1], condition_register), "for loop condition")
            result += conditional
            nop = ops["nop"].ins()
            result.append(ops["jf"].ins(condition_register, nop))
            result += annotate(self.emit_statement(node[3]), "for loop body")
            result.append(annotate(ops["goto"].ins(eachtime[0]), "for loop loop"))
            result.append(nop)
            return result
        elif node.i("if"):
            result = []
            condition_register = self.scope.allocate(self.types.decide_type(node[0], self.scope))
            if not condition_register.type.is_boolean():
                raise typesys.TypingError(node, "Condition of an if statement must be boolean")
            conditional = annotate(self.emit_expr(node[0], condition_register), "if predicate")
            statement = annotate(self.emit_statement(node[1]), "if true path")
            nop = ops["nop"].ins()
            result += conditional
            result.append(ops["jf"].ins(condition_register, nop))
            result += statement
            result.append(nop)
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
            node.compile_error("Unexpected")
        return opcodes

    def encode(self, opcodes):
        pass

    def emit(self):
        for arg in self.top[1]:
            # TODO: Hardcoded parameter type
            self.scope.let(arg.data, self.scope.allocate(self.types.int_type), arg)
        return self.emit_statement(self.top[2])

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
        self.num_registers=None
        self.opcodes = None

    def emit_opcodes(self, program):
        emitter = MethodEmitter(self.method, program)
        ret = emitter.emit()
        # TODO: This is spaghetti code
        self.emitter = emitter
        self.num_registers = emitter.scope.register_ids
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

class Program:
    def __init__(self, methods, emitter):
        self.methods = methods
        self.emitter = emitter
        self.segments = list(methods) # Shallow copy
        self.types = typesys.TypeSystem()

    def get_entrypoint(self):
        for method in self.methods:
            if method.signature.name == "main" and method.signature.nargs == 0:
                return method.signature
        return None

    def has_entrypoint(self):
        return self.get_entrypoint() is not None

    def evaluate(self):
        for method in self.methods:
            method.evaluate(self)

    def get_header_representation(self):
        return header.HeaderRepresentation.from_program(self)

    def add_metadata(self):
        if self.has_entrypoint():
            self.segments.append(MetadataSegment(entrypoint=self.get_entrypoint(), headers=self.get_header_representation()))
        else:
            self.segments.append(MetadataSegment(headers=self.get_header_representation()))

class Emitter:
    def __init__(self, top):
        self.top = top

    def emit_method(self, method):
        # method[0].data is method.name
        return MethodSegment(method, get_method_signature(method[0].data, method))

    def emit_program(self):
        for method in self.top:
            method.xattrs["top"] = self.top
        methods = []
        for method in self.top:
            methods.append(self.emit_method(method))

        program = Program(methods, self)
        program.add_metadata()

        return program

    def emit_bytes(self, program):
        import bytecode
        return bytecode.emit(program.segments)

def prescan_top_for_method_signatures(top):
    top.xattrs["signatures"] = MethodSignature.scan(top)

def add_include(top, headers):
    for method in headers.methods:
        if method.name == "main":
            # TODO: Remove this and add support for not having a main method
            print("Skipping main methood")
            continue
        top.xattrs["signatures"].append(MethodSignature(method.name, [arg.name for arg in method.args], ["int"] * len(method.args)))

if __name__ == "__main__":
    import argparse
    argparser = argparse.ArgumentParser()
    argparser.add_argument("file")
    argparser.add_argument("--ast", action="store_true", help="display AST (for debugging)")
    argparser.add_argument("--segments", action="store_true", help="display segments (for debugging)")
    argparser.add_argument("--hexdump", action="store_true", help="display hexdump (for debugging)")
    argparser.add_argument("--headers", action="store_true", help="display headers (for debugging)")
    argparser.add_argument("-o", "--output", metavar="file", help="file for bytecode output")
    argparser.add_argument("-i", "--include", metavar="file", action="append", help="files to link against")
    args = argparser.parse_args()

    if args.output is None:
        args.output = "%s.slb" % args.file[:-4]

    import parser
    with open(args.file, "r") as f:
        tree = parser.Parser(parser.Toker("\n" + f.read())).parse()

    if args.ast:
        tree.output()

    prescan_top_for_method_signatures(tree)
    if args.include:
        for include in args.include:
            add_include(tree, header.from_slb(include))

    emitter = Emitter(tree)
    program = emitter.emit_program()

    program.evaluate()
    if args.segments:
        for segment in program.segments:
            segment.print_(emitter)

    if args.headers:
        import json
        print(json.dumps(program.get_header_representation().serialize(), indent=4))

    outbytes = emitter.emit_bytes(program)

    if args.hexdump:
        import binascii
        print(binascii.hexlify(outbytes).decode("ascii"))

    with open(args.output, "wb") as f:
        f.write(outbytes)
