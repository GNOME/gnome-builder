if get_option('with_gnome_code_assistance')

gca_resources = gnome.compile_resources(
  'gca-resources',
  'gnome-code-assistance.gresource.xml',
  c_name: 'ide_gca',
)

gca_sources = [
  'gca-diagnostics.c',
  'gca-diagnostics.h',
  'gca-service.c',
  'gca-service.h',
  'gca-structs.c',
  'gca-structs.h',
  'gca-plugin.c',
  'ide-gca-diagnostic-provider.c',
  'ide-gca-diagnostic-provider.h',
  'ide-gca-preferences-addin.c',
  'ide-gca-preferences-addin.h',
  'ide-gca-service.c',
  'ide-gca-service.h',
]

gnome_builder_plugins_sources += files(gca_sources)
gnome_builder_plugins_sources += gca_resources[0]

install_data('org.gnome.builder.gnome-code-assistance.gschema.xml',
  install_dir: schema_dir)

endif
