if get_option('plugin_gnome_code_assistance')

install_data('org.gnome.builder.gnome-code-assistance.gschema.xml', install_dir: schema_dir)

plugins_sources += files([
  'gca-diagnostics.c',
  'gca-service.c',
  'gca-structs.c',
  'gca-plugin.c',
  'ide-gca-diagnostic-provider.c',
  'ide-gca-preferences-addin.c',
  'ide-gca-service.c',
])

plugin_gnome_code_assistance_resources = gnome.compile_resources(
  'gnome-code-assistance-resources',
  'gnome-code-assistance.gresource.xml',
  c_name: 'gbp_gnome_code_assistance',
)

plugins_sources += plugin_gnome_code_assistance_resources

endif
