libide_header_dir = join_paths(get_option('includedir'), 'gnome-builder', 'libide')
libide_header_subdir = join_paths('gnome-builder', 'libide')

libide_enum_headers = []
libide_generated_headers = []
libide_public_headers = []
libide_public_sources = []
libide_private_sources = []
libide_args = []

version_data = configuration_data()
version_data.set('MAJOR_VERSION', MAJOR_VERSION)
version_data.set('MINOR_VERSION', MINOR_VERSION)
version_data.set('MICRO_VERSION', MICRO_VERSION)
version_data.set('VERSION', meson.project_version())
version_data.set_quoted('BUILD_CHANNEL', get_option('with_channel'))
version_data.set_quoted('BUILD_TYPE', get_option('buildtype'))

libide_version_h = configure_file(
          input: 'ide-version.h.in',
         output: 'ide-version.h',
    install_dir: libide_header_dir,
        install: true,
  configuration: version_data)
libide_generated_headers += [libide_version_h]

libide_build_ident_h = vcs_tag(
     fallback: meson.project_version(),
        input: 'ide-build-ident.h.in',
       output: 'ide-build-ident.h',
)
libide_generated_headers += [libide_build_ident_h]

libide_public_headers += [
  'ide.h',
  'ide-context.h',
  'ide-global.h',
  'ide-object.h',
  'ide-pausable.h',
  'ide-service.h',
  'ide-types.h',
  'ide-version-macros.h',
]

libide_public_sources += [
  'ide.c',
  'ide-context.c',
  'ide-object.c',
  'ide-pausable.c',
  'ide-service.c',
]

subdir('application')
subdir('buildconfig')
subdir('buildui')
subdir('buildsystem')
subdir('buffers')
subdir('config')
subdir('debugger')
subdir('devices')
subdir('diagnostics')
subdir('doap')
subdir('directory')
subdir('documentation')
subdir('editor')
subdir('files')
subdir('formatting')
subdir('genesis')
subdir('greeter')
subdir('gsettings')
subdir('highlighting')
subdir('keybindings')
subdir('langserv')
subdir('layout')
subdir('local')
subdir('logging')
subdir('modelines')
subdir('plugins')
subdir('preferences')
subdir('projects')
subdir('rename')
subdir('runner')
subdir('runtimes')
subdir('search')
subdir('snippets')
subdir('sourceview')
subdir('subprocess')
subdir('symbols')
subdir('template')
subdir('terminal')
subdir('testing')
subdir('threading')
subdir('toolchain')
subdir('transfers')
subdir('util')
subdir('vcs')
subdir('workbench')
subdir('workers')

libide_enums = gnome.mkenums('ide-enums',
      h_template: 'ide-enums.h.in',
      c_template: 'ide-enums.c.in',
         sources: libide_enum_headers,
  install_header: true,
     install_dir: libide_header_dir,
)
libide_public_sources += [libide_enums[0]]
libide_generated_headers += [libide_enums[1]]

libide_conf = configuration_data()
libide_conf.set10('ENABLE_TRACING', get_option('enable_tracing'))
libide_conf.set('BUGREPORT_URL', 'https://gitlab.gnome.org/GNOME/gnome-builder/issues')
libide_debug_h = configure_file(
         input: 'ide-debug.h.in',
         output: 'ide-debug.h',
  configuration: libide_conf,
        install: true,
    install_dir: libide_header_dir,
)
libide_generated_headers += [libide_debug_h]

install_headers([
  'ide.h',
  'ide-context.h',
  'ide-global.h',
  'ide-object.h',
  'ide-pausable.h',
  'ide-service.h',
  'ide-types.h',
  'ide-version-macros.h',
], subdir: libide_header_subdir)

libide_resources = gnome.compile_resources('ide-resources',
  'libide.gresource.xml',
  c_name: 'ide',
)
libide_generated_headers += [libide_resources[1]]

libide_icons_resources = gnome.compile_resources('ide-icons-resources',
  join_paths(meson.source_root(), 'data/icons/hicolor/icons.gresource.xml'),
  source_dir: join_paths(meson.source_root(), 'data/icons/hicolor'),
  c_name: 'ide_icons',
)
libide_generated_headers += [libide_icons_resources[1]]

libide_sources = ['gconstructor.h']
libide_sources += libide_private_sources
libide_sources += libide_generated_headers
libide_sources += libide_public_sources

contrib_dir = join_paths(meson.source_root(), 'contrib/')

if get_option('with_webkit')
  libide_sources += ['webkit/ide-webkit.c']
