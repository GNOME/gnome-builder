plugins_sources += files([
  'shortcutui-plugin.c',
  'gbp-shortcutui-dialog.c',
  'gbp-shortcutui-model.c',
  'gbp-shortcutui-row.c',
  'gbp-shortcutui-shortcut.c',
  'gbp-shortcutui-tweaks-addin.c',
])

plugin_shortcutui_resources = gnome.compile_resources(
  'shortcutui-resources',
  'shortcutui.gresource.xml',
  c_name: 'gbp_shortcutui',
)

plugins_sources += plugin_shortcutui_resources
