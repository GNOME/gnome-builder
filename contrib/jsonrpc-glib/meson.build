libjsonrpc_public_sources = [
  'jsonrpc-client.c',
  'jsonrpc-client.h',
  'jsonrpc-input-stream.c',
  'jsonrpc-input-stream.h',
  'jsonrpc-output-stream.c',
  'jsonrpc-output-stream.h',
  'jsonrpc-server.c',
  'jsonrpc-server.h',
]

libjsonrpc_api_version = '1.0'

jsonrpc_conf = configuration_data()
# TODO: This is just based on gnome-builders version
jsonrpc_conf.set('VERSION', meson.project_version())
jsonrpc_conf.set('MAJOR_VERSION', MAJOR_VERSION)
jsonrpc_conf.set('MINOR_VERSION', MINOR_VERSION)
jsonrpc_conf.set('MICRO_VERSION', MICRO_VERSION)
configure_file(
  input: 'jsonrpc-version.h.in',
  output: 'jsonrpc-version.h',
  configuration: jsonrpc_conf,
)

libjsonrpc_private_sources = [
  'jcon.c',
]

libjsonrpc_deps = [
  libjson_glib_dep,
]

libjsonrpc = shared_library('jsonrpc-glib-' + libjsonrpc_api_version,
  libjsonrpc_private_sources + libjsonrpc_public_sources,
  dependencies: libjsonrpc_deps,
  version: '0.0.0',
  c_args: ['-DJSONRPC_GLIB_COMPILATION'],
  install: true,
  install_dir: pkglibdir,
)

libjsonrpc_dep = declare_dependency(
  link_with: libjsonrpc,
  include_directories: include_directories('.'),
)

if get_option('with_introspection')

  libjsonrpc_gir = gnome.generate_gir(libjsonrpc,
    sources: libjsonrpc_public_sources,
    nsversion: libjsonrpc_api_version,
    namespace: 'Jsonrpc',
    symbol_prefix: 'jsonrpc',
    identifier_prefix: 'Jsonrpc',
    includes: ['Gio-2.0', 'Json-1.0'],
    install: true,
    install_dir_gir: pkggirdir,
    install_dir_typelib: pkgtypelibdir,
    extra_args: [
      '-DTMPL_GLIB_COMPILATION',
      '--c-include=jsonrpc-glib.h',
    ]
  )

  if get_option('with_vapi')

    libjsonrpc_vapi = gnome.generate_vapi('jsonrpc-glib-' + libjsonrpc_api_version,
      sources: libjsonrpc_gir[0],
      packages: ['gio-2.0', 'json-glib-1.0'],
      install: true,
      install_dir: pkgvapidir,
    )

  endif
endif
