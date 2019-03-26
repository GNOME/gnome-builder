if get_option('plugin_code_index')

install_data('gsettings/org.gnome.builder.plugins.color_picker_plugin.gschema.xml', install_dir: schema_dir)

plugins_deps += libgstyle_dep

plugins_sources += files([
  'gb-color-picker-document-monitor.c',
  'gb-color-picker-editor-addin.c',
  'gb-color-picker-editor-page-addin.c',
  'gb-color-picker-helper.c',
  'gb-color-picker-plugin.c',
  'gb-color-picker-prefs-list.c',
  'gb-color-picker-prefs-palette-list.c',
  'gb-color-picker-prefs-palette-row.c',
  'gb-color-picker-prefs.c',
])

plugin_color_picker_resources = gnome.compile_resources(
  'gbp-color-picker-resources',
  'color-picker.gresource.xml',
  c_name: 'gbp_color_picker',
)

plugins_sources += plugin_color_picker_resources

endif
