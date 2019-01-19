project('interpreter', 'c')
src = ['src/main.c', 'src/bytecode.c', 'src/common.c', 'src/interpreter.c', 'src/typesys.c', 'src/classes.c', 'src/garbagecollector.c']
incdir = include_directories('include')
executable('interpreter', src, include_directories: incdir, link_args: ['-Wl,--format=binary', '-Wl,../../compiler/stdlib/bin/stdlib.slb', '-Wl,--format=default'], link_depends: ['../compiler/stdlib/bin/stdlib.slb'])
