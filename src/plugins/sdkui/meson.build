plugins_sources += files([
  'sdkui-plugin.c',
  'gbp-sdkui-tweaks-addin.c',
])

plugin_sdkui_resources = gnome.compile_resources(
  'sdkui-resources',
  'sdkui.gresource.xml',
  c_name: 'gbp_sdkui',
)

plugins_sources += plugin_sdkui_resources
