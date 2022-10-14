if get_option('plugin_rust_analyzer')

plugin_rust_analyzer_resources = gnome.compile_resources(
  'rust-analyzer-resources',
  'rust-analyzer.gresource.xml',
  c_name: 'rust_analyzer'
)

plugins_sources += plugin_rust_analyzer_resources

endif
