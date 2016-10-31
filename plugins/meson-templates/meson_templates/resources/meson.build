project('{{name}}', '{{language}}',
  version: '{{project_version}}',
  meson_version: '>= 0.36.0',
)

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', '{{name}}')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
configure_file(
  output: 'config.h',
  configuration: config_h,
)
add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(),
], language: '{{language}}')

subdir('src')
{{if enable_i18n}}subdir('po'){{end}}
