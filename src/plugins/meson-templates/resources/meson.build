project('{{name}}', {{if language == "c"}}'c',{{else if language == "c++"}}['cpp', 'c'],{{else if language == "vala"}}['c', 'vala'],{{else if language == "c♯"}}'cs',{{end}} 
          version: '{{project_version}}',
    meson_version: '>= 0.50.0',
  default_options: [ 'warning_level=2',
{{if language == "c"}}
                     'c_std=gnu11',
{{end}}
{{if language == "c++"}}
                     'cpp_std=gnu++2a',
{{end}}
                   ],
)

{{if enable_i18n}}
i18n = import('i18n')
{{end}}

{{if language == "c"}}
config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
{{if enable_i18n}}
config_h.set_quoted('GETTEXT_PACKAGE', '{{name}}')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
{{end}}
configure_file(
  output: '{{prefix}}-config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.build_root(),
], language: 'c')
{{end}}

{{if enable_gnome}}
subdir('data')
{{end}}
subdir('src')
{{if enable_i18n}}
subdir('po')
{{end}}

{{if enable_gnome}}
meson.add_install_script('build-aux/meson/postinstall.py'){{end}}
