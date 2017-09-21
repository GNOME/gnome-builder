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
  color_picker_resources[0],
]

color_picker_deps = plugin_deps + [
  libgstyle_dep,
]

shared_module('color-picker-plugin', color_picker_sources,
   dependencies: color_picker_deps,
      link_args: plugin_link_args,
   link_depends: plugin_link_deps,
        install: true,
    install_dir: plugindir,
  install_rpath: pkglibdir_abs,
)

install_data('gsettings/org.gnome.builder.plugins.color_picker_plugin.gschema.xml',
  install_dir: schema_dir,
)

configure_file(
          input: 'color-picker.plugin',
         output: 'color-picker.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
