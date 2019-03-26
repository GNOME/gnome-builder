plugins_sources += files([
  'auto-save-plugin.c',
  'gbp-auto-save-buffer-addin.c',
])

plugin_auto_save_resources = gnome.compile_resources(
  'gbp-auto-save-resources',
  'auto-save.gresource.xml',
  c_name: 'gbp_auto_save',
)

plugins_sources += plugin_auto_save_resources
