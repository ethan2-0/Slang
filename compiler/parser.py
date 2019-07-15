import string
from typing import NoReturn, Optional, Tuple, List, NewType, Any, Dict, Union, Iterator

class ParseError(ValueError):
    def __init__(self, explanation: str, line: int, chr: int):
        ValueError.__init__(self, "%s (near line %s, char %s)" % (explanation, line, chr))

class Token:
    def __init__(self, type: str, data: Optional[str]=None) -> None:
        self.type = type
        self.data = data

    def isn(self, type: str, data: Optional[str]=None) -> bool:
        if self.type != type:
            return False
        if data is not None and self.data != data:
            return False
        return True

    def of(self, *types: str) -> bool:
        for type in types:
            if self.isn(type):
                return True
        return False

    def __repr__(self) -> str:
        return "%s(%s)" % (self.type, self.data if self.data is not None else "")

    def __str__(self) -> str:
        return repr(self)

TokerState = NewType("TokerState", Tuple[int, int, int])
class Toker:
    keywords = ["using", "namespace", "class", "override", "entrypoint", "fn", "ctor", "extends", "let", "return", "while", "for", "if", "else", "new", "true", "false", "and", "or", "not", "null", "as", "instanceof"]
    escapes = {"n": "\n", "\\": "\\", "'": "'", "\"": "\"", "r": "\r", "0": "\0", "b": "\b", "v": "\v", "t": "\t", "f": "\f"}
    ident_start_chars = string.ascii_letters + "_"
    ident_chars = ident_start_chars + string.digits
    def __init__(self, src: str) -> None:
        self.src = src
        self.ptr = 0
        self.line = 0
        self.chn = 0

    def ch(self, num: int=1) -> str:
        return self.src[self.ptr:self.ptr + num]

    @property
    def chi(self) -> int:
        return self.ptr - self.chn

    def adv(self, num: int=1) -> str:
        self.ptr += num
        return self.src[self.ptr - num:self.ptr]

    def is_eof(self) -> bool:
        return self.ptr >= len(self.src)

    def tokenize_string_body_ch(self) -> str:
        ch = self.adv()
        ret = None
        if ch == "\\":
            ch = self.adv()
            if ch in Toker.escapes:
                return Toker.escapes[ch]
            self.throw("Invalid escape sequence: '\\%s'" % ch)
        return ch

    def next(self) -> Token:
        if self.is_eof():
            return Token("EOF")

        if self.ch(2) == "//":
            # NOTE: If / does anything other than division and /=, then we need
            #       to make sure // always means a comment.
            while self.ch() != "\n":
                self.adv()
            return self.next()
        elif self.ch() in Toker.ident_start_chars:
            s = ""
            while self.ch() in Toker.ident_chars and not self.is_eof():
                s += self.adv()
            if s in self.keywords:
                return Token(s)
            else:
                return Token("ident", s)
        elif self.ch() == "'":
            self.adv()
            ret = ord(self.tokenize_string_body_ch())
            if self.adv() != "'":
                self.throw("Unclosed character literal.")
            return Token("number", str(ret))
        elif self.ch() == "\"":
            self.adv()
            s = ""
            while self.ch() != "\"":
                s += self.tokenize_string_body_ch()
            self.adv()
            return Token("string", s)
        elif self.ch() in string.digits:
            s = ""
            while self.ch() in string.digits:
                s += self.adv()
            return Token("number", s)
        elif self.ch(2) in ["<=", ">=", "==", "!=", "+=", "-=", "*=", "/=", "++", "--"]:
            return Token(self.adv(2))
        elif self.ch() in "[]{}(),;=<>*/+-^|&~%:.#":
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

    def throw(self, explanation: str) -> NoReturn:
        raise ParseError(explanation, self.line, self.chi)

    def get_state(self) -> TokerState:
        return TokerState((self.ptr, self.line, self.chn))

    def set_state(self, state: TokerState) -> None:
        self.ptr, self.line, self.chn = state

    def peek(self, num: int=1) -> Token:
        state = self.get_state()
        ret = None
        for i in range(num):
            ret = self.next()
        self.set_state(state)
        if ret is None:
            self.throw("Unexpected EOF")
        assert ret is not None
        return ret

    def isn(self, type: str, data: str=None, num: int=1) -> bool:
        peek = self.peek(num)
        if peek.type != type:
            return False
        if data is not None and peek.data != data:
            return False
        return True

    def expect(self, type: str, data: str=None) -> Token:
        next = self.next()
        if next.type != type:
            self.throw("Expected '%s', got '%s'" % (type, next.type))
        if data is not None and next.data != data:
            self.throw("Found '%s' but expected data of '%s' as opposed to '%s'" % (type, data, next.data))
        return next

