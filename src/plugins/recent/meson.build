plugins_sources += files([
  'recent-plugin.c',
  'gbp-recent-section.c',
  'gbp-recent-workbench-addin.c',
])

plugin_recent_resources = gnome.compile_resources(
  'recent-resources',
  'recent.gresource.xml',
  c_name: 'gbp_recent',
)

plugins_sources += plugin_recent_resources
