plugins_sources += files([
  'gbp-messages-workspace-addin.c',
  'gbp-messages-panel.c',
  'messages-plugin.c',
])

plugin_messages_resources = gnome.compile_resources(
  'messages-resources',
  'messages.gresource.xml',
  c_name: 'gbp_messages',
)

plugins_sources += plugin_messages_resources
