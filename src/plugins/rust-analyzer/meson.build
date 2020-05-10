if get_option('plugin_rust_analyzer')

plugins_sources += files([
  'rust-analyzer.c',
  'rust-analyzer-service.c',
  'rust-analyzer-completion-provider.c',
  'rust-analyzer-symbol-resolver.c',
  'rust-analyzer-diagnostic-provider.c',
  'rust-analyzer-formatter.c',
  'rust-analyzer-transfer.c',
  'rust-analyzer-workbench-addin.c',
])

plugin_rust_analyzer_resources = gnome.compile_resources(
  'rust-analyzer-resources',
  'rust-analyzer.gresource.xml',
  c_name: 'rust_analyzer'
)

plugins_sources += plugin_rust_analyzer_resources

endif
