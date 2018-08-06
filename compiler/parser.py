import string

class ParseError(ValueError):
    def __init__(self, explanation, line, chr):
        ValueError.__init__(self, "%s (near line %s, char %s)" % (explanation, line + 1, chr))

class Token:
    def __init__(self, type, data=None):
        self.type = type
        self.data = data

    def isn(self, type, data=None):
        if self.type != type:
            return False
        if data is not None and self.data != data:
            return False
        return True

    def of(self, *types):
        for type in types:
            if self.isn(type):
                return True
        return False

    def __repr__(self):
        return "%s(%s)" % (self.type, self.data if self.data is not None else "")

    def __str__(self):
        return repr(self)

class Toker:
    keywords = ["fn", "let", "and", "or", "not", "if", "else", "while", "for", "return", "true", "false"]
    def __init__(self, src):
        self.src = src
        self.ptr = 0
        self.line = 0
        self.chn = 0

    def ch(self, num=1):
        return self.src[self.ptr:self.ptr + num]

    @property
    def chi(self):
        return self.ptr - self.chn

    def adv(self, num=1):
        self.ptr += num
        return self.src[self.ptr - num:self.ptr]

    def is_eof(self):
        return self.ptr >= len(self.src)

    def next(self):
        if self.is_eof():
            return Token("EOF")

        if self.ch(2) == "//":
            while self.ch() != "\n":
                self.adv()
            return self.next()
        elif self.ch() in string.ascii_letters:
            s = ""
            while self.ch() in string.ascii_letters + string.digits + "_" and not self.is_eof():
                s += self.adv()
            if s in self.keywords:
                return Token(s)
            else:
                return Token("ident", s)
        elif self.ch() == "'":
            self.adv()
            ret = ord(self.adv())
            if self.adv() != "'":
                self.throw("Unclosed character literal.")
            return Token("number", str(ret))
        elif self.ch() in string.digits:
            s = ""
            while self.ch() in string.digits:
                s += self.adv()
            return Token("number", s)
        elif self.ch(2) in ["<=", ">=", "==", "!=", "+=", "-=", "*=", "/=", "++", "--"]:
            return Token(self.adv(2))
        elif self.ch() in "{}(),;=<>*/+-^|&~%:.":
            return Token(self.adv())
        elif self.ch() == "\n":
            self.adv()
            self.line += 1
            self.chn = self.ptr
            while self.ch() in " \t" and self.ptr < len(self.src):
                self.adv()
            return self.next()
        elif self.ch() in " \t":
            while self.ch() in " \t":
                self.adv()
            return self.next()
        else:
            raise ValueError("Unexpected character '%s'" % self.ch())

    def throw(self, explanation):
        raise ParseError(explanation, self.line, self.chi)

    def peek(self, num=1):
        ptr = self.ptr
        line = self.line
        chn = self.chn
        ret = None
        for i in range(num):
            ret = self.next()
        self.ptr = ptr
        self.line = line
        self.chn = chn
        return ret

    def isn(self, type, data=None, num=1):
        peek = self.peek(num)
        if peek.type != type:
            return False
        if data is not None and peek.data != data:
            return False
        return True

    def expect(self, type, data=None):
        next = self.next()
        if next.type != type:
            self.throw("Expected '%s', got '%s'" % (type, next.type))
        if data is not None and next.data != data:
            self.throw("Found '%s' but expected data of '%s' as opposed to '%s'" % (type, data, next.data))
        return next

