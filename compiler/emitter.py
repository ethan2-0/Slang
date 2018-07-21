from opcodes import opcodes as ops

class RegisterHandle:
    def __init__(self, id):
        self.id = id

    def __repr__(self):
        return "RegisterHandle(id=%s)" % self.id

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

    def push(self):
        self.locals.append(dict())

    def pop(self):
        self.locals.pop()

    def allocate(self):
        ret = RegisterHandle(self.register_ids)
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
    def __init__(self, name, args):
        self.name = name
        self.args = args
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
            ret.append(MethodSignature(method[0].data, signature))
        return ret
MethodSignature.sequential_id = 0

def get_method_signature(name, top_):
    top = top_.xattrs["top"]
    if "signatures" not in top.xattrs:
        top.xattrs["signatures"] = MethodSignature.scan(top)

    for signature in top.xattrs["signatures"]:
        if signature.name == name:
            return signature

class MethodEmitter:
    def __init__(self, top, emitter):
        self.top = top
        self.opcodes = []
        self.scope = Scopes()

    def emit_expr(self, node, register):
        opcodes = []
        if node.i("number"):
            nt = int(node.data)
            opcodes.append(ops["load"].ins(register, abs(nt)))
            if nt < 0:
                opcodes.append(ops["twocomp"].ins(register))
        elif node.of("+", "*", "^", "&", "|", "==", ">=", "<=", ">", "<", "!=", "and", "or"):
            opcodes += self.emit_expr(node[0], register)
            rhs = self.scope.allocate()
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
            }[node.type]].ins(register, rhs, register))
            if node.i("!="):
                opcodes.append(ops["invert"].ins(register))
        elif node.i("not"):
            opcodes += self.emit_expr(node[0], register)
            opcodes.append(ops["invert"].ins(register))
        elif node.i("~"):
            opcodes += self.emit_expr(node[0], register)
            reg = self.scope.allocate()
            opcodes.append(ops["load"].ins(reg, 0xffffffff))
            opcodes.append(ops["xor"].ins(reg, register, register))
        elif node.i("ident"):
            opcodes.append(annotate(ops["mov"].ins(self.scope.resolve(node.data, node), register), "access %s" % node.data))
        elif node.i("-"):
            if len(node.children) == 1:
                opcodes += self.emit_expr(node[0], register)
                opcodes.append(ops["twocomp"].ins(register))
            else:
                opcodes += self.emit_expr(node[0], register)
                rhs = self.scope.allocate()
                opcodes += self.emit_expr(node[1], rhs)
                opcodes.append(ops["twocomp"].ins(rhs))
                opcodes.append(ops["add"].ins(register, rhs, register))
        elif node.i("call"):
            for param, index in zip(node.children[1:], range(len(node.children) - 1)):
                reg = self.scope.allocate()
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
            result = self.scope.allocate()
            opcodes += self.emit_expr(node, result)
        elif node.i("let"):
            reg = self.scope.allocate()
            self.scope.let(node[0].data, reg, node)
            opcodes += annotate(self.emit_expr(node[1], reg), "let %s" % node[0].data)
        elif node.i("return"):
            reg = self.scope.allocate()
            if len(node.children) > 0:
                opcodes += self.emit_expr(node[0], reg)
            else:
                opcodes.append(ops["zero"].ins(reg))
            opcodes.append(ops["return"].ins(reg))
        elif node.i("assignment"):
            reg = self.scope.resolve(node[0].data, node[0])
            opcodes += self.emit_expr(node[1], reg)
        elif node.of("+=", "*=", "-="):
            reg = self.scope.allocate()
            opcodes += self.emit_expr(node[1], reg)
            varreg = self.scope.resolve(node[0].data, node)
            opcodes.append(ops[{
                "+=": "add",
                "*=": "mult",
                "-=": "add"
            }[node.type]].ins(varreg, reg, varreg))
            if node.i("-="):
                opcodes.append(ops["twocomp"].ins(varreg))
        elif node.i("while"):
            result = []
            condition_register = self.scope.allocate()
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
            condition_register = self.scope.allocate()
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
            condition_register = self.scope.allocate()
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
            reg = self.scope.allocate()
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
            self.scope.let(arg.data, self.scope.allocate(), arg)
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

    def evaluate(self, emitter):
        pass

class MethodSegment(Segment):
    def __init__(self, method, signature):
        Segment.__init__(self, "method", 0x00)
        self.method = method
        self.signature = signature
        self.opcodes = None
        self.num_registers=None

    def emit_opcodes(self, emitter):
        emitter = MethodEmitter(self.method, emitter)
        ret = emitter.emit()
        # TODO: This is spaghetti code
        self.num_registers = emitter.scope.register_ids
        self.opcodes = ret
        return ret

    def __str__(self):
        return "MethodSegment(type='%s', typecode=%s, signature=%s)" % (self.humantype, self.realtype, self.signature)

    def print_(self, emitter):
        Segment.print_(self, emitter)
        opcodes = self.emit_opcodes(emitter)
        for opcode, index in zip(opcodes, range(len(opcodes))):
            print("    %2d: %s" % (index, str(opcode)))

    def evaluate(self, emitter):
        self.emit_opcodes(emitter)

class MetadataSegment(Segment):
    def __init__(self, entrypoint):
        Segment.__init__(self, "metadata", 0x01)
        self.entrypoint = entrypoint

    def __str__(self):
        return "MetadataSegment(type='%s', typecode=%s, entrypoint=%s)" % (self.humantype, self.realtype, self.entrypoint)

    def print_(self, emitter):
        Segment.print_(self, emitter)
        print("    entrypoint=%s" % self.entrypoint)

    def evaluate(self, emitter):
        Segment.evaluate(self, emitter)

class Emitter:
    def __init__(self, top):
        self.top = top

    def emit_method(self, method):
        return MethodSegment(method, get_method_signature(method[0].data, method))

    def emit_segments(self):
        for method in self.top:
            method.xattrs["top"] = self.top
        segments = []
        for method in self.top:
            segments.append(self.emit_method(method))

        entrypoint = None
        for segment in segments:
            if isinstance(segment, MethodSegment) and segment.signature.name == "main" and segment.signature.nargs == 0:
                entrypoint = segment.signature

        if entrypoint is None:
            self.top.compile_error("No main()")

        segments.append(MetadataSegment(entrypoint))

        return segments


if __name__ == "__main__":
    import argparse
    argparser = argparse.ArgumentParser()
    argparser.add_argument("file")
    argparser.add_argument("--ast", action="store_true", help="display AST (for debugging)")
    argparser.add_argument("--segments", action="store_true", help="display segments (for debugging)")
    argparser.add_argument("--hexdump", action="store_true", help="display hexdump (for debugging)")
    argparser.add_argument("-o", "--output", metavar="file", help="file for bytecode output")
    args = argparser.parse_args()

    if args.output is None:
        args.output = "%s.slb" % args.file[:-4]

    import parser
    with open(args.file, "r") as f:
        tree = parser.Parser(parser.Toker("\n" + f.read())).parse()

    if args.ast:
        tree.output()

    emitter = Emitter(tree)
    segments = emitter.emit_segments()

    if args.segments:
        for segment in segments:
            segment.print_(emitter)
    else:
        for segment in segments:
            segment.evaluate(emitter)

    import bytecode
    outbytes = bytecode.emit(segments)

    if args.hexdump:
        import binascii
        print(binascii.hexlify(outbytes).decode("ascii"))

    with open(args.output, "wb") as f:
        f.write(outbytes)
