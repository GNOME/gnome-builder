if get_option('plugin_rust_analyzer')

plugins_sources += files([
  'gbp-rust-analyzer-code-action-provider.c',
  'gbp-rust-analyzer-completion-provider.c',
  'gbp-rust-analyzer-diagnostic-provider.c',
  'gbp-rust-analyzer-formatter.c',
  'gbp-rust-analyzer-highlighter.c',
  'gbp-rust-analyzer-hover-provider.c',
  'gbp-rust-analyzer-rename-provider.c',
  'gbp-rust-analyzer-search-provider.c',
  'gbp-rust-analyzer-service.c',
  'gbp-rust-analyzer-symbol-resolver.c',
  'gbp-rust-analyzer-tweaks-addin.c',
  'rust-analyzer-plugin.c',
])

plugin_rust_analyzer_resources = gnome.compile_resources(
  'rust-analyzer-resources',
  'rust-analyzer.gresource.xml',
  c_name: 'rust_analyzer'
)

plugins_sources += plugin_rust_analyzer_resources

install_data(['org.gnome.builder.rust-analyzer.gschema.xml'], install_dir: schema_dir)

endif