class Node:
    def __init__(self, typ, *children, data=None):
        self.line = last_parser.toker.line
        for child in children:
            if type(child) is not Node:
                raise ValueError("Unexpected type %s of child (expected %s)" % (type(child), Node))
            if child.line < self.line:
                self.line = child.line
        if type(typ) is Token:
            self.type = typ.type
            self.data = typ.data
        else:
            self.type = typ
            self.data = data
        self.children = list(children)
        self.xattrs = dict()

    def __repr__(self):
        return "%s(%s)%s" % (self.type,
            self.data if self.data is not None else "",
            " %s" % self.xattrs if len(self.xattrs) > 0 else "")

    def __str__(self):
        return repr(self)

    def output(self, indent=0):
        print("%s %s%s" % (self.line, "    " * indent, repr(self)))
        for child in self.children:
            child.output(indent=indent + 1)

    def __getitem__(self, index):
        if type(index) is str:
            for child in self.children:
                if child.type == index:
                    return child
            raise ValueError("Asked for nonexistent child %s" % index)
        else:
            return self.children[index]

    def add(self, *children):
        for child in children:
            if type(child) is not Node:
                raise ValueError("Unexpected type %s of child (expected %s)" % (type(child), Node))
            if child.line < self.line:
                self.line = child.line
        self.children += list(children)

    def isn(self, type):
        return self.type == type

    def i(self, type):
        return self.isn(type)

    def of(self, *types):
        for typ in types:
            if self.i(typ):
                return True
        return False

    def __iter__(self):
        return iter(self.children)

    def __len__(self):
        return len(self.children)

    def compile_error(self, message):
        raise ValueError("Line %s @ %s : '%s'" % (self.line, repr(self), message))

last_parser = None

