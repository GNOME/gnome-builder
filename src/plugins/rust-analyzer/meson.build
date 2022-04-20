if get_option('plugin_rust_analyzer')

plugins_sources += files([
  'rust-analyzer-code-action-provider.c',
  'rust-analyzer-completion-provider.c',
  'rust-analyzer-diagnostic-provider.c',
  'rust-analyzer-formatter.c',
  'rust-analyzer-highlighter.c',
  'rust-analyzer-hover-provider.c',
  'rust-analyzer-pipeline-addin.c',
  'rust-analyzer-plugin.c',
  'rust-analyzer-preferences-addin.c',
  'rust-analyzer-rename-provider.c',
  'rust-analyzer-search-provider.c',
  'rust-analyzer-service.c',
  'rust-analyzer-symbol-resolver.c',
])

plugin_rust_analyzer_resources = gnome.compile_resources(
  'rust-analyzer-resources',
  'rust-analyzer.gresource.xml',
  c_name: 'rust_analyzer'
)

plugins_sources += plugin_rust_analyzer_resources

install_data(['org.gnome.builder.rust-analyzer.gschema.xml'], install_dir: schema_dir)

endif
