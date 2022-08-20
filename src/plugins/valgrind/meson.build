if get_option('plugin_valgrind')

plugins_sources += files([
  'valgrind-plugin.c',
  'gbp-valgrind-tool.c',
  'gbp-valgrind-tweaks-addin.c',
])

plugin_valgrind_resources = gnome.compile_resources(
  'valgrind-resources',
  'valgrind.gresource.xml',
  c_name: 'gbp_valgrind',
)

plugins_sources += plugin_valgrind_resources

install_data(['org.gnome.builder.valgrind.gschema.xml'], install_dir: schema_dir)

endif
