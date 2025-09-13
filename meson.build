project('gnome-builder', 'c',
          license: 'GPL3+',
          version: '49.1',
    meson_version: '>= 1.4.0',
  default_options: [ 'c_std=gnu11',
                     'cpp_std=gnu++2a',
                     'warning_level=2',
                     'channel=other',
                   ],
)

config_h = configuration_data()
config_h.set_quoted('PACKAGE_NAME', 'gnome-builder')
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

# Development builds get an alternate application-id
if get_option('development')
  app_id = 'org.gnome.Builder.Devel'
  config_h.set10('DEVELOPMENT_BUILD', true)
else
  app_id = 'org.gnome.Builder'
endif
config_h.set_quoted('APP_ID', app_id)

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
libide_api_version = '@0@'.format(MAJOR_VERSION)

pkgdocdir_abs = join_paths(get_option('prefix'), get_option('datadir'), 'doc', 'gnome-builder')
pkglibdir_abs = join_paths(get_option('prefix'), get_option('libdir'), 'gnome-builder')
pkglibdir = join_paths(get_option('libdir'), 'gnome-builder')
pkgincludedir = join_paths(get_option('includedir'), 'gnome-builder')
pkggirdir = join_paths(get_option('datadir'),'gnome-builder', 'gir-1.0')
pkgtypelibdir = join_paths(get_option('libdir'), 'gnome-builder', 'girepository-1.0')
pkgdatadir = join_paths(get_option('datadir'), 'gnome-builder')

# Check dependencies
glib_req_version = '2.85'
gtk_req_version = '4.15.5'
gtksourceview_req_version = '5.17'
libadwaita_req_version = '1.8.alpha'
libdex_req_version = '0.7'
gobject_introspection_req_version = '1.74'

glib_req = '>= @0@'.format(glib_req_version)
gtk_req = '>= @0@'.format(gtk_req_version)
gtksourceview_req = '>= @0@'.format(gtksourceview_req_version)
libadwaita_req = '>= @0@'.format(libadwaita_req_version)
libdex_req = '>= @0@'.format(libdex_req_version)

glib_major = glib_req_version.split('.')[0].to_int()
glib_minor = glib_req_version.split('.')[1].to_int()
gtk_major = gtk_req_version.split('.')[0].to_int()
gtk_minor = gtk_req_version.split('.')[1].to_int()
adw_major = libadwaita_req_version.split('.')[0].to_int()
adw_minor = libadwaita_req_version.split('.')[1].to_int()
gtksourceview_major = gtksourceview_req_version.split('.')[0].to_int()
gtksourceview_minor = gtksourceview_req_version.split('.')[1].to_int()

if glib_minor % 2 == 1
  glib_minor = glib_minor + 1
endif
if gtk_minor % 2 == 1
  gtk_minor = gtk_minor + 1
endif
if gtksourceview_minor % 2 == 1
  gtksourceview_minor = gtksourceview_minor + 1
endif

# These default values are obtained by running /bin/sh without setting PATH
safe_path = get_option('with_safe_path')
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


# Message of Doom
if version_split[1][0] in ['a', 'b', 'r']
  MINOR_VERSION = 0
  status += [
    'You are building a development version of GNOME Builder. There may be more',
    'bugs in this version than you are comfortable with. Additionally, there',
    'is a chance it will eat all of your work and leave you sad and alone.',
    '', ''
  ]
else
  MINOR_VERSION = version_split[1].to_int()
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
  'Development Build ..... : @0@'.format(get_option('development')),
  'Profiling ............. : @0@'.format(get_option('profiling')),
  'tcmalloc_minimal ...... : @0@'.format(get_option('tcmalloc')),
  '',
  'Help Docs ............. : @0@'.format(get_option('help')),
  'API Docs .............. : @0@'.format(get_option('docs')),
  '',
]

config_h.set_quoted('PACKAGE_ABI_S', libide_api_version)
config_h.set('PACKAGE_ABI', libide_api_version)
config_h.set_quoted('PACKAGE_STRING', 'gnome-builder-' + meson.project_version())
config_h.set_quoted('PACKAGE_DATADIR', join_paths(get_option('prefix'), pkgdatadir))
config_h.set_quoted('PACKAGE_ICONDIR', join_paths(get_option('prefix'), get_option('datadir'), 'gnome-builder/icons'))
config_h.set_quoted('PACKAGE_DOCDIR', join_paths(get_option('prefix'), get_option('datadir'), 'doc/gnome-builder'))
config_h.set_quoted('PACKAGE_LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
config_h.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
config_h.set_quoted('PACKAGE_LIBEXECDIR', join_paths(get_option('prefix'), get_option('libexecdir')))
config_h.set_quoted('SAFE_PATH', safe_path)
config_h.set('GETTEXT_PACKAGE', 'PACKAGE_NAME')
config_h.set('LOCALEDIR', 'PACKAGE_LOCALE_DIR')
config_h.set10('ENABLE_NLS', true) # Always enabled

# We should probably avoid using these
config_h.set_quoted('SRCDIR', meson.project_source_root())
config_h.set_quoted('BUILDDIR', meson.project_build_root())

add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.project_build_root(), # config.h
  '-D_GNU_SOURCE',
  '-DIDE_COMPILATION',
], language: 'c')

libide_args = []