class Parser:
    def __init__(self, toker):
        self.toker = toker
        global last_parser
        last_parser = self

    def next(self):
        return self.toker.next()

    def peek(self, num=1):
        return self.toker.peek(num)

    def isn(self, type, data=None, num=1):
        return self.toker.isn(type, data, num)

    def expect(self, type, data=None):
        return self.toker.expect(type, data)

    def throw(self, explanation):
        if type(explanation) is Token:
            self.toker.throw("Unexpected token %s" % explanation)
        else:
            self.toker.throw(explanation)

    def parse_qualified_name(self):
        lhs = Node(self.expect("ident"))
        if self.isn("."):
            self.expect(".")
            return Node(".", self.parse_qualified_name(), lhs)
        return lhs

    def parse_type(self):
        lhs = self.parse_qualified_name()
        if self.isn("<"):
            self.expect("<")
            lhs = Node("<>", lhs)
            while True:
                lhs.add(self.parse_type())
                if not self.isn(","):
                    break
                self.expect(",")
            self.expect(">")
        return lhs

    def parse_type_annotation(self):
        self.expect(":")
        return self.parse_type()

    def parse_fn_params(self):
        self.expect("(")
        nod = Node("fnparams")
        should_loop = not self.isn(")")
        while should_loop:
            param = Node("fnparam", Node(self.expect("ident")))
            self.expect(":")
            param.add(self.parse_type())
            nod.add(param)
            if not self.isn(","):
                break
            self.expect(",")
        self.expect(")")
        return nod

    def parse_parens(self):
        if self.isn("("):
            self.expect("(")
            ret = self.parse_expr()
            self.expect(")")
            return ret
        elif self.peek().type in ["-", "~"]:
            return Node(self.next(), self.parse_parens())
        elif self.isn("not"):
            return Node(self.expect("not"), self.parse_comparison())
        elif self.isn("number"):
            return Node(self.expect("number"))
        elif self.peek().of("true", "false"):
            return Node(self.next())
        elif self.isn("ident"):
            if self.isn("(", num=2):
                return self.parse_fcall()
            else:
                return Node(self.expect("ident"))
        else:
            self.throw(self.next())

    def parse_bitwise(self):
        nod = self.parse_parens()
        while self.peek().type in "&^|":
            nod = Node(self.next(), nod, self.parse_parens())
        return nod

    def parse_mult(self):
        nod = self.parse_bitwise()
        while self.isn("*") or self.isn("/") or self.isn("%"):
            nod = Node(self.next(), nod, self.parse_bitwise())
        return nod

    def parse_add(self):
        nod = self.parse_mult()
        while self.isn("+") or self.isn("-"):
            nod = Node(self.next(), nod, self.parse_mult())
        return nod

    def parse_comparison(self):
        nod = self.parse_add()
        if self.peek().type in ["==", ">=", "<=", ">", "<", "!="]:
            nod = Node(self.next(), nod, self.parse_add())
        return nod

    def parse_logical(self):
        nod = self.parse_comparison()
        while self.peek().type in ["and", "or"]:
            nod = Node(self.next(), nod, self.parse_comparison())
        return nod

    def parse_expr(self):
        return self.parse_logical()

    def parse_fcall(self):
        nod = Node("call", Node(self.expect("ident")))
        self.expect("(")
        while not self.isn(")"):
            nod.add(self.parse_expr())
            if self.isn(","):
                self.next()
        self.expect(")")

        while self.isn(";"):
            self.expect(";")

        return nod

    def parse_statement(self):
        tok = self.peek()
        if tok.isn("{"):
            self.next()
            nod = Node("statements")
            while not self.peek().isn("}"):
                nod.add(self.parse_statement())
            self.expect("}")
            return nod
        elif tok.isn("if"):
            ret = Node(self.next(), self.parse_expr(), self.parse_statement())
            if self.isn("else"):
                self.expect("else")
                ret.add(self.parse_statement())
            return ret
        elif tok.isn("return"):
            ret = Node(self.next())
            if not self.isn(";"):
                ret.add(self.parse_expr())
            if self.isn(";"):
                self.expect(";")
            return ret
        elif tok.isn("while"):
            return Node(self.next(), self.parse_expr(), self.parse_statement())
        elif tok.isn("for"):
            nod = Node(self.expect("for"))
            if self.isn("("):
                self.expect("(")
            nod.add(self.parse_statement())
            nod.add(self.parse_expr())
            if self.isn(";"):
                self.expect(";")
            nod.add(self.parse_statement())
            if self.isn(")"):
                self.expect(")")
            nod.add(self.parse_statement())
            return nod
        elif tok.isn("let"):
            nod = Node(self.expect("let"), Node(self.expect("ident")))
            if self.isn(":"):
                self.expect(":")
                nod.add(self.parse_type())
            else:
                # This is so that the second child of a `let` node is always
                # the type, and the third is always the value or nonpresent.
                nod.add(Node("infer_type"))
            if self.isn("="):
                self.expect("=")
                nod.add(self.parse_expr())
            while self.isn(";"):
                self.expect(";")
            return nod
        elif tok.isn("ident"):
            ident = self.peek()
            if self.isn("=", num=2):
                self.next()
                self.next()
                expr = self.parse_expr()
                while self.isn(";"):
                    self.expect(";")
                return Node("assignment", Node(ident), expr)
            elif self.isn("(", num=2):
                return self.parse_fcall()
            elif self.peek(num=2).type in ["+=", "-=", "*=", "/="]:
                ident = self.next()
                nod = Node(self.next(), Node(ident), self.parse_expr())
                while self.isn(";"):
                    self.expect(";")
                return nod
            elif self.peek(num=2).type in ["++", "--"]:
                ident = self.next()
                nod = Node(self.next(), Node(ident))
                while self.isn(";"):
                    self.expect(";")
                return nod
            else:
                self.next()
                self.throw(self.next())
        else:
            self.throw(tok)

    def parse_fn(self):
        tok = self.expect("fn")
        fn = Node("fn", Node(self.expect("ident")), self.parse_fn_params(), self.parse_type_annotation(), self.parse_statement())
        return fn

    def parse(self):
        global last_parser
        last_parser = self
        nod = Node("top")
        while not self.isn("EOF"):
            nod.add(self.parse_fn())
        return nod

def parse_type(data):
    return Parser(Toker(data)).parse_type()

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    args = parser.parse_args()

    data = None
    with open(args.filename, "r") as f:
        data = f.read()

    Parser(Toker(data)).parse().output()
