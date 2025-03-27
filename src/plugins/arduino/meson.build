if get_option('plugin_arduino')

libyaml_dep = dependency('yaml-0.1')
json_glib_dep = dependency('json-glib-1.0')

plugins_sources += files([
  'gbp-arduino-application-addin.c',
  'gbp-arduino-board-option-row.c',
  'gbp-arduino-board-option.c',
  'gbp-arduino-board-options-widget.c',
  'gbp-arduino-board.c',
  'gbp-arduino-build-system-discovery.c',
  'gbp-arduino-build-system.c',
  'gbp-arduino-config-provider.c',
  'gbp-arduino-device-monitor.c',
  'gbp-arduino-device-provider.c',
  'gbp-arduino-libraries-editor.c',
  'gbp-arduino-libraries-manager.c',
  'gbp-arduino-library-info.c',
  'gbp-arduino-option-value.c',
  'gbp-arduino-pipeline-addin.c',
  'gbp-arduino-platforms-editor.c',
  'gbp-arduino-platforms-manager.c',
  'gbp-arduino-platform-editor-row.c',
  'gbp-arduino-platform-info.c',
  'gbp-arduino-platform.c',
  'gbp-arduino-port.c',
  'gbp-arduino-urls-editor.c',
  'gbp-arduino-profile.c',
  'gbp-arduino-string-row.c',
  'gbp-arduino-template-provider.c',
  'gbp-arduino-template.c',
  'gbp-arduino-tweaks-addin.c',
  'arduino-plugin.c',
])

plugin_arduino_resources = gnome.compile_resources(
  'arduino-resources',
  'arduino.gresource.xml',
  c_name: 'gbp_arduino',
)

plugins_sources += plugin_arduino_resources

plugins_deps += [libyaml_dep, json_glib_dep]

endif
