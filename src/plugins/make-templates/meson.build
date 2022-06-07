if get_option('plugin_make_templates')

plugins_sources += files([
  'make-templates-plugin.c',
  'gbp-make-template.c',
  'gbp-make-template-provider.c',
])

plugin_make_templates_resources = gnome.compile_resources(
  'make-templates-resources',
  'make-templates.gresource.xml',
  c_name: 'gbp_make_templates',
)

plugins_sources += plugin_make_templates_resources

endif
