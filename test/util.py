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

def do_compile(filename, outfile, include=[], include_json=[], parse_only=False):
    include_specifiers = list(itertools.chain.from_iterable([["--include", incl] for incl in include]))
    include_specifiers += list(itertools.chain.from_iterable([["--include-json", incl] for incl in include_json]))
    return subprocess.run(["python3", resolve_filename("../compiler/emitter.py"), "-o", outfile,] + include_specifiers + (["--parse-only"] if parse_only else []) + [resolve_filename(filename)] + ['--ast', '--segments'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

def assert_compile_succeeds(filename, message="", include=[], include_json=[], parse_only=False):
    assert isinstance(include, list)
    assert isinstance(include_json, list)
    outfile = resolve_temp_path(os.path.split(filename)[1])
    completed_process = do_compile(filename, outfile, include=include, include_json=include_json, parse_only=parse_only)
    # I know, I shouldn't just coerce to utf8...
    stdout = completed_process.stdout.decode("utf8")
    if completed_process.returncode != 0 or message not in stdout:
        print(stdout)
        assert False
    return outfile

def assert_compile_fails(filename, message="", include=[]):
    outfile = resolve_temp_path(os.path.split(filename)[1])
    completed_process = do_compile(filename, outfile, include)
    stdout = completed_process.stdout.decode("utf8")
    if completed_process.returncode == 0 or message not in stdout:
        print(stdout)
        assert False
    return outfile

def interpret(*filenames, expect_fail=False, stdin=None):
    result = subprocess.run(["../interpreter/builddir/interpreter"] + list(filenames), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, input=stdin)
    success = (result.returncode == 0) ^ expect_fail
    if not success:
        print("Stdout:")
        print(result.stdout.decode("utf8"))
        assert success
    return result.stdout.decode("utf8")

def clean_tmp():
    if not os.path.isdir(tmpdir):
        os.makedirs(tmpdir)
    # This won't clear subdirectories, but it shouldn't matter
    for file in os.listdir(tmpdir):
        os.remove(os.path.join(tmpdir, file))