last_parser: "Optional[Parser]" = None

class Node:
    def __init__(self, typ: Union[str, Token], *children: "Node", data: str=None) -> None:
        assert last_parser is not None
        self.line = last_parser.toker.line
        for child in children:
            if type(child) is not Node:
                raise ValueError("Unexpected type %s of child (expected %s)" % (type(child), Node))
            if child.line < self.line:
                self.line = child.line
        if isinstance(typ, Token):
            self.type = typ.type
            self.data = typ.data
        elif isinstance(typ, str):
            self.type = typ
            self.data = data
        else:
            raise ValueError("Unexpected type %s of typ (expected str or Token)" % type(typ))
        self.children = list(children)
        self.xattrs: Dict[Any, Any] = dict()

    @property
    def data_strict(self) -> str:
        if self.data is None:
            raise ValueError("Node %s: expected non-null data")
        return self.data

    def __repr__(self) -> str:
        return "%s(%s)%s" % (self.type,
            self.data if self.data is not None else "",
            " %s" % self.xattrs if len(self.xattrs) > 0 else "")

    def __str__(self) -> str:
        return repr(self)

    def output(self, indent: int=0) -> None:
        print("%s %s%s" % (self.line, "    " * indent, repr(self)))
        for child in self.children:
            child.output(indent=indent + 1)

    def __getitem__(self, index: Union[str, int]) -> "Node":
        if isinstance(index, str):
            for child in self.children:
                if child.type == index:
                    return child
            raise ValueError("Asked for nonexistent child %s" % index)
        else:
            return self.children[index]

    def has_child(self, type: str) -> bool:
        for child in self.children:
            if child.type == type:
                return True
        return False

    def add(self, *children: "Node") -> None:
        for child in children:
            if type(child) is not Node:
                raise ValueError("Unexpected type %s of child (expected %s)" % (type(child), Node))
            if child.line < self.line:
                self.line = child.line
        self.children += list(children)

    def isn(self, type: str) -> bool:
        return self.type == type

    def i(self, type: str) -> bool:
        return self.isn(type)

    def of(self, *types: str) -> bool:
        for typ in types:
            if self.i(typ):
                return True
        return False

    def __iter__(self) -> "Iterator[Node]":
        return iter(self.children)

    def __len__(self) -> int:
        return len(self.children)

    def compile_error(self, message: str) -> NoReturn:
        raise ValueError("Line %s @ %s : '%s'" % (self.line, repr(self), message))

    def warn(self, message: str) -> None:
        # TODO: use `logging` and do this properly
        print("Line %s @ %s: Warning: %s" % (self.line, repr(self), message))

