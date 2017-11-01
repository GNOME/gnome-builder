project('gnome-builder', 'c',
          license: 'GPL3+',
          version: '3.26.2',
    meson_version: '>= 0.40.1',
  default_options: [
                     'c_std=gnu11',
                     'cpp_std=c++11',
                     'warning_level=2',
                     'with_channel=other',
                   ],
)

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]

pkgdocdir_abs = join_paths(get_option('prefix'), get_option('datadir'), 'doc', 'gnome-builder')
pkglibdir_abs = join_paths(get_option('prefix'), get_option('libdir'), 'gnome-builder')
pkglibdir = join_paths(get_option('libdir'), 'gnome-builder')
pkgincludedir = join_paths(get_option('includedir'), 'gnome-builder')
pkggirdir = join_paths(get_option('datadir'),'gnome-builder', 'gir-1.0')
pkgtypelibdir = join_paths(get_option('libdir'), 'gnome-builder', 'girepository-1.0')
pkgvapidir = join_paths(get_option('datadir'), 'gnome-builder/vapi')


status = [
  '', '',
  'gnome-builder @0@ (@1@)'.format(meson.project_version(), get_option('with_channel')),
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
  'Channel ............... : @0@'.format(get_option('with_channel')),
  'Build Type ............ : @0@'.format(get_option('buildtype')),
  '',
  'Prefix ................ : @0@'.format(get_option('prefix')),
  'Libdir ................ : @0@'.format(join_paths(get_option('prefix'), get_option('libdir'))),
  '',
  'Tracing ............... : @0@'.format(get_option('enable_tracing')),
  'Profiling ............. : @0@'.format(get_option('enable_profiling')),
  'RDTSCP ................ : @0@'.format(get_option('enable_rdtscp')),
  '', ''
]

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'gnome-builder')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', 'gnome-builder-' + meson.project_version())
conf.set_quoted('PACKAGE_DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
conf.set_quoted('PACKAGE_DOCDIR', join_paths(get_option('prefix'), get_option('datadir'), 'doc/gnome-builder'))
conf.set_quoted('PACKAGE_LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
conf.set_quoted('PACKAGE_LOCALE_DIR', join_paths(get_option('prefix'), get_option('datadir'), 'locale'))
conf.set_quoted('PACKAGE_LIBEXECDIR', join_paths(get_option('prefix'), get_option('libexecdir')))

conf.set('GETTEXT_PACKAGE', 'PACKAGE_NAME')
conf.set('LOCALEDIR', 'PACKAGE_LOCALE_DIR')

conf.set10('ENABLE_NLS', true) # Always enabled

# We should probably avoid using these
conf.set_quoted('SRCDIR', meson.source_root())
conf.set_quoted('BUILDDIR', meson.build_root())

add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(), # config.h
  '-D_GNU_SOURCE',
  '-DIDE_COMPILATION',
], language: 'c')

cc = meson.get_compiler('c')
global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  ['-Werror=format-security', '-Werror=format=2'],
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wmissing-include-dirs',
  '-Wnested-externs',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-strict-aliasing',
  '-Wno-uninitialized',
  '-Wno-unused-parameter',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
if get_option('enable_profiling')
  test_c_args += '-pg'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach
add_project_arguments(
  global_c_args,
  language: 'c'
)

global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
if not get_option('buildtype').startswith('debug')

  # TODO: Maybe reuse 'b_ndebug' option
  add_global_arguments([
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ], language: 'c')

  test_link_args += [
    '-Wl,-Bsymbolic',
    '-fno-plt',
  ]

endif

foreach arg: test_link_args
  if cc.has_argument(arg)
    global_link_args += arg
  endif
endforeach
add_project_link_arguments(
  global_link_args,
  language: 'c'
)

if cc.has_function('sched_getcpu')
  conf.set('HAVE_SCHED_GETCPU', true)
endif

configure_file(
         output: 'config.h',
  configuration: conf
)

# Check if we can use version scripts for ABI exports
ld_supports_version_script = cc.links('''
  int main (void) { return 0; }
''', args: '-Wl,--version-script,' + join_paths(meson.source_root(), 'libide/ide.map'))
message('Linker supports --version-script: @0@'.format(ld_supports_version_script))

# Commonly used deps
libgio_dep = dependency('gio-2.0', version: '>= 2.53.2')
libgiounix_dep = dependency('gio-unix-2.0')
libgtk_dep = dependency('gtk+-3.0', version: '>= 3.22.1')
libpeas_dep = dependency('libpeas-1.0', version: '>= 1.22.0')
libm_dep = cc.find_library('m', required: false)
libjson_glib_dep = dependency('json-glib-1.0', version: '>= 1.2.0')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.26.1', required: false)
libtemplate_glib_dep = dependency('template-glib-1.0', version: '>= 3.26.1', required: false)
libjsonrpc_glib_dep = dependency('jsonrpc-glib-1.0', version: '>= 3.26.1',
  fallback: ['jsonrpc-glib', 'libjsonrpc_glib_dep'],
  default_options: [
    'with_introspection=false',
    'with_vapi=false',
    'package_subdir=' + meson.project_name()
  ],
)

