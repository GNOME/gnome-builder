plugins_sources += files([
  'restore-cursor-plugin.c',
  'gbp-restore-cursor-buffer-addin.c',
])

plugin_restore_cursor_resources = gnome.compile_resources(
  'gbp-restore-cursor-resources',
  'restore-cursor.gresource.xml',
  c_name: 'gbp_restore_cursor',
)

plugins_sources += plugin_restore_cursor_resources
