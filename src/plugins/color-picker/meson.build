if get_option('with_color_picker')

color_picker_resources = gnome.compile_resources(
  'gb-color-picker-resources',
  'gb-color-picker.gresource.xml',
  c_name: 'gb_color_picker',
)

color_picker_sources = [
  'gb-color-picker-editor-addin.c',
  'gb-color-picker-editor-addin.h',
  'gb-color-picker-editor-view-addin.c',
  'gb-color-picker-editor-view-addin.h',
  'gb-color-picker-helper.c',
  'gb-color-picker-helper.h',
  'gb-color-picker-plugin.c',
  'gb-color-picker-document-monitor.c',
  'gb-color-picker-document-monitor.h',
  'gb-color-picker-prefs.c',
  'gb-color-picker-prefs.h',
  'gb-color-picker-prefs-list.c',
  'gb-color-picker-prefs-palette-list.h',
  'gb-color-picker-prefs-palette-list.c',
  'gb-color-picker-prefs-list.h',
  'gb-color-picker-prefs-palette-row.c',
  'gb-color-picker-prefs-palette-row.h',
  'gb-color-picker-private.h',
]

gnome_builder_plugins_deps += [libgstyle_dep]
gnome_builder_plugins_sources += files(color_picker_sources)
gnome_builder_plugins_sources += color_picker_resources[0]

install_data('gsettings/org.gnome.builder.plugins.color_picker_plugin.gschema.xml',
  install_dir: schema_dir,
)

endif