class Parser:
    method_modifiers = ["override", "entrypoint"]
    def __init__(self, toker: Toker) -> None:
        self.toker = toker
        global last_parser
        last_parser = self

    def next(self) -> Token:
        return self.toker.next()

    def peek(self, num: int=1) -> Token:
        return self.toker.peek(num)

    def isn(self, type: str, data: Optional[str]=None, num: int=1) -> bool:
        return self.toker.isn(type, data, num)

    def expect(self, type: str, data: str=None) -> Token:
        return self.toker.expect(type, data)

    def throw(self, explanation: Union[str, Token]) -> NoReturn:
        if isinstance(explanation, Token):
            self.toker.throw("Unexpected token %s" % explanation)
        else:
            self.toker.throw(explanation)

    def parse_qualified_name(self) -> Node:
        lhs = Node(self.expect("ident"))
        if self.isn("."):
            self.expect(".")
            return Node(".", lhs, self.parse_qualified_name())
        return lhs

    def parse_type(self) -> Node:
        if self.isn("["):
            self.expect("[")
            lhs = Node("[", self.parse_type())
            self.expect("]")
            return lhs
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

    def parse_type_annotation(self) -> Node:
        self.expect(":")
        return self.parse_type()

    def parse_fn_params(self) -> Node:
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

    def parse_parens(self) -> Node:
        if self.peek().type in ["-", "~"]:
            return Node(self.next(), self.parse_parens())
        elif self.isn("not"):
            return Node(self.expect("not"), self.parse_comparison())
        elif self.isn("number"):
            return Node(self.expect("number"))
        elif self.isn("string"):
            return Node(self.expect("string"))
        elif self.peek().of("true", "false", "null"):
            return Node(self.next())
        elif self.isn("new"):
            return Node(self.next(), Node(self.expect("ident")), *self.parse_fcall_params())
        elif self.isn("ident") or self.isn("("):
            if self.isn("("):
                self.expect("(")
                ret = self.parse_expr()
                self.expect(")")
            else:
                ret = Node(self.expect("ident"))
            while self.isn(".") or self.isn("(") or self.isn("["):
                if self.isn("."):
                    ret = Node(self.expect("."), ret, Node(self.expect("ident")))
                elif self.isn("("):
                    ret = Node("call", ret, *self.parse_fcall_params())
                elif self.isn("["):
                    self.expect("[")
                    ret = Node("access", ret, self.parse_expr())
                    self.expect("]")
            while self.isn(";"):
                self.expect(";")
            return ret
        elif self.isn("#"):
            # Why parse_add()? So that #a == b gets parsed as (#a) == (b) as opposed to #(a == b)
            return Node(self.expect("#"), self.parse_add())
        elif self.isn("["):
            self.expect("[")
            state = self.toker.get_state()
            can_parse_type = True
            try:
                self.parse_type()
            except:
                can_parse_type = False
            next_tok = self.next()
            self.toker.set_state(state)
            if next_tok.isn(":"):
                ret = Node("arrinst", self.parse_type())
                self.expect(":")
                ret.add(self.parse_expr())
                self.expect("]")
                return ret
            else:
                ret = Node("[")
                while not self.isn("]"):
                    ret.add(self.parse_expr())
                    if not self.isn("]"):
                        self.expect(",")
                self.expect("]")
                return ret
        else:
            self.throw(self.next())

    def parse_cast(self) -> Node:
        nod = self.parse_parens()
        if self.peek().isn("as"):
            nod = Node(self.expect("as"), nod, self.parse_type())
        return nod

    def parse_bitwise(self) -> Node:
        nod = self.parse_cast()
        while self.peek().type in "&^|":
            nod = Node(self.next(), nod, self.parse_cast())
        return nod

    def parse_mult(self) -> Node:
        nod = self.parse_bitwise()
        while self.isn("*") or self.isn("/") or self.isn("%"):
            nod = Node(self.next(), nod, self.parse_bitwise())
        return nod

    def parse_add(self) -> Node:
        nod = self.parse_mult()
        while self.isn("+") or self.isn("-"):
            nod = Node(self.next(), nod, self.parse_mult())
        return nod

    def parse_comparison(self) -> Node:
        nod = self.parse_add()
        if self.peek().type in ["==", ">=", "<=", ">", "<", "!="]:
            nod = Node(self.next(), nod, self.parse_add())
        if self.peek().isn("instanceof"):
            nod = Node(self.expect("instanceof"), nod, self.parse_type())
        return nod

    def parse_logical(self) -> Node:
        nod = self.parse_comparison()
        while self.peek().type in ["and", "or"]:
            nod = Node(self.next(), nod, self.parse_comparison())
        return nod

    def parse_expr(self) -> Node:
        return self.parse_logical()

    def parse_fcall_params(self) -> List[Node]:
        ret = []
        self.expect("(")
        while not self.isn(")"):
            ret.append(self.parse_expr())
            if self.isn(","):
                self.next()
        self.expect(")")
        return ret

    def parse_fcall(self) -> Node:
        nod = Node("call", Node(self.expect("ident")), *self.parse_fcall_params())

        while self.isn(";"):
            self.expect(";")

        return nod

    def parse_statement(self) -> Node:
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
            lhs_expr = self.parse_expr()
            if self.isn("="):
                self.expect("=")
                rhs_expr = self.parse_expr()
                while self.isn(";"):
                    self.expect(";")
                return Node("assignment", lhs_expr, rhs_expr)
            elif self.peek().of("+=", "-=", "*=", "/="):
                nod = Node(self.next(), lhs_expr, self.parse_expr())
                while self.isn(";"):
                    self.expect(";")
                return nod
            elif self.peek().of("++", "--"):
                nod = Node(self.next(), lhs_expr)
                while self.isn(";"):
                    self.expect(";")
                return nod
            return Node("expr", lhs_expr)
        else:
            self.throw(tok)

    def parse_fn(self) -> Node:
        modifiers = Node("modifiers")
        while self.peek().of(*Parser.method_modifiers):
            modifier = Node(self.next())
            if modifiers.has_child(modifier.type):
                modifier.compile_error("Duplicate modifier %s" % modifier)
            modifiers.add(modifier)
        self.expect("fn")
        name = self.parse_qualified_name()
        return Node("fn", name, self.parse_fn_params(), self.parse_type_annotation() if self.isn(":") else Node("ident", data="void"), self.parse_statement(), modifiers)

    def parse_ctor(self) -> Node:
        return Node(self.expect("ctor"), self.parse_fn_params(), self.parse_statement())

    def parse_class(self) -> Node:
        nod = Node(self.expect("class"), Node(self.expect("ident")))
        if self.isn("extends"):
            nod.add(Node(self.expect("extends"), self.parse_type()))
        else:
            nod.add(Node("extends"))
        body = Node("classbody")
        nod.add(body)
        self.expect("{")
        while not self.isn("}"):
            if self.isn("ident"):
                body.add(Node("classprop", Node(self.expect("ident")), self.parse_type_annotation()))
                while self.isn(";"):
                    self.expect(";")
            elif self.isn("fn") or self.peek().of(*Parser.method_modifiers):
                body.add(self.parse_fn())
            elif self.isn("ctor"):
                body.add(self.parse_ctor())
            else:
                self.throw(self.peek())
        self.expect("}")
        return nod

    def parse_using(self) -> Node:
        self.expect("using")
        ret = Node("using", Node(self.expect("ident")))
        while self.isn("."):
            self.expect(".")
            ret.add(Node(self.expect("ident")))
        while self.isn(";"):
            self.expect(";")
        return ret

    def parse_namespace(self) -> Node:
        self.expect("namespace")
        ret = Node("namespace", Node(self.expect("ident")))
        while self.isn("."):
            self.expect(".")
            ret.add(Node(self.expect("ident")))
        while self.isn(";"):
            self.expect(";")
        return ret

    def parse(self) -> Node:
        global last_parser
        last_parser = self
        nod = Node("top")
        while not self.isn("EOF"):
            if self.isn("fn") or self.peek().of(*Parser.method_modifiers):
                nod.add(self.parse_fn())
            elif self.isn("class"):
                nod.add(self.parse_class())
            elif self.isn("using"):
                nod.add(self.parse_using())
            elif self.isn("namespace"):
                nod.add(self.parse_namespace())
            else:
                self.throw(self.next())
        return nod

def parse_type(data: str) -> Node:
    if not isinstance(data, str):
        raise ValueError("This is a compiler bug.")
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
