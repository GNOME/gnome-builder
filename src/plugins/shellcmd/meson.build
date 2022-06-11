if get_option('plugin_shellcmd')

plugins_sources += files([
  'shellcmd-plugin.c',
  'gbp-shellcmd-list.c',
  'gbp-shellcmd-preferences-addin.c',
  'gbp-shellcmd-run-command.c',
  'gbp-shellcmd-run-command-provider.c',
])

plugin_shellcmd_enum_headers = [
  'gbp-shellcmd-command.h',
]

plugin_shellcmd_resources = gnome.compile_resources(
  'shellcmd-resources',
  'shellcmd.gresource.xml',
  c_name: 'gbp_shellcmd',
)

plugins_sources += plugin_shellcmd_resources
plugins_include_directories += [include_directories('.')]

install_data(['org.gnome.builder.shellcmd.gschema.xml'], install_dir: schema_dir)
install_data(['org.gnome.builder.shellcmd.command.gschema.xml'], install_dir: schema_dir)

endif
