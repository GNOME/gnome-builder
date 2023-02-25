plugins_sources += files([
  'buildui-plugin.c',
  'gbp-buildui-editor-page-addin.c',
  'gbp-buildui-environment-editor.c',
  'gbp-buildui-environment-row.c',
  'gbp-buildui-log-pane.c',
  'gbp-buildui-omni-bar-section.c',
  'gbp-buildui-pane.c',
  'gbp-buildui-runtime-categories.c',
  'gbp-buildui-runtime-row.c',
  'gbp-buildui-stage-row.c',
  'gbp-buildui-status-indicator.c',
  'gbp-buildui-status-popover.c',
  'gbp-buildui-targets-dialog.c',
  'gbp-buildui-tweaks-addin.c',
  'gbp-buildui-workbench-addin.c',
  'gbp-buildui-workspace-addin.c',
])

plugin_buildui_resources = gnome.compile_resources(
  'buildui-resources',
  'buildui.gresource.xml',
  c_name: 'gbp_buildui',
)

plugins_sources += plugin_buildui_resources
