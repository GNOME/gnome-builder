plugins_sources += files([
  'deviceui-plugin.c',
  'gbp-deviceui-workspace-addin.c',
])

plugin_deviceui_resources = gnome.compile_resources(
  'deviceui-resources',
  'deviceui.gresource.xml',
  c_name: 'gbp_deviceui',
)

plugins_sources += plugin_deviceui_resources
