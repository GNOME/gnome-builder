if get_option('plugin_shellcmd')

plugins_sources += files([
  'shellcmd-plugin.c',
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

endif
