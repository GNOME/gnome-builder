if get_option('plugin_shellcmd')

plugins_sources += files([
  'shellcmd-plugin.c',
  'gbp-shellcmd-application-addin.c',
  'gbp-shellcmd-command.c',
  'gbp-shellcmd-command-editor.c',
  'gbp-shellcmd-command-model.c',
  'gbp-shellcmd-command-provider.c',
  'gbp-shellcmd-command-row.c',
  'gbp-shellcmd-list.c',
  'gbp-shellcmd-preferences-addin.c',
])

plugin_shellcmd_enum_headers = [
  'gbp-shellcmd-command.h',
]

plugin_shellcmd_enums = gnome.mkenums_simple('gbp-shellcmd-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-gui.h>',
         sources: plugin_shellcmd_enum_headers,
)

plugin_shellcmd_resources = gnome.compile_resources(
  'shellcmd-resources',
  'shellcmd.gresource.xml',
  c_name: 'gbp_shellcmd',
)

plugins_sources += plugin_shellcmd_enums
plugins_sources += plugin_shellcmd_resources
plugins_include_directories += [include_directories('.')]

endif
