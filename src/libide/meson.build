libide_header_subdir = join_paths('gnome-builder-@0@'.format(MAJOR_VERSION), 'libide')
libide_header_dir = join_paths(get_option('includedir'), libide_header_subdir)
libide_include_directories = []

subdir('core')
subdir('plugins')
subdir('threading')
subdir('io')
subdir('gtk')
subdir('code')
subdir('vcs')
subdir('projects')
subdir('search')
subdir('foundry')
subdir('debugger')
subdir('gui')
subdir('terminal')
subdir('sourceview')
subdir('editor')
subdir('lsp')
subdir('greeter')
subdir('tree')

if get_option('webkit').enabled()
  subdir('webkit')
endif

py_installation = pymod.find_installation('python3')
py_installation.install_sources('Ide.py', subdir: 'gi' / 'overrides')
