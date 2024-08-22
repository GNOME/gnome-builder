libide_header_subdir = join_paths('gnome-builder-@0@'.format(MAJOR_VERSION), 'libide')
libide_header_dir = join_paths(get_option('includedir'), libide_header_subdir)
libide_include_directories = []

if get_option('webkit').enabled()
  libide_webkit_api_conf = configuration_data()
  libide_webkit_api_conf.set('WEBKIT_INCLUDE', webkit_include)
  configure_file(
            input: 'libide-webkit-api.h.in',
           output: 'libide-webkit-api.h',
    configuration: libide_webkit_api_conf,
      install_dir: libide_header_dir,
          install: true,
  )
endif

subdir('core')
subdir('plugins')
subdir('threading')
subdir('io')
subdir('gtk')
subdir('search')
subdir('tweaks')
subdir('code')
subdir('vcs')
subdir('tree')
subdir('projects')
subdir('foundry')
subdir('debugger')
subdir('sourceview')
subdir('gui')
subdir('terminal')
subdir('editor')
subdir('lsp')
subdir('greeter')

if get_option('webkit').enabled()
  subdir('webkit')
endif
