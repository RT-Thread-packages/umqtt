Import('RTT_ROOT')
from building import *

# get current directory
cwd = GetCurrentDir()

# The set of source files associated with this SConscript file.
src = Glob('src/*.c')
src += Glob('src/pkgs/*.c')
src += Glob('src/trans/*.c');

if GetDepend('PKG_USING_UMQTT_EXAMPLE'):
    src += Glob('samples/*.c');

if GetDepend('PKG_USING_UMQTT_TEST'):
    src += Glob('tests/*.c');

path = [cwd + '/inc']

group = DefineGroup('umqtt', src, depend = ['PKG_USING_UMQTT'], CPPPATH = path)

Return('group')

