import emitter
import parser
import header
import argparse
import os.path

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
argparser.add_argument("--no-stdlib", action="store_true", help="don't link against standard library")
argparser.add_argument("-o", "--output", metavar="file", help="file for bytecode output")
argparser.add_argument("-i", "--include", metavar="file", action="append", help="files to link against")
argparser.add_argument("-j", "--include-json", metavar="file", action="append", help="json to link against")
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

compiler = emitter.Emitter(tree)
program = compiler.emit_program()
if args.include:
    for include in args.include:
        program.add_include(header.from_slb(include))

if args.include_json:
    for include in args.include_json:
        program.add_include(header.from_json(include))

if not args.no_stdlib:
    compiler_dir = os.path.dirname(__file__)
    program.add_include(header.from_slb(os.path.join(compiler_dir, "stdlib/bin/stdlib.slb")))

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
        segment.print_(compiler)

if args.headers:
    import json
    print(json.dumps(program.get_header_representation().serialize(), indent=4))
if args.ast_after:
    tree.output()

outbytes = compiler.emit_bytes(program)

if args.hexdump:
    import binascii
    print(binascii.hexlify(outbytes).decode("ascii"))

# I don't really know what's going on here, but it's some sort of mypy quirk
with open(args.output, "wb") as f: # type: ignore
    f.write(outbytes) # type: ignore