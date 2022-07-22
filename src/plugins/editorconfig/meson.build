if get_option('plugin_editorconfig')

libeditorconfig_dep = dependency('editorconfig')

plugins_sources += files([
  'editorconfig-glib.c',
  'editorconfig-plugin.c',
  'gbp-editorconfig-file-settings.c',
])

plugin_editorconfig_resources = gnome.compile_resources(
  'gbp-editorconfig-resources',
  'editorconfig.gresource.xml',
  c_name: 'gbp_editorconfig',
)

plugins_sources += plugin_editorconfig_resources
plugins_deps += libeditorconfig_dep

endif
