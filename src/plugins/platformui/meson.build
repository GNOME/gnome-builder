plugins_sources += files([
  'platformui-plugin.c',
  'gbp-platformui-tweaks-addin.c',
])

plugin_platformui_resources = gnome.compile_resources(
  'platformui-resources',
  'platformui.gresource.xml',
  c_name: 'gbp_platformui',
)

plugins_sources += plugin_platformui_resources
