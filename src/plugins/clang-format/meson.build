plugins_sources += files([
  'clang-format-plugin.c',
  'gbp-clang-formatter.c',
])

plugin_clang_format_resources = gnome.compile_resources(
  'gb-clang-format-resources',
  'clang-format.gresource.xml',
  c_name: 'gb_clang_format',
)

plugins_sources += plugin_clang_format_resources

clang_format_dir = join_paths(pkgdatadir, 'clang-format')
install_data(['.clang-format'], install_dir: clang_format_dir)

if not find_program('clang-format', required: false).found()
  message('Please install clang-format as runtime dependency')
endif
