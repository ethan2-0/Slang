project('interpreter', 'c')
whereami_src = ['lib/src/whereami.c']
src = whereami_src + ['src/main.c', 'src/bytecode.c', 'src/common.c', 'src/interpreter.c', 'src/typesys.c', 'src/classes.c', 'src/garbagecollector.c', 'src/staticvars.c', 'src/nativelibs.c']
incdir = include_directories('include', 'lib/include')
rtlib_source = ['rtlib/rtlib.c']
rtlib = shared_library('rtlib', rtlib_source, include_directories: incdir, name_prefix: '')

cc = meson.get_compiler('c')
libdl = cc.find_library('dl')

executable('interpreter', src, include_directories: incdir, dependencies: libdl, link_args: ['-Wl,--format=binary', '-Wl,../../compiler/stdlib/bin/stdlib.slb', '-Wl,--format=default'], link_depends: ['../compiler/stdlib/bin/stdlib.slb'])
