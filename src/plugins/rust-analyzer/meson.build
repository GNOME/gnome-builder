if get_option('plugin_rust_analyzer')

plugins_deps += [
  dependency('libsoup-2.4'),
]

plugins_sources += files([
  'rust-analyzer.c',
  'rust-analyzer-service.c',
  'rust-analyzer-completion-provider.c',
  'rust-analyzer-symbol-resolver.c',
  'rust-analyzer-diagnostic-provider.c',
  'rust-analyzer-formatter.c',
  'rust-analyzer-highlighter.c',
  'rust-analyzer-hover-provider.c',
  'rust-analyzer-rename-provider.c',
  'rust-analyzer-transfer.c',
  'rust-analyzer-search-provider.c',
  'rust-analyzer-search-result.c',
  'rust-analyzer-workbench-addin.c',
  'rust-analyzer-preferences-addin.c',
])

plugin_rust_analyzer_resources = gnome.compile_resources(
  'rust-analyzer-resources',
  'rust-analyzer.gresource.xml',
  c_name: 'rust_analyzer'
)

plugins_sources += plugin_rust_analyzer_resources

install_data(['org.gnome.builder.rust-analyzer.gschema.xml'], install_dir: schema_dir)

endif
