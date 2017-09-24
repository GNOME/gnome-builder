if get_option('with_valgrind')

valgrind_resources = gnome.compile_resources(
  'valgrind_plugin',
  'valgrind-plugin.gresource.xml',
  c_name: 'gbp_valgrind',
)

gnome_builder_plugins_sources += valgrind_resources[0]

endif
