import subprocess
import os
import os.path
import sys
import tempfile
import itertools

def resolve_filename(filename):
    return os.path.join(os.path.split(__file__)[0], filename)

tmpdir = resolve_filename("tmp")

def resolve_temp_path(temp_path):
    return os.path.join(tmpdir, temp_path)

def do_compile(filename, outfile, include=[], include_json=[], parse_only=False, no_stdlib=False):
    arguments = ["python3", resolve_filename("../compiler/compiler.py"), "-o", outfile, "--ast", "--segments"]
    arguments += list(itertools.chain.from_iterable([["--include", incl] for incl in include]))
    arguments += list(itertools.chain.from_iterable([["--include-json", incl] for incl in include_json]))
    if parse_only:
        arguments.append("--parse-only")
    if no_stdlib:
        arguments.append("--no-stdlib")
    arguments += [resolve_filename(filename)]
    return subprocess.run(arguments, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

def assert_compile_succeeds(filename, message="", include=[], include_json=[], parse_only=False, no_warnings=True, no_stdlib=False):
    if not os.path.isdir(tmpdir):
        os.makedirs(tmpdir)
    assert isinstance(include, list)
    assert isinstance(include_json, list)
    if not isinstance(message, list):
        message = [message]
    outfile = resolve_temp_path(os.path.split(filename)[1])
    completed_process = do_compile(filename, outfile, include=include, include_json=include_json, parse_only=parse_only, no_stdlib=no_stdlib)
    # I know, I shouldn't just coerce to utf8...
    stdout = completed_process.stdout.decode("utf8")
    if completed_process.returncode != 0 or any(msg not in stdout for msg in message) or ("Warning" in stdout and no_warnings):
        print(stdout)
        assert False
    return outfile

def assert_compile_fails(filename, message="", include=[], no_stdlib=False):
    if not isinstance(message, list):
        message = [message]
    outfile = resolve_temp_path(os.path.split(filename)[1])
    completed_process = do_compile(filename, outfile, include, no_stdlib=no_stdlib)
    stdout = completed_process.stdout.decode("utf8")
    if completed_process.returncode == 0 or any(msg not in stdout for msg in message):
        print(stdout)
        assert False
    return outfile

def interpret(*filenames, expect_fail=False, stdin=None, timeout=None):
    result = subprocess.run(["../interpreter/builddir/interpreter"] + list(filenames), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, input=stdin, timeout=timeout)
    success = (result.returncode == 0) ^ expect_fail
    if not success:
        print("Stdout:")
        print(result.stdout.decode("utf8"))
        assert success
    return result.stdout.decode("utf8")
