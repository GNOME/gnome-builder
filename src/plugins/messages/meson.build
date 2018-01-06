messages_resources = gnome.compile_resources(
  'messages-resources',
  'messages.gresource.xml',
  c_name: 'gbp_messages',
)

messages_sources = [
  'gbp-messages-editor-addin.c',
  'gbp-messages-editor-addin.h',
  'gbp-messages-panel.c',
  'gbp-messages-panel.h',
  'gbp-messages-plugin.c',
]

gnome_builder_plugins_sources += files(messages_sources)
gnome_builder_plugins_sources += messages_resources[0]
