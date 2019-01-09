libide_header_subdir = join_paths('gnome-builder-@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION), 'libide')
libide_header_dir = join_paths(get_option('includedir'), libide_header_subdir)
libide_include_directories = []

subdir('core')
subdir('plugins')
subdir('threading')
subdir('io')
subdir('code')
subdir('vcs')
subdir('projects')
subdir('search')
subdir('foundry')
subdir('debugger')
subdir('themes')
subdir('gui')
subdir('terminal')
subdir('sourceview')
subdir('lsp')
subdir('editor')
subdir('greeter')
subdir('webkit')
subdir('tree')

# We want to find the subdirectory to install our override into:
python_libprefix = get_option('python_libprefix')
if python_libprefix != ''
  pygobject_override_dir = join_paths(get_option('libdir'), python_libprefix, 'site-packages', 'gi', 'overrides')
else
python3 = find_program('python3')

get_overridedir = '''
import os
import sysconfig

libdir = sysconfig.get_config_var('LIBDIR')
if not libdir:
  libdir = '/usr/lib'

try:
  import gi
  overridedir = gi._overridesdir
except ImportError:
  purelibdir = sysconfig.get_path('purelib')
  overridedir = os.path.join(purelibdir, 'gi', 'overrides')

if overridedir.startswith(libdir):
  overridedir = overridedir[len(libdir) + 1:]
elif overridedir.startswith('@0@'):
  # Do nothing if its in our same prefix
  pass
else:
  overridedir = overridedir[len('/usr/lib') + 1:]

print(overridedir)
'''.format(get_option('prefix'))

ret = run_command([python3, '-c', get_overridedir])
if ret.returncode() != 0
  error('Failed to determine pygobject overridedir')
else
  pygobject_override_dir = join_paths(get_option('libdir'), ret.stdout().strip())
endif
endif

install_data('Ide.py', install_dir: pygobject_override_dir)
