project('gnome-builder', 'c',
          license: 'GPL3+',
          version: '3.37.0',
    meson_version: '>= 0.51.2',
  default_options: [ 'c_std=gnu11',
                     'cpp_std=gnu++2a',
                     'warning_level=2',
                     'channel=other',
                   ],
)

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]

libide_api_version = '@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION)

pkgdocdir_abs = join_paths(get_option('prefix'), get_option('datadir'), 'doc', 'gnome-builder')
pkglibdir_abs = join_paths(get_option('prefix'), get_option('libdir'), 'gnome-builder')
pkglibdir = join_paths(get_option('libdir'), 'gnome-builder')
pkgincludedir = join_paths(get_option('includedir'), 'gnome-builder')
pkggirdir = join_paths(get_option('datadir'),'gnome-builder', 'gir-1.0')
pkgtypelibdir = join_paths(get_option('libdir'), 'gnome-builder', 'girepository-1.0')

safe_path = get_option('with_safe_path')

# These default values are obtained by running /bin/sh without setting PATH
if safe_path == ''
  if host_machine.system() == 'freebsd'
    safe_path = '/bin:/usr/bin:/usr/local/bin'
  elif host_machine.system() == 'dragonfly'
    safe_path = '/bin:/usr/bin:/usr/local/bin:/usr/pkg/bin'
  elif host_machine.system() == 'netbsd'
    safe_path = '/usr/bin:/bin:/usr/pkg/bin:/usr/local/bin'
  elif host_machine.system() == 'openbsd'
    safe_path = '/usr/bin:/bin:/usr/X11R6/bin:/usr/local/bin'
  else
    safe_path = '/usr/bin:/bin'
  endif
endif

status = [
  '', '',
  'gnome-builder @0@ (@1@)'.format(meson.project_version(), get_option('channel')),
  '', '',
]

# Message of doom
if (version_split[1].to_int() % 2 == 1)
  status += [
    'You are building a debug version of GNOME builder. There may be more',
    'bugs in this version that you are comfortable with. Addtionally, there',
    'is a chance it will eat all of your work and leave you sad and alone.',
    '', ''
  ]
endif

status += [
  'Version ............... : @0@'.format(meson.project_version()),
  'Channel ............... : @0@'.format(get_option('channel')),
  'Build Type ............ : @0@'.format(get_option('buildtype')),
  '',
  'Prefix ................ : @0@'.format(get_option('prefix')),
  'Libdir ................ : @0@'.format(join_paths(get_option('prefix'), get_option('libdir'))),
  'Safe PATH ............. : @0@'.format(safe_path),
  '',
  'Tracing ............... : @0@'.format(get_option('tracing')),
  'Profiling ............. : @0@'.format(get_option('profiling')),
  'tcmalloc_minimal ...... : @0@'.format(get_option('tcmalloc')),
  '',
  'Help Docs ............. : @0@'.format(get_option('help')),
  'API Docs .............. : @0@'.format(get_option('docs')),
  '', ''
]

config_h = configuration_data()
config_h.set_quoted('PACKAGE_NAME', 'gnome-builder')
config_h.set_quoted('PACKAGE_ABI_S', libide_api_version)
config_h.set('PACKAGE_ABI', libide_api_version)
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PACKAGE_STRING', 'gnome-builder-' + meson.project_version())
config_h.set_quoted('PACKAGE_DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
config_h.set_quoted('PACKAGE_ICONDIR', join_paths(get_option('prefix'), get_option('datadir'), 'gnome-builder/icons'))
config_h.set_quoted('PACKAGE_DOCDIR', join_paths(get_option('prefix'), get_option('datadir'), 'doc/gnome-builder'))
config_h.set_quoted('PACKAGE_LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
config_h.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
config_h.set_quoted('PACKAGE_LIBEXECDIR', join_paths(get_option('prefix'), get_option('libexecdir')))
config_h.set_quoted('SAFE_PATH', safe_path)

config_h.set('GETTEXT_PACKAGE', 'PACKAGE_NAME')
config_h.set('LOCALEDIR', 'PACKAGE_LOCALE_DIR')

config_h.set10('ENABLE_NLS', true) # Always enabled

# Development build setup
config_h.set('DEVELOPMENT_BUILD', version_split[1].to_int().is_odd())

# We should probably avoid using these
config_h.set_quoted('SRCDIR', meson.source_root())
config_h.set_quoted('BUILDDIR', meson.build_root())

add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(), # config.h
  '-D_GNU_SOURCE',
  '-DIDE_COMPILATION',
], language: 'c')

