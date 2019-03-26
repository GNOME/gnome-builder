plugins_sources += files([
  'gbp-history-editor-page-addin.c',
  'gbp-history-frame-addin.c',
  'gbp-history-item.c',
  'history-plugin.c',
])

plugin_history_resources = gnome.compile_resources(
  'history-resources',
  'history.gresource.xml',
  c_name: 'gbp_history',
)

plugins_sources += plugin_history_resources
