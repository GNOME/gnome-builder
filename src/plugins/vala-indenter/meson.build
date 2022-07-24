if get_option('plugin_vala_indenter')

plugins_sources += files([
  'ide-vala-indenter.c',
  'vala-indenter-plugin.c',
])

plugin_vala_indenter_resources = gnome.compile_resources(
  'vala-indenter-resources',
  'vala-indenter.gresource.xml',
  c_name: 'gbp_vala_indenter',
)

plugins_sources += plugin_vala_indenter_resources

endif
