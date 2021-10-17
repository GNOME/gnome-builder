plugins_sources += files([
  'clang-format-plugin.c',
  'gb-clang-format-buffer-addin.c',
])

plugin_clang_format_resources = gnome.compile_resources(
  'gb-clang-format-resources',
  'clang-format.gresource.xml',
  c_name: 'gb_clang_format',
)

plugins_sources += plugin_clang_format_resources

if not find_program('clang-format', required: false).found()
  message('Please install clang-format as runtime dependency')
endif
