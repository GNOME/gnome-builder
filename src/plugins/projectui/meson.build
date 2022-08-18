plugins_sources += files([
  'projectui-plugin.c',
  'gbp-projectui-tweaks-addin.c',
])

plugin_projectui_resources = gnome.compile_resources(
  'projectui-resources',
  'projectui.gresource.xml',
  c_name: 'gbp_projectui',
)

plugins_sources += plugin_projectui_resources
