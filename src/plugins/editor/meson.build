plugins_sources += files([
  'editor-plugin.c',
  'gbp-confirm-save-dialog.c',
  'gbp-editor-application-addin.c',
  'gbp-editor-frame-addin.c',
  'gbp-editor-frame-controls.c',
  'gbp-editor-hover-provider.c',
  'gbp-editor-session-addin.c',
  'gbp-editor-workbench-addin.c',
  'gbp-editor-workspace-addin.c',
])

plugin_editor_resources = gnome.compile_resources(
  'gbp-editor-resources',
  'editor.gresource.xml',
  c_name: 'gbp_editor',
)

plugins_sources += plugin_editor_resources