cc = meson.get_compiler('c')
global_c_args = [
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major, glib_minor),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(glib_major, glib_minor),
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_@0@_@1@'.format(gtk_major, gtk_minor),
  '-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_@0@_@1@'.format(gtk_major, gtk_minor),
  '-DGDK_DISABLE_DEPRECATION_WARNINGS',
  '-DADW_VERSION_MIN_REQUIRED=ADW_VERSION_@0@_@1@'.format(adw_major, adw_minor),
  '-DADW_VERSION_MAX_ALLOWED=ADW_VERSION_@0@_@1@'.format(adw_major, adw_minor),
  '-DGTK_SOURCE_VERSION_MIN_REQUIRED=GTK_SOURCE_VERSION_@0@_@1@'.format(gtksourceview_major, gtksourceview_minor),
  '-DGTK_SOURCE_VERSION_MAX_ALLOWED=GTK_SOURCE_VERSION_@0@_@1@'.format(gtksourceview_major, gtksourceview_minor),
]
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
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  ['-Werror=format-security', '-Werror=format=2'],
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  # '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
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

if get_option('development')
  global_c_args += '-DDEX_ENABLE_DEBUG'
endif

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
''', args: '-Wl,--version-script,' + join_paths(meson.project_source_root(), 'libide/ide.map'))
message('Linker supports --version-script: @0@'.format(ld_supports_version_script))

# Commonly used deps
libdex_dep = dependency('libdex-1', version: libdex_req)
libglib_dep = dependency('glib-2.0', version: glib_req)
libgio_dep = dependency('gio-2.0', version: glib_req)
libgiounix_dep = dependency('gio-unix-2.0', version: glib_req)
libgtk_dep = dependency('gtk4', version: gtk_req)
libgir_dep = dependency('girepository-2.0', version: glib_req)
libadwaita_dep = dependency('libadwaita-1', version: libadwaita_req)
libpanel_dep = dependency('libpanel-1', version: '>= 1.7.0')
libgtksource_dep = dependency('gtksourceview-5', version: gtksourceview_req)
libjson_glib_dep = dependency('json-glib-1.0', version: '>= 1.2.0')
libjsonrpc_glib_dep = dependency('jsonrpc-glib-1.0', version: '>= 3.43.0')
libm_dep = cc.find_library('m', required: false)
libpeas_dep = dependency('libpeas-2', version: '>= 1.99.0')
libportal_dep = dependency('libportal-gtk4', required: false)
libtemplate_glib_dep = dependency('template-glib-1.0', version: '>= 3.37.1')
libvte_dep = dependency('vte-2.91-gtk4', version: '>= 0.75.0')
libxml2_dep = dependency('libxml-2.0', version: '>= 2.9.0')

if get_option('webkit').enabled()
  libwebkit_dep = dependency('webkitgtk-6.0', required: false)
  if libwebkit_dep.found()
    webkit_api = 'webkitgtk-6.0'
    webkit_gir = 'WebKit-6.0'
    webkit_include = '#include <webkit/webkit.h>'
  else
    libwebkit_dep = dependency('webkit2gtk-5.0', required: false)
    if not libwebkit_dep.found()
      error('Package webkitgtk-6.0 is required to build with -Dwebkit=enabled, or optionally, webkit2gtk-5.0 which may have security implications')
    endif
    webkit_api = 'webkit2gtk-5.0'
    webkit_gir = 'WebKit2-5.0'
    webkit_include = '#include <webkit2/webkit2.h>'
  endif
  status += [
    'WebKit ................ : @0@ (@1@)'.format(get_option('webkit').enabled(), webkit_api),
    '',
  ]
  config_h.set_quoted('HAVE_WEBKIT', webkit_api)
  config_h.set_quoted('PACKAGE_WEBKIT_GIR_NAME', webkit_gir.split('-')[0])
  config_h.set_quoted('PACKAGE_WEBKIT_GIR_VERSION', webkit_gir.split('-')[1])
else
  status += [
    'WebKit ................ : Disabled',
    '',
  ]
endif

check_functions = [
  # pty
  ['HAVE_GRANTPT', 'grantpt'],
  ['HAVE_POSIX_OPENPT', 'posix_openpt'],
  ['HAVE_PTSNAME', 'ptsname'],
  ['HAVE_PTSNAME_R', 'ptsname_r'],
  ['HAVE_UNLOCKPT', 'unlockpt'],

  # scheduling
  ['HAVE_SCHED_GETCPU', 'sched_getcpu'],
]
foreach func: check_functions
  config_h.set(func[0], cc.has_function(func[1]))
endforeach

# If we have sysprof-capture-4, we can export tracing information
# to the SYSPROF_TRACE_FD.
libsysprof_capture = dependency('sysprof-capture-4', version: '>= 45.0', static: true, required: false)
if libsysprof_capture.found()
  config_h.set10('ENABLE_TRACING_SYSCAP', true)
endif

# Give sources access to know if libportal is available
if libportal_dep.found()
  config_h.set10('ENABLE_LIBPORTAL', true)
endif

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')

status += ['']

subdir('data')
subdir('src')
subdir('po')
subdir('doc')

configure_file(output: 'config.h', configuration: config_h)

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)

meson.add_install_script('build-aux/meson/post_install.py')

message('\n  '.join(status))
