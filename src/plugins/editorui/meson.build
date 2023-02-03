plugins_sources += files([
  'editorui-plugin.c',
  'gbp-editorui-application-addin.c',
  'gbp-editorui-position-label.c',
  'gbp-editorui-preview.c',
  'gbp-editorui-scheme-selector.c',
  'gbp-editorui-search-provider.c',
  'gbp-editorui-search-result.c',
  'gbp-editorui-tweaks-addin.c',
  'gbp-editorui-workbench-addin.c',
  'gbp-editorui-workspace-addin.c',
])

plugin_editorui_resources = gnome.compile_resources(
  'gbp-editorui-resources',
  'editorui.gresource.xml',
  c_name: 'gbp_editorui',
)

plugins_sources += plugin_editorui_resources
