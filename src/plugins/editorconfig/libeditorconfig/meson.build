libeditorconfig_sources = [
  'ec_glob.c',
  'ec_glob.h',
  'editorconfig.c',
  'editorconfig.h',
  'editorconfig/editorconfig.h',
  'editorconfig/editorconfig_handle.h',
  'editorconfig_handle.c',
  'editorconfig_handle.h',
  'global.h',
  'ini.c',
  'ini.h',
  'misc.c',
  'misc.h',
  'utarray.h',
]

libeditorconfig_deps = [
  dependency('libpcre')
]

# FIXME: Actually test these
libeditorconfig_args = [
  '-DHAVE_STRCASECMP',
  '-DHAVE_STRICMP',
  '-DHAVE_STRDUP',
  '-DHAVE_STRNDUP',
  '-DUNIX',
  '-Deditorconfig_VERSION_MAJOR=0',
  '-Deditorconfig_VERSION_MINOR=0',
  '-Deditorconfig_VERSION_PATCH=0',
  '-Deditorconfig_VERSION_SUFFIX=0',
]

libeditorconfig = static_library('editorconfig',
  libeditorconfig_sources,
  dependencies: libeditorconfig_deps,
        c_args: libeditorconfig_args,
           pic: true,
)

libeditorconfig_dep = declare_dependency(
            link_with: libeditorconfig,
  include_directories: include_directories('.'),
)
