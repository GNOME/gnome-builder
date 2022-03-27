project('{{name}}',{{if language == "c"}} 'c',{{else if language == "c++"}} ['cpp', 'c'],{{else if language == "vala"}} ['c', 'vala'],{{else if language == "c♯"}} 'cs',{{else if language == "rust"}} 'rust', {{end}}

          version: '{{project_version}}',
    meson_version: '>= 0.59.0',
  default_options: [ 'warning_level=2',
                     'werror=false',
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

{{if enable_gnome}}
gnome = import('gnome')
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
  '-I' + meson.project_build_root(),
], language: 'c')
{{end}}

{{if language == "rust"}}
cargo_sources = files(
  'Cargo.toml',
  'Cargo.lock',
)
{{end}}

{{if enable_gnome}}
subdir('data')
{{end}}
subdir('src')
{{if enable_i18n}}
subdir('po')
{{end}}

{{if enable_gnome}}
gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
{{end}}