libtemplate_glib_gir = 'Template-1.0'
libtemplate_glib_vapi = 'template-glib-1.0'
if not libtemplate_glib_dep.found()
  libtemplate_glib = subproject('template-glib',
    default_options: [
      'with_introspection=true',
      'with_vapi=true',
      'package_subdir=' + meson.project_name()
    ]
  )
  libtemplate_glib_dep = libtemplate_glib.get_variable('libtemplate_glib_dep')
  libtemplate_glib_gir = libtemplate_glib.get_variable('libtemplate_glib_gir')[0]
  libtemplate_glib_vapi = libtemplate_glib.get_variable('libtemplate_glib_vapi')
endif

libdazzle_gir = 'Dazzle-1.0'
libdazzle_vapi = 'libdazzle-1.0'
libdazzle_bundled = false
if not libdazzle_dep.found()
  libdazzle = subproject('libdazzle',
    default_options: [
      'with_introspection=true',
      'with_vapi=true',
      'package_subdir=' + meson.project_name()
    ]
  )
  libdazzle_dep = libdazzle.get_variable('libdazzle_dep')
  libdazzle_gir = libdazzle.get_variable('libdazzle_gir')[0]
  libdazzle_vapi = libdazzle.get_variable('libdazzle_vapi')
  libdazzle_bundled = true
endif


libgd = subproject('libgd',
  default_options: [
    'static=false',
    'pkglibdir=' + pkglibdir,
    'with-tagged-entry=true'
  ]
)
libgd_dep = libgd.get_variable('libgd_dep')

if get_option('with_flatpak') or get_option('with_git')
  libgit_dep = dependency('libgit2-glib-1.0', version: '>= 0.25.0')

  libgit_thread_safe_check = '''
    #include <libgit2-glib/ggit.h>
    int main(int argc, const char *argv[])
    {
      ggit_init ();
      return ((ggit_get_features() & GGIT_FEATURE_THREADS) != 0) ? 0 : 1;
    }
  '''
  res = cc.run(libgit_thread_safe_check,
    dependencies: libgit_dep,
  )
  if res.returncode() != 0
    error('libgit2 was not compiled with -DTHREADSAFE:BOOL=ON')
  endif

  libgit_ssh_check = '''
    #include <libgit2-glib/ggit.h>
    int main(int argc, const char *argv[])
    {
      ggit_init ();
      return ((ggit_get_features() & GGIT_FEATURE_SSH) != 0) ? 0 : 1;
    }
  '''
  res = cc.run(libgit_ssh_check,
    dependencies: libgit_dep,
  )
  if res.returncode() != 0
    error('libgit2 was not compiled with SSH support')
  endif
endif

gnome = import('gnome')
i18n = import('i18n')

subdir('data')
subdir('data/fonts')
subdir('data/gsettings')
subdir('data/icons')
subdir('data/style-schemes')
subdir('contrib/libeditorconfig')
subdir('contrib/xml')
subdir('contrib/gstyle') # Depends on libxml
subdir('libide')
subdir('plugins')
subdir('po')
subdir('tests')
subdir('doc')

exe_link_args = [ '-pie' ]
exe_c_args = [ '-fPIE' ]

gnome_builder_sources = [
  'src/main.c',
  'src/bug-buddy.c',
  'src/bug-buddy.h',
]

executable('gnome-builder', gnome_builder_sources,
        gui_app: true,
        install: true,
         c_args: exe_c_args,
      link_args: exe_link_args,
  install_rpath: pkglibdir_abs,
   dependencies: libide_deps + [libide_dep],
)

executable('gnome-builder-cli', gnome_builder_sources,
        gui_app: false,
        install: true,
         c_args: exe_c_args,
      link_args: exe_link_args,
  install_rpath: pkglibdir_abs,
   dependencies: libide_deps + [libide_dep],
)

executable('gnome-builder-worker', gnome_builder_sources,
        gui_app: false,
        install: true,
    install_dir: get_option('libexecdir'),
         c_args: exe_c_args,
      link_args: exe_link_args,
  install_rpath: pkglibdir_abs,
   dependencies: libide_deps + [libide_dep],
)

meson.add_install_script('build-aux/meson/meson_post_install.py')

# 'dist' will be used by meson, avoid conflicting
run_target('builder-dist',
  command: [
    find_program('build-aux/meson/meson_dist.sh'),
    meson.project_version(),
    meson.project_name(),
  ]
)

message('\n  '.join(status))