libide_args = []

cc = meson.get_compiler('c')
global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  ['-Werror=format-security', '-Werror=format=2' ],
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  # '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
if get_option('profiling')
  test_c_args += '-pg'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach

if cc.has_multi_arguments('-Wmissing-declarations')
  libide_args += '-Wmissing-declarations'
endif

# Detect and set symbol visibility
hidden_visibility_args = []
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
    if cc.get_id() == 'msvc'
      config_h.set('_IDE_EXTERN', '__declspec(dllexport) extern')
    elif cc.has_argument('-fvisibility=hidden')
      config_h.set('_IDE_EXTERN', '__attribute__((visibility("default"))) __declspec(dllexport) extern')
      hidden_visibility_args = ['-fvisibility=hidden']
    endif
  elif cc.has_argument('-fvisibility=hidden')
    config_h.set('_IDE_EXTERN', '__attribute__((visibility("default"))) extern')
    hidden_visibility_args = ['-fvisibility=hidden']
  endif
endif

libide_args += hidden_visibility_args

add_project_arguments(global_c_args, language: 'c')

release_args = []
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
if not get_option('buildtype').startswith('debug')

  # TODO: Maybe reuse 'b_ndebug' option
  add_global_arguments(['-DG_DISABLE_CAST_CHECKS'], language: 'c')

  release_args += [ '-DG_DISABLE_ASSERT' ]

  test_link_args += [
    '-Wl,-Bsymbolic',
    '-fno-plt',
  ]

endif

foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(global_link_args, language: 'c')

if get_option('tcmalloc')
  tcmalloc_ldflags = [
    '-fno-builtin-malloc',
    '-fno-builtin-calloc',
    '-fno-builtin-realloc',
    '-fno-builtin-free',
    '-Wl,--push-state,--no-as-needed',
    '-ltcmalloc_minimal',
    '-Wl,--pop-state'
  ]
  add_project_link_arguments(tcmalloc_ldflags, language: 'c')
endif

# Check if we can use version scripts for ABI exports
ld_supports_version_script = cc.links('''
  int main (void) { return 0; }
''', args: '-Wl,--version-script,' + join_paths(meson.source_root(), 'libide/ide.map'))
message('Linker supports --version-script: @0@'.format(ld_supports_version_script))

# Commonly used deps
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.37.0')
libglib_dep = dependency('glib-2.0', version: '>= 2.61.2')
libgio_dep = dependency('gio-2.0', version: '>= 2.61.2')
libgiounix_dep = dependency('gio-unix-2.0')
libgtk_dep = dependency('gtk+-3.0', version: '>= 3.22.26')
libgtksource_dep = dependency('gtksourceview-4', version: '>= 4.0.0')
libjson_glib_dep = dependency('json-glib-1.0', version: '>= 1.2.0')
libjsonrpc_glib_dep = dependency('jsonrpc-glib-1.0', version: '>= 3.29.91')
libm_dep = cc.find_library('m', required: false)
libpangoft2_dep = dependency('pangoft2', version: '>= 1.38.0')
libpeas_dep = dependency('libpeas-1.0', version: '>= 1.22.0')
libtemplate_glib_dep = dependency('template-glib-1.0', version: '>= 3.28.0')
libvte_dep = dependency('vte-2.91', version: '>= 0.40.2')
libwebkit_dep = dependency('webkit2gtk-4.0', version: '>= 2.22')
libxml2_dep = dependency('libxml-2.0', version: '>= 2.9.0')

check_functions = [
  # pty
  ['HAVE_GRANTPT', 'grantpt'],
  ['HAVE_POSIX_OPENPT', 'posix_openpt'],
  ['HAVE_PTSNAME', 'ptsname'],
  ['HAVE_PTSNAME_R', 'ptsname_r'],
  ['HAVE_UNLOCKPT', 'unlockpt'],
]
foreach func: check_functions
  config_h.set(func[0], cc.has_function(func[1]))
endforeach

#
# If we have sysprof-capture-3, we can export tracing information
# to the SYSPROF_TRACE_FD.
#
libsysprof_capture = dependency('sysprof-capture-3', version: '>= 3.33.1', static: true, required: false)
if libsysprof_capture.found()
  config_h.set10('ENABLE_TRACING_SYSCAP', true)
endif

configure_file(output: 'config.h', configuration: config_h)

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')
pymod = import('python')

subdir('data')
subdir('src')
subdir('po')
subdir('doc')

meson.add_install_script('build-aux/meson/post_install.py')

message('\n  '.join(status))