endif

if get_option('with_editorconfig')
  libide_sources += [
    'editorconfig/editorconfig-glib.c',
    'editorconfig/ide-editorconfig-file-settings.c',
  ]
endif

# We want to find the subdirectory to install our override into:
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

if overridedir.startswith(libdir): # Should always be True..
  overridedir = overridedir[len(libdir) + 1:]

print(overridedir)
'''

ret = run_command([python3, '-c', get_overridedir])
if ret.returncode() != 0
  error('Failed to determine pygobject overridedir')
else
  pygobject_override_dir = join_paths(get_option('libdir'), ret.stdout().strip())
endif

install_data('Ide.py', install_dir: pygobject_override_dir)

libide_deps = [
  libdazzle_dep,
  libgio_dep,
  libgiounix_dep,
  libgtk_dep,
  libgtksource_dep,
  libjson_glib_dep,
  libjsonrpc_glib_dep,
  libpangoft2_dep,
  libpeas_dep,
  libtemplate_glib_dep,
  libvte_dep,
  libxml2_dep,
]

if get_option('with_webkit')
  libide_deps += [dependency('webkit2gtk-4.0', version: '>=2.12.0')]
endif

if get_option('with_editorconfig')
  libide_args += '-DENABLE_EDITORCONFIG'
  libide_deps += libeditorconfig_dep
endif

libide = shared_library('ide-' + libide_api_version,
  libide_resources + libide_icons_resources + libide_sources,
  dependencies: libide_deps,
        c_args: libide_args,
       install: true,
   install_dir: pkglibdir,
 install_rpath: pkglibdir_abs,
)

libide_dep = declare_dependency(
              sources: libide_generated_headers,
         dependencies: [ libdazzle_dep,
                         libgio_dep,
                         libgtk_dep,
                         libgtksource_dep,
                         libjson_glib_dep,
                         libtemplate_glib_dep,
                         libjsonrpc_glib_dep ],
            link_with: libide,
  include_directories: include_directories('.'),
)

# Doesn't link to libide
# TODO: I think we can remove most of the links here and just setup includes
libide_plugin_dep = declare_dependency(
              sources: libide_generated_headers,
  include_directories: include_directories('.'),
         dependencies: [ libdazzle_dep,
                         libgio_dep,
                         libgtk_dep,
                         libgtksource_dep,
                         libtemplate_glib_dep,
                         libjson_glib_dep,
                         libjsonrpc_glib_dep,
                         libvte_dep ],
)

pkgg = import('pkgconfig')

pkgg.generate(
    libraries: [libide],
      subdirs: [ 'gnome-builder/libide' ],
      version: meson.project_version(),
         name: 'Libide',
     filebase: 'libide-1.0',
  description: 'Libide contains the components used to build the GNOME Builder IDE.',
     requires: [ 'gtk+-3.0', 'gtksourceview-3.0', 'libdazzle-1.0', 'template-glib-1.0', 'jsonrpc-glib-1.0', 'libpeas-1.0', 'vte-2.91' ],
  install_dir: join_paths(pkglibdir, 'pkgconfig'),
)

libide_gir = gnome.generate_gir(libide,
	      sources: libide_generated_headers + libide_public_headers + libide_public_sources,
            nsversion: libide_api_version,
            namespace: 'Ide',
        symbol_prefix: 'ide',
    identifier_prefix: 'Ide',
             includes: [ 'Gio-2.0', 'GtkSource-3.0', 'Peas-1.0', 'Dazzle-1.0', 'Json-1.0', 'Template-1.0', 'Vte-2.91' ],
	      install: true,
      install_dir_gir: pkggirdir,
  install_dir_typelib: pkgtypelibdir,
           extra_args: [ '--c-include=ide.h', '--pkg-export=libide-1.0' ]
)

if get_option('with_vapi')

configure_file(
          input: 'libide-' + libide_api_version + '.deps',
         output: 'libide-' + libide_api_version + '.deps',
  configuration: configuration_data(),
        install: true,
    install_dir: pkgvapidir,
)

libide_vapi = gnome.generate_vapi('libide-' + libide_api_version,
      sources: libide_gir[0],
      install: true,
  install_dir: pkgvapidir,
     packages: [ 'gio-2.0',
                 'gtk+-3.0',
                 'gtksourceview-3.0',
                 'json-glib-1.0',
                 'libdazzle-1.0',
                 'libpeas-1.0',
                 'template-glib-1.0' ],
)

endif
