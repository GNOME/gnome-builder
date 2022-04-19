if get_option('plugin_valgrind')

plugins_sources += files([
  'valgrind-plugin.c',
  'gbp-valgrind-workbench-addin.c',
])

plugin_valgrind_resources = gnome.compile_resources(
  'valgrind-resources',
  'valgrind.gresource.xml',
  c_name: 'gbp_valgrind',
)

plugins_sources += plugin_valgrind_resources

endif
