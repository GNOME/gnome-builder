libtmpl_enum_headers = [
  'tmpl-error.h',
  'tmpl-expr-types.h',
]

libtmpl_headers_subdir = 'gnome-builder-' + meson.project_version() + '/template-glib'
libtmpl_headers_dir = join_paths(get_option('includedir'), libtmpl_headers_subdir)

libtmpl_enums = gnome.mkenums('tmpl-enums',
  h_template: 'tmpl-enums.h.in',
  c_template: 'tmpl-enums.c.in',
  sources: libtmpl_enum_headers,
)

libtmpl_conf = configuration_data()
libtmpl_conf.set10('ENABLE_TRACING', get_option('enable_tracing'))
libtmpl_debug = configure_file(
  input: 'tmpl-debug.h.in',
  output: 'tmpl-debug.h',
  configuration: libtmpl_conf,
)

libtmpl_headers = [
  'tmpl-error.h',
  'tmpl-expr-types.h',
  'tmpl-expr.h',
  'tmpl-glib.h',
  'tmpl-scope.h',
  'tmpl-symbol.h',
  'tmpl-template-locator.h',
  'tmpl-template.h',
]

install_headers(libtmpl_headers,
  subdir: libtmpl_headers_subdir
)

libtmpl_generated_headers = [
  libtmpl_debug,
  libtmpl_enums[1],
]

libtmpl_sources = libtmpl_generated_headers + libtmpl_headers + [
  libtmpl_enums[0],
  'tmpl-branch-node.c',
  'tmpl-branch-node.h',
  'tmpl-condition-node.c',
  'tmpl-condition-node.h',
  'tmpl-error.c',
  'tmpl-expr-eval.c',
  'tmpl-expr-node.c',
  'tmpl-expr-node.h',
  'tmpl-expr-parser-private.h',
  'tmpl-expr-private.h',
  'tmpl-expr.c',
  'tmpl-gi-private.h',
  'tmpl-gi.c',
  'tmpl-iter-node.c',
  'tmpl-iter-node.h',
  'tmpl-iterator.c',
  'tmpl-iterator.h',
  'tmpl-lexer.c',
  'tmpl-lexer.h',
  'tmpl-node.c',
  'tmpl-node.h',
  'tmpl-parser.c',
  'tmpl-parser.h',
  'tmpl-scope.c',
  'tmpl-symbol.c',
  'tmpl-template-locator.c',
  'tmpl-template.c',
  'tmpl-text-node.c',
  'tmpl-text-node.h',
  'tmpl-token-input-stream.c',
  'tmpl-token-input-stream.h',
  'tmpl-token.c',
  'tmpl-token.h',
  'tmpl-util-private.h',
  'tmpl-util.c',
]

libtmpl_deps = [
  libgio_dep,
  dependency('gobject-introspection-1.0', version: '>= 1.48.0'),
  libm_dep,
]

flex = find_program('flex')
bison = find_program('bison')
sed = find_program('sed')

tmpl_expr_parser = custom_target('tmpl-expr-parser',
  input: 'tmpl-expr-parser.y',
  output: ['tmpl-expr-parser.c', 'tmpl-expr-parser.h'],
  command: [bison, '@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@']

)

# This is an awful workaround, flex files embed options to control the output
# this fails with meson which wants to output to specific directories
# and just to avoid any regressions we don't want to modify the autotools files
tmpl_expr_scanner_flex = custom_target('tmpl-expr-scanner-flex',
  input: 'tmpl-expr-scanner.l',
  output: 'tmpl-expr-meson-scanner.l',
  capture: true,
  command: [sed, '-e', 's/^%option header-file.*$//',
                 '-e', 's/^%option outfile.*$//', '@INPUT@']
)
tmpl_expr_scanner = custom_target('tmpl-expr-scanner',
  input: tmpl_expr_scanner_flex,
  output: 'tmpl-expr-scanner.c',
  command: [flex, '-o', '@OUTPUT@', '@INPUT@']
)

libtmpl_sources = libtmpl_sources + [
  tmpl_expr_scanner, tmpl_expr_parser,
]

libtmpl_api_version = '1.0'

libtmpl = shared_library('template-glib-' + libtmpl_api_version, libtmpl_sources,
  dependencies: libtmpl_deps,
  version: '0.0.0',
  c_args: ['-DTMPL_GLIB_COMPILATION'],
  install: true,
  install_dir: pkglibdir,
)

libtmpl_dep = declare_dependency(
  sources: libtmpl_generated_headers,
  link_with: libtmpl,
  include_directories: include_directories('.'),
)

if get_option('with_introspection')

  libtmpl_introspection_sources = [
    libtmpl_enums[0],
    libtmpl_enums[1],
    'tmpl-error.c',
    'tmpl-error.h',
    'tmpl-expr.c',
    'tmpl-expr.h',
    'tmpl-expr-types.h',
    'tmpl-scope.c',
    'tmpl-scope.h',
    'tmpl-symbol.c',
    'tmpl-symbol.h',
    'tmpl-template-locator.c',
    'tmpl-template-locator.h',
    'tmpl-template.c',
    'tmpl-template.h',
  ]

  libtmpl_gir = gnome.generate_gir(libtmpl,
    sources: libtmpl_introspection_sources,
    nsversion: libtmpl_api_version,
    namespace: 'Template',
    symbol_prefix: 'tmpl',
    identifier_prefix: 'Tmpl',
    includes: ['Gio-2.0'],
    install: true,
    install_dir_gir: pkggirdir,
    install_dir_typelib: pkgtypelibdir,
    extra_args: [
      '-DTMPL_GLIB_COMPILATION',
    ]
  )

  if get_option('with_vapi')

    libtmpl_vapi = gnome.generate_vapi('template-glib-' + libtmpl_api_version,
      sources: [libtmpl_gir[0], 'Template-1.0.metadata'],
      packages: 'gio-2.0',
      install: true,
      install_dir: pkgvapidir,
    )

  endif
endif
