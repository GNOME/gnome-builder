if get_option('plugin_shellcmd')

plugins_sources += files([
  'shellcmd-plugin.c',
  'gbp-shellcmd-command-dialog.c',
  'gbp-shellcmd-command-model.c',
  'gbp-shellcmd-run-command.c',
  'gbp-shellcmd-run-command-provider.c',
  'gbp-shellcmd-search-provider.c',
  'gbp-shellcmd-search-result.c',
  'gbp-shellcmd-shortcut-provider.c',
  'gbp-shellcmd-tweaks-addin.c',
])

plugin_shellcmd_enum_headers = [
  'gbp-shellcmd-run-command.h',
]

plugin_shellcmd_enums = gnome.mkenums_simple('gbp-shellcmd-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
         sources: plugin_shellcmd_enum_headers,
  install_header: false,
)

plugin_shellcmd_resources = gnome.compile_resources(
  'shellcmd-resources',
  'shellcmd.gresource.xml',
  c_name: 'gbp_shellcmd',
)

plugins_sources += plugin_shellcmd_resources
plugins_sources += plugin_shellcmd_enums
plugins_include_directories += [include_directories('.')]

install_data(['org.gnome.builder.shellcmd.gschema.xml'], install_dir: schema_dir)
install_data(['org.gnome.builder.shellcmd.command.gschema.xml'], install_dir: schema_dir)

endif
