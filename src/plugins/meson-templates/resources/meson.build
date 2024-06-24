project('{{name}}',{{if language == "c"}} 'c',{{else if language == "c++"}} ['cpp', 'c'],{{else if language == "vala"}} ['c', 'vala'],{{else if language == "c♯"}} 'cs',{{else if language == "rust"}} 'rust', {{end}}

          version: '{{project_version}}',
    meson_version: '>= 1.0.0',
  default_options: [ 'warning_level=2', 'werror=false',{{if language == "c"}} 'c_std=gnu11',{{else if language == "c++"}} 'cpp_std=gnu++2a',{{end}} ],
)

{{if enable_i18n}}
i18n = import('i18n')
{{end}}
{{if enable_gnome}}
gnome = import('gnome')
{{end}}
{{if language == "c"}}
cc = meson.get_compiler('c')
{{end}}
{{if language == "vala"}}
valac = meson.get_compiler('vala')
{{end}}

{{if language == "c"}}
config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
{{if enable_i18n}}
config_h.set_quoted('GETTEXT_PACKAGE', '{{name}}')
config_h.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
{{end}}
configure_file(output: 'config.h', configuration: config_h)
add_project_arguments(['-I' + meson.project_build_root()], language: 'c')
{{end}}

{{if language == "vala"}}
{{if !is_cli}}
srcdir = meson.project_source_root() / 'src'

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version ())
{{if enable_i18n}}
config_h.set_quoted('GETTEXT_PACKAGE', '{{name}}')
config_h.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
{{end}}
configure_file(output: 'config.h', configuration: config_h)

config_dep = valac.find_library ('config', dirs: srcdir)
config_inc = include_directories('.')

add_project_arguments('-DGETTEXT_PACKAGE="' + meson.project_name () + '"', language: 'c')
{{end}}
{{end}}

{{if language == "c"}}
project_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')
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
